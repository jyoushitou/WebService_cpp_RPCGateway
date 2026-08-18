cmake_minimum_required(VERSION 4.0)
# ============================================================
# 自动查找 vcpkg toolchain（必须在 project() 之前设置）
# 可通过以下任一方式覆盖：
#   1. 命令行: cmake .. -DCMAKE_TOOLCHAIN_FILE=<路径>
#   2. 环境变量: VCPKG_ROOT=<vcpkg目录>
# ============================================================
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(_vcpkg_toolchain NOTFOUND)

    # 方法1: 优先使用环境变量 VCPKG_ROOT
    if(DEFINED ENV{VCPKG_ROOT})
        file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}" _vcpkg_root)
        set(_vcpkg_toolchain "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
        if(NOT EXISTS "${_vcpkg_toolchain}")
            set(_vcpkg_toolchain NOTFOUND)
        endif()
    endif()

    # 方法2: 尝试常见安装路径
    if(NOT _vcpkg_toolchain)
        foreach(_path
            "C:/vcpkg"
            "D:/vcpkg"
            "D:/app/vcpkg"
            "E:/app/vcpkg"
            "$ENV{USERPROFILE}/vcpkg")
            set(_candidate "${_path}/scripts/buildsystems/vcpkg.cmake")
            if(EXISTS "${_candidate}")
                set(_vcpkg_toolchain "${_candidate}")
                break()
            endif()
        endforeach()
    endif()

    if(_vcpkg_toolchain)
        set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_toolchain}" CACHE FILEPATH "vcpkg toolchain file")
        message(STATUS "找到 vcpkg toolchain: ${_vcpkg_toolchain}")
    else()
        message(WARNING "未找到 vcpkg toolchain。\n"
            "如果 Protobuf 查找失败，请手动指定：\n"
            "  cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg目录>/scripts/buildsystems/vcpkg.cmake\n"
            "或设置环境变量 VCPKG_ROOT=<vcpkg目录>")
    endif()
endif()

project(GateWay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Protobuf REQUIRED)
find_package(Boost COMPONENTS json REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    ../../common/cpp/body/Common.pb.cc
    ../../common/cpp/body/Utils.cpp
    ../../common/cpp/body/ThreadPool.cpp
    ../../common/cpp/body/NetConnection.cpp
    ../../common/cpp/body/NetServer.cpp
    body/NetHttpServer.cpp
    body/RPCGateWayWork.cpp
    body/NetClient.cpp
)

# Common.pb.cc 中 include 的是 "common/Common.pb.h"，需要 ../common
# main.cpp 中 include 的是 "Common.pb.h"，需要 ../common/include
# ============================================================
# 解决 Common.pb.cc 中 #include "common/Common.pb.h" 的路径映射
# 在构建目录下创建 include/common，存放生成的头文件副本
# ============================================================
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/common")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../../common/cpp/include/Common.pb.h"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/include/common/")

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/cpp/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    protobuf::libprotobuf
    Boost::json
)

# 让 MSVC 按 UTF-8 解析源码，解决 C4819 警告（中文注释/字符串）
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
