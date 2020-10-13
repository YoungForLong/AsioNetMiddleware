#pragma once

#include <functional>

#include "basic_async_session.h"
#include "parallel_core/RingBuffer.h"
#include "NetUtils.hpp"
#include "protocol.hpp"

// LAN inner connection

namespace net_middleware
{
    struct session_config
    {
        bool is_connector_;

        uint32_t tick_interval_;
        uint32_t keep_alive_timeout_;

        // connector
        std::string ip_;
        uint16_t port_;
        
        // acceptor
        uint16_t listened_port_;
    };

    class inner_pair_session : std::enable_shared_from_this<inner_pair_session>
    {
    public:
        using done_action = std::function<void()>;

        using session_sptr = std::shared_ptr<basic_async_session>;

    public:
        inner_pair_session();
        virtual ~inner_pair_session();

        inner_pair_session(const inner_pair_session& other) = delete;
        inner_pair_session& operator=(const inner_pair_session& other) = delete;

        inline void set_config(const session_config& cfg) { _cfg = cfg; }

        void start();

        void stop();

        void send(unsigned char* data, size_t length);

        bool pick_msg(protocol_head* ret_head, unsigned char* ret_data);

        inline basic_async_session::StateSocket get_session_state() { return _session->get_state(); }

        inline void bind_connect_done(done_action act = nullptr) { _connect_done = act; }

    private:
        job_excutor_sptr _session_executor;

        session_sptr _session;

        session_config _cfg;

        // actions
        done_action _connect_done;

        lockfree_buffer_sptr _storage;
        protocol_head::head_sptr _already_read_head;
    };
}
