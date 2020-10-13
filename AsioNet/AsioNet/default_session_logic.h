#pragma once

#include "session_logic.h"
#include "protocol.hpp"
#include "basic_async_session.h"
#include "parallel_core/ThreadSafeObjectPool.h"
#include "parallel_core/SafeRandom.hpp"

namespace net_middleware
{
    class client_session_logic;
    class server_session_logic;

    // all sessions are default session at the beginning
    // after being verified, it may be set to other session logic
    class default_session_logic : public session_logic_interface, public std::enable_shared_from_this<default_session_logic>
    {
    public:

#pragma region (dis)constructors
        default_session_logic();
        virtual ~default_session_logic();
#pragma endregion

#pragma region inherit

        virtual void apply_session(std::weak_ptr<basic_async_session> session_holder) final;

        virtual bool is_free_session() final { return true; }

        virtual SessionType get_session_type() final { return SessionType::SINGLE_SESSION_BEGIN; }

        virtual bool unwrap_received_data(once_buffer_sptr buffer, protocol_head::head_sptr head, once_buffer_sptr ret_block) final;

        virtual void wrap_to_send_data(once_buffer_sptr& buffer) final;

        virtual bool try_copy_to_storage(once_buffer_sptr data, protocol_head::head_sptr head) final;

        virtual size_t prefix_size() final { return PROTO_HEAD_SIZE; }

        virtual void kick_peer() final { /* has no peer to kick */ }

#pragma endregion

        std::shared_ptr<server_session_logic> trans_to_server();

        std::shared_ptr<client_session_logic> trans_to_client();

        inline std::shared_ptr<default_session_logic> reset()
        {
            _authentication = AuthenticationState::BEFORE_VERIFY;
            _target_uid = 0;
            rc4_info new_one;
            _rc4_info = new_one;
            _seq = 0;

            return shared_from_this();
        }

    protected:
        bool verify_authentication(once_buffer_sptr tmp_buffer, protocol_cmd cmd);

        void echo_authentication_res(TransferError ec);

    private:
        AuthenticationState _authentication;

        std::weak_ptr<basic_async_session> _session_holder;

        session_uid _target_uid;

        rc4_info _rc4_info;

        server_info _server_info;

        uint32_t _seq;
    };

#define DEFAULT_SESSION_LOGIC parallel_core::ThreadSafeObjectPool<default_session_logic>::instance()->get_shared()->reset()
}
