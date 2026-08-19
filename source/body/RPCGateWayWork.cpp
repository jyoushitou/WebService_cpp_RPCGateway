#pragma once

#include "RPCGateWayWork.h"

// 服务器ID
int Utils::serviceID = 1;

//===服务器===

// 启动 HTTP 服务器的函数
void RunHttpServer(int tcp_port, unsigned short http_port)
{
    // 注册回调
    Utils::Exit::RegisterStopCallback(
        []()
        {
            // 停止 HTTP 服务器
            if (server_ptr)
                server_ptr->Stop();

            // 清空 pending 表（避免后续客户端回包指向已关闭的 HttpSession）
            {
                std::lock_guard<std::mutex> lock(g_pending_mutex);
                g_pending.clear();
            }

            // 1.3 保存所有连接副本并停止所有后端客户端连接
            std::vector<std::shared_ptr<ConnItem>> active_conns;
            {
                std::lock_guard<std::mutex> lock(conns_mutex);
                for (auto& conn : conns)
                {
                    if (conn && conn->client)
                    {
                        active_conns.push_back(conn);
                        conn->client->Stop();
                    }
                }
            }

            // 确定性等待所有客户端 IO 线程结束
            constexpr int max_wait_ms = 5000;
            const auto wait_start = std::chrono::steady_clock::now();
            while (true)
            {
                bool all_done = true;
                for (auto& conn : active_conns)
                {
                    if (!conn->io_finished.load(std::memory_order_acquire))
                    {
                        all_done = false;
                        break;
                    }
                }
                if (all_done)
                    break;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - wait_start) > std::chrono::milliseconds(max_wait_ms))
                {
                    Utils::Out::Out_Err("等待客户端 IO 线程结束超时，继续退出流程");
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // 注意：不要在停止回调中 reset server_ptr / http_io_ptr！
            // io_thread 可能仍在 http_io->run() 中执行，
            // 提前析构 io_context 会导致 use-after-free → abort。
            // 资源释放统一放在 RunHttpServer 中 join 完 io_thread 之后。
        });

    // 注册信号处理
    std::signal(SIGINT, Utils::Exit::Onsignal);
#ifdef _WIN32
    SetConsoleCtrlHandler(Utils::Exit::ConsoleCtrlHandler, TRUE);
#endif

    Utils::Out::Out_Msg("正在启动 HTTP 服务器（TCP端口=" + std::to_string(tcp_port) +
                        ", HTTP端口=" + std::to_string(http_port) + "）");

    // 创建 io_context
    http_io_ptr = std::make_unique<boost::asio::io_context>();

    // 创建 TCP 端点
    boost::asio::ip::tcp::endpoint ep(boost::asio::ip::tcp::v4(), tcp_port);

    // 创建 HttpServer 实例（绑定全局堆分配 io_context）
    server_ptr = std::make_shared<Net::Server::HttpServer::HttpServer>(*http_io_ptr, ep, http_port);

    // 注册回调
    auto http_server = std::dynamic_pointer_cast<Net::Server::HttpServer::HttpServer>(server_ptr);
    http_server->SetHandleVueRequestCallback(HandleVueBiz);

    // 开始接收 HTTP 请求
    std::dynamic_pointer_cast<Net::Server::HttpServer::HttpServer>(server_ptr)->StartHttpAccept();

    Utils::Out::Out_Msg("HTTP 服务器已启动，等待 Vue 前端请求...");

    // io_context 在独立线程运行（捕获全局堆分配指针，而非局部变量引用）
    auto http_io = http_io_ptr.get();
    std::thread io_thread([http_io]() { http_io->run(); });

    // 主线程等待退出标志
    Utils::Exit::WaitExit();

    // 等待 io_context 线程结束
    if (io_thread.joinable())
        io_thread.join();

    // 释放顺序必须正确：先 server，再 http_io（必须在线程 join 之后，否则 abort）
    server_ptr.reset();
    http_io_ptr.reset();
}

