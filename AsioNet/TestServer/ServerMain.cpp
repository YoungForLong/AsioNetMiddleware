#include "proxy_manager.h"
#include "protocol.hpp"
#include <iostream>
#include "parallel_core/SafeRandom.hpp"
#include "WLog/WLog.h"

using namespace net_middleware;
using parallel_core::SafeRandom;

int main()
{	
    WLogMgr->Init("F:\\ProxyLog\\", "log", false);
    WLogMgr->AppendLogMask(ELogType::ELT_Error);
    WLogMgr->AppendLogMask(ELogType::ELT_Info);

    PROXY_MGR->start();

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
	

	return 0;
}