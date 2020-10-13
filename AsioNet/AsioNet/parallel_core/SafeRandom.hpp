#pragma once
#include <atomic>
#include <time.h>

#include "ThreadCacheContainer.h"
#include "Spinlock.hpp"
#include "SafeSingleton.h"

namespace parallel_core
{
	class SafeRandom:public SafeSingleton<SafeRandom>
	{
    public:
        SafeRandom()
        {
            seed(time(0));
        }

        ~SafeRandom()
        {
            SAFE_DELETE(_rand_state);
        }
	public:

		// @return from zero to one
		uint64_t next()
		{
			SpinlockHolder lock(&_spinLock);
			return xorshift64(_rand_state);
		}

	private:

		// from wikipedia
		struct xorshift64_state {
			uint64_t a;
		};

		uint64_t xorshift64(struct xorshift64_state* state)
		{
			uint64_t x = state->a;
			x ^= x << 13;
			x ^= x >> 7;
			x ^= x << 17;
			return state->a = x;
		}

		void seed(uint64_t origin)
		{
			_rand_state = new xorshift64_state();
			_rand_state->a = origin;
		}

		xorshift64_state* _rand_state;

		Spinlock _spinLock;
	};
}

#define SAFE_RAND parallel_core::SafeRandom::instance()->next()