#include "client_session_logic.h"
#include "LogUtils.hpp"
#include "proto_mask.hpp"
#include "protocol.hpp"
#include "rc4.hpp"
#include "compression/decompress.hpp"
#include "compression/compress.hpp"
#include "basic_async_session.h"
#include "proxy_manager.h"

void net_middleware::client_session_logic::apply_session(std::weak_ptr<basic_async_session> session_holder)
{
    _session_holder = session_holder;
}

bool net_middleware::client_session_logic::unwrap_received_data(once_buffer_sptr buffer, protocol_head::head_sptr head, once_buffer_sptr ret_block)
{
    // check seq
    if (UNLIKELY(head->seq != ++_seq))
    {
        LOG("seq error, maybe it's hacked; remote is %d, mine is %d", head->seq, _seq);

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
        return true;
    }

    // decrypt
    rc4::rc4_crypt(
        _rc4_info.rc4_modvt_,
        _rc4_info.rc4_key_,
        RC4_KEY_LEN,
        buffer->buffer(),
        head->len,
        _rc4_info.rc4_subtract_,
        0
    );

    // decompress
    if (UNLIKELY(head->get_compressed()))
    {
        gzip::Decompressor gzip_decompressor;
        try
        {
            gzip_decompressor.decompress(*ret_block, (char*)buffer->buffer(), head->len);
        }
        catch (std::exception e)
        {
            LOG("gzip decompress failed %s", e.what());
            return false;
        }
    }

    return true;
}

void net_middleware::client_session_logic::wrap_to_send_data(once_buffer_sptr& buffer)
{
    // compress
    bool compressed = false;
    if (buffer->length > K_SIZE_COMPRESS)
    {
        auto compress_ret = TEMP_BUFFER;
        gzip::Compressor gzip_compressor;
        try
        {
            gzip_compressor.compress(*compress_ret, (char*)buffer->buffer(), buffer->length);

            if (UNLIKELY(compress_ret->length == 0))
            {
                return;
            }

            if (LIKELY(compress_ret->length < buffer->length))
            {
                buffer = compress_ret;
                compressed = true;
            }
        }
        catch (std::exception e)
        {
            LOG("gzip compress failed %s", e.what());
            return;
        }
    }

    // encrypt
    rc4::rc4_crypt(
        _rc4_info.rc4_modvt_,
        _rc4_info.rc4_key_,
        RC4_KEY_LEN,
        buffer->buffer(),
        buffer->length,
        _rc4_info.rc4_subtract_,
        1
    );

    auto head = protocol_head::get_a_head(FRAME_KEY, (uint16_t)buffer->length, 0, (uint16_t)net_middleware::protocol_cmd::Commands_RoutingTransparent, compressed, _seq);
    uint16_t inverse_mask = proto_mask::get_mask((unsigned char*)head.get(), PROTO_HEAD_SIZE, buffer->buffer(), buffer->length);
    head->mask = inverse_mask;

    std::memcpy(buffer->origin_buffer(), head.get(), PROTO_HEAD_SIZE);
    buffer->offset -= PROTO_HEAD_SIZE;
    buffer->length += PROTO_HEAD_SIZE;

    assert(buffer->offset == 0 && "offset align error");
}

bool net_middleware::client_session_logic::try_copy_to_storage(once_buffer_sptr data, protocol_head::head_sptr head)
{
    if (UNLIKELY(_session_holder.expired()))
    {
        LOG("unexpected session expired");
        return false;
    }
    
    write_uint32(data->origin_buffer(PROTO_HEAD_SIZE), _session_holder.lock()->get_uuid());
    data->offset -= sizeof(session_uid);

    PROXY_MGR->send_to_server(_target_uid, data);

    return true;
}

void net_middleware::client_session_logic::kick_peer()
{
    auto head_buffer = TINY_BUFFER;
    auto data_block = TEMP_BUFFER;

    if (UNLIKELY(_session_holder.expired()))
    {
        LOG("unexpected session expired");
        return;
    }

    write_uint32(data_block->buffer() ,_session_holder.lock()->get_uuid());
    data_block->length = sizeof(session_uid);

    auto head = protocol_head::get_a_head(FRAME_KEY, 0, 0, (uint16_t)net_middleware::protocol_cmd::Commands_Kick, false, _seq);
    uint16_t inverse_mask = proto_mask::get_mask((unsigned char*)head.get(), PROTO_HEAD_SIZE, data_block->buffer(), data_block->length);
    head->mask = inverse_mask;

    std::memcpy(head_buffer->origin_buffer(), head.get(), PROTO_HEAD_SIZE);

    PROXY_MGR->send_to_server_multi(_target_uid, head_buffer, data_block);
}

void net_middleware::client_session_logic::inherit_logic(rc4_info rc4_info_, uint32_t seq_, server_info server_info_, session_uid target_uid)
{
    _rc4_info = rc4_info_;
    _seq = seq_;
    _server_info = server_info_;
    _target_uid = target_uid;
}
