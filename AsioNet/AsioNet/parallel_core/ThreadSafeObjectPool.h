#pragma once

#include "parallel_core/ThreadCacheContainer.h"
#include "parallel_core/ObjectPool.h"
#include "parallel_core/SafeSingleton.h"
#include <assert.h>
#include <memory>
#include <mutex>
#include <tuple>

namespace parallel_core
{
	template <class T>
	class ThreadSafeObjectPool : public SafeSingleton<ThreadSafeObjectPool<T>>
	{
	public:
		template <class ... P>
		inline std::tuple<int, T*> get(P&& ... p);

		inline void release(T* element, int threadId);

		// enable datacopy from one thread to another
		template <class ...P>
		inline std::shared_ptr<T> get_shared(P&& ...p);

		inline void clearCache();
	private:
		ThreadCacheContiner<ObjectPool<T>> _container;
	};

	template<class T>
	template<class ... P>
	inline std::tuple<int, T*> ThreadSafeObjectPool<T>::get(P&& ... p)
	{
		auto* element = _container.getThreadCache()->get(std::forward<P>(p) ...);
		return std::make_tuple(GET_CURRENT_THREAD_ID, element);
	}

	template<class T>
	inline void ThreadSafeObjectPool<T>::release(T* element, int threadId)
	{
		assert(NULL != element);
		_container.getThreadCache(threadId)->release(element);
	}

	template<class T>
	template <class ...P>
	inline std::shared_ptr<T> ThreadSafeObjectPool<T>::get_shared(P&& ... p)
	{
		auto* ptr = _container.getThreadCache()->get(std::forward<P>(p)...);

		auto pid = GET_CURRENT_THREAD_ID;
		return std::shared_ptr<T>(ptr, [this, pid](T* p) {
			_container.getThreadCache(pid)->release(p);
		});
	}

	template<class T>
	inline void ThreadSafeObjectPool<T>::clearCache()
	{
		_container.clearUnusedThreadCache();
	}

}