
#include "ThreadWrapper.h"
#include "ParallelUtils.h"

std::thread parallel_core::ThreadWrapper::createThread(const ThreadDelegate& func)
{
	return std::thread([&, this, func]() {
		afterCreateThread(GET_CURRENT_THREAD_ID);
#if defined(__cpp_lib_invoke)
		std::invoke(func);
#else
		func();
#endif
		beforeDestroyThread(GET_CURRENT_THREAD_ID);
	});
}

void parallel_core::ThreadWrapper::registerListener(int pid, std::shared_ptr<ThreadLifecircleListener> listener)
{
	std::lock_guard<std::recursive_mutex> lock(_mapMut);
	if (LIKELY(HAS_KEY(_pidListenerMap, pid)))
	{
		ListenerList& list_ = _pidListenerMap[pid];
		list_.push_back(listener);
	}
	else
	{
		ListenerList list_;
		list_.push_back(listener);
		_pidListenerMap.insert({ pid, list_ });
	}
}

void parallel_core::ThreadWrapper::afterCreateThread(int pid)
{
}

void parallel_core::ThreadWrapper::beforeDestroyThread(int pid)
{
	std::lock_guard<std::recursive_mutex> lock(_mapMut);
	if (LIKELY(HAS_KEY(_pidListenerMap, pid)))
	{
		for (std::shared_ptr<parallel_core::ThreadLifecircleListener> listener : _pidListenerMap[pid])
		{
			listener->onThreadClear(pid);
		}

		_pidListenerMap.erase(pid);
	}
}
