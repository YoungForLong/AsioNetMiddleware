#pragma once

#include "session_logic.h"
#include "parallel_core/ThreadSafeObjectPool.h"
#include "basic_async_session.h"

namespace net_middleware
{
    // actively connect to proxy
    // settled into server
    class active_server_session_logic : public session_logic_interface, public std::enable_shared_from_this<active_server_session_logic>
    {
    public:
        active_server_session_logic();
        virtual ~active_server_session_logic();

        virtual void apply_session(std::weak_ptr<basic_async_session> session_holder) final;

        virtual bool is_free_session() final { return false; }

        virtual SessionType get_session_type() final { return SessionType::ACTIVE_GAMESVR; }

        virtual bool unwrap_received_data(once_buffer_sptr buffer, protocol_head::head_sptr head, once_buffer_sptr ret_block) final;

        virtual size_t prefix_size() final;

        virtual void wrap_to_send_data(once_buffer_sptr& buffer) final;

        virtual bool try_copy_to_storage(once_buffer_sptr data, protocol_head::head_sptr head) final;

        virtual void kick_peer() final;

        void send_verify_authentication();

        void set_server_info(const server_info& s_info);

        std::shared_ptr<active_server_session_logic> reset();

        void share_storage(lockfree_buffer_sptr storage);

    private:
        void check_verify_res(once_buffer_sptr data);

        void remote_session_info_confirm(once_buffer_sptr data);

    private:
        std::weak_ptr<basic_async_session> _session_holder;
        
        AuthenticationState _authentication;

        uint32_t _seq;

        server_info _server_info;

        lockfree_buffer_sptr _storage;
    };

#define ACTIVE_SERVER_SESSION_LOGIC parallel_core::ThreadSafeObjectPool<active_server_session_logic>::instance()->get_shared()->reset()
}