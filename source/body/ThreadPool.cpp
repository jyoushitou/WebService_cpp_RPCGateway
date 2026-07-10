#include "ThreadPool.h" //引入ThreadPool.h 为其编写实现函数

namespace threadpool // 命名空间threadpool
{
    ThreadPool::ThreadPool(size_t num_threads) : stop(0) // 构造函数的实现并为stop初始化为0（false）
    {
        for (size_t i = 0; i < num_threads; i++) // 循环num_threads此，去创建num_threads的线程数
        {
            workers.emplace_back([this]
                                 { this->worker(); }); // workers内部通过匿名函数指向私有worker函数构造子线程
        }
    }

    ThreadPool::~ThreadPool() // 析构函数
    {
        // 更改停止标识
        {
            std::unique_lock<std::mutex>(mtx); // 给mtx上锁
            stop = 1;
        }
        cv.notify_all();                         // 唤醒所有的阻塞线程
        for (std::thread &otherthread : workers) // 循环为未完成的线程
        {
            otherthread.join(); // 阻塞主线程使未完成的线程完成
        }
    }

    template <typename F, typename... Arg> // F为单个固定的模版参数，Arg为不固定数量的模版类型
    auto ThreadPool::enques(F &&f, Arg &&...arg)
        -> std::future<typename std::result_of<F(Arg...)>::type>
    {
        using functype = typename std::resulit_of<F(Arg..)>::type; // 获取得到的函数类型

        auto task = std::make_shared<std::packaged_task<functype>>(
            std::bind(std::forward<F>(f), std::forward<Arg>(arg)...));

        std::future<functype> retfuture = task->get_future();
        {
            std::lock_guard<std::mutx> lock_guard(this->mtx);
            if (stop)
                throw std::runtime_error("error:threadpool on off");
            work_que.emplace([this](){
                (*task)();
            });
        }
        cv.notify_all();

        return retfuture;
    }
} // namespace threadpool
