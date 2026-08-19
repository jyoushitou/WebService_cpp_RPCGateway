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

// 连接数
constexpr int ServiceNum = 1;

// 加锁变量
inline std::mutex conns_mutex;
// 储存18 条连接
inline std::vector<std::shared_ptr<ConnItem>> conns;
// 总连接数（CreateConnection 中递增）
inline std::atomic<size_t> total_conns{0};
// 已关闭连接数（OnClose 中递增）
inline std::atomic<size_t> closed_conns{0};

//===服务器参数===

// 全局服务器指针，供信号处理函数使用
inline std::shared_ptr<Net::Server::Server> server_ptr;

// HTTP 服务器专用 io_context（堆分配，避免 RunHttpServer 返回后局部 io 析构导致悬垂引用）
inline std::unique_ptr<boost::asio::io_context> http_io_ptr;

// msg_id → PendingRequest 映射
inline std::mutex g_pending_mutex;
inline std::unordered_map<unsigned long long, PendingRequest> g_pending;

//===服务器函数===

// HTTP服务器启动函数
void RunHttpServer(int tcp_port, unsigned short http_port);

// 转发函数
std::string HandleVueBiz(std::shared_ptr<Net::Server::HttpServer::HttpSession> session, const std::string& path,
                         const std::string& cmd_str);

//===客户端函数===
// 客户端的

// 工作函数
void ClientWork(size_t idx, unsigned long long msg_id, const std::string& msg);

// 关闭函数
void Close(size_t idx);

// 启动客户端
void CreateConnection(size_t idx, const std::string& host, const std::string& port);
