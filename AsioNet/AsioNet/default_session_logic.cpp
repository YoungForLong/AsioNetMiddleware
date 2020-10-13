#include "default_session_logic.h"
#include "protocol.hpp"
#include "proxy_manager.h"
#include "server_session_logic.h"
#include "client_session_logic.h"
#include "proto_mask.hpp"

net_middleware::default_session_logic::default_session_logic():
    _seq(0),
    _authentication(AuthenticationState::BEFORE_VERIFY)
{
}

net_middleware::default_session_logic::~default_session_logic()
{
}

void net_middleware::default_session_logic::apply_session(std::weak_ptr<basic_async_session> session_holder)
{
    _session_holder = session_holder;
}

bool net_middleware::default_session_logic::unwrap_received_data(once_buffer_sptr buffer, protocol_head::head_sptr head, once_buffer_sptr ret_block)
{
    // check seq
    if (UNLIKELY(head->seq != ++_seq))
    {
        LOG("seq error, maybe it's hacked remote is : %d; local is: %d", head->seq, _seq);

        return false;
    }

    // check mask
    if (UNLIKELY(0 != net_middleware::proto_mask::get_mask((unsigned char*)head.get(), PROTO_HEAD_SIZE, buffer->buffer(), head->len)))
    {
        LOG("check mask failed, maybe it's hacked");

        return false;
    }
    

    // decrypt none

    // decompress none

    if (_authentication == AuthenticationState::BEFORE_VERIFY)
    {
        verify_authentication(buffer, (protocol_cmd)head->get_cmd());
    }

    return false;
}

void net_middleware::default_session_logic::wrap_to_send_data(once_buffer_sptr& buffer)
{
    // compress none

    // encrypt none

    auto head = protocol_head::get_a_head(FRAME_KEY, (uint16_t)buffer->length, 0, (uint16_t)net_middleware::protocol_cmd::Commands_AuthenticationAAA, false, _seq);
    uint16_t inverse_mask = proto_mask::get_mask((unsigned char*)head.get(), PROTO_HEAD_SIZE, buffer->buffer(), head->len);
    head->mask = inverse_mask;

    std::memcpy(buffer->origin_buffer(), head.get(), PROTO_HEAD_SIZE);
    buffer->offset -= PROTO_HEAD_SIZE;
    buffer->length += PROTO_HEAD_SIZE;
}

bool net_middleware::default_session_logic::try_copy_to_storage(once_buffer_sptr data, protocol_head::head_sptr head)
{
    return true;
}

std::shared_ptr<net_middleware::server_session_logic> net_middleware::default_session_logic::trans_to_server()
{
    auto server_logic = SERVER_SESSION_LOGIC;
    server_logic->apply_session(_session_holder);
    server_logic->inherit_logic(_server_info, _seq);
    return server_logic;
}

std::shared_ptr<net_middleware::client_session_logic> net_middleware::default_session_logic::trans_to_client()
{
    auto client_logic = CLIENT_SESSION_LOGIC;
    client_logic->apply_session(_session_holder);
    client_logic->inherit_logic(_rc4_info, _seq, _server_info, _target_uid);
    return client_logic;
}

bool net_middleware::default_session_logic::verify_authentication(once_buffer_sptr tmp_buffer, protocol_cmd cmd)
{
    auto aaa_request = authentication_aaa_request::unpack(tmp_buffer->buffer(), (uint16_t)tmp_buffer->length);
    auto server_uid_ = cal_server_uid(aaa_request->area_id, aaa_request->server_id, aaa_request->platform);

    _server_info.area_id_ = aaa_request->area_id;
    _server_info.server_id_ = aaa_request->server_id;
    _server_info.platform_ = aaa_request->platform;
    _server_info.link_type_ = aaa_request->link_type;

    if (UNLIKELY(_session_holder.expired()))
    {
        LOG("unexpected session expired");
        return false;
    }
    
    auto holder = _session_holder.lock();

    do
    {
        if (UNLIKELY(cmd != protocol_cmd::Commands_AuthenticationAAA))
            break;

        if (UNLIKELY(!PROXY_MGR->verify_authentication((SessionType)aaa_request->link_type, server_uid_, &_target_uid, holder->get_uuid(), holder->get_remote_ip())))
            break;

        _authentication = AuthenticationState::VERIFY_SUCC;
        echo_authentication_res(net_middleware::TransferError::EC_Success);
        return true;

    } while (0);

    LOG("verify failed link_type: %d, area_id: %d, server_id: %d, platform: %s",
        aaa_request->link_type,
        aaa_request->area_id,
        aaa_request->server_id,
        aaa_request->platform.c_str());

    _authentication = AuthenticationState::VERIFY_FAILED;

    echo_authentication_res(net_middleware::TransferError::EC_ServerNotFound);

    return false;
}

void net_middleware::default_session_logic::echo_authentication_res(TransferError ec)
{
    auto send_buffer = TEMP_BUFFER;
    uint16_t len = 0;

    send_buffer->offset = prefix_size();
    authentication_aaa_response::pack(send_buffer->buffer(), len,
        ec,
        _rc4_info.rc4_subtract_,
        _rc4_info.rc4_modvt_,
        _rc4_info.rc4_key_,
        RC4_KEY_LEN,
        _target_uid,
        _server_info.link_type_
    );
    send_buffer->length += len;

    if (UNLIKELY(_session_holder.expired()))
    {
        LOG("unexpected session expired");
        return;
    }

    if (ec != TransferError::EC_Success)
    {
        _session_holder.lock()->async_send(send_buffer, [this]() {
            if (!_session_holder.expired())
            {
                LOG("verify failed, kick");
                _session_holder.lock()->kick();
            }
        });
    }
    else
        _session_holder.lock()->async_send(send_buffer);
}
