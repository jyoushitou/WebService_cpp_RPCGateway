#pragma once
#include <string>

#include <boost/json.hpp>

#include "Utils.h"
#include "NetHttpServer.h"

namespace Json
{
    // 路由Proto
    std::string UrlToProto(const Net::Server::HttpServer::Url& url);
    // 解析
    std::string ProtoToJson(std::string msg);
} // namespace Json