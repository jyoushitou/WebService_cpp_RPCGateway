// RPCGateWay/main.cpp
#include "RPCGateWayWork.h"

int main()
{
    Utils::init();

    // 创建自动复位事件（初始无信号）
    g_exit_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_exit_event)
    {
        Utils::Out_Err("创建退出事件失败", ServiceID_RPCGateWay);
        return 1;
    }

    // 注册控制台信号处理（必须在创建连接之前）
    if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE))
    {
        Utils::Out_Err("注册控制台处理函数失败", ServiceID_RPCGateWay);
        CloseHandle(g_exit_event);
        return 1;
    }

    // 创建SQL连接（idx=0，与 HandleVueBiz 中 g_conns[0] 对应）
    CreateConnection(0, ServiceID_RPCGateWay, "127.0.0.1", "60000");

    Utils::Out_Msg("网关内网运行中", ServiceID_RPCGateWay);

    // TODO去完成前端的vue收发
    RunHttpServer(60906, 8080, ServiceID_RPCGateWay);

    // 主线程完全挂起，等待退出事件被 SetEvent
    // 此时不占任何 CPU，这是事件对象比 sleep 轮询的绝对优势
    WaitForSingleObject(g_exit_event, INFINITE);

    // 优雅关闭流程
    Utils::Out_Msg("收到退出信号，正在关闭所有连接...", ServiceID_RPCGateWay);

    // TODO接收web服务器的数据

    // Stop 所有连接：内部 post 到各自 IO 线程，线程安全
    // 跳过空槽位（断开后等待重连期间的 nullptr）
    for (auto& conn : g_conns)
        if (conn && conn->client)
            conn->client->Stop();

    // 注意：Close() 回调（IO 线程内）已经将 io_thread detach 并置空缺省槽位，
    // 因此这里不能也不应再 join（join 已 detach 的线程会抛 system_error）。
    // 给 IO 线程一点时间完成关闭，随后进程退出时 OS 会清理剩余资源。
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 清理
    CloseHandle(g_exit_event);
    Utils::Out_Msg("网关服务退出", ServiceID_RPCGateWay);
    return 0;
}