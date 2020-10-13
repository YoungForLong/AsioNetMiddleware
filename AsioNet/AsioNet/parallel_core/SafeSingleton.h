#pragma once

#include <mutex>

namespace parallel_core
{
	template<class T>
	class SafeSingleton
	{
	protected:
		SafeSingleton() = default;
		virtual ~SafeSingleton() = default;

		SafeSingleton(const SafeSingleton& other) = delete;
		SafeSingleton& operator= (const SafeSingleton& other) = delete;
	public:
		static T* instance();
		
	private:
		static T* _inst;
		static std::once_flag _once;
	};

	template<class T>
	T* SafeSingleton<T>::_inst = NULL;

	template<class T>
	std::once_flag SafeSingleton<T>::_once;

	template<class T>
	inline T * SafeSingleton<T>::instance()
	{
		std::call_once(_once, [&]() {
			_inst = new T;
		});

		return _inst;
	}
}

