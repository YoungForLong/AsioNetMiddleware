#include "ParallelUtils.h"

#ifdef __linux__
#include "unistd.h"
#include "sys/sysinfo.h"
// assign each thread a processor
size_t get_standard_thread_num()
{
	return sysconf(_SC_NPROCESSORS_CONF);
}
#else
#include "winsock2.h"
#include "windows.h"

size_t get_standard_thread_num()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwNumberOfProcessors;
}

#endif