// 转发回调
std::string HandleVueBiz(std::shared_ptr<Net::Server::HttpServer::HttpSession> session, const std::string& path,
                         const std::string& cmd_str)
{
    Utils::Out::Out_Msg("[业务层] 收到命令: " + cmd_str + ", path=" + path);

    // 3. 分配 msg_id + 记录回包会话
    unsigned long long msg_id = Net::g_net_msg_id.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_pending_mutex);
        g_pending[msg_id] = PendingRequest{session, path};
    }

    std::shared_ptr<Net::Client::Client> client;
    {
        std::lock_guard<std::mutex> lock(conns_mutex);
        if (conns.empty() || !conns[0] || !conns[0]->client)
        {
            // 没有后端连接：立即清理刚插入的 pending 条目，避免泄漏
            std::lock_guard<std::mutex> lock2(g_pending_mutex);
            g_pending.erase(msg_id);
            return "{\"code\":500,\"msg\":\"no backend connection\"}";
        }
        client = conns[0]->client;
    }

    // 4. 直接把字符串发给目标连接（微服务端收到这个字符串后自行处理）
    client->ToSend(msg_id, cmd_str);

    // 不在此处返回响应，由 ClientWork 收到微服务结果后通过 session->AsyncSendResponse(msg) 异步返回
    return "";
}

//===客户端===

// 工作函数
void ClientWork(size_t idx, unsigned long long msg_id, const std::string& msg)
{
    // 查表：这个 msg_id 是不是有 HTTP 会话在等
    std::shared_ptr<Net::Server::HttpServer::HttpSession> session;
    {
        std::lock_guard<std::mutex> lock(g_pending_mutex);
        auto it = g_pending.find(msg_id);
        if (it != g_pending.end())
        {
            session = it->second.session;
            // 取走就删，防止堆积
            g_pending.erase(it);
        }
    }

    // 有就回给前端（AsyncSendResponse 内部 post 到 HTTP io_context 线程执行，避免跨线程操作 socket）
    if (session)
    {
        session->AsyncSendResponse(msg); // msg 就是微服务返回的结果
    }
    else
    {
        // 没有对应HTTP请求，就是微服务主动推送，按原样打印
        Utils::Out::Out_Net_Msg(msg_id, "线程" + std::to_string(idx) + "收到消息: " + msg);
    }
}

// 关闭函数
void Close(size_t idx)
{
    Utils::Out::Out_Msg("正在关闭:" + std::to_string(static_cast<int>(10 + idx)) + "线程");

    // 从 conns 中移除对应槽位（后续 CreateConnection 会复用该位置）
    {
        std::lock_guard<std::mutex> lock(conns_mutex);
        if (idx < conns.size() && conns[idx])
        {
            // 关键修复：本回调正运行在 conns[idx]->io_thread 上（IO 线程）。
            // 若直接置空，ConnItem 析构时销毁 joinable 的 std::thread → std::terminate → abort()
            // 因此必须先 detach，让线程自然退出
            if (conns[idx]->io_thread.joinable())
                conns[idx]->io_thread.detach();
            conns[idx] = nullptr;
        }
    }

    // 仅当主动退出时才唤醒主线程（g_exit_flag 由 GracefulShutdown / Ctrl+C 设置）
    if (Utils::Exit::exit_flag.load())
    {
        return;
    }

    // 意外断开：不退出，保持 HTTP 服务继续运行，并尝试重连
    Utils::Out::Out_Msg("连接断开，2 秒后尝试重连...");
    std::thread(
        [idx]()
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            CreateConnection(idx, "127.0.0.1", "60000");
        })
        .detach();
}

// 创建连接
void CreateConnection(size_t idx, const std::string& host, const std::string& port)
{
    Utils::Out::Out_Msg("正在连接");

    // 线程数统计自增
    total_conns.fetch_add(1);

    // 创建io_context的线程指针
    auto conn = std::make_shared<ConnItem>();

    // 创建专属io_context
    conn->io = std::make_unique<boost::asio::io_context>();

    // 创建线程独立的客户端
    conn->client = std::make_shared<Net::Client::Client>(*conn->io);

    // 注册回调（捕获 idx，避免共享状态）
    conn->client->SetMessageCallback([idx](unsigned long long id, std::string msg) { ClientWork(idx, id, msg); });

    // 设置关闭回调
    conn->client->SetCloseCallback([idx]() { Close(idx); });

    // 异步连接，先发起连接保证 io_context 中有任务
    conn->client->Connect(host, port);

    // 每连接 1 个线程驱动自己的 io_context
    conn->io_thread = std::thread(
        [conn]
        {
            conn->io->run(); // 阻塞直到该连接 Stop() 后 io_context 无任务
            // run() 返回后说明该连接所有异步操作已结束，标记完成（供退出时确定性等待）
            conn->io_finished.store(true, std::memory_order_release);
        });

    // 将独立io_context加入数组（复用 idx 槽位，避免无限增长）
    {
        std::lock_guard<std::mutex> lock(conns_mutex);
        if (idx >= conns.size())
            conns.resize(idx + 1);
        conns[idx] = conn;
    }
}