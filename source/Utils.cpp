//Utils.cpp
//通用工具实现文件
//功能：日志输出、文件读取、MIME 类型识别、JSON解析（Utils::Json）、Token认证（Utils::Auth）

#include "Utils.h"

namespace Utils{
    //日志输出实现
    //不同前缀便于区分日志来源，错误日志使用 cerr（无缓冲输出）
    void Out_System(std::string str){
        std::cout << "[输出]" << str << std::endl;          // 普通信息：std::cout
    }

    void Out_Sysrem(std::string user , std::string str){
        std::cout << "[" << user << "]" << str << std::endl;// 用户使用输出
    }

    void Out_System_Mysql(std::string str){
        std::cout << "[MySQL]" << str << std::endl;         // MySQL 操作日志
    }

    void Out_System_Http(std::string str){
        std::cout << "[HTTP]" << str << std::endl;          // HTTP 请求日志
    }

    void Out_System_Error(std::string str){
        std::cerr << "[error]" << str << std::endl;         // 错误信息：std::cerr（不缓冲，立即输出）
    }

    void Out_System_Exit(std::string str){
        std::cout << "[exit]" << str <<std::endl;           // 退出日志
    }

    void Out_System_user(std::string user,std::string str){
        std::cout << "["<<user<<"]"<<str<<std::endl;        //用户操作
    }

    //控制台输入
    int Input_System_int(){
        int a;
        std::cin >> a;
        return a;                                           // 返回输入的整数
    }

    std::string Input_System_string(){
        std::string a;
        std::cin >> a;
        return a;                                           // 返回输入的字符串
    }

    //读取文件全部内容到字符串
    std::string Read_File(const std::string& path) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file) return "";                               // 打开失败返回空字符串
        std::ostringstream ss;                              // 字符串输出流
        ss << file.rdbuf();                                 // 将文件内容读入流
        return ss.str();                                    // 返回完整字符串
    }

    //MIME 类型映射：根据文件扩展名返回对应的 MIME 类型
    std::string Get_MimeType(const std::string& ext) {
        if (ext == ".html" || ext == ".htm") return MIME_HTML;  // 网页
        if (ext == ".js")   return MIME_JS;                     // JavaScript
        if (ext == ".css")  return MIME_CSS;                    // 样式表
        if (ext == ".json") return MIME_JSON;                   // JSON 数据
        return "application/octet-stream";                      // 二进制流（下载处理）
    }

    //Json 子命名空间实现 — JSON 解析工具
    namespace Json{
        std::string Parse_Json_String(const std::string& body, const std::string& key) {
            std::string searchKey = "\"" + key + "\"";             // 构造 "\"key\""
            size_t keyPos = body.find(searchKey);                  // 找到 key 位置
            if (keyPos == std::string::npos) return "";            // 没找到返回空字符串
            size_t colonPos = body.find(":", keyPos);              // 找到冒号
            if (colonPos == std::string::npos) return "";          // 没找到冒号返回空字符串
            size_t quoteStart = body.find("\"", colonPos);         // 找到值起始引号
            if (quoteStart == std::string::npos) return "";        // 没找到引号返回空字符串
            size_t quoteEnd = body.find("\"", quoteStart + 1);     // 找到值结束引号
            if (quoteEnd == std::string::npos) return "";          // 没找到结束引号返回空字符串
            return body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);  // 截取中间的字符串
        }

        std::string Parse_Json_Value_Raw(const std::string& body, const std::string& key) {
            std::string searchKey = "\"" + key + "\"";             // 构造 "\"key\""
            size_t keyPos = body.find(searchKey);                  // 找到 key 位置
            if (keyPos == std::string::npos) return "";            // 没找到返回空字符串
            size_t colonPos = body.find(":", keyPos + searchKey.length());// 从 key 之后找冒号
            if (colonPos == std::string::npos) return "";          // 没找到冒号返回空字符串
            size_t valueStart = body.find_first_not_of(" \t\n\r", colonPos + 1);// 跳过空白
            if (valueStart == std::string::npos) return "";        // 没找到值返回空字符串
            if (body[valueStart] == '"') {                         // 字符串值：截取引号内容
                size_t valueEnd = body.find("\"", valueStart + 1); // 找结束引号
                if (valueEnd == std::string::npos) return "";      // 没找到结束引号返回空字符串
                return body.substr(valueStart + 1, valueEnd - valueStart - 1);// 返回引号内内容
            } else {                                               // 数字/布尔值：截取到逗号、花括号或换行
                size_t valueEnd = body.find_first_of(",}\n\r", valueStart);
                if (valueEnd == std::string::npos) valueEnd = body.length();
                std::string raw = body.substr(valueStart, valueEnd - valueStart);
                // 去除尾部空白
                while (!raw.empty() && (raw.back() == ' ' || raw.back() == '\t')) {
                    raw.pop_back();
                }
                return raw;                                        // 返回原始值字符串
            }
        }
    }

    //Auth 子命名空间实现 — Token 认证与会话管理
    namespace Auth{
        // 全局变量定义（唯一实例）
        std::unordered_map<std::string, SessionInfo> g_tokenStore;  // Token 存储表
        std::mutex g_tokenMutex;                                    // 保护 g_tokenStore 的互斥锁

        // 生成一个新的安全认证 Token（32位随机十六进制字符串）
        std::string Generate_Token() {
            std::random_device rd;                              // 硬件随机数种子
            std::mt19937 gen(rd());                             // 梅森旋转算法随机数引擎
            std::uniform_int_distribution<> dis(0, 15);         // 0~15 均匀分布
            std::stringstream ss;
            for (int i = 0; i < 32; i++) {                      // 生成 32 个随机十六进制位
                ss << std::hex << dis(gen);
            }
            return ss.str();                                    // 返回 Token 字符串
        }

        // 从 Authorization 头字符串中提取 Token
        std::string Get_Token_From_Request(const std::string& authHeader) {
            if (authHeader.empty()) return "";

            size_t bearerPos = authHeader.find("Bearer ");      // 查找 "Bearer " 前缀
            if (bearerPos == std::string::npos) {
                bearerPos = authHeader.find("bearer ");         // 兼容小写
            }
            if (bearerPos == std::string::npos) return authHeader; // 无前缀则返回整个字符串

            return authHeader.substr(bearerPos + 7);             // 跳过 "Bearer " 返回 Token
        }

        // 验证 Token 并返回对应的 SessionInfo
        SessionInfo* Validate_Token(const std::string& token) {
            if (token.empty()) return nullptr;                  // 空 Token 直接返回空指针
            std::lock_guard<std::mutex> lock(g_tokenMutex);     // 加锁保护 Token 表
            auto it = g_tokenStore.find(token);
            if (it == g_tokenStore.end()) return nullptr;       // 未找到返回空指针
            return &it->second;                                 // 返回对应会话信息的指针
        }
    }
}