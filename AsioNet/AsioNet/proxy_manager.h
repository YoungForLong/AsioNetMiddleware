#pragma once

#include <vector>
#include <unordered_map>

#include "parallel_core/SafeSingleton.h"
#include "parallel_core/SafeHashMap.hpp"
#include "JsonUtils.hpp"
#include "NetUtils.hpp"
#include "basic_async_session.h"

namespace net_middleware
{
	class proxy_config
	{
	public:
		int16_t listened_port_;
		uint16_t session_thread_num_;
        uint32_t tick_interval_;
        uint32_t keep_alive_timeout_;
        uint32_t max_send_delay_;
		
		proxy_config(const std::string& filepath = "../../../../proxy_config.json")
		{
			if (json_utils::open_file(filepath.c_str(), _dom))
			{
				try
				{
					listened_port_      = _dom["listened_port"].GetInt();
					session_thread_num_ = _dom["session_thread_num"].GetInt();
                    tick_interval_      = _dom["tick_interval"].GetInt();
                    keep_alive_timeout_ = _dom["keep_alive_timeout"].GetInt();
                    max_send_delay_     = _dom["max_send_delay"].GetInt();

					return;
				}
				catch (std::exception e)
				{
					LOG("failed to load proxy config: %s", e.what());
				}
			}

			abort();
		}

	private:
		rapidjson::Document _dom;
	};

	class proxy_manager : public parallel_core::SafeSingleton<proxy_manager>
	{
	public:
		typedef std::shared_ptr<basic_async_session> session_sptr;
		typedef parallel_core::SafeHashMap<session_uid, session_sptr> session_set;
        using SessionType = net_middleware::session_logic_interface::SessionType;

    public:
        proxy_manager();
        ~proxy_manager();

		proxy_manager(const proxy_manager& other) = delete;
		proxy_manager& operator=(const proxy_manager& other) = delete;

	public:
        // start thread, executor & acceptor loop,
        // none blocking
		void start();

        // stop
		void stop_proxy();

        // authentication
		bool verify_authentication(SessionType session_type_, server_id server_id_, session_uid* target_uid, session_uid session_uid_, uint32_t ip);

        // send buffer to a managed server session
		void send_to_server(session_uid target, once_buffer_sptr msg);

        void send_to_server_multi(session_uid target, tiny_buffer_sptr head, once_buffer_sptr msg);

        // send buffer to a managed client session
		void send_to_client(session_uid client_uid, once_buffer_sptr msg);

        void send_to_client_multi(session_uid client_uid, tiny_buffer_sptr head, once_buffer_sptr msg);

        // move a free session to managed session
		void move_client_available(session_uid client_uid);

        // move a free session to managed session
        void move_server_available(session_uid server_uid);

        bool kick_one_client(session_uid client_uid);

        bool kick_server_peer(SessionType server_type);

    private:
        void accept_a_session();

        void clean_up_closed_session();

	private:
		proxy_config _config;

		job_excutor_sptr _acceptor_executor;
		std::shared_ptr<job_agent> _acceptor_job_agent;
		asio::ip::tcp::acceptor _acceptor;

		parallel_core::SafeHashMap<session_uid, session_sptr> _server_sessions;

		session_set _client_sessions;

		session_set _un_managed_sessions;
		
		job_excutor_sptr _session_excutor;

		asio::steady_timer _clean_up_timer;
	};

#define PROXY_MGR net_middleware::proxy_manager::instance()
}
