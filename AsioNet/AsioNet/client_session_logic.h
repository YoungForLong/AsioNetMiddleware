#pragma once

#include "parallel_core/ThreadSafeObjectPool.h"
#include "session_logic.h"

namespace net_middleware
{
    class client_session_logic : public session_logic_interface, public std::enable_shared_from_this<client_session_logic>
    {
    public:
#pragma region inherit
        virtual void apply_session(std::weak_ptr<basic_async_session> session_holder) final;

        virtual bool is_free_session() final { return false; }

        virtual SessionType get_session_type() final { return SessionType::CLIENT_PROXY; }

        virtual bool unwrap_received_data(once_buffer_sptr buffer, protocol_head::head_sptr head, once_buffer_sptr ret_block) final;

        virtual void wrap_to_send_data(once_buffer_sptr& buffer) final;

        virtual bool try_copy_to_storage(once_buffer_sptr data, protocol_head::head_sptr head) final;

        virtual size_t prefix_size() final { return PROTO_HEAD_SIZE + sizeof(session_uid); }

        inline SessionType get_target_server_type() { return (SessionType)_server_info.link_type_; }

        virtual void kick_peer() final;

#pragma endregion

        void inherit_logic(rc4_info rc4_info_, uint32_t seq_, server_info server_info_, session_uid target_uid);

    private:
        rc4_info _rc4_info;
        uint32_t _seq;
        server_info _server_info;
        session_uid _target_uid;
        std::weak_ptr<basic_async_session> _session_holder;
    };

#define CLIENT_SESSION_LOGIC parallel_core::ThreadSafeObjectPool<client_session_logic>::instance()->get_shared()
}