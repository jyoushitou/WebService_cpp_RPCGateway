#include "ProtoBufToJson.h"

namespace Json
{
    // Blog路由
    void UrlToBlogString(const Net::Server::HttpServer::Url url, std::string& msg)
    {
        Blog::router blog;
        blog.mutable_head()->CopyFrom(url.head);
        if (url.head.command() == 1)
        {
            blog.set_body("");
        }
        else if (url.head.command() == 2)
        {
            blog.set_body(url.body);
        }
        else
        {
        }
        blog.SerializeToString(&msg);
    }

    // 解析传回来的Proto成JSON
    std::string ProtoToJson(std::string msg)
    {
        // 反序列化
        common::header head;
        head.ParseFromString(msg);
        if (head.serviceid() == 13)
        {
            if (head.command() == 1)
            {
                Blog::get_articles getarticle;
                getarticle.ParseFromString(head.msg());
                return Json::Blog::ArticleMeta(getarticle.);
            }
        }
    }
    namespace Blog
    {
        static std::string ArticleMeta()

            // 所有的文章头解析
            std::string ArticleHead(const std::string& body);
    } // namespace Blog
} // namespace Json