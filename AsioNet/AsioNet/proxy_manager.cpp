#include "proxy_manager.h"
#include "LogUtils.hpp"
#include "default_session_logic.h"
#include "server_session_logic.h"
#include "client_session_logic.h"

using namespace std;
using namespace net_middleware;
using asio::ip::tcp;

net_middleware::proxy_manager::proxy_manager():
	_config(),
	_acceptor_executor(new async_job_executor(1)),
	_acceptor_job_agent(JOB_AGENT(_acceptor_executor)),
	_acceptor(_acceptor_job_agent->strand_to_run(), tcp::endpoint(tcp::v4(), _config.listened_port_)),
	_session_excutor(new async_job_executor(_config.session_thread_num_)),
	_clean_up_timer(_acceptor_executor->context_to_run())
{
	
}

net_middleware::proxy_manager::~proxy_manager()
{
    _clean_up_timer.cancel();
}

void net_middleware::proxy_manager::start()
{
	_session_excutor->start();
	_acceptor_executor->start();

	// wait thread init
	std::this_thread::sleep_for(std::chrono::seconds(2));

	accept_a_session();

	clean_up_closed_session();
}

void net_middleware::proxy_manager::accept_a_session()
{
	if (!_acceptor_job_agent)
		return;

	_acceptor_job_agent->strand_to_run().post([this]() {

		auto new_session = std::make_shared<basic_async_session>(_session_excutor);
        new_session->modify_session_logic(DEFAULT_SESSION_LOGIC);
		_acceptor.async_accept(new_session->socket_to_accept(), [new_session, this](asio::error_code ec) {
			if (UNLIKELY(ec))
			{
				LOG("fatal accept a socket, %s", ec.message().c_str());
				stop_proxy();
				return;
			}
			
            {
                LOG_NON_SENSITIVE("accept a new session");
            }

            _un_managed_sessions.insert(new_session->get_uuid(), new_session);

            new_session->passively_connect_succ();

            new_session->start_tick(
                std::chrono::milliseconds(_config.tick_interval_),
                std::chrono::milliseconds(_config.keep_alive_timeout_));

			accept_a_session();
		});
	});
}

void net_middleware::proxy_manager::stop_proxy()
{
	LOG("STOP PROXY");
	_acceptor_executor->stop();
	_session_excutor->stop();
}

bool net_middleware::proxy_manager::verify_authentication(SessionType session_type_, server_id server_id_, session_uid* target_uid, session_uid session_uid_, uint32_t ip)
{
	if (session_type_ == SessionType::CLIENT_PROXY)
	{
		bool find = false;
        session_sptr target_server_session;
		auto find_method = [&find, server_id_, &target_uid, &target_server_session](std::unordered_map<session_uid, session_sptr> container) {
			for (auto& pair : container)
			{
                auto logic = pair.second->get_logic();

                auto server_logic = session_logic_interface::session_cast<server_session_logic>(logic);
                if (server_logic->get_server_id() == server_id_)
                {
                    *target_uid = pair.second->get_uuid();
                    target_server_session = pair.second;
                    find = true;
                }
			}
		};

		_server_sessions.complex_operation(find_method);

		if (!find)
		{
			LOG("cannot find server, self session type: %d,  target server_id: %llu", session_type_, server_id_);
			return false;
		}

        // send confirm connect msg to server
        auto logic = target_server_session->get_logic();
        auto server_logic = session_logic_interface::session_cast<server_session_logic>(logic);
        server_logic->send_confirm_connect(session_uid_, ip);

        move_client_available(session_uid_);

		return true;
	}
    else if (session_type_ > SessionType::MULTI_SESSION_BEGIN)
    {
        if (UNLIKELY(_server_sessions.contains_key(session_uid_)))
        {
            LOG("duplicate server, %d", session_uid_);
            return false;
        }
        else
        {
            move_server_available(session_uid_);
            return true;
        }
    }

	return false;
}

void net_middleware::proxy_manager::send_to_server(session_uid target, once_buffer_sptr msg)
{
	session_sptr server;
	if (UNLIKELY(!_server_sessions.try_get(target, server)))
	{
		LOG("target session id not found, uid %d", target);
		return;
	}

	server->async_send(msg);
}

void net_middleware::proxy_manager::send_to_server_multi(session_uid target, tiny_buffer_sptr head, once_buffer_sptr msg)
{
    session_sptr server;
    if (UNLIKELY(!_server_sessions.try_get(target, server)))
    {
        LOG("target session id not found, uid %d", target);
        return;
    }

    server->async_send_multi(head, msg);
}

void net_middleware::proxy_manager::send_to_client(session_uid client_uid, once_buffer_sptr msg)
{
	session_sptr client;
	if (UNLIKELY(!_client_sessions.try_get(client_uid, client)))
	{
		LOG("cannot find client, client uid: %lu", client_uid);
		return;
	}

	client->async_send(msg);
}

