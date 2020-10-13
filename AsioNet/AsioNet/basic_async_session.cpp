#include "basic_async_session.h"
#include "LogUtils.hpp"
#include "proto_mask.hpp"
#include "default_session_logic.h"

net_middleware::basic_async_session::basic_async_session(std::shared_ptr<async_job_executor> job_excutor):
    _job_agent(JOB_AGENT(job_excutor)),
    _state(StateSocket::INIT),
    _sock(_job_agent->strand_to_run()),
    _timer(_job_agent->strand_to_run()),
    _tick_timer(_job_agent->strand_to_run()),
    _uuid(0)
{
    update_recv_time();
    update_send_time();
    generate_uuid();
}

net_middleware::basic_async_session::~basic_async_session()
{
    clear_all_timer();
}

void net_middleware::basic_async_session::generate_uuid()
{
    _uuid = (uint32_t)clock();
}

void net_middleware::basic_async_session::init_options()
{
    if (_sock.is_open())
    {
        asio::error_code ec;

        _sock.set_option(asio::socket_base::keep_alive(false), ec); // promise it by application layer protocol
        if (UNLIKELY(ec))
        {
            LOG("set option error: %s", ec.message().c_str());
        }

        _sock.set_option(asio::socket_base::linger(true, 10), ec); // promise close send FIN and close within 10s
        if (UNLIKELY(ec))
        {
            LOG("set option error: %s", ec.message().c_str());
        }

        _sock.set_option(asio::ip::tcp::no_delay(true), ec); // promise it by application layer protocol
        if (UNLIKELY(ec))
        {
            LOG("set option error: %s", ec.message().c_str());
        }

        _sock.set_option(asio::socket_base::reuse_address(true), ec);
        if (UNLIKELY(ec))
        {
            LOG("set option error: %s", ec.message().c_str());
        }
    }
}

void net_middleware::basic_async_session::async_connect(const tcp::endpoint& ep, std::function<void()> cb)
{
    if (UNLIKELY(!_logic))
    {
        LOG("logic is not applied");
        return;
    }

    if (UNLIKELY(_state != StateSocket::INIT))
    {
        LOG("state is not INIT, %d", (int)_state);
        return;
    }

    _state = StateSocket::WAIT_CONNECT;
    asio::error_code ec;

    auto self(shared_from_this());
    _sock.async_connect(ep, [this, self, cb](asio::error_code ec) {
        if (UNLIKELY(ec))
        {
            LOG("fatal connect socket, %s", ec.message().c_str());
            close(false);
            return;
        }
        _state = StateSocket::CONNECTING;
        init_options();
        async_recv_loop();

        if (cb)
            cb();
    });
}

void net_middleware::basic_async_session::async_recv_loop()
{
    if (UNLIKELY(_state != StateSocket::CONNECTING))
    {
        LOG("state is not CONNECTING, %d", (int)_state);
        return;
    }

    auto self(shared_from_this());
    // last time received is not a entire protocol
    if (!_recv_not_entire)
    {
        _recv_not_entire = TEMP_BUFFER;
    }

    _sock.async_read_some(asio::buffer(_recv_not_entire->buffer(_recv_not_entire->length), _recv_not_entire->available_capacity() - _recv_not_entire->length),
        [this, self](asio::error_code ec, size_t length)
        {
            update_recv_time();

            if (UNLIKELY(ec))
            {
                LOG("fatal recv, %s", ec.message().c_str());
                close(false);
            }

            _recv_not_entire->length += length;

            pick_entire_msgs();
        }
    );
}

void net_middleware::basic_async_session::async_send(once_buffer_sptr tmp_buffer, std::function<void()> cb)
{
    if (UNLIKELY(_state != StateSocket::CONNECTING))
    {
        LOG("state is not CONNECTING, %d", (int)_state);
        return;
    }

    update_send_time();

    auto mutable_buffer = tmp_buffer;
    _logic->wrap_to_send_data(mutable_buffer);

    auto self(shared_from_this());
    _sock.async_write_some(asio::buffer(mutable_buffer->buffer(), mutable_buffer->length), [this, self, cb](asio::error_code ec, size_t length) {
        if (UNLIKELY(ec))
        {
            LOG("fatal send, %s", ec.message().c_str());
            close(false);
            return;
        }

        if (cb)
            cb();
    });
}

void net_middleware::basic_async_session::async_send_multi(tiny_buffer_sptr head, once_buffer_sptr msg, std::function<void()> cb)
{
    if (UNLIKELY(_state != StateSocket::CONNECTING))
    {
        LOG("state is not CONNECTING, %d", (int)_state);
        return;
    }

    update_send_time();

    std::initializer_list<asio::mutable_buffers_1> buffer_arr;
    if (nullptr == msg)
    {
        buffer_arr = {
            asio::mutable_buffers_1(head->buffer(), head->length)
        };
    }
    else if (nullptr == head)
    {
        buffer_arr = {
            asio::mutable_buffers_1(msg->buffer(), msg->length)
        };
    }
    else
    {
        buffer_arr = {
            asio::mutable_buffers_1(head->buffer(), head->length),
            asio::mutable_buffers_1(msg->buffer(), msg->length),
        };
    }


    auto self1(shared_from_this());
    _sock.async_write_some(buffer_arr, [this, self1, cb](asio::error_code ec, size_t length) {
        if (UNLIKELY(ec))
        {
            LOG("fatal send, %s", ec.message().c_str());
            close(false);
            return;
        }

        if (cb)
            cb();
    });
}

