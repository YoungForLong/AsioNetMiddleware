#pragma once

#include <unordered_map>
#include <mutex>
#include <functional>
#include "ParallelUtils.h"
#include "LogUtils.hpp"

namespace parallel_core
{
	// using double check to improve performance
	template<class Key, class Value, class _Hasher = std::hash<Key>, class _Keyeq = std::equal_to<Key>>
	class SafeHashMap
	{
	public:
		bool contains_key(const Key& k)
		{
            return HAS_KEY(_container, k);
		}

		bool clear()
		{
			std::lock_guard<std::recursive_mutex> lock(_mut);
            LOG_NON_SENSITIVE("clear %d", k);
			_container.clear();
		}

		size_t size()
		{
			return _container.size();
		}

		bool empty()
		{
			return _container.empty();
		}

		bool insert(const Key& k, const Value& v)
		{
			std::atomic_thread_fence(std::memory_order_acquire);
			if (!HAS_KEY(_container, k))
			{
				std::lock_guard<std::recursive_mutex> lock(_mut);
				std::atomic_thread_fence(std::memory_order_release);

				auto ret = _container.insert(std::make_pair(k, v));
                LOG_NON_SENSITIVE("insert one %d", k);

				return ret.second;
			}
			return false;
		}

		void insert_or_set(const Key& k, const Value& v)
		{
			std::atomic_thread_fence(std::memory_order_acquire);
			if (!HAS_KEY(_container, k))
			{
				std::lock_guard<std::recursive_mutex> lock(_mut);
				std::atomic_thread_fence(std::memory_order_release);

				auto ret = _container.insert(std::make_pair(k, v));
                LOG_NON_SENSITIVE("insert one, %d", k);

				if (!ret.second)
				{
					_container[k] = v;
				}
			}
			return false;
		}

		bool insert(const std::initializer_list<std::pair<Key, Value>>& l)
		{
			std::lock_guard<std::recursive_mutex> lock(_mut);
            LOG_NON_SENSITIVE("insert list");
			_container.insert(l);
		}

		template< class... Args >
		bool emplace(Args&&... args)
		{
            std::atomic_thread_fence(std::memory_order_release);

            LOG_NON_SENSITIVE("emplace one");
            auto ret = _container.emplace(std::forward<Args>(args)...);

            return ret.second;
		}

		bool erase(const Key& k)
		{
			std::atomic_thread_fence(std::memory_order_acquire);
			if (!HAS_KEY(_container, k))
			{
				std::lock_guard<std::recursive_mutex> lock(_mut);
				std::atomic_thread_fence(std::memory_order_release);

				auto ret = _container.erase(k);
                LOG_NON_SENSITIVE("erase one %d", k);

				return ret > 0;
			}
			return false;
		}

		bool try_get(const Key& k, Value& ref_val)
		{
			std::atomic_thread_fence(std::memory_order_acquire);
			if (HAS_KEY(_container, k))
			{
				std::lock_guard<std::recursive_mutex> lock(_mut);
				std::atomic_thread_fence(std::memory_order_release);

                LOG_NON_SENSITIVE("find one %d", k);

                auto iter = _container.find(k);
				if (iter != _container.end())
				{
					ref_val = iter->second;
					return true;
				}
				else
				{
					return false;
				}
			}
			return false;
		}

		// for complex state
		typedef std::function<void(std::unordered_map<Key, Value, _Hasher, _Keyeq>)> container_handler;

		void complex_operation(container_handler)
		{
			std::lock_guard<std::recursive_mutex> lock(_mut);
			container_handler(_container);
		}

	private:
		std::recursive_mutex _mut;
		std::unordered_map<Key, Value, _Hasher, _Keyeq> _container;
	};
}

