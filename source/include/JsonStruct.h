#pragma once

#include <string>
#include <vector>

// 文章头
struct ArticleHead
{
    std::string slug;              // 路径
    std::string title;             // 标题
    std::string date;              // 时间
    std::vector<std::string> tags; // 标签
};

// 整篇获取
struct ArticleDetail
{
    ArticleHead head; // 文章头
    std::string body; // 正文
};