#pragma once

#include <thread>     //线程池
#include <vector>     //数组
#include <mutex>      //共享量
#include <functional> //函数的扩展库
#include <queue>      //队列库
#include <future>     //future库

namespace threadpool // 命名空间为threadpool
{                    // 线程池的命名空间
    class ThreadPool // 定义线程池
    {                // 定义线程池的连接函数
    public:
        explicit ThreadPool(size_t num_threads = 4); // 默认创建线程数为4的构造函数

        template <typename F, typename... Arg>                                                     // F为单个固定的模版参数，Arg为不固定数量的模版类型
        auto enques(F &&f, Arg &&...arg) -> std::future<typename std::result_of<F(Arg...)>::type>; // 线程池的使用（添加函数）

        ~ThreadPool(); // 析构函数

    private:
        void worker();                              // 线程执行内容
        bool stop;                                  // 记录线程池是否停止
        std::condition_variable cv;                 // 条件变量
        std::mutex mtx;                             // 互斥锁
        std::vector<std::thread> workers;           // 线程集合（线程池）
        std::queue<std::function<void()>> work_que; // 任务队列
    };

} // namespace threadpool