#pragma once
#include <string>

#include <boost/json.hpp>

#include "Utils.h"
#include "NetHttpServer.h"

namespace Json
{
    namespace blog
    {
        // Url解析成客户端对应的
        void UrlToBlogString(const Net::Server::HttpServer::Url url, std::string& msg);
        // 解析
        std::string ProtoToJson(std::string msg);
        // 所有的文章头解析
        std::string ArticleHead(const std::string& body);
    } // namespace blog
} // namespace Json