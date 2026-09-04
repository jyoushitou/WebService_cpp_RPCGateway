#pragma once

#include "RPCGateWayWork.h"
#include "Blog.pb.h"

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

            // 清空 pending 表
            {
                std::lock_guard<std::mutex> lock(g_pending_mutex);
                g_pending.clear();
            }

            // 取出唯一内网连接并停止（g_conn_mutex 单独加锁，不嵌套）
            std::shared_ptr<ConnItem> old_conn;
            {
                std::lock_guard<std::mutex> lock(conn_mutex);
                old_conn = std::move(conn);
                if (old_conn && old_conn->io && old_conn->client)
                {
                    auto client_io = old_conn->io.get();
                    auto client = old_conn->client;
                    boost::asio::post(*client_io, [client]() { client->Stop(); });
                }
            }

            // 确定性等待该连接 IO 线程结束（最多 5 秒）
            if (old_conn)
            {
                constexpr int max_wait_ms = 5000;
                const auto wait_start = std::chrono::steady_clock::now();
                while (!old_conn->io_finished.load(std::memory_order_acquire))
                {
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - wait_start) > std::chrono::milliseconds(max_wait_ms))
                    {
                        Utils::Out::Out_Err("等待客户端 IO 线程结束超时，继续退出流程");
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        });

    // 注册信号处理
    std::signal(SIGINT, Utils::Exit::Onsignal);
#ifdef _WIN32
    SetConsoleCtrlHandler(Utils::Exit::ConsoleCtrlHandler, TRUE);
#endif

    Utils::Out::Out_Msg("正在启动 HTTP 服务器（TCP端口：" + std::to_string(tcp_port) + ", HTTP端口：" +
                        std::to_string(http_port) + "）");

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

    // 启动空闲 Session 清理线程（每 5 秒检查一次，空闲 60 秒清理）
    std::thread cleanup_thread(
        [http_server]()
        {
            constexpr long long kIdleTimeoutMs = 60 * 1000; // 60 秒无活动则关闭
            constexpr int kCheckIntervalMs = 5000;          // 每 5 秒检查一次

            while (!Utils::Exit::exit_flag.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(kCheckIntervalMs));

                // 睡醒后再次检查：退出过程中不再执行清理
                if (Utils::Exit::exit_flag.load())
                    break;

                // 清理空闲超时的 HTTP 会话
                http_server->CleanupIdleSessions(kIdleTimeoutMs);
            }

            Utils::Out::Out_Msg("HTTP Session 清理线程退出");
        });

    // 等待 io_context 线程结束
    if (io_thread.joinable())
        io_thread.join();

    // 置退出标志，唤醒清理线程退出
    Utils::Exit::exit_flag.store(true);

    // 等待清理线程结束
    if (cleanup_thread.joinable())
        cleanup_thread.join();

    // 释放顺序必须正确：先 server，再 http_io（必须在线程 join 之后，否则 abort）
    server_ptr.reset();
    http_io_ptr.reset();
}

// 转发回调
std::string HandleVueBiz(std::shared_ptr<Net::Server::HttpServer::HttpSession> session,
                         const Net::Server::HttpServer::Url url)
{
    Utils::Out::Out_Msg("收到命令vue3命令: " + std::to_string(url.head.command()) + "目标服务器" +
                        ServiceID[url.head.serviceid()]);
    // 分配 msg_id + 记录回包会话
    unsigned long long msg_id = Net::g_net_msg_id.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_pending_mutex);
        g_pending[msg_id] = PendingRequest{session, url};
    }

    std::shared_ptr<Net::Client::Client> client;
    {
        std::lock_guard<std::mutex> lock(conn_mutex);
        if (conn && conn->client)
            client = conn->client;
    }

    // 没有后端连接：单独锁 g_pending_mutex 删条目，返回错误
    if (!client)
    {
        std::lock_guard<std::mutex> lock(g_pending_mutex);
        g_pending.erase(msg_id);
        return "{\"code\":500,\"msg\":\"no backend connection\"}";
    }

    // 构建发送消息变量
    std::string msg = "";

    // 变量
    if (url.head.serviceid() == 1)
    {
        BlogRouter(url, msg);
    }

    // 把转换好的string发给服务器
    client->ToSend(msg_id, msg);

    // 不在此处返回响应，由 ClientWork 收到微服务结果后通过 session->AsyncSendResponse(msg) 异步返回
    return "";
}