void net_middleware::proxy_manager::send_to_client_multi(session_uid client_uid, tiny_buffer_sptr head, once_buffer_sptr msg)
{
    session_sptr client;
    if (UNLIKELY(!_client_sessions.try_get(client_uid, client)))
    {
        LOG("cannot find client, client uid: %lu", client_uid);
        return;
    }

    client->async_send_multi(head, msg);
}

void net_middleware::proxy_manager::move_client_available(session_uid client_uid)
{
    session_sptr c_s;
    if (LIKELY(_un_managed_sessions.try_get(client_uid, c_s)))
    {
        _un_managed_sessions.erase(client_uid);
        
        auto origin_logic = c_s->get_logic();
        if (UNLIKELY(origin_logic->get_session_type() != SessionType::SINGLE_SESSION_BEGIN))
        {
            LOG("trans session logic failed, session logic was %d", origin_logic->get_session_type());
            return;
        }

        auto default_logic = session_logic_interface::session_cast<default_session_logic>(origin_logic);
        auto new_client_logic = default_logic->trans_to_client();
        
        c_s->modify_session_logic(session_logic_interface::session_cast<session_logic_interface>(new_client_logic));

        if (UNLIKELY(!_client_sessions.insert(client_uid, c_s)))
        {
            LOG("duplicate client session %d", client_uid);
        }
    }
    else
    {
        LOG("_un_managed_sessions doesn't have this session %u, please check", client_uid);
    }
	
}

void net_middleware::proxy_manager::move_server_available(session_uid server_uid)
{
    LOG_NON_SENSITIVE("move server, log size: %llu", _un_managed_sessions.size());

    session_sptr s_s;
    if (LIKELY(_un_managed_sessions.try_get(server_uid, s_s)))
    {
        _un_managed_sessions.erase(server_uid);

        auto origin_logic = s_s->get_logic();
        if (UNLIKELY(origin_logic->get_session_type() != SessionType::SINGLE_SESSION_BEGIN))
        {
            LOG("trans session logic failed, session logic was %d", origin_logic->get_session_type());
            return;
        }

        auto default_logic = session_logic_interface::session_cast<default_session_logic>(origin_logic);
        auto new_server_logic = default_logic->trans_to_server();

        s_s->modify_session_logic(session_logic_interface::session_cast<session_logic_interface>(new_server_logic));

        if (UNLIKELY(!_server_sessions.insert(server_uid, s_s)))
        {
            LOG("duplicate server session %d", server_uid);
        }
    }
    else
    {
        LOG("_un_managed_sessions doesn't have this session %u, please check", server_uid);
    }
}

bool net_middleware::proxy_manager::kick_one_client(session_uid client_uid)
{
    session_sptr cln;
    if (LIKELY(_client_sessions.try_get(client_uid, cln)))
    {
        cln->kick();
        return true;
    }
    else
    {
        LOG("cannot find client to kick, uid = %lu", client_uid);
        return false;
    }
}

bool net_middleware::proxy_manager::kick_server_peer(SessionType server_type)
{
    auto kick_method = [server_type](std::unordered_map<session_uid, session_sptr> container) {
        for (auto& pair : container)
        {
            auto logic = pair.second->get_logic();
            auto cln_logic = session_logic_interface::session_cast<client_session_logic>(logic);
            if (cln_logic->get_target_server_type() == server_type)
            {
                pair.second->kick();
            }
        }
    };

    _client_sessions.complex_operation(kick_method);

    auto kick_self_method = [server_type](std::unordered_map<session_uid, session_sptr> container) {
        for (auto& pair : container)
        {
            auto logic = pair.second->get_logic();
            if (logic->get_session_type() == server_type)
            {
                pair.second->kick();
            }
        }
    };
    
    _server_sessions.complex_operation(kick_self_method);

    return false;
}

void net_middleware::proxy_manager::clean_up_closed_session()
{
	_clean_up_timer.expires_from_now(std::chrono::milliseconds(PROXY_CLEAN_PERIOD_MS));

	_clean_up_timer.async_wait([this](asio::error_code ec) {
		if (UNLIKELY(ec))
		{
			LOG("timer error occurred %s", ec.message().c_str());
		}

		auto close_method = [](std::unordered_map<session_uid, session_sptr> container) {
			for (auto iter = container.begin(); iter != container.end();)
			{
				if (iter->second->is_session_closed())
					iter = container.erase(iter);
				else
					++iter;
			}
		};

		_client_sessions.complex_operation(close_method);

		_server_sessions.complex_operation(close_method);

		clean_up_closed_session();
	});
}
