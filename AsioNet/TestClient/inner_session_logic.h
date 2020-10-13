#pragma once
#include "NetUtils.hpp"
#include "session_logic.h"

namespace net_middleware
{
    class inner_session_logic : public session_logic_interface, public std::enable_shared_from_this<inner_session_logic>
    {
    public:
        inner_session_logic();
        virtual ~inner_session_logic();

        void share_storage(lockfree_buffer_sptr storage);

#pragma region inherit
        virtual void apply_session(std::weak_ptr<basic_async_session> session_holder) final;

        virtual bool is_free_session() final { return false; }

        virtual SessionType get_session_type() final { return SessionType::SESSION_PAIR; }

        virtual bool unwrap_received_data(once_buffer_sptr buffer, protocol_head::head_sptr head, once_buffer_sptr ret_block) final;

        virtual size_t prefix_size() final;

        virtual void wrap_to_send_data(once_buffer_sptr& buffer) final;

        virtual bool try_copy_to_storage(once_buffer_sptr data, protocol_head::head_sptr head) final;

        virtual void kick_peer();
#pragma endregion

    private:
        lockfree_buffer_sptr _storage;
        std::weak_ptr<basic_async_session> _session_holder;
    };
}