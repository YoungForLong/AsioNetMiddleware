#pragma once

#include "session_logic.h"
#include "parallel_core/ThreadSafeObjectPool.h"

namespace net_middleware 
{
    class server_session_logic : public session_logic_interface, public std::enable_shared_from_this<server_session_logic>
    {
    public:
        inline const server_id get_server_id() const { return cal_server_uid(_server_info.area_id_, _server_info.server_id_, _server_info.platform_); }

        void inherit_logic(server_info s, uint32_t seq);

#pragma region inherit

        virtual void apply_session(std::weak_ptr<basic_async_session> session_holder) final;

        virtual bool is_free_session() final { return false; }

        virtual SessionType get_session_type() final { return (SessionType)_server_info.link_type_; }

        virtual bool unwrap_received_data(once_buffer_sptr buffer, protocol_head::head_sptr head, once_buffer_sptr ret_block) final;

        virtual bool try_copy_to_storage(once_buffer_sptr data, protocol_head::head_sptr head) final;

        virtual void wrap_to_send_data(once_buffer_sptr& buffer) final;

        virtual size_t prefix_size() final { return PROTO_HEAD_SIZE - sizeof(session_uid); }

        virtual void kick_peer() final;

#pragma endregion

        // send some extra info to server
        void send_confirm_connect(session_uid client_id, uint32_t ip);

    private:
        uint32_t _seq;
        server_info _server_info;
        std::weak_ptr<basic_async_session> _session_holder;
    };

#define SERVER_SESSION_LOGIC parallel_core::ThreadSafeObjectPool<server_session_logic>::instance()->get_shared()
}
