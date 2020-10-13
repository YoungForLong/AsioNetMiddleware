#pragma once

#include "parallel_core/ThreadSafeObjectPool.h"
#include "parallel_core/RingBuffer.h"
#include "async_job.h"
#include "error_code.hpp"
#include "reusabel_buffer.hpp"

#define LITTLE_ENDIAN 1

#define NET_PREDICTION_DELAY_MS_MAX 2000

#define ONCE_BUFFER_SIZE 1024 * 64
#define TINY_ONCE_BUFFER_SIZE 16 // although _mm128
#define TOTAL_CACHE_SIZE 1024 * 1024 * 10

typedef std::shared_ptr<reusabel_buffer<TINY_ONCE_BUFFER_SIZE>> tiny_buffer_sptr;
typedef std::shared_ptr<reusabel_buffer<ONCE_BUFFER_SIZE>> once_buffer_sptr;
typedef std::shared_ptr<parallel_core::RingBuffer<unsigned char>> lockfree_buffer_sptr;

#define RING_BUFFER_POOL parallel_core::ThreadSafeObjectPool<parallel_core::RingBuffer<unsigned char>>::instance()
#define TEMP_BUFFER_POOL parallel_core::ThreadSafeObjectPool<reusabel_buffer<ONCE_BUFFER_SIZE>>::instance()
#define TINY_BUFFER_POOL parallel_core::ThreadSafeObjectPool<reusabel_buffer<TINY_ONCE_BUFFER_SIZE>>::instance()

#define LOCK_FREE_BUFFER(name) name = RING_BUFFER_POOL->get_shared(TOTAL_CACHE_SIZE); \
name->clear()

#define TEMP_BUFFER TEMP_BUFFER_POOL->get_shared()->reset()
#define TINY_BUFFER TINY_BUFFER_POOL->get_shared()->reset()

#define STRING_BUFFER parallel_core::ThreadSafeObjectPool<std::string>::instance()->get_shared()

#pragma warning(push) 
#pragma warning(disable:4244)


// host byte order
inline void write_uint16(unsigned char* block, uint16_t val)
{
	block[1] = (val & 0x00ffu);
	block[0] = (val & 0xff00u) >> 8;
}

inline uint16_t read_uint16(unsigned char* block)
{
	uint16_t ret;
	ret = (uint16_t)block[1] | ((uint16_t)block[0] << 8);
	return ret;
}

inline void write_uint32(unsigned char* block, uint32_t val)
{
	block[3] =  val & 0x000000ffu;
	block[2] = (val & 0x0000ff00u) >> 8;
	block[1] = (val & 0x00ff0000u) >> 16;
	block[0] = (val & 0xff000000u) >> 24;
}

inline uint32_t read_uint32(unsigned char* block)
{
	uint32_t ret;
	ret = (uint32_t)block[3] | ((uint32_t)block[2] << 8) | ((uint32_t)block[1] << 16 | ((uint32_t)block[0] << 24));
	return ret;
}

inline void write_uint64(unsigned char* block, uint64_t val)
{
	block[7] =  val & 0x00000000000000ffull;
	block[6] = (val & 0x000000000000ff00ull) >> 8;
	block[5] = (val & 0x0000000000ff0000ull) >> 16;
	block[4] = (val & 0x00000000ff000000ull) >> 24;
	block[3] = (val & 0x000000ff00000000ull) >> 32;
	block[2] = (val & 0x0000ff0000000000ull) >> 40;
	block[1] = (val & 0x00ff000000000000ull) >> 48;
    block[0] = (val & 0xff00000000000000ull) >> 56;
}

inline int64_t read_uint64(unsigned char* block)
{
	int64_t ret;
	ret =(uint64_t)block[7]        |
		((uint64_t)block[6] << 8)  |
		((uint64_t)block[5] << 16) |
		((uint64_t)block[4] << 24) |
		((uint64_t)block[3] << 32) |
		((uint64_t)block[2] << 40) |
		((uint64_t)block[1] << 48) |
		((uint64_t)block[0] << 56);
	return ret;
}

#pragma warning(pop)

typedef uint64_t server_id;
typedef uint32_t session_uid;

#define PROXY_CLEAN_PERIOD_MS 3000

#define RC4_KEY_LEN 10

#define KEEP_ALIVE_PRECISION 1000