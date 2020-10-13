#include "active_server_session_logic.h"
#include "protocol.hpp"
#include "LogUtils.hpp"
#include "proto_mask.hpp"
#include "active_server_session_mgr.h"

using namespace net_middleware;

net_middleware::active_server_session_logic::active_server_session_logic():
    _seq(0),
    _authentication(AuthenticationState::BEFORE_VERIFY)
{
}

net_middleware::active_server_session_logic::~active_server_session_logic()
{
}

void net_middleware::active_server_session_logic::apply_session(std::weak_ptr<basic_async_session> session_holder)
{
    _session_holder = session_holder;
}

bool net_middleware::active_server_session_logic::unwrap_received_data(once_buffer_sptr buffer, protocol_head::head_sptr head, once_buffer_sptr ret_block)
{
    // check mask
    if (UNLIKELY(0 != net_middleware::proto_mask::get_mask((unsigned char*)head.get(), PROTO_HEAD_SIZE, buffer->buffer(), head->len)))
    {
        LOG("check mask failed, maybe it's hacked");

        return false;
    }

    if (head->len == 0)
    {
        return false;
    }

    if (_authentication == AuthenticationState::BEFORE_VERIFY)
    {
        check_verify_res(buffer);
        return false;
    }

    if (head->get_cmd() == (uint16_t)protocol_cmd::Commands_ConnectionConfirm)
    {
        remote_session_info_confirm(buffer);
        return false;
    }

    std::memcpy(ret_block->buffer(), buffer->buffer(), head->len);
    ret_block->length = head->len;

    return true;
}

size_t net_middleware::active_server_session_logic::prefix_size()
{
    return PROTO_HEAD_SIZE + sizeof(session_uid);
}

void net_middleware::active_server_session_logic::wrap_to_send_data(once_buffer_sptr& buffer)
{
    uint16_t cmd = _authentication == AuthenticationState::BEFORE_VERIFY ?
        (uint16_t)net_middleware::protocol_cmd::Commands_AuthenticationAAA :
        (uint16_t)net_middleware::protocol_cmd::Commands_RoutingTransparent;

    auto head = protocol_head::get_a_head(FRAME_KEY, (uint16_t)buffer->length, 0, cmd, false, ++_seq);

    uint16_t inverse_mask = proto_mask::get_mask((unsigned char*)head.get(), PROTO_HEAD_SIZE, buffer->buffer(), buffer->length);
    head->mask = inverse_mask;

    std::memcpy(buffer->origin_buffer(), head.get(), PROTO_HEAD_SIZE);
    buffer->offset -= PROTO_HEAD_SIZE;
    buffer->length += PROTO_HEAD_SIZE;

    if (buffer->offset != 0)
    {
        LOG("offset align error");
    }
}

bool net_middleware::active_server_session_logic::try_copy_to_storage(once_buffer_sptr data, protocol_head::head_sptr head)
{
    // id(4) + cmd(2) + length(2) + data

    session_uid target_client_uid = read_uint32(data->buffer());
    data->offset += sizeof(session_uid);
    data->length -= sizeof(session_uid);
    
    if (_storage->full(8 + data->length))
    {
        LOG("storage is full, considering a larger size, current is %d", TOTAL_CACHE_SIZE);
        return false;
    }
    
    unsigned char uid[4];
    write_uint32(uid, target_client_uid);
    _storage->tryWrite(uid, 4);

    unsigned char cmd[2];
    write_uint16(cmd, head->get_cmd());
    _storage->tryWrite(cmd, 2);

    unsigned char len[2];
    write_uint16(len, data->length);
    _storage->tryWrite(len, 2);

    _storage->tryWrite(data->buffer(), data->length);

    return true;
}

void net_middleware::active_server_session_logic::kick_peer()
{
}

void net_middleware::active_server_session_logic::send_verify_authentication()
{
    auto send_buffer = TEMP_BUFFER;
    uint16_t len = 0;
    send_buffer->offset = PROTO_HEAD_SIZE;

    authentication_aaa_request::pack(send_buffer->buffer(), len,
        _server_info.area_id_,
        _server_info.server_id_,
        (unsigned char)SessionType::ACTIVE_GAMESVR,
        (unsigned char*)_server_info.platform_.data(),
        _server_info.platform_.length()
    );
    send_buffer->length = len;

    if (UNLIKELY(_session_holder.expired()))
    {
        LOG("unexpected session expired");
        return;
    }

    _session_holder.lock()->async_send(send_buffer);
}

void net_middleware::active_server_session_logic::set_server_info(const server_info& s_info)
{
    _server_info = s_info;
}

std::shared_ptr<active_server_session_logic> net_middleware::active_server_session_logic::reset()
{
    _seq = 0;
    _authentication = AuthenticationState::BEFORE_VERIFY;

    return shared_from_this();
}

void net_middleware::active_server_session_logic::share_storage(lockfree_buffer_sptr storage)
{
    _storage = storage;
}

void net_middleware::active_server_session_logic::check_verify_res(once_buffer_sptr data)
{
    auto aaa_res = authentication_aaa_response::unpack(data->buffer(), data->length);
    if (aaa_res->ec == TransferError::EC_Success)
    {
        _authentication = AuthenticationState::VERIFY_FAILED;
        return;
    }

    // shouldn't be any encrypt or compress between proxy and gamesvr s
    _authentication = AuthenticationState::VERIFY_SUCC;

    return;
}

void net_middleware::active_server_session_logic::remote_session_info_confirm(once_buffer_sptr data)
{
    session_uid s_id = read_uint32(data->buffer());
    data->offset += sizeof(session_uid);
    data->length -= sizeof(session_uid);

    auto confirm_msg = session_connect_confirm::unpack(data->buffer(), data->length);
    ACTIVE_SERVER_MGR->set_session_remote_ip(s_id, confirm_msg->ip);
}
