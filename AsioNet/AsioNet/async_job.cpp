#include "async_job.h"
#include "NetUtils.hpp"

net_middleware::async_job_executor::async_job_executor(size_t n):
	_strand_num(n),
	_continious_job(_io_context)
{
	for (int i = 0; i < n; ++i)
		_strands.emplace_back(_io_context);
}

net_middleware::async_job_executor::~async_job_executor()
{
	_io_context.stop();

	for (auto& thd : _threads)
	{
		thd.join();
	}
}

void net_middleware::async_job_executor::start()
{
	for (int i = 0; i < _strand_num; ++i)
	{
		auto self(shared_from_this());
		_threads.push_back(THREAD_WRAPPER->createThread([this, self]() {
			_io_context.run();
		}));
	}
}

uint32_t net_middleware::job_agent::_sc = 1;