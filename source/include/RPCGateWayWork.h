// RPCGateWayWork
#pragma once

#include "NetClient.h"
#include "NetServer.h"
#include "NetHttpServer.h"

#include "Utils.h"
#include "Message.h"

#include <boost/asio.hpp>

#include <memory>
#include <thread>
#include <iostream>
#include <csignal>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// io_context管理结构
struct ConnItem
{
    std::unique_ptr<boost::asio::io_context> io;
    std::shared_ptr<Net::Client::Client> client;
    std::thread io_thread;
    // io_thread.run() 返回后置为 true（表示该连接所有异步操作已结束，可安全释放关联资源）
    std::atomic<bool> io_finished{false};
};

// 一条 HTTP 请求对应的上下文：记住是哪个会话在等回包
struct PendingRequest
{
    // 等回包的 HTTP 会话
    std::shared_ptr<Net::Server::HttpServer::HttpSession> session;
    // 原来的请求路径
    std::string path;
};

//===客户端参数===

// 微服务ID
constexpr int ServiceID_RPCGateWay = 1;
// 连接数
constexpr int ServiceNum = 1;

// 加锁变量
inline std::mutex g_conns_mutex;
// 储存18 条连接
inline std::vector<std::shared_ptr<ConnItem>> g_conns;
// 运行标志（回调线程只碰这个）
inline std::atomic<bool> g_running{true};
// 总连接数（CreateConnection 中递增）
inline std::atomic<size_t> g_total_conns{0};
// 已关闭连接数（OnClose 中递增）
inline std::atomic<size_t> g_closed_conns{0};
// 退出事件（主线程等待它）
inline HANDLE g_exit_event = nullptr;

//===服务器参数===

// 全局服务器指针，供信号处理函数使用
inline std::shared_ptr<Net::Server::Server> g_server;

// HTTP 服务器专用 io_context（堆分配，避免 RunHttpServer 返回后局部 io 析构导致悬垂引用）
inline std::unique_ptr<boost::asio::io_context> g_http_io;
// 退出标志
inline std::atomic<bool> g_exit_flag{false};
// 防止 Stop() 被多次调用的标志
inline std::atomic<bool> g_stop_called{false};

// msg_id → PendingRequest 映射
inline std::mutex g_pending_mutex;
inline std::unordered_map<unsigned long long, PendingRequest> g_pending;

//===服务器函数===

// 统一优雅退出逻辑（保证只执行一次）
void GracefulShutdown();

// Ctrl+C / SIGTERM 处理函数
void OnSignal(int);

// HTTP服务器启动函数
void RunHttpServer(int tcp_port, unsigned short http_port, int ServiceID_);

// 转发函数
std::string HandleVueBiz(std::shared_ptr<Net::Server::HttpServer::HttpSession> session, const std::string& path,
                         const std::string& cmd_str);

//===客户端函数===
// 客户端的
inline BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:     // Ctrl+C
    case CTRL_BREAK_EVENT: // Ctrl+Break
    case CTRL_CLOSE_EVENT: // 用户点关闭窗口
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT: // 系统关机
    {
        // 在 Windows 专用回调线程中执行：
        // ⚠️ 只允许做这两件事，绝不打印日志、绝不调 Stop()（会内部 post，有线程安全问题风险）
        g_running = false;
        // 唤醒主线程
        g_exit_flag = true;

        SetEvent(g_exit_event);
        return TRUE;
    }
    default:
        return FALSE; // 其他信号交给系统默认
    }
}

// 工作函数
void ClientWork(size_t, int, unsigned long long, const std::string&);

// 关闭函数
void Close(size_t, int);

// 启动客户端
void CreateConnection(size_t, int, const std::string&, const std::string&);
