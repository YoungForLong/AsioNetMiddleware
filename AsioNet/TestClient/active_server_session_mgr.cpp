#include "active_server_session_mgr.h"
#include "NetUtils.hpp"
#include "proto_mask.hpp"

using namespace net_middleware;

net_middleware::active_server_session_mgr::active_server_session_mgr():
    _cluster_config(),
    _session_excutor(new async_job_executor(1))
{
    LOCK_FREE_BUFFER(_storage);
}

net_middleware::active_server_session_mgr::~active_server_session_mgr()
{
}

void net_middleware::active_server_session_mgr::start()
{
    _session_excutor->start();

    _session = std::make_shared<basic_async_session>(_session_excutor);
    _session->modify_session_logic(ACTIVE_SERVER_SESSION_LOGIC);
    
    server_info s_info;
    s_info.area_id_ = _cluster_config.area_id_;
    s_info.server_id_ = _cluster_config.sid_;
    s_info.platform_ = _cluster_config.platform_;
    s_info.link_type_ = (unsigned char)session_logic_interface::SessionType::ACTIVE_GAMESVR;

    auto logic = _session->get_logic();
    auto inst_logic = session_logic_interface::session_cast<active_server_session_logic>(logic);
    inst_logic->set_server_info(s_info);
    inst_logic->share_storage(_storage);

    _session->async_connect(tcp::endpoint(asio::ip::address::from_string(_cluster_config.proxy_ip_), _cluster_config.proxy_port_),
        [this]() {
            _session->start_tick(
                std::chrono::milliseconds(_cluster_config.tick_interval_),
                std::chrono::milliseconds(_cluster_config.keep_alive_timeout_));

            auto logic = _session->get_logic();
            auto inst_logic = session_logic_interface::session_cast<active_server_session_logic>(logic);
            inst_logic->send_verify_authentication();
        });
}

void net_middleware::active_server_session_mgr::stop()
{
    if (!_storage->empty())
    {
        LOG("buffer was not clear before stop");
    }
    _session_excutor->stop();
}

void net_middleware::active_server_session_mgr::send(session_uid target_id, unsigned char* data_block, uint16_t len)
{
    // 此处是异步的行为，必须保证该data_block的生命周期
    // to guarantee life circle of data, use sptr instead of ptr
    once_buffer_sptr tmp_buffer = TEMP_BUFFER;
    tmp_buffer->offset = _session->get_logic()->prefix_size();
    std::memcpy(tmp_buffer->buffer(), data_block, len);
    tmp_buffer->length = len;

    write_uint32(tmp_buffer->origin_buffer(PROTO_HEAD_SIZE), target_id);
    tmp_buffer->offset -= sizeof(session_uid);
    tmp_buffer->length += sizeof(session_uid);

    _session->async_send(tmp_buffer);
}

void net_middleware::active_server_session_mgr::broadcast(const std::vector<session_uid>& targets, unsigned char* data_block, uint16_t len)
{
    if (targets.size() > 0xFFFF)
    {
        LOG("too many targets to broadcast %llu", targets.size());
        return;
    }

    auto buffer = TEMP_BUFFER;
    buffer->offset = PROTO_HEAD_SIZE + targets.size() * sizeof(session_uid) + 2;
    std::memcpy(buffer->buffer(), data_block, len);
    buffer->length = len;

    write_uint16(buffer->origin_buffer(PROTO_HEAD_SIZE), targets.size());

    for (size_t i = 0; i < targets.size(); ++i)
    {
        write_uint32(buffer->origin_buffer(PROTO_HEAD_SIZE + 2 + i * sizeof(session_uid)), targets[i]);
    }
    buffer->offset = PROTO_HEAD_SIZE;
    
    _session->async_send(buffer);
}

bool net_middleware::active_server_session_mgr::pick_msg(session_uid* from_id, unsigned char* ret_cmd, unsigned char* ret_block, uint16_t* ret_len)
{
    // id(4) + cmd(2) + length(2) + data

    if (_already_read_length != 0)
    {
        if (_storage->empty(8))
        {
            return false;
        }

        unsigned char uid[4];
        _storage->tryRead(4, uid);
        _already_read_uid = read_uint32(uid);

        unsigned char cmd[2];
        _storage->tryRead(2, cmd);
        _already_read_cmd = read_uint16(cmd);

        unsigned char len[2];
        _storage->tryRead(2, len);
        _already_read_length = read_uint16(len);
    }

    if (_storage->empty(_already_read_length))
    {
        return false;
    }

    _storage->tryRead(_already_read_length, ret_block);

    *from_id = _already_read_uid;
    *ret_cmd = _already_read_cmd;
    *ret_len = _already_read_length;
    
    _already_read_uid = 0;
    _already_read_cmd = 0;
    _already_read_length = 0;

    return true;
}

uint32_t net_middleware::active_server_session_mgr::get_session_remote_ip(session_uid s_uid)
{
    auto sock_info = _sock_info_map.find(s_uid);
    if (sock_info != _sock_info_map.end())
    {
        return sock_info->second.remote_ip;
    }

    return 0;
}

void net_middleware::active_server_session_mgr::set_session_remote_ip(session_uid s_uid, uint32_t ip)
{
    auto sock_info = _sock_info_map.find(s_uid);
    if (sock_info == _sock_info_map.end())
    {
        extra_socket_info s;
        s.remote_ip = ip;
        _sock_info_map.insert(std::make_pair(s_uid, s));
    }
    else
    {
        sock_info->second.remote_ip = ip;
    }
}
