#pragma once

#include <memory>
#include <functional>
#include <thread>
#include <time.h>

#include "asio.hpp"

namespace net_middleware
{
#define hash_n(uid, n) uid % n

	class async_job_executor : public std::enable_shared_from_this<async_job_executor>
	{
	public:

#pragma region constructors
		explicit async_job_executor(size_t n);
		~async_job_executor();
		async_job_executor(const async_job_executor&) = delete;
		async_job_executor& operator= (const async_job_executor&) = delete;
#pragma endregion
		
		inline asio::io_context& context_to_run() { return _io_context; }

		inline asio::io_context::strand& strand_to_run(unsigned long long user_id) { return _strands[hash_n(user_id, _strand_num)]; }

		void start();

		inline void stop() { _io_context.stop(); }
	private:
		size_t _strand_num;
		std::vector<asio::io_context::strand> _strands;
		asio::io_context _io_context;
		asio::io_context::work _continious_job;
		std::vector<std::thread> _threads;
	};

	class job_agent : public std::enable_shared_from_this<job_agent>
	{
	public:
		job_agent(std::shared_ptr<async_job_executor> excutor):
			_owner(excutor)
		{
		}
		
        inline std::shared_ptr<job_agent> reset()
        {
            _uuid = (unsigned long long)clock() + (_sc++ << 15 >> 15);
            return shared_from_this();
        }

		inline unsigned long long uuid() const { return _uuid; }

		inline asio::io_context::strand& strand_to_run()
		{
			return _owner->strand_to_run(_uuid);
		}

	private:
		unsigned long long _uuid;
		static uint32_t _sc;
		std::shared_ptr<async_job_executor> _owner;
	};
}

typedef std::shared_ptr<net_middleware::async_job_executor> job_excutor_sptr;
#define JOB_AGENT_POOL parallel_core::ThreadSafeObjectPool<net_middleware::job_agent>::instance()
#define JOB_AGENT(excutor) JOB_AGENT_POOL->get_shared(excutor)->reset()