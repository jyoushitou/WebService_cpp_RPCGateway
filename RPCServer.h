// RPCServer.h
// 基于 Boost.Beast + Boost.Asio 的 WebSocket JSON-RPC 服务端
// 功能：替代 HTTP API，通过 WebSocket 实现前后端 RPC 交互
// 使用 JSON-RPC 2.0 协议

#ifndef RPCSERVER_H
#define RPCSERVER_H

#include <iostream>
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <vector>
#include <map>
#include <sstream>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json.hpp>

#include "MyMySQL.h"
#include "Utils.h"
#include "UserThread.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace json = boost::json;
using tcp = net::ip::tcp;

namespace RPC {

    // RPC 服务端配置
    struct RPCServerConfig {
        std::string host = "0.0.0.0";
        int port = 60906;                       // 与原来 HTTP 端口一致
        int threadCount = 4;                    // I/O 线程数
    };

    // RPC 请求结构体
    struct RPCRequest {
        std::string jsonrpc;                    // "2.0"
        std::string method;                     // 方法名
        json::value params;                     // 参数
        json::value id;                         // 请求 ID
        std::string token;                      // 认证 token（从 params 中提取）
    };

    // RPC 响应结构体
    struct RPCResponse {
        json::value id;
        bool isError = false;
        int errorCode = 0;
        std::string errorMessage;
        json::value result;
    };

    // RPC 方法处理器
    using RPCMethodHandler = std::function<RPCResponse(const RPCRequest&, MySQL::mysql*)>;

    // RPC 服务端类
    class RPCServer {
    public:
        explicit RPCServer(const RPCServerConfig& config = RPCServerConfig());
        ~RPCServer();

        // ===== 服务器生命周期 =====
        bool Start();
        void Stop();
        bool IsRunning() const;

        // ===== 方法注册 =====
        void RegisterMethod(const std::string& method, RPCMethodHandler handler);

        // ===== 批量注册所有业务方法 =====
        void RegisterBusinessMethods(MySQL::mysql* db);

        // ===== 获取数据库连接 =====
        void SetDatabase(MySQL::mysql* db) { m_db = db; }

    private:
        void DoAccept();
        void OnAccept(beast::error_code ec, tcp::socket socket);
        void HandleSession(tcp::socket socket);

        // JSON-RPC 处理
        std::string HandleRPCRequest(const std::string& requestStr);
        RPCResponse HandleRPCMethod(const RPCRequest& req);
        RPCRequest ParseRPCRequest(const std::string& jsonStr);
        std::string SerializeRPCResponse(const RPCResponse& res);

        RPCServerConfig m_config;
        std::unique_ptr<net::io_context> m_ioc;
        std::unique_ptr<tcp::acceptor> m_acceptor;
        std::atomic<bool> m_running{false};

        // 方法路由表
        std::unordered_map<std::string, RPCMethodHandler> m_methods;
        std::mutex m_methodsMutex;

        // I/O 线程池
        std::vector<std::thread> m_threads;

        // 数据库连接
        MySQL::mysql* m_db = nullptr;
    };

} // namespace RPC

#endif // !RPCSERVER_H