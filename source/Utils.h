//Utils.h
//通用工具库头文件
//功能：MIME 类型定义、日志输出、文件读取、控制台输入
//       子命名空间：Utils::Json（JSON解析）、Utils::Auth（Token认证）

#ifndef UTILS_H
#define UTILS_H
#include <iostream>             // 输入输出流
#include <fstream>              // 文件输入输出流
#include <sstream>              // 字符串流
#include <string>               // 字符串
#include <random>               // 随机数生成
#include <iomanip>              // 输入输出格式化
#include <mutex>                // 互斥锁
#include <unordered_map>        // 无序映射容器（哈希表）
#include <ctime>                // 时间日期

// HTTP MIME 类型常量
#define MIME_HTML "text/html"               // HTML 网页
#define MIME_JS   "application/javascript"  // JavaScript 脚本
#define MIME_CSS  "text/css"                // CSS 样式表
#define MIME_JSON "application/json"        // JSON 数据（RESTful API 常用）

namespace Utils{
    //日志输出
    void Out_System(std::string);           // 普通系统日志 [输出]
    void Out_Sysrem(std::string , std::string);// 用户操作输出
    void Out_System_Mysql(std::string);     // MySQL 相关日志 [MySQL]
    void Out_System_Http(std::string);      // HTTP 相关日志 [HTTP]
    void Out_System_Error(std::string);     // 错误日志 [错误]（输出到 stderr）
    void Out_System_Exit(std::string);      // 退出日志 [Exit]
    void Out_System_user(std::string,std::string);// 用户登录

    //控制台输入
    int Input_System_int();                 // 从控制台读取整数
    std::string Input_System_string();      // 从控制台读取字符串

    //文件操作
    std::string Read_File(const std::string& path);     // 读取文件全部内容到字符串

    //MIME 类型工具
    std::string Get_MimeType(const std::string& ext);   // 根据文件扩展名返回 MIME 类型

    //Json 子命名空间 — JSON 解析工具（无第三方依赖）
    namespace Json{
        std::string Parse_Json_String(const std::string&, const std::string&);     // 解析JSON中指定key的字符串值
        std::string Parse_Json_Value_Raw(const std::string&, const std::string&);  // 解析JSON中指定key的原始值（数字/布尔/嵌套）
    }

    //Auth 子命名空间 — Token 认证与会话管理
    namespace Auth{
        // 设备信息，记录用户登录时的设备详情
        struct DeviceInfo {
            std::string deviceName;     // 自定义设备名（前端传入）
            std::string userAgent;      // User-Agent 请求头
            std::string clientIp;       // 客户端 IP 地址
            std::time_t loginTime;      // 登录时间戳
        };

        // 会话信息，存储已认证用户的会话数据
        struct SessionInfo {
            int userId;                 // 用户 ID
            int level;                  // 用户权限等级
            std::string name;           // 用户显示名称
            DeviceInfo device;          // 当前会话的设备信息
        };

        // 外部变量
        extern std::unordered_map<std::string, SessionInfo> g_tokenStore;   // 全局 Token 存储表：Token 字符串 -> 会话信息
        extern std::mutex g_tokenMutex;                                     // 保护 g_tokenStore 并发访问的互斥锁

        // 外部可使用函数
        std::string Generate_Token();                                       // 生成一个新的安全认证 Token（32位随机十六进制）
        std::string Get_Token_From_Request(const std::string& authHeader);  // 从 Authorization 头字符串中提取 Token
        SessionInfo* Validate_Token(const std::string&);                    // 验证 Token 并返回对应的 SessionInfo
    }
}
#endif //!UTILS_H