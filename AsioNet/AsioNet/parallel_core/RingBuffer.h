#pragma once

#include <malloc.h>
#include "ParallelUtils.h"

#define ALLOW_BLOCK_COPY 1

namespace parallel_core
{
	// consumer & producer are single
	// lock-free queue
	template<class T>
	class RingBuffer
	{
	private:
		T* _buffer;
		size_t _readIndex;
		size_t _writeIndex;
		size_t _capacity;
	public:
		RingBuffer(size_t capacity);

		~RingBuffer();

		size_t count() const;

		bool full(size_t add = 0) const;

		bool empty(size_t del = 0) const;

#ifdef ALLOW_BLOCK_COPY
#pragma message("low-performance datablock copy implement, use tryReadWithoutCopy instead")
		bool tryRead(size_t length, T* ret, bool withoutShift = false);

		bool tryWrite(T* data, size_t length = 0);
#else
		// @pram withoutShift
		// unless u know why its lock-free for sure,
		// do not use withoutShift = false
		bool tryReadWithoutCopy(size_t length, T* toReadBlock1, size_t* toReadLength1, T* toReadBlock2, size_t* toReadLength2, bool withoutShift = true);

		bool tryWriteWithoutCopy(size_t length, T* toWriteBlock1, size_t* toWriteLeng1, T* toWriteBlock2, size_t* toWriteLeng2, bool withoutShit = true);
#endif
		bool tryReadShift(size_t length);

		bool tryWriteShit(size_t length);

		void clear();
	};

	template<class T>
	inline RingBuffer<T>::RingBuffer(size_t capacity) :
		_capacity(capacity),
		_readIndex(0),
		_writeIndex(0)
	{
		_buffer = (T*)malloc(sizeof(T) * capacity);
	}

	template<class T>
	inline RingBuffer<T>::~RingBuffer()
	{
		free(_buffer);
	}

	template<class T>
	inline size_t RingBuffer<T>::count() const
	{
		return (_writeIndex - _readIndex + _capacity) % _capacity;
	}

	template<class T>
	inline bool RingBuffer<T>::full(size_t add) const
	{
		return (count() + add) >= _capacity;
	}

	template<class T>
	inline bool RingBuffer<T>::empty(size_t del) const
	{
		return count() - del <= 0;
	}

#ifdef ALLOW_BLOCK_COPY
	template<class T>
	inline bool RingBuffer<T>::tryRead(size_t length, T* ret, bool withoutShift)
	{
		if (empty())
			return false;

		for (int i = 0; i < length; ++i)
		{
			ret[i] = _buffer[(_readIndex + i) % _capacity];
		}

		if (LIKELY(!withoutShift))
		{
			_readIndex += length;
			_readIndex %= _capacity;
		}

        return true;
	}

	template<class T>
	inline bool RingBuffer<T>::tryWrite(T* data, size_t length)
	{
		if (full())
			return false;
		else
		{
			if (length + count() >= _capacity)
				return false;
			for (int i = 0; i < length; ++i)
			{
				_buffer[(_writeIndex + i) % _capacity] = data[i];
			}
			_writeIndex += length;
			_writeIndex %= _capacity;
		}

		return true;
	}
	
#else

	template<class T>
	inline bool RingBuffer<T>::tryReadWithoutCopy(size_t length, T* toReadBlock1, size_t* toReadLength1, T* toReadBlock2, size_t* toReadLength2, bool withoutShift)
	{
		if (UNLIKELY(empty(length)))
			return false;

		toReadBlock1 = &_buffer[_readIndex];
		if (UNLIKELY(_readIndex + length > _capacity))
		{
			toReadLength1 = _capacity - _readIndex;
			toReadBlock2 = _buffer;
			toReadLength2 = length - _capacity + _readIndex;
		}
		else
		{
			toReadLength1 = length;
		}

		if (UNLIKELY(!withoutShift))
		{
			_readIndex += length;
			_readIndex %= _capacity;
		}
	}

	template<class T>
	inline bool RingBuffer<T>::tryWriteWithoutCopy(size_t length, T* toWriteBlock1, size_t* toWriteLeng1, T* toWriteBlock2, size_t* toWriteLeng2, bool withoutShit)
	{
		if (UNLIKELY(full(length)))
			return false;
		else
		{
			toWriteBlock1 = &_buffer[_writeIndex];
			if (UNLIKELY(_writeIndex + length > _capacity))
			{
				toWriteLeng1 = _capacity - _writeIndex;
				toWriteBlock2 = _buffer;
				toWriteLeng2 = length - _capacity + _writeIndex;
			}
			else
			{
				toWriteLeng1 = length;
			}

			if (UNLIKELY(!withoutShit))
			{
				_writeIndex += length;
				_writeIndex %= _capacity;
			}
		}

		return true;
	}
#endif

	template<class T>
	inline bool RingBuffer<T>::tryReadShift(size_t length)
	{
		if (UNLIKELY(empty(length)))
			return false;

		_readIndex += length;
		_readIndex %= _capacity;
		return true;
	}

	template<class T>
	inline bool RingBuffer<T>::tryWriteShit(size_t length)
	{
		if (UNLIKELY(full(length)))
			return false;

		_writeIndex += length;
		_writeIndex %= _capacity;
		return true;
	}

	template<class T>
	inline void RingBuffer<T>::clear()
	{
		_readIndex = 0;
		_writeIndex = 0;
	}

}