// NetHttpServer.h
#pragma once

#include <vector>
#include <functional>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include "Utils.h"
#include "NetConnection.h"
#include "NetServer.h"

namespace Net
{
    namespace Server
    {
        namespace HttpServer
        {
            // 向前声明
            class HttpSession;

            // Http接收服务端
            class HttpServer : public Server
            {
            public:
                // 构造函数（额外传入 http_port 用于 Vue 前端）
                HttpServer(boost::asio::io_context& io, boost::asio::ip::tcp::endpoint ep, int serviceID_,
                           unsigned short http_port_);

                // 开始接受 HTTP 连接
                void StartHttpAccept();

                // 重写 Stop：同时关闭 HTTP acceptor
                void Stop() override;

                // 业务回调类型
                using VueRequestCallback = std::function<std::string(
                    std::shared_ptr<HttpSession> session, const std::string& path, const std::string& cmd_str)>;

                // 注册回调
                void SetHandleVueRequestCallback(VueRequestCallback cb);

                // 处理 Vue3 请求，返回 JSON 响应字符串
                std::string HandleVueRequest(std::shared_ptr<HttpSession> session, const std::string& path,
                                             const std::string& body);

            private:
                // 业务回调存储
                // HTTP 监听器
                boost::asio::ip::tcp::acceptor http_acceptor;
                // HTTP 端口
                unsigned short http_port;
                // HTTP 会话集合
                std::vector<std::shared_ptr<HttpSession>> http_sessions;

                // 回调函数
                VueRequestCallback handle_vue_cb;
            };

            // HTTP 会话：继承 Session
            class HttpSession : public Session
            {
            public:
                HttpSession(boost::asio::io_context& io, boost::asio::ip::tcp::socket sock, int serviceID_,
                            HttpServer* http_server);

                // 安全获取自身 shared_ptr（重载返回 HttpSession 类型，避免 protected 访问问题）
                std::shared_ptr<HttpSession> shared_from_this();

                // 重写 Start：不读二进制头，改为读 HTTP 请求
                void Start() override;

                // 发送 HTTP 响应给前端（必须在 io_context 线程内调用）
                void HttpSendResponse(const std::string& body);

                // 线程安全异步发送：任何线程均可调用，内部 post 到 HTTP io_context 线程执行
                void AsyncSendResponse(const std::string& body);

                // 关闭连接
                void Stop();

            protected:
                // 读取请求体
                void ReadBody();
                // 处理请求（解析 body 并回复）
                void HandleRequest(const std::string& body);

                // Boost.Beast 成员
                boost::beast::flat_buffer buffer_;
                boost::beast::http::request_parser<boost::beast::http::string_body> parser_;

                // 所属 HttpServer
                HttpServer* http_server;
                // 请求方法、路径
                std::string method_;
                std::string path_;
            };
        } // namespace HttpServer
    } // namespace Server
} // namespace Net