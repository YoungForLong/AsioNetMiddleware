// AsioNet.cpp: 定义应用程序的入口点。
//
#include <iostream>
#include <string>
#include "active_server_session_mgr.h"
#include "inner_pair_session.h"

using namespace net_middleware;
int main()
{
    WLogMgr->Init("F:\\ProxyLog\\", "Svrlog", false);
    WLogMgr->AppendLogMask(ELogType::ELT_Error);
    WLogMgr->AppendLogMask(ELogType::ELT_Info);

    // server to proxy session example
    /*ACTIVE_SERVER_MGR->start();

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }*/


    // LAN inner session pair example
    /*inner_pair_session session_inst;
    session_inst.set_config(session_config());
    session_inst.start();*/

	return 0;
}
