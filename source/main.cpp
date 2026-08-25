// RPCGateWay/main.cpp
#include "RPCGateWayWork.h"

// 定义输出代码
extern int Utils::serviceID;

int main()
{
    // 初始化控制台
    Utils::init();
    try
    {
        // 创建SQL连接（idx=0，与 HandleVueBiz 中 g_conns[0] 对应）
        CreateConnection("127.0.0.1", "60919");

        Utils::Out::Out_Msg("网关运行中");

        // TODO去完成前端的vue收发
        RunHttpServer(60906, 8080);

        // 主线程阻塞等待退出事件
        Utils::Exit::WaitExit();
    }
    catch (boost::system::error_code ec)
    {
        Utils::Out::Out_Err("错误信息：" + ec.what());
    }
    Utils::Out::Out_Msg("网关服务退出");

    return 0;
}