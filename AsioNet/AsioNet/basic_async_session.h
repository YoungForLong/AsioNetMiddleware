#pragma once

#include <memory>
#include <thread>
#include <asio.hpp>

#include "async_job.h"
#include "NetUtils.hpp"
#include "protocol.hpp"
#include "session_logic.h"

using asio::ip::tcp;

namespace net_middleware
{
    class basic_async_session : public std::enable_shared_from_this<basic_async_session>
    {
    public:
#pragma region enum
        enum class StateSocket
        {
            INIT, // set a socket open and wait

            WAIT_CONNECT, // wait connect or accept

            CONNECTING, // connected & no error occurred

            TO_CLOSE, // actively called shutdown send to close

            CLOSE_DONE, // recv RST || recv FD_CLOSED || timeout , then call close, after which managers will destroy this session
        };
#pragma endregion
        
#pragma region (dis)ctors

        explicit basic_async_session(std::shared_ptr<async_job_executor> job_excutor);

        virtual ~basic_async_session();

        basic_async_session(const basic_async_session&) = delete;

        basic_async_session& operator=(const basic_async_session&) = delete;

#pragma endregion

#pragma region (g)strs

        inline tcp::socket& socket_to_accept() { return _sock; }
        
        inline void modify_session_logic(std::shared_ptr<session_logic_interface> logic)
        { 
            _logic = logic;
            _logic->apply_session(shared_from_this());
        }

        inline bool is_session_closed() { return _state == StateSocket::CLOSE_DONE; }

        inline std::shared_ptr<session_logic_interface> get_logic() { return _logic; }

        inline session_uid get_uuid() { return _uuid; }

        inline StateSocket get_state() { return _state; }

#pragma endregion
        void generate_uuid();

        void init_options();

        void async_connect(const tcp::endpoint& ep, std::function<void()> cb = nullptr);

        void async_recv_loop();

        void async_send(once_buffer_sptr tmp_buffer, std::function<void()> cb = nullptr);

        void async_send_multi(tiny_buffer_sptr head, once_buffer_sptr msg, std::function<void()> cb = nullptr);

        uint32_t get_remote_ip();

        // @param elegantly: wait remote confirm to close
        void close(bool elegantly = true);

        void kick();

        // being accept
        void passively_connect_succ();

        // register tick into strand to promise its safety
        void start_tick(const std::chrono::milliseconds& interval, const std::chrono::milliseconds& timeout);

    private:
        void fixed_tick();

        // tcp application layer protocol
        bool pick_a_entire_msg(protocol_head::head_sptr head, once_buffer_sptr data_block);

        void pick_entire_msgs();

        void tick_alive();

        void update_recv_time();

        void update_send_time();

        void clear_all_timer();

        inline bool is_tick_frame_effective(const uint64_t tick, const uint64_t precision, uint64_t* count)
        {
            ++(*count);
            *count %= (precision / tick);

            return *count == 0;
        }

    private:
        std::shared_ptr<job_agent> _job_agent;

        tcp::socket _sock;

        StateSocket _state;

        once_buffer_sptr _recv_not_entire;

        asio::steady_timer _timer;

        asio::steady_timer _tick_timer;
        std::chrono::milliseconds _tick_interval;

        int _keep_alive_frame_count;
        std::chrono::milliseconds _keep_alive_timeout;
        std::chrono::system_clock::time_point _last_recv_time;
        std::chrono::system_clock::time_point _last_send_time;

        std::shared_ptr<session_logic_interface> _logic;

        uint32_t _uuid;
    };
}