#include "server_session_logic.h"
#include "LogUtils.hpp"
#include "protocol.hpp"
#include "proto_mask.hpp"
#include "proxy_manager.h"
#include "NetUtils.hpp"

void net_middleware::server_session_logic::inherit_logic(server_info s, uint32_t seq)
{
    _server_info = s;
    _seq = seq;
}

void net_middleware::server_session_logic::apply_session(std::weak_ptr<basic_async_session> session_holder)
{
    _session_holder = session_holder;
}

bool net_middleware::server_session_logic::unwrap_received_data(once_buffer_sptr buffer, protocol_head::head_sptr head, once_buffer_sptr ret_block)
{
    // check seq
    if (UNLIKELY(head->seq != ++_seq))
    {
        LOG("seq error, maybe it's hacked, local is %d, remote is %d", _seq, head->seq);

        return false;
    }

    // check mask
    if (UNLIKELY(0 != net_middleware::proto_mask::get_mask((unsigned char*)head.get(), PROTO_HEAD_SIZE, buffer->buffer(), head->len)))
    {
        LOG("check mask failed, maybe it's hacked");

        return false;
    }

    if (head->len == 0)
    {
        LOG_NON_SENSITIVE("accept a keep alive proto");
        return false;
    }

    std::memcpy(ret_block->buffer(), buffer->buffer(), head->len);
    ret_block->length = head->len;

    return true;
}

bool net_middleware::server_session_logic::try_copy_to_storage(once_buffer_sptr data, protocol_head::head_sptr head)
{
    if (head->get_cmd() == (uint16_t)protocol_cmd::Commands_BroadCast)
    {
        size_t size = read_uint16(data->buffer()); // avoid warning
        
        auto mutable_buffer = TEMP_BUFFER;
        std::memcpy(mutable_buffer->buffer(), data->buffer(2 + 4 * size), data->length - (2 + 4 * size));
        
        for (size_t i = 0; i < size; ++i)
        {
            session_uid uid = read_uint32(data->buffer(2 + i * 4));
            
            auto head_buffer = TINY_BUFFER;
            auto new_head = protocol_head::get_a_head(FRAME_KEY, (uint16_t)mutable_buffer->length, 0, (uint16_t)net_middleware::protocol_cmd::Commands_RoutingTransparent, false, _seq);
            uint16_t inverse_mask = proto_mask::get_mask((unsigned char*)new_head.get(), PROTO_HEAD_SIZE, mutable_buffer->buffer(), mutable_buffer->length);
            new_head->mask = inverse_mask;
            std::memcpy(head_buffer->buffer(), new_head.get(), PROTO_HEAD_SIZE);

            PROXY_MGR->send_to_client_multi(uid, head_buffer, mutable_buffer);
        }
    }
    else
    {
        session_uid target_client_uid = read_uint32(data->buffer());
        data->offset += sizeof(session_uid);
        data->length -= sizeof(session_uid);

        PROXY_MGR->send_to_client(target_client_uid, data);
    }

    return true;
}

void net_middleware::server_session_logic::wrap_to_send_data(once_buffer_sptr& buffer)
{
    auto head = protocol_head::get_a_head(FRAME_KEY, (uint16_t)buffer->length, 0, (uint16_t)net_middleware::protocol_cmd::Commands_RoutingTransparent, false, _seq);
    uint16_t inverse_mask = proto_mask::get_mask((unsigned char*)head.get(), PROTO_HEAD_SIZE, buffer->buffer(), buffer->length);
    head->mask = inverse_mask;

    std::memcpy(buffer->origin_buffer(), head.get(), PROTO_HEAD_SIZE);
    buffer->offset -= PROTO_HEAD_SIZE;
    buffer->length += PROTO_HEAD_SIZE;

    assert(buffer->offset == 0 && "offset align error");
}

void net_middleware::server_session_logic::kick_peer()
{
    PROXY_MGR->kick_server_peer(get_session_type());
}

void net_middleware::server_session_logic::send_confirm_connect(session_uid client_id, uint32_t ip)
{
    auto buffer = TEMP_BUFFER;
    
    uint16_t length = 0;
    session_connect_confirm::pack(buffer->origin_buffer(PROTO_HEAD_SIZE + sizeof(session_uid)), length, ip);
    buffer->length = length + PROTO_HEAD_SIZE + sizeof(session_uid);

    write_uint32(buffer->origin_buffer(PROTO_HEAD_SIZE), client_id);

    auto head = protocol_head::get_a_head(FRAME_KEY, (uint16_t)buffer->length, 0, (uint16_t)net_middleware::protocol_cmd::Commands_ConnectionConfirm, false, _seq);
    uint16_t inverse_mask = proto_mask::get_mask((unsigned char*)head.get(), PROTO_HEAD_SIZE, buffer->origin_buffer(PROTO_HEAD_SIZE), buffer->length - PROTO_HEAD_SIZE);
    head->mask = inverse_mask;

    std::memcpy(buffer->origin_buffer(), head.get(), PROTO_HEAD_SIZE);

    if (UNLIKELY(_session_holder.expired()))
    {
        LOG("unexpected session expired");
        return;
    }

    _session_holder.lock()->async_send_multi(nullptr, buffer);
}
