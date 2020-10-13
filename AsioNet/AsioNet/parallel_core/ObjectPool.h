#pragma once

#include <assert.h>
#include <stack>
#include <mutex>

#include "Spinlock.hpp"

namespace parallel_core
{
	template<class T>
	class ObjectPool
	{
	public:
		ObjectPool();

		~ObjectPool();

		inline int countAll();

		inline int countActive();

		inline int countInactive();

		template <class ... P>
		inline T* get(P&& ... p);

		inline void release(T* element);

		inline void clear();
	private:
		std::atomic<int> _count;
		Spinlock _lock;
		std::stack<T*> _pool;
	};

	template<class T>
	inline ObjectPool<T>::ObjectPool()
	{
		_count.store(0);
	}

	template<class T>
	ObjectPool<T>::~ObjectPool()
	{
		clear();
	}

	template<class T>
	inline int ObjectPool<T>::countAll()
	{
		return _count.load();
	}

	template<class T>
	inline int ObjectPool<T>::countActive()
	{
		return _count.load() - countInactive();
	}

	template<class T>
	inline int ObjectPool<T>::countInactive()
	{
		return _pool.size();
	}

	template<class T>
	template <class ... P>
	inline T* ObjectPool<T>::get(P&& ... p)
	{
		if (LIKELY(_pool.size() == 0))
		{
			_count++;
			return new(std::nothrow) T(std::forward<P>(p) ...);
		}

		SpinlockHolder lk(&_lock);
		T* ret = _pool.top();
		_pool.pop();
		return ret;
	}

	template<class T>
	inline void ObjectPool<T>::release(T * element)
	{
		assert(NULL != element);

		SpinlockHolder lk(&_lock);
		_pool.push(element);
	}

	template<class T>
	inline void ObjectPool<T>::clear()
	{
		for (;;)
		{
			_count -= 1;

			SpinlockHolder lk(&_lock);
            if (!_pool.empty())
            {
                T* one = _pool.top();
                _pool.pop();

                _lock.unlock();
                SAFE_DELETE(one);
                _lock.lock();
            }
            else
                break;
		}
		
	}
}
