// RPCServer.cpp
// 基于 Boost.Beast + Boost.Asio 的 WebSocket JSON-RPC 服务端实现
// 替代原有的 HTTP API，通过 WebSocket 实现前后端 RPC 交互

#include "../header/RPCServer.h"

namespace RPC {

    // ===== 构造函数 =====
    RPCServer::RPCServer(const RPCServerConfig& config)
        : m_config(config)
        , m_ioc(std::make_unique<net::io_context>(config.threadCount))
    {
        Utils::Out_System("RPC 服务器已创建，监听端口: " + std::to_string(config.port));
    }

    // ===== 析构函数 =====
    RPCServer::~RPCServer() {
        Stop();
    }

    // ===== 启动服务器 =====
    bool RPCServer::Start() {
        try {
            Utils::Out_System("RPC 服务器正在启动...");

            // 1. 创建 TCP acceptor
            tcp::endpoint endpoint(net::ip::make_address(m_config.host), m_config.port);
            m_acceptor = std::make_unique<tcp::acceptor>(*m_ioc, endpoint);

            m_running = true;
            Utils::Out_System("RPC 服务器已启动，监听 " + m_config.host + ":" + std::to_string(m_config.port));

            // 2. 开始接受连接
            DoAccept();

            // 3. 启动 I/O 线程池
            for (int i = 0; i < m_config.threadCount; ++i) {
                m_threads.emplace_back([this]() {
                    m_ioc->run();
                });
            }

            return true;

        } catch (const std::exception& e) {
            Utils::Out_System_Error("RPC 服务器启动失败: " + std::string(e.what()));
            m_running = false;
            return false;
        }
    }

