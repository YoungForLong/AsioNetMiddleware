#include "inner_session_logic.h"
#include "protocol.hpp"
#include "LogUtils.hpp"
#include "proto_mask.hpp"

net_middleware::inner_session_logic::inner_session_logic()
{
}

net_middleware::inner_session_logic::~inner_session_logic()
{
}

void net_middleware::inner_session_logic::share_storage(lockfree_buffer_sptr storage)
{
    _storage = storage;
}

void net_middleware::inner_session_logic::apply_session(std::weak_ptr<basic_async_session> session_holder)
{
    _session_holder = session_holder;
}

bool net_middleware::inner_session_logic::unwrap_received_data(once_buffer_sptr buffer, protocol_head::head_sptr head, once_buffer_sptr ret_block)
{
    // ignore seq

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

    std::memcpy(ret_block->buffer(), buffer->buffer(), head->len);
    ret_block->length = head->len;

    return true;
}

size_t net_middleware::inner_session_logic::prefix_size()
{
    return PROTO_HEAD_SIZE;
}

void net_middleware::inner_session_logic::wrap_to_send_data(once_buffer_sptr& buffer)
{
    auto head = protocol_head::get_a_head(FRAME_KEY, (uint16_t)buffer->length, 0, (uint16_t)protocol_cmd::Commands_RoutingTransparent, false, 0);

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

bool net_middleware::inner_session_logic::try_copy_to_storage(once_buffer_sptr data, protocol_head::head_sptr head)
{
    if (_storage->full(PROTO_HEAD_SIZE + data->length))
    {
        LOG("storage is full, considering a larger size, current is %d", TOTAL_CACHE_SIZE);
        return false;
    }

    _storage->tryWrite((unsigned char*)head.get(), PROTO_HEAD_SIZE);

    _storage->tryWrite(data->buffer(), data->length);

    return true;
}

void net_middleware::inner_session_logic::kick_peer()
{
}


