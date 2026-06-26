//ServerInit.h
//服务器初始化头文件
//功能：全局变量声明、MySQL/RPC 初始化函数声明

#ifndef SERVERINIT_H
#define SERVERINIT_H

#include <string>           // 字符串
#include <atomic>           // 原子操作

#ifdef _WIN32
    #ifndef NOMINMAX
    #define NOMINMAX        // 避免 min/max 宏冲突
    #endif
    #include <winsock2.h>   // Windows Socket API
    #include <windows.h>    // Windows API
    #include <process.h>    // 进程/线程管理（_beginthreadex 等）
#else
    #include <unistd.h>     // POSIX API（fork()、sleep() 等）
    #include <sys/wait.h>   // wait() 系列函数
    #include <signal.h>     // 信号处理
#endif

#include "MyMySQL.h"        // MySQL 数据库操作封装
#include "Utils.h"          // 通用工具函数（含日志、JSON解析、Token认证）
#include "Task.h"           // 任务系统
#include "UserThread.h"     // 用户线程管理
#include "RPCServer.h"      // RPC 服务器（基于 Boost.Beast WebSocket）

//外部全局变量（定义在 ServerInit.cpp）
extern RPC::RPCServer* g_rpcServer;                 // 全局 RPC 服务器实例指针
extern std::atomic<bool> g_running;                 // 程序运行状态标志（原子操作，线程安全）

//函数声明
void Initiate_RPC(int Port, MySQL::mysql* db);      // 初始化并启动 RPC 服务器
MySQL::mysql* Initiate_MySQL();                     // 初始化并获取 MySQL 数据库连接实例

#endif //!SERVERINIT_H