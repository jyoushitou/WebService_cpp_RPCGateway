//main.cpp
//程序入口点
//功能：初始化 MySQL、启动 RPC 服务器（替代 HTTP）、保持主进程存活

#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>

#ifdef _WIN32
//注意：winsock2.h 必须在 windows.h 之前包含！
#include <winsock2.h>
#include <windows.h>
#endif




#include "Utils.h"          // 工具库
#include "ServerInit.h"      // 服务器初始化

//外部变量/函数声明（定义在 FrameWork.cpp）
extern RPC::RPCServer* g_rpcServer;         // 全局 RPC 服务器指针
extern std::atomic<bool> g_running;         // 全局运行状态标志

extern void Initiate_RPC(int Port, MySQL::mysql* db);  // 初始化并启动 RPC 服务器
extern MySQL::mysql* Initiate_MySQL();      // 初始化 MySQL 数据库连接

//main — 程序入口
int main(){
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);            // 设置控制台 UTF-8 编码（支持中文输出）
#endif
    Tools::Out_System("Web_Server启动");     // 打印启动日志

    MySQL::mysql* web_db = Initiate_MySQL();// 初始化 MySQL

    // 使用 RPC 服务器替代 HTTP 服务器
    // 前端通过 WebSocket 连接，使用 JSON-RPC 2.0 协议进行通信
    Initiate_RPC(60906, web_db);            // 启动 RPC 服务器（端口 60906）

    Tools::Out_System("主进程正在运行，RPC 服务器已启动...");

    while (g_running) {                     // 主进程保持存活
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    //服务器停止后的收尾
    if (g_rpcServer) {
        delete g_rpcServer;
        g_rpcServer = nullptr;
    }

    if (web_db) {
        delete web_db;
        web_db = nullptr;
    }

    return 0;
}