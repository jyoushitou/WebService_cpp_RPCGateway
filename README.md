# RPCGateway — C++ WebSocket JSON-RPC 网关

> WebServer 微服务架构的**前置网关**，负责统一协议转换、请求路由和业务分发。

![C++](https://img.shields.io/badge/C++-17-%2300599C?style=flat-square&logo=c%2B%2B)
![Boost](https://img.shields.io/badge/Boost-Beast/Asio-%23F6822B?style=flat-square&logo=boost)
![JSON-RPC](https://img.shields.io/badge/JSON--RPC-2.0-%23000000?style=flat-square)
![WebSocket](https://img.shields.io/badge/WebSocket-1.0-%234285F4?style=flat-square)

---

## 📖 概述

RPCGateway 是 WebServer 微服务架构的**统一网络入口**。它基于 Boost.Beast + Boost.Asio 实现 WebSocket 服务端，使用 **JSON-RPC 2.0** 协议替代传统的 HTTP REST API，实现前后端的高效 RPC 通信。

> **原名**：原单体架构中的 `cpp_WebServer`，在微服务架构演进中剥离为独立网关服务。

---

## ✨ 功能特性

- 🔌 **WebSocket 长连接** — 基于 Boost.Beast，支持全双工通信
- 📦 **JSON-RPC 2.0 协议** — 标准化 RPC 调用格式
- 🔐 **Token 认证与会话管理** — 完整的用户鉴权体系
- 🧵 **用户专属线程模型** — 每个登录用户拥有独立业务线程
- 📋 **异步任务系统** — 任务投递、查询、结果回调
- 🗄️ **MySQL 数据库操作** — 集成连接池，支持用户 CRUD
- 👥 **多设备登录管理** — 同一账号可在多设备登录，支持设备列表查看/下线

---

## 🏗️ 架构设计

```
┌──────────────┐     WebSocket      ┌──────────────────────────────┐
│   Vue 前端    │ ◄───────────────► │      RPCGateway              │
│  (gRPC-Web)   │   JSON-RPC 2.0    │                              │
└──────────────┘                    │  ┌─ RPCServer ─────────────┐ │
                                    │  │  • WebSocket 服务端      │ │
                                    │  │  • JSON-RPC 协议解析     │ │
                                    │  │  • 方法路由分发           │ │
                                    │  └──────────────────────────┘ │
                                    │                              │
                                    │  ┌─ 用户线程模型 ──────────┐ │
                                    │  │  • 每个用户一个线程       │ │
                                    │  │  • 独立任务队列           │ │
                                    │  │  • 独立 MySQL 连接        │ │
                                    │  └──────────────────────────┘ │
                                    │                              │
                                    │  ┌─ 认证系统 ──────────────┐ │
                                    │  │  • Token 生成/验证        │ │
                                    │  │  • 会话管理               │ │
                                    │  │  • 设备管理               │ │
                                    │  └──────────────────────────┘ │
                                    └──────────────────────────────┘
                                               │
                                        MySQL  │  连接池
                                               ▼
                                    ┌──────────────────────┐
                                    │      MySQL 数据库     │
                                    │    (web_server)       │
                                    └──────────────────────┘
```

---

## 📂 项目结构

```
RPCGateway/
├── source/                        # 源代码
│   ├── main.cpp                   # 程序入口：初始化 MySQL，启动 RPC
│   ├── ServerInit.h               # 全局变量声明、初始化函数
│   ├── RPCServer.h                # RPC 服务端类声明
│   ├── RPCServer.cpp              # RPC 服务端实现（WebSocket + JSON-RPC）
│   ├── Utils.h                    # 工具函数头文件（日志、JSON解析、Token认证）
│   ├── Utils.cpp                  # 工具函数实现
│   ├── Task.h                     # 任务系统头文件
│   ├── Task.cpp                   # 任务系统实现
│   ├── UserThread.h               # 用户线程管理头文件
│   └── UserThread.cpp             # 用户线程管理实现
├── LICENSE                        # MIT 许可证
└── README.md                      # 本文件
```

---

## 🚀 快速开始

### 前置依赖

| 组件 | 版本要求 | 说明 |
|------|----------|------|
| C++ 编译器 | C++17 (GCC 8+ / MSVC 2019+) | |
| CMake | 3.10+ | 构建系统 |
| Boost | 1.70+ | 需要 Beast、Asio、System、JSON |
| MySQL | 8.0+ | 数据库 + C API 开发库 |

### 构建与运行

```bash
# 克隆仓库（如果是独立使用）
git clone https://github.com/jyoushitou/WebService_cpp_RPCGateway.git
cd RPCGateway

# 构建
mkdir build && cd build
cmake ..
cmake --build .

# 运行（默认端口 60906）
./RPCGateway
```

---

## 🔌 RPC 接口文档


RPCGateway 使用 **JSON-RPC 2.0** 协议，通过 WebSocket 通信。

### JSON-RPC 请求格式

```json
{
  "jsonrpc": "2.0",
  "method": "方法名",
  "params": { ... },
  "id": 1
}
```

### 用户认证

| 方法 | 参数 | 说明 |
|------|------|------|
| `user.login` | name, password, device_name(可选), client_ip(可选), user_agent(可选) | 用户登录 |
| `user.register` | name, password | 用户注册 |
| `user.info` | token | 获取当前用户信息 |
| `user.logout` | token | 退出登录 |

### 设备管理

| 方法 | 参数 | 说明 |
|------|------|------|
| `device.list` | token | 获取该用户所有已登录设备 |

### 任务系统

| 方法 | 参数 | 说明 |
|------|------|------|
| `task.submit` | token, ... | 提交数据处理任务 |
| `task.update` | token, ... | 提交数据更新任务 |
| `task.delete` | token, ... | 提交删除任务 |
| `task.result` | token, task_id | 查询异步任务执行结果 |

### 系统

| 方法 | 参数 | 说明 |
|------|------|------|
| `system.ping` | (无) | 心跳检测 |

---

## 🧵 用户线程模型

本网关实现了独特的**用户专属线程模型**：

1. 用户登录成功后，自动创建一个**独立工作线程**
2. 该线程拥有自己的 **MySQL 数据库连接**
3. 该线程拥有独立的**任务队列**，通过条件变量等待任务
4. 客户端通过 `task.submit` 等方法**投递任务到线程队列**
5. 线程处理完成后，通过 `task.result` **查询执行结果**
6. 用户退出登录后，线程自动**安全关闭**

### 任务类型

| 类型 | 说明 |
|------|------|
| `PING` | 心跳检测，返回 ping/pong 响应 |
| `PROCESS_DATA` | 数据处理任务 |
| `SEND_NOTIFICATION` | 发送通知任务 |
| `SYNC_DATABASE` | 数据库同步任务 |
| `CUSTOM_EVENT` | 自定义事件任务 |
| `SHUTDOWN` | 关闭/停止任务 |

---

## 🔐 Token 认证机制

- 登录成功生成 **32 位随机十六进制 Token**
- Token 与**会话信息**（用户 ID、权限、设备信息）绑定存储
- 支持**多设备**同一账号同时登录，每个设备独立 Token
- 退出登录时仅销毁当前 Token，不影响其他设备

---

## 📋 开发计划

- [ ] **gRPC 协议迁移** — 从 JSON-RPC（WebSocket）迁移到标准 gRPC
- [ ] **连接池优化** — 集成 MySQL 独立连接池微服务
- [ ] **服务发现集成** — 对接 ServiceRegistry 实现动态路由
- [ ] **限流控制** — 添加 Rate Limiter 保护后端
- [ ] **链路追踪** — 集成 OpenTelemetry 标准

---

## 📬 联系方式

- 仓库地址：[https://github.com/jyoushitou/WebService_cpp_RPCGateway.git](https://github.com/jyoushitou/WebService_cpp_RPCGateway.git)
- 父项目：[WebServer](https://github.com/jyoushitou/WebServer)
