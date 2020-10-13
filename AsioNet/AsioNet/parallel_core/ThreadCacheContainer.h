#pragma once

#include <thread>
#include <mutex>
#include <unordered_map>
#include <assert.h>

#include "ParallelUtils.h"
#include "ThreadLifecircleListener.h"
#include "ThreadWrapper.h"

namespace parallel_core
{
	template<class T>
	class ThreadCacheContiner : public ThreadLifecircleEvents
	{
	public:
		ThreadCacheContiner();
		~ThreadCacheContiner();

		T* getThreadCache(int pid = -1);

		void clearUnusedThreadCache();

		virtual void onThreadCreated(int pid) override;

		virtual void onThreadClear(int pid) override;

	private:
		std::recursive_mutex _mut;
		std::unordered_map<int, T*> _threadIdMap;
	};

	template<class T>
	ThreadCacheContiner<T>::ThreadCacheContiner()
	{
	}

	template<class T>
	ThreadCacheContiner<T>::~ThreadCacheContiner()
	{
		clearUnusedThreadCache();
	}

	template<class T>
	inline T * ThreadCacheContiner<T>::getThreadCache(int pid)
	{
		T* obj;
		int threadId = (pid == -1 ? GET_CURRENT_THREAD_ID : pid);

		std::atomic_thread_fence(std::memory_order_acquire);
		if (LIKELY(!HAS_KEY(_threadIdMap, threadId)))
		{
			std::lock_guard<std::recursive_mutex> lock(_mut);
			
			obj = new(std::nothrow) T;
			if (UNLIKELY(!obj))
			{
				return NULL;
			}

			// 此条语句是非原子的，可能在过程中产生find通过的情况，需要添加内存屏障
			// this express is not atomic, we need to add acquire&release memory order
			std::atomic_thread_fence(std::memory_order_release);
			auto ret = _threadIdMap.insert(std::make_pair(threadId, obj));

			std::shared_ptr<ThreadLifecircleListener> listenerSptr(new ThreadLifecircleListener(this));
			THREAD_WRAPPER->registerListener(threadId, listenerSptr);

			if (UNLIKELY(!ret.second))
			{
				SAFE_DELETE(obj);
				return _threadIdMap[threadId];
			}
		}

		obj = _threadIdMap[threadId];
		return obj;
	}

	template<class T>
	inline void ThreadCacheContiner<T>::clearUnusedThreadCache()
	{
		std::lock_guard<std::recursive_mutex> lock(_mut);
		for (auto& one : _threadIdMap)
		{
			SAFE_DELETE(one.second);
		}
		_threadIdMap.clear();
	}
	
	template<class T>
	inline void ThreadCacheContiner<T>::onThreadCreated(int pid)
	{
	}

	template<class T>
	inline void ThreadCacheContiner<T>::onThreadClear(int pid)
	{
		std::lock_guard<std::recursive_mutex> lock(_mut);

		if (LIKELY(HAS_KEY(_threadIdMap, pid)))
		{
			T* one = _threadIdMap[pid];
			SAFE_DELETE(one);
			_threadIdMap.erase(pid);
		}
		else
		{
			assert(false);
		}
	}
}