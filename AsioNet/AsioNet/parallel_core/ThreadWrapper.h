#pragma once

#include <thread>
#include <functional>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <mutex>

#include "SafeSingleton.h"
#include "ThreadLifecircleListener.h"

namespace parallel_core
{
	class ThreadWrapper : public SafeSingleton<ThreadWrapper>
	{
	public:
		using ThreadDelegate = std::function<void()>;
		using ListenerList = std::vector<std::shared_ptr<ThreadLifecircleListener>>;
		using PidListenerMap = std::unordered_map<unsigned long, ListenerList>;
	public:
		// use move to avoid delegate destructor
		std::thread createThread(const ThreadDelegate& func);

		// not necessary to unregister
		void registerListener(int pid, std::shared_ptr<ThreadLifecircleListener> listener);

		void afterCreateThread(int pid);

		void beforeDestroyThread(int pid);
	private:
		std::recursive_mutex _mapMut;
		PidListenerMap _pidListenerMap;
	};
}

#define THREAD_WRAPPER parallel_core::ThreadWrapper::instance()