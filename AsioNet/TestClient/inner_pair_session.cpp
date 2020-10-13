#include "inner_pair_session.h"
#include "LogUtils.hpp"
#include "inner_session_logic.h"

using namespace net_middleware;
using asio::ip::tcp;

inner_pair_session::inner_pair_session():
    _session_executor(new async_job_executor(1))
{
    LOCK_FREE_BUFFER(_storage);
}

inner_pair_session::~inner_pair_session()
{
}

void inner_pair_session::start()
{
    _session_executor->start();

    if (_cfg.is_connector_)
    {
        _session = std::make_shared<basic_async_session>(_session_executor);
        _session->modify_session_logic(std::make_shared<inner_session_logic>());
        auto logic = _session->get_logic();
        auto inst_logic = session_logic_interface::session_cast<inner_session_logic>(logic);
        inst_logic->share_storage(_storage);

        _session->async_connect(tcp::endpoint(asio::ip::address::from_string(_cfg.ip_), _cfg.port_),
            [this]() {
            _session->start_tick(
                std::chrono::milliseconds(_cfg.tick_interval_),
                std::chrono::milliseconds(_cfg.keep_alive_timeout_));

            if (_connect_done)
            {
                _connect_done();
            }
        });
    }
    else
    {
        auto acceptor_executor = std::make_shared<async_job_executor>(1);
        auto acceptor_job_agent = std::make_shared<job_agent>(acceptor_executor);
        auto acceptor = std::make_shared<asio::ip::tcp::acceptor>(acceptor_job_agent->strand_to_run(), tcp::endpoint(tcp::v4(), _cfg.listened_port_));

        _session = std::make_shared<basic_async_session>(_session_executor);
        _session->modify_session_logic(std::make_shared<inner_session_logic>());

        auto self = shared_from_this();
        acceptor_job_agent->strand_to_run().post([this, self, acceptor_executor, acceptor]() {
            acceptor->async_accept(_session->socket_to_accept(), [this, self, acceptor_executor](asio::error_code ec) {
                if (UNLIKELY(ec))
                {
                    LOG("fatal accept a socket, %s", ec.message().c_str());
                    stop();
                    return;
                }

                { LOG_NON_SENSITIVE("accept a session"); }
                
                _session->passively_connect_succ();
                _session->start_tick(
                    std::chrono::milliseconds(_cfg.tick_interval_),
                    std::chrono::milliseconds(_cfg.keep_alive_timeout_)
                );
                
                acceptor_executor->stop();
            });
        });
    }
}

void inner_pair_session::stop()
{
    _session_executor->stop();
}

void net_middleware::inner_pair_session::send(unsigned char* data, size_t length)
{
    once_buffer_sptr tmp_buffer = TEMP_BUFFER;
    tmp_buffer->offset = _session->get_logic()->prefix_size();
    std::memcpy(tmp_buffer->buffer(), data, length);
    tmp_buffer->length = length;

    _session->async_send(tmp_buffer);
}

bool inner_pair_session::pick_msg(protocol_head* ret_head, unsigned char* ret_data)
{
    // length(2) + data

    if (!_already_read_head)
    {
        if (_storage->empty(PROTO_HEAD_SIZE))
        {
            return false;
        }

        _already_read_head = HEAD_FROM_POOL;
        _storage->tryRead(PROTO_HEAD_SIZE, (unsigned char*)_already_read_head.get());
    }

    if (!_storage->tryRead(_already_read_head->len, ret_data))
    {
        return false;
    }

    std::memcpy(ret_head, _already_read_head.get(), PROTO_HEAD_SIZE);

    _already_read_head.reset();

    return false;
}