    // ===== 停止服务器 =====
    void RPCServer::Stop() {
        if (!m_running.load()) return;

        m_running = false;
        Utils::Out_System("RPC 服务器正在停止...");

        // 停止 acceptor
        if (m_acceptor) {
            beast::error_code ec;
            m_acceptor->close(ec);
        }

        // 停止 I/O 上下文
        m_ioc->stop();

        // 等待所有线程退出
        for (auto& t : m_threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        m_threads.clear();

        Utils::Out_System("RPC 服务器已停止");
    }

    // ===== 检查是否运行中 =====
    bool RPCServer::IsRunning() const {
        return m_running.load();
    }

    // ===== 注册方法 =====
    void RPCServer::RegisterMethod(const std::string& method, RPCMethodHandler handler) {
        std::lock_guard<std::mutex> lock(m_methodsMutex);
        m_methods[method] = handler;
        Utils::Out_System("RPC 方法已注册: " + method);
    }

    // ===== 接受连接 =====
    void RPCServer::DoAccept() {
        if (!m_running) return;

        m_acceptor->async_accept(
            [this](beast::error_code ec, tcp::socket socket) {
                if (!ec) {
                    Utils::Out_System("RPC 新客户端连接: " + socket.remote_endpoint().address().to_string());
                    // 为每个客户端创建独立线程处理
                    std::thread(&RPCServer::HandleSession, this, std::move(socket)).detach();
                }
                // 继续接受下一个连接
                DoAccept();
            });
    }

    // ===== 处理单个会话 =====
    void RPCServer::HandleSession(tcp::socket socket) {
        try {
            // 1. 创建 WebSocket 流
            websocket::stream<tcp::socket> ws(std::move(socket));

            // 2. 接受 WebSocket 握手
            ws.accept();

            Utils::Out_System("WebSocket 握手成功");

            // 3. 消息循环
            beast::flat_buffer buffer;
            while (m_running) {
                // 读取消息
                buffer.clear();
                beast::error_code ec;
                ws.read(buffer, ec);

                if (ec == websocket::error::closed) {
                    Utils::Out_System("WebSocket 客户端断开连接");
                    break;
                }
                if (ec) {
                    Utils::Out_System_Error("WebSocket 读取错误: " + ec.message());
                    break;
                }

                // 解析并处理 RPC 请求
                std::string requestStr = beast::buffers_to_string(buffer.data());
                std::string responseStr = HandleRPCRequest(requestStr);

                // 发送响应
                ws.write(net::buffer(responseStr));
            }

            // 4. 关闭 WebSocket
            ws.close(websocket::close_code::normal);

        } catch (const std::exception& e) {
            Utils::Out_System_Error("会话处理异常: " + std::string(e.what()));
        }
    }

    // ===== 解析 RPC 请求 =====
    RPCRequest RPCServer::ParseRPCRequest(const std::string& jsonStr) {
        RPCRequest req;
        try {
            json::value jv = json::parse(jsonStr);
            json::object& obj = jv.as_object();

            req.jsonrpc = obj["jsonrpc"].as_string().c_str();
            req.method = obj["method"].as_string().c_str();
            req.params = obj["params"];
            req.id = obj["id"];

            // 从 params 中提取 token（如果存在）
            if (req.params.is_object()) {
                json::object& paramsObj = req.params.as_object();
                if (paramsObj.contains("token")) {
                    req.token = paramsObj["token"].as_string().c_str();
                }
            }

        } catch (const std::exception& e) {
            Utils::Out_System_Error("解析 RPC 请求失败: " + std::string(e.what()));
        }
        return req;
    }

    // ===== 序列化 RPC 响应 =====
    std::string RPCServer::SerializeRPCResponse(const RPCResponse& res) {
        json::object response;
        response["jsonrpc"] = "2.0";
        response["id"] = res.id;

        if (res.isError) {
            json::object error;
            error["code"] = res.errorCode;
            error["message"] = res.errorMessage;
            response["error"] = error;
        } else {
            response["result"] = res.result;
        }

        return json::serialize(response);
    }

    // ===== 处理 RPC 请求 =====
    std::string RPCServer::HandleRPCRequest(const std::string& requestStr) {
        try {
            RPCRequest req = ParseRPCRequest(requestStr);
            RPCResponse res = HandleRPCMethod(req);
            return SerializeRPCResponse(res);
        } catch (const std::exception& e) {
            RPCResponse res;
            res.isError = true;
            res.errorCode = -32700;  // Parse error
            res.errorMessage = std::string("解析错误: ") + e.what();
            res.id = nullptr;
            return SerializeRPCResponse(res);
        }
    }

    // ===== 处理方法调用 =====
    RPCResponse RPCServer::HandleRPCMethod(const RPCRequest& req) {
        RPCResponse res;
        res.id = req.id;

        // 查找方法
        RPCMethodHandler handler;
        {
            std::lock_guard<std::mutex> lock(m_methodsMutex);
            auto it = m_methods.find(req.method);
            if (it == m_methods.end()) {
                res.isError = true;
                res.errorCode = -32601;  // Method not found
                res.errorMessage = "方法未找到: " + req.method;
                return res;
            }
            handler = it->second;
        }

        // 执行方法
        try {
            return handler(req, m_db);
        } catch (const std::exception& e) {
            res.isError = true;
            res.errorCode = -32603;  // Internal error
            res.errorMessage = std::string("内部错误: ") + e.what();
            return res;
        }
    }

    // ===== 批量注册所有业务方法 =====
    void RPCServer::RegisterBusinessMethods(MySQL::mysql* db) {
        m_db = db;

        // ===== user.login 用户登录 =====
        RegisterMethod("user.login", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
            RPCResponse res;
            res.id = req.id;

            if (!db) {
                res.isError = true;
                res.errorCode = -32000;
                res.errorMessage = "数据库不可用";
                return res;
            }

            json::object params = req.params.as_object();
            std::string name = params["name"].as_string().c_str();
            std::string password = params["password"].as_string().c_str();
            std::string deviceName = params.contains("device_name") ?
                params["device_name"].as_string().c_str() : "";
            std::string clientIp = params.contains("client_ip") ?
                params["client_ip"].as_string().c_str() : "unknown";
            std::string userAgent = params.contains("user_agent") ?
                params["user_agent"].as_string().c_str() : "";

            if (name.empty() || password.empty()) {
                res.isError = true;
                res.errorCode = -32602;
                res.errorMessage = "用户名或密码不能为空";
                return res;
            }

            if (deviceName.empty()) {
                if (!userAgent.empty()) {
                    deviceName = userAgent.substr(0, 30);
                    size_t spacePos = deviceName.find(' ');
                    if (spacePos != std::string::npos && spacePos > 5) {
                        deviceName = deviceName.substr(0, spacePos);
                    }
                } else {
                    deviceName = "未知设备";
                }
            }

            std::string userAvatar;
            int permission = db->User(name, password, &userAvatar);

            if (permission > 0) {
                int userId = db->Get_UserId(name);

                // 更新用户登录信息
                std::time_t now = std::time(nullptr);
                std::string updateSql = "UPDATE users SET "
                    "last_login_time = FROM_UNIXTIME(" + std::to_string(now) + "), "
                    "last_login_ip = '" + clientIp + "', "
                    "login_count = login_count + 1 "
                    "WHERE id = " + std::to_string(userId);
                mysql_query(db->conn, updateSql.c_str());

                std::string token = Utils::Auth::Generate_Token();

                {
                    std::lock_guard<std::mutex> lock(Utils::Auth::g_tokenMutex);
                    Utils::Auth::DeviceInfo device;
                    device.deviceName = deviceName;
                    device.userAgent = userAgent;
                    device.clientIp = clientIp;
                    device.loginTime = now;
                    Utils::Auth::g_tokenStore[token] = {userId, permission, name, device};
                }

                Create_User_Thread(userId, name, permission);

                TaskSystem::Post_User_Task(userId, TaskSystem::UserTaskType::CUSTOM_EVENT,
                    "{\"event\":\"login\",\"device\":\"" + deviceName + "\"}", "登录系统");

                // 收集设备列表
                json::array devices;
                {
                    std::lock_guard<std::mutex> lock(Utils::Auth::g_tokenMutex);
                    for (const auto& [t, session] : Utils::Auth::g_tokenStore) {
                        if (session.userId == userId) {
                            json::object dev;
                            dev["device_name"] = session.device.deviceName;
                            dev["client_ip"] = session.device.clientIp;
                            dev["login_time"] = session.device.loginTime;
                            devices.push_back(dev);
                        }
                    }
                }

                json::object result;
                result["status"] = "success";
                result["message"] = "登录成功";
                result["token"] = token;
                result["user_id"] = userId;
                result["level"] = permission;
                result["avatar"] = userAvatar;
                result["device_name"] = deviceName;
                result["devices"] = devices;
                res.result = result;
            } else {
                res.isError = true;
                res.errorCode = -32001;
                res.errorMessage = "用户名或密码错误";
            }

            return res;
        });

        // ===== user.register 用户注册 =====
        RegisterMethod("user.register", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
            RPCResponse res;
            res.id = req.id;

            if (!db) {
                res.isError = true;
                res.errorCode = -32000;
                res.errorMessage = "数据库不可用";
                return res;
            }

            json::object params = req.params.as_object();
            std::string name = params["name"].as_string().c_str();
            std::string password = params["password"].as_string().c_str();

            if (name.empty() || password.empty()) {
                res.isError = true;
                res.errorCode = -32602;
                res.errorMessage = "用户名或密码不能为空";
                return res;
            }

            int existingId = db->Get_UserId(name);
            if (existingId > 0) {
                res.isError = true;
                res.errorCode = -32002;
                res.errorMessage = "用户名已存在";
                return res;
            }

            std::string sql = "INSERT INTO users (name, password, permission, login_count, created_at) VALUES ('"
                + name + "', '" + password + "', 1, 0, NOW())";
            if (mysql_query(db->conn, sql.c_str())) {
                res.isError = true;
                res.errorCode = -32003;
                res.errorMessage = "注册失败: " + std::string(mysql_error(db->conn));
                return res;
            }

            Utils::Out_System_Mysql("新用户注册成功: " + name);

            json::object result;
            result["status"] = "success";
            result["message"] = "注册成功";
            res.result = result;
            return res;
        });

