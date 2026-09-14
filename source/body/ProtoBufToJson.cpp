#include "ProtoBufToJson.h"

#include "Blog.pb.h"

namespace Json
{
    // 博客专属解析
    namespace blog
    {
        // Blog路由
        void UrlToBlogString(const Net::Server::HttpServer::Url url, std::string& msg)
        {
            Blog::router route;
            route.mutable_head()->CopyFrom(url.head);
            if (url.head.command() == 1)
            {
                route.set_body("");
            }
            else if (url.head.command() == 2)
            {
                route.set_body(url.body);
            }
            route.SerializeToString(&msg);
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

                    return Json::blog::ArticleHead(head.msg());
                }
            }
        }
        static std::string ArticleMeta(const std::string& Meta);

        // 所有的文章头解析
        std::string ArticleHead(const std::string& body)
        {
            // 反序列化字符串
            Blog::get_articles getarticle;
            getarticle.ParseFromString(body);

            // 分开所有的头
            std::vector<std::string> metas = Utils::String::split(getarticle.articles(), getarticle.count(), '|');
            // 解析Json
            boost::json::array arr;
            for (auto i : metas)
            {
                // 添加到末尾
                arr.emplace_back(ArticleMeta(i));
            }
            return boost::json::serialize(arr);
        }
    } // namespace blog
} // namespace Json