//===客户端===

// 工作函数
void ClientWork(unsigned long long msg_id, const std::string& msg)
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
        Utils::Out::Out_Net_Msg(msg_id, "收到微服务主动推送: " + msg);
    }
}

// 关闭函数
void Close()
{
    Utils::Out::Out_Msg("内网连接断开");

    // 防止关断回调重入
    if (conn_closed.exchange(true))
        return;

    if (Utils::Exit::exit_flag.load())
        return;

    Utils::Out::Out_Msg("2 秒后尝试重连...");
    std::thread(
        []()
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // 退出期间不再重连（防止创建新线程后无法 join → std::terminate → abort）
            if (Utils::Exit::exit_flag.load())
                return;

            // 先安全收藏旧连接（不在此处直接 reset，等待 IO 线程退出）
            std::shared_ptr<ConnItem> old_conn;
            {
                std::lock_guard<std::mutex> lock(conn_mutex);
                old_conn = std::move(conn);
                conn = nullptr;
                conn_closed.store(false); // 允许下一次重连
            }

            // 等待旧 IO 线程结束（最多 3 秒），结束后再重建
            if (old_conn)
            {
                if (old_conn->io && old_conn->client)
                {
                    auto client_io = old_conn->io.get();
                    auto client = old_conn->client;
                    boost::asio::post(*client_io, [client]() { client->Stop(); });
                }
                for (int i = 0; i < 30 && !old_conn->io_finished.load(); ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            // old_conn 在此作用域结束后安全析构（IO 线程已退出）

            // 复用首次连接时记录的后端地址，不写死
            CreateConnection(g_backend_host, g_backend_port);
        })
        .detach();
}

// 创建连接（单连接）
void CreateConnection(const std::string& host, const std::string& port)
{
    // 记录后端地址，断线重连时复用（不写死）
    g_backend_host = host;
    g_backend_port = port;

    Utils::Out::Out_Msg("正在连接内网服务: " + host + ":" + port);

    // 创建io_context的线程指针
    auto new_conn = std::make_shared<ConnItem>();

    // 创建专属io_context
    new_conn->io = std::make_unique<boost::asio::io_context>();

    // 创建线程独立的客户端（使用 new_conn 自己的 io，不能用全局 conn）
    new_conn->client = std::make_shared<Net::Client::Client>(*new_conn->io);

    // 注册返回回调
    new_conn->client->SetMessageCallback([](unsigned long long id, std::string msg) { ClientWork(id, msg); });
    // 注册关闭回调
    new_conn->client->SetCloseCallback([]() { Close(); });

    // 创建连接
    new_conn->client->Connect(host, port);

    // 创建线程
    new_conn->io_thread = std::thread(
        [new_conn]
        {
            new_conn->io->run();
            new_conn->io_finished.store(true, std::memory_order_release);
        });

    {
        std::lock_guard<std::mutex> lock(conn_mutex);
        conn = new_conn;
    }
}

// Blog路由
void BlogRouter(const Net::Server::HttpServer::Url url, std::string& msg)
{
    Blog::router blog;
    blog.mutable_head()->CopyFrom(url.head);
    if (url.head.command() == 1)
    {
        blog.set_body("");
    }
    else if (url.head.command() == 2)
    {
        blog.set_body(url.body);
    }
    else
    {
    }
    blog.SerializeToString(&msg);
}