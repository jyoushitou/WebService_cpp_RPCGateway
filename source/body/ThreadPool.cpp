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
    {                                                              // 添加一个不定名作用域，避免因加锁而导致的死锁
        using functype = typename std::resulit_of<F(Arg..)>::type; // 获取得到的函数类型

        auto task = std::make_shared<std::packaged_task<functype>>(
            std::bind(std::forward<F>(f), std::forward<Arg>(arg)...)); // 打包一个传入函数为异步任务

        std::future<functype> retfuture = task->get_future();        // 获取上一步打包的异步函数
        {                                                            // 定义锁的作用域避免死锁
            std::lock_guard<std::mutx> lock_mtx(this->mtx);          // 为线程中调用函数加一个智能锁
            if (stop)                                                // 判断是否线程池停止
                throw std::runtime_error("error:threadpool on off"); // 抛出停止异常
            work_que.emplace([this]()
                             { (*task)(); }); // 将异步任务的函数名解出并加入线程队列
        }
        cv.notify_one(); // 唤醒一个线程去执行

        return retfuture; // 返回异步执行的结果
    }

    void ThreadPool::worker() // 每个线程执行函数
    {
        while (1) // 死循环
        {
            std::function<void()> task; // 定义一临时存储异步任务的变量
            {
                std::unique_lock<std::mutex> lock(mtx); // 给mtx上锁
                cv.wait(lock, [this]
                        { return this->stop || !this->work_que.empty(); }); // 等待条件（是否是stop变量为false，或者任务队列是否为空）
                if (stop && work_que.empty())                               // stop==1并且任务队列为空，则关闭线程
                    return;
                task = std::move(this->work_que.front()); // 给队列上面的赋值给task
                this->work_que.pop();                     // 出队
            }
            task(); // 执行task
        }
    }
} // namespace threadpool