        // ===== user.info 获取当前用户信息 =====
        RegisterMethod("user.info", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
            RPCResponse res;
            res.id = req.id;

            std::string token = req.token;
            Utils::Auth::SessionInfo* session = Utils::Auth::Validate_Token(token);
            if (!session) {
                res.isError = true;
                res.errorCode = -32010;
                res.errorMessage = "未登录或 token 已失效";
                return res;
            }

            json::object result;
            result["status"] = "success";
            result["user_id"] = session->userId;
            result["level"] = session->level;
            result["name"] = session->name;
            result["device_name"] = session->device.deviceName;
            result["client_ip"] = session->device.clientIp;
            result["login_time"] = session->device.loginTime;
            res.result = result;
            return res;
        });

        // ===== task.result 查询异步任务执行结果 =====
        RegisterMethod("task.result", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
            RPCResponse res;
            res.id = req.id;

            json::object params = req.params.as_object();
            std::string taskId = params["task_id"].as_string().c_str();
            std::string token = req.token;

            if (taskId.empty()) {
                res.isError = true;
                res.errorCode = -32602;
                res.errorMessage = "请提供 task_id";
                return res;
            }

            Utils::Auth::SessionInfo* session = Utils::Auth::Validate_Token(token);
            if (!session) {
                res.isError = true;
                res.errorCode = -32010;
                res.errorMessage = "未登录";
                return res;
            }

            TaskSystem::TaskResult* taskResult = TaskSystem::Get_Task_Result(taskId);
            if (!taskResult) {
                res.isError = true;
                res.errorCode = -32011;
                res.errorMessage = "任务不存在或已过期";
                return res;
            }

            if (taskResult->userId != session->userId) {
                res.isError = true;
                res.errorCode = -32012;
                res.errorMessage = "无权查看此任务";
                return res;
            }

            std::string statusStr;
            switch (taskResult->status) {
                case TaskSystem::TaskStatus::PENDING:    statusStr = "pending";    break;
                case TaskSystem::TaskStatus::PROCESSING: statusStr = "processing"; break;
                case TaskSystem::TaskStatus::COMPLETED:  statusStr = "completed";  break;
                case TaskSystem::TaskStatus::FAILED:     statusStr = "failed";     break;
                default: statusStr = "unknown"; break;
            }

            json::object result;
            result["status"] = "success";
            result["task_id"] = taskResult->taskId;
            result["task_status"] = statusStr;
            if (!taskResult->output.empty()) {
                try {
                    result["output"] = json::parse(taskResult->output);
                } catch (...) {
                    result["output"] = taskResult->output;
                }
            } else {
                result["output"] = nullptr;
            }
            result["error"] = taskResult->errorMessage;
            result["create_time"] = taskResult->createTime;
            result["complete_time"] = taskResult->completeTime;
            res.result = result;
            return res;
        });

        // ===== device.list 获取该用户所有已登录设备 =====
        RegisterMethod("device.list", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
            RPCResponse res;
            res.id = req.id;

            std::string token = req.token;
            Utils::Auth::SessionInfo* session = Utils::Auth::Validate_Token(token);
            if (!session) {
                res.isError = true;
                res.errorCode = -32010;
                res.errorMessage = "未登录或 token 已失效";
                return res;
            }

            int userId = session->userId;
            json::array devices;
            {
                std::lock_guard<std::mutex> lock(Utils::Auth::g_tokenMutex);
                for (const auto& [t, s] : Utils::Auth::g_tokenStore) {
                    if (s.userId == userId) {
                        json::object dev;
                        dev["token_prefix"] = t.substr(0, 8) + "...";
                        dev["is_current"] = (t == token);
                        dev["device_name"] = s.device.deviceName;
                        dev["client_ip"] = s.device.clientIp;
                        dev["user_agent"] = s.device.userAgent;
                        dev["login_time"] = s.device.loginTime;
                        devices.push_back(dev);
                    }
                }
            }

            json::object result;
            result["status"] = "success";
            result["user_id"] = userId;
            result["devices"] = devices;
            res.result = result;
            return res;
        });

        // ===== user.logout 退出登录 =====
        RegisterMethod("user.logout", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
            RPCResponse res;
            res.id = req.id;

            std::string token = req.token;
            std::string deviceName = "未知";

            if (!token.empty()) {
                int userId = -1;
                bool otherDevicesOnline = false;
                std::string userName;

                {
                    std::lock_guard<std::mutex> lock(Utils::Auth::g_tokenMutex);
                    auto it = Utils::Auth::g_tokenStore.find(token);
                    if (it == Utils::Auth::g_tokenStore.end()) {
                        json::object result;
                        result["status"] = "success";
                        result["message"] = "已退出登录";
                        res.result = result;
                        return res;
                    }

                    deviceName = it->second.device.deviceName;
                    userId = it->second.userId;
                    userName = it->second.name;

                    for (const auto& [otherToken, otherSession] : Utils::Auth::g_tokenStore) {
                        if (otherToken != token && otherSession.userId == userId) {
                            otherDevicesOnline = true;
                            break;
                        }
                    }

                    Utils::Auth::g_tokenStore.erase(it);
                }

                if (!otherDevicesOnline && userId > 0) {
                    TaskSystem::Post_User_Task(userId, TaskSystem::UserTaskType::SHUTDOWN, "{}", "logout");
                }
            }

            json::object result;
            result["status"] = "success";
            result["message"] = "设备 '" + deviceName + "' 已退出登录";
            res.result = result;
            return res;
        });

        // ===== task.submit 提交数据处理任务 =====
        RegisterMethod("task.submit", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
            RPCResponse res;
            res.id = req.id;

            std::string token = req.token;
            Utils::Auth::SessionInfo* session = Utils::Auth::Validate_Token(token);
            if (!session) {
                res.isError = true;
                res.errorCode = -32010;
                res.errorMessage = "未登录";
                return res;
            }

            std::string input = json::serialize(req.params);
            std::string taskId = TaskSystem::Post_User_Task(session->userId,
                TaskSystem::UserTaskType::PROCESS_DATA, input, "task.submit");

            if (!taskId.empty()) {
                json::object result;
                result["status"] = "success";
                result["message"] = "数据已提交到用户线程处理";
                result["task_id"] = taskId;
                result["user_id"] = session->userId;
                res.result = result;
            } else {
                res.isError = true;
                res.errorCode = -32020;
                res.errorMessage = "用户线程不可用";
            }
            return res;
        });

        // ===== task.update 提交数据更新任务 =====
        RegisterMethod("task.update", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
            RPCResponse res;
            res.id = req.id;

            std::string token = req.token;
            Utils::Auth::SessionInfo* session = Utils::Auth::Validate_Token(token);
            if (!session) {
                res.isError = true;
                res.errorCode = -32010;
                res.errorMessage = "未登录";
                return res;
            }

            std::string input = json::serialize(req.params);
            std::string taskId = TaskSystem::Post_User_Task(session->userId,
                TaskSystem::UserTaskType::SYNC_DATABASE, input, "task.update");

            if (!taskId.empty()) {
                json::object result;
                result["status"] = "success";
                result["message"] = "更新请求已提交到用户线程";
                result["task_id"] = taskId;
                result["user_id"] = session->userId;
                res.result = result;
            } else {
                res.isError = true;
                res.errorCode = -32020;
                res.errorMessage = "用户线程不可用";
            }
            return res;
        });

        // ===== task.delete 提交删除任务 =====
        RegisterMethod("task.delete", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
            RPCResponse res;
            res.id = req.id;

            std::string token = req.token;
            Utils::Auth::SessionInfo* session = Utils::Auth::Validate_Token(token);
            if (!session) {
                res.isError = true;
                res.errorCode = -32010;
                res.errorMessage = "未登录";
                return res;
            }

            std::string taskId = TaskSystem::Post_User_Task(session->userId,
                TaskSystem::UserTaskType::CUSTOM_EVENT,
                "{\"action\":\"delete\"}", "task.delete");

            if (!taskId.empty()) {
                json::object result;
                result["status"] = "success";
                result["message"] = "删除请求已提交到用户线程";
                result["task_id"] = taskId;
                result["user_id"] = session->userId;
                res.result = result;
            } else {
                res.isError = true;
                res.errorCode = -32020;
                res.errorMessage = "用户线程不可用";
            }
            return res;
        });

        // ===== system.ping 心跳检测 =====
        RegisterMethod("system.ping", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
            RPCResponse res;
            res.id = req.id;

            json::object result;
            result["pong"] = true;
            result["timestamp"] = std::time(nullptr);
            res.result = result;
            return res;
        });

        Utils::Out_System("所有 RPC 业务方法已注册");
    }

} // namespace RPC