uint32_t net_middleware::basic_async_session::get_remote_ip()
{
    auto remote_ep = _sock.remote_endpoint();
    return remote_ep.address().to_v4().to_uint();
}

void net_middleware::basic_async_session::close(bool elegantly)
{
    if (_state == StateSocket::CLOSE_DONE)
        return;

    auto self(shared_from_this());
    _job_agent->strand_to_run().post([this, self, elegantly]() {
        if (_state == StateSocket::CLOSE_DONE)
            return;

        clear_all_timer();

        if (_sock.is_open())
        {
            asio::error_code ec;
            if (elegantly)
            {
                _state = StateSocket::TO_CLOSE;
                _sock.shutdown(asio::socket_base::shutdown_send, ec);
                if (UNLIKELY(ec))
                {
                    LOG("fatal shutdown socket, %s", ec.message().c_str());
                }
            }
            else
            {
                _state = StateSocket::CLOSE_DONE;
                _sock.close(ec);
                if (UNLIKELY(ec))
                {
                    LOG("fatal close socket, %s", ec.message().c_str());
                }
            }
        }
    });
}

void net_middleware::basic_async_session::kick()
{
    close(true);
}

void net_middleware::basic_async_session::passively_connect_succ()
{
    _state = StateSocket::CONNECTING;
    init_options();
    async_recv_loop();
}

void net_middleware::basic_async_session::start_tick(const std::chrono::milliseconds& interval, const std::chrono::milliseconds& timeout)
{
    _tick_interval = interval;
    _keep_alive_timeout = timeout;

    update_send_time();
    update_recv_time();

    fixed_tick();
}

void net_middleware::basic_async_session::fixed_tick()
{
    if (is_tick_frame_effective(_tick_interval.count(), KEEP_ALIVE_PRECISION, (uint64_t*)&_keep_alive_frame_count))
    {
        tick_alive();
    }

    _tick_timer.expires_from_now(_tick_interval);
    _tick_timer.async_wait(std::bind(&net_middleware::basic_async_session::fixed_tick, this));
}

void net_middleware::basic_async_session::tick_alive()
{
    if (_state != StateSocket::CONNECTING)
    {
        return;
    }

    auto now_time = std::chrono::system_clock::now();
    auto duration = now_time - _last_recv_time;
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    if (duration_ms > _keep_alive_timeout)
    {
        kick();
        return;
    }

    duration = now_time - _last_send_time;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(duration) > (_keep_alive_timeout - std::chrono::milliseconds(NET_PREDICTION_DELAY_MS_MAX)))
    {
        auto empty_buffer = TEMP_BUFFER;
        empty_buffer->offset = PROTO_HEAD_SIZE;
        empty_buffer->length = 0;
        async_send(empty_buffer);
        return;
    }
}

void net_middleware::basic_async_session::update_recv_time()
{
    _last_recv_time = std::chrono::system_clock::now();
}

void net_middleware::basic_async_session::update_send_time()
{
    _last_send_time = std::chrono::system_clock::now();
}

void net_middleware::basic_async_session::clear_all_timer()
{
    asio::error_code ec;

    // why don't us has timer.expired?
    _timer.cancel(ec);
    
    _tick_timer.cancel(ec);
}

bool net_middleware::basic_async_session::pick_a_entire_msg(protocol_head::head_sptr head, once_buffer_sptr data_block)
{
    size_t already_read = 0;

    // when recv a 0 content, which means no data but merely a head, > should be >=
    if (_recv_not_entire && _recv_not_entire->length >= PROTO_HEAD_SIZE)
    {
        protocol_head::unpack_head(head, _recv_not_entire->buffer());

        if (_recv_not_entire->length - PROTO_HEAD_SIZE < head->len)
        {
            return false;
        }
        already_read += PROTO_HEAD_SIZE;

        protocol_head::unpack_msg(data_block->buffer(), _recv_not_entire->buffer(already_read), head->len);
        data_block->length = head->len;

        already_read += head->len;

        do
        {
            once_buffer_sptr unwrap_data = TEMP_BUFFER;
            // 提前预留空间填充包头
            // avoid coping
            unwrap_data->offset = _logic->prefix_size();
            if (UNLIKELY(!_logic->unwrap_received_data(data_block, head, unwrap_data)))
            {
                // cannot kick here
                break;
            }

            // logic may be modified

            if (UNLIKELY(!_logic->try_copy_to_storage(unwrap_data, head)))
            {
                auto self(shared_from_this());
                _timer.expires_from_now(std::chrono::milliseconds(100));
                _timer.async_wait([this, self](asio::error_code ec) {
                    if (UNLIKELY(ec))
                    {
                        LOG("timer error occurred %s", ec.message().c_str());
                        close(false);
                        return;
                    }

                    pick_entire_msgs();
                });

                return false;
            }
        } while (false);
        

        _recv_not_entire->length -= already_read;
        if (_recv_not_entire->length != 0)
        {
            std::memmove(_recv_not_entire->buffer(), _recv_not_entire->buffer(already_read), _recv_not_entire->length);
        }
        else
        {
            _recv_not_entire.reset();
        }

        return true;
    }

    return false;
}

void net_middleware::basic_async_session::pick_entire_msgs()
{
    protocol_head::head_sptr head = HEAD_FROM_POOL;
    once_buffer_sptr tmp_buffer = TEMP_BUFFER;

    while (pick_a_entire_msg(head, tmp_buffer))
    { }

    async_recv_loop();
}
