#pragma once
#include <string>

#include <boost/json.hpp>

#include "Utils.h"

namespace Json
{

    namespace Blog
    {
        // 所有的文章头解析
        std::string ArticleMeta(const std::string& body);
    } // namespace Blog
} // namespace Json