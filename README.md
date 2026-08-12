# RPCGateway — C++ Protobuf 微服务网关

> WebServer 微服务架构的**前置网关**，负责 Vue 前端请求的协议转换、proto 序列化转发与响应解码。

![C++](https://img.shields.io/badge/C++-17-%2300599C?style=flat-square&logo=c%2B%2B)
![Boost](https://img.shields.io/badge/Boost-Beast/Asio-%23F6822B?style=flat-square&logo=boost)
![Protobuf](https://img.shields.io/badge/Protobuf-3.x-%23FF6A00?style=flat-square&logo=google)
![HTTP](https://img.shields.io/badge/HTTP-JSON-%234285F4?style=flat-square)

---

## 📖 概述

RPCGateway 是 WebServer 微服务架构的**统一网络入口**。它作为前端（Vue3）与后端微服务之间的**协议转换网关**：

前端通过 HTTP 发送 **JSON** 请求 → 网关将请求数据 **编码为 Protobuf** → 转发给对应的微服务 → 微服务返回 Protobuf 响应 → 网关 **解码回 JSON** → 返回给前端。

> **核心职责**：JSON ↔ Protobuf 的双向协议转换 + 请求路由 + 响应关联。

---

## ✨ 功能特性

- 🌐 **HTTP 前端接入** — 接收 Vue3 前端 JSON 请求（端口 8080）
- 🔄 **JSON → Protobuf 编码** — 将前端 JSON 请求序列化为 protobuf 消息
- 🔄 **Protobuf → JSON 解码** — 将微服务 protobuf 响应反序列化为 JSON
- 📮 **TCP 长连接转发** — 基于 Boost.Asio 与微服务保持 TCP 长连接
- 🎫 **消息关联（msg_id）** — 通过自增 msg_id 关联请求与响应，实现异步转发
- 🔌 **多微服务路由** — 根据请求路径（path）路由到不同微服务
- 🛡️ **断线重连** — 微服务断开后自动 2 秒重连
- 🧵 **每连接独立 IO 线程** — 每个微服务连接独立 io_context + 线程

---

## 🏗️ 架构设计

```
┌──────────────┐   HTTP/JSON    ┌──────────────────────────────────────┐   TCP/Protobuf   ┌─────────────┐
│   Vue 前端    │ ◄────────────► │            RPCGateway               │ ◄───────────────► │  微服务服务    │
│   (Vue3)     │    :8080       │                                      │    :60000        │  (MySQL等)   │
└──────────────┘                │  ┌──────────────────────────────┐   │                   └─────────────┘
                                │  │   HttpServer :8080           │   │
                                │  │  • 接收 Vue JSON 请求         │   │
                                │  │  • 解析 JSON                  │   │
                                │  └──────────────┬───────────────┘   │
                                │                 │ cmd_str + session │
                                │                 ▼                   │
                                │  ┌──────────────────────────────┐   │
                                │  │   业务路由层 HandleVueBiz     │   │
                                │  │  • 分配 msg_id               │   │
                                │  │  • 查询 g_conns 可用连接      │   │
                                │  │  • 记录 PendingRequest       │   │
                                │  └──────────────┬───────────────┘   │
                                │                 │ proto 序列化     │
                                │                 ▼                   │
                                │  ┌──────────────────────────────┐   │
                                │  │   NetClient TCP 客户端        │   │
                                │  │  • 连接微服务 :60000          │   │
                                │  │  • 编码 proto 发送            │   │
                                │  └──────────────┬───────────────┘   │
                                └─────────────────┼────────────────────┘
                                                  │ proto 响应
                                                  ▼
                                ┌──────────────────────────────┐
                                │       ClientWork 回调         │
                                │  • proto 解码为 JSON          │
                                │  • 查 msg_id → session        │
                                │  • AsyncSendResponse 回前端   │
                                └──────────────────────────────┘
```

---

## 📂 项目结构

```
RPCGateway/
├── source/                        # 源代码目录
│   ├── main.cpp                   # 程序入口：初始化事件/信号/连接，启动 HTTP 服务
│   ├── CMakeLists.txt             # CMake 构建脚本（自动查找 vcpkg）
│   ├── include/                   # 头文件目录
│   │   ├── RPCGateWayWork.h       # 网关核心：连接管理/消息路由/回调注册
│   │   ├── NetClient.h            # TCP 客户端封装（转发 proto 到微服务）
│   │   ├── NetHttpServer.h        # HTTP 服务端封装（接收 Vue 请求）
│   │   └── Utils.h                # 工具函数（日志等）
│   └── body/                      # 实现文件目录
│       ├── RPCGateWayWork.cpp     # 网关业务：路由/编码/回包/重连
│       ├── NetClient.cpp          # TCP 客户端实现
│       ├── NetHttpServer.cpp      # HTTP 服务端实现
│       └── Utils.cpp              # 工具函数实现
├── .gitignore                     # Git 忽略规则
├── LICENSE                        # MIT 许可证
└── README.md                      # 本文件
```

---

## 🚀 快速开始

### 前置依赖

| 组件       | 版本要求                    | 说明                           |
| ---------- | --------------------------- | ------------------------------ |
| C++ 编译器 | C++17 (GCC 8+ / MSVC 2019+) |                                |
| CMake      | 3.10+                       | 构建系统                       |
| Boost      | 1.70+                       | 需要 Beast、Asio、System、JSON |
| Protobuf   | 3.x+                        | 协议序列化                     |

### 构建与运行

```bash
# 克隆仓库（如果是独立使用）
git clone https://github.com/jyoushitou/WebService_cpp_RPCGateway.git
cd RPCGateway

# 构建
cd source
mkdir build && cd build
cmake ..
cmake --build .

# 运行（默认端口 60906）
./GateWay.exe
```

---

## 🔌 请求流转流程

### 1. Vue 前端 → 网关（HTTP JSON）

```json
{
  "cmd": "GetUser",
  "params": { "uid": 10001 }
}
```

### 2. 网关 → 微服务（Protobuf）

网关将 JSON 序列化为对应 proto 消息，通过 TCP 长连接发送：

```protobuf
// proto 定义（示例）
message UserRequest {
  header head = 1;      // 消息头
  uint32 UID = 2;       // 用户 ID
}
```

### 3. 微服务 → 网关（Protobuf 响应）

微服务处理完后返回 protobuf 结果。

### 4. 网关 → Vue 前端（JSON 响应）

网关将 protobuf 响应解码为 JSON 返回：

```json
{ "code": 0, "data": { "uid": 10001, "name": "张三" } }
```

---

## 🧵 线程模型

```
┌─────────────────────────────────────────────────────┐
│                    主线程 (main)                     │
│  • 创建退出事件                                      │
│  • 注册 Ctrl+C 信号处理                              │
│  • 调用 RunHttpServer（阻塞直到退出）                │
└────────────────────────┬────────────────────────────┘
                         │
        ┌────────────────┼─────────────────┐
        ▼                ▼                 ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ HTTP IO 线程  │  │ 客户端 IO 线程 │  │ 重连线程(临时) │
│ :8080 监听    │  │ :60000 转发   │  │ 断开后 2s 重连 │
│ 处理 Vue 请求 │  │ 收发 proto    │  │              │
└──────────────┘  └──────────────┘  └──────────────┘
```

---

## 📋 开发计划

### ⏳ 进行中 / TODO

- [ ] **Proto 序列化实现** — 实现 JSON ↔ Protobuf 编解码（基于 proto/source 中已定义的 `.proto` 文件）
  - [ ] `Common.proto` — 消息头（MSGID/MSGLen）与通用结构
  - [ ] `MySQL.proto` — MySQL 服务请求消息
  - [ ] `select.proto` — MySQL 查询请求消息
  - [ ] `User.proto` — 用户微服务消息
- [ ] **链路追踪** — 根据 msg_id 记录完整的请求链路（网关 → 微服务 → 返回），支持日志分析和问题定位->后续将会将链路追踪到对应的微服务
- [ ] **多微服务路由** — 根据 path/cmd 动态路由到不同微服务实例

### ✅ 已完成

- [x] HTTP 服务端（端口 8080）接收 Vue JSON 请求
- [x] TCP 客户端长连接（端口 60000）转发微服务
- [x] msg_id 消息关联（请求响应异步匹配）
- [x] 断线自动重连（2 秒间隔）
- [x] 每连接独立 io_context + 线程模型
- [x] 优雅退出（等待 IO 线程确定性结束）

---

## 📬 联系方式

- 仓库地址：[https://github.com/jyoushitou/WebService_cpp_RPCGateway.git](https://github.com/jyoushitou/WebService_cpp_RPCGateway.git)
- 父项目：[WebServer](https://github.com/jyoushitou/WebServer)