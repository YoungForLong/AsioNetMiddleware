#pragma once
#include <memory>
#include <cstring>
#include <assert.h>

#include "parallel_core/ThreadCacheContainer.h"
#include "NetUtils.hpp"

namespace net_middleware
{
    // keep the same with client
    enum class protocol_cmd
    {
        Commands_LCriticalSI = 0x7e8d,
        Commands_AuthenticationAAA,
        Commands_Heartbeat,
        Commands_RoutingTransparent,
        Commands_BroadCast,
        Commands_ConnectionConfirm,
        Commands_Kick,
        Commands_RCriticalSI = 0xFFFF,
    };

#pragma pack(push, 1)
    struct protocol_head
    {
#define HEAD_FROM_POOL parallel_core::ThreadSafeObjectPool<protocol_head>::instance()->get_shared()
        typedef std::shared_ptr<protocol_head> head_sptr;

#ifdef _MSC_VER
        unsigned char	fk;
        uint16_t		len;
        uint16_t		mask;
        uint16_t		cmd_compressed_flag;
        uint32_t		seq;

        inline uint16_t get_cmd()
        {
#if LITTLE_ENDIAN
            return (cmd_compressed_flag & 0x7FFF);
#endif
        }

        inline bool get_compressed()
        {
#if LITTLE_ENDIAN
            return (cmd_compressed_flag >> 15) != 0;
#endif
        }

        static head_sptr get_a_head(unsigned char fk_, uint16_t len_, uint16_t mask_, uint16_t cmd_, bool compressed_, uint32_t seq_)
        {
            head_sptr head = HEAD_FROM_POOL;
            head->fk = fk_;
            head->len = len_;
            head->mask = mask_;
#if LITTLE_ENDIAN
            head->cmd_compressed_flag = compressed_ ? (uint16_t)(1 << 15) : (uint16_t)0;
            head->cmd_compressed_flag |= (uint16_t)(cmd_ & 0x7FFF);
#endif
            head->seq = seq_;

            return head;
        }
#else
        unsigned char		fk;
        uint16_t			len;
        uint16_t			mask;
        struct
        {
            uint16_t		cmd : 15;
            unsigned char	compressed : 1;
        };
        uint32_t			seq;

        static head_sptr get_a_head(unsigned char fk_, uint16_t len_, uint16_t mask_, uint16_t cmd_, bool compressed_, uint32_t seq_)
        {
            head_sptr head = HEAD_FROM_POOL;
            head->fk = fk_;
            head->len = len_;
            head->mask = mask_;
            head->cmd = cmd_;
            head->compressed = (unsigned char)compressed_;
            head->seq = seq_;

            return head;
        }

        inline uint16_t get_cmd()
        {
            return cmd;
        }

        inline bool get_compressed()
        {
            return compressed != 0;
        }
#endif

        //serialize a msg
        static void pack(unsigned char* ret_block, uint16_t& ret_length, unsigned char* msg, uint16_t length, uint16_t cmd_, uint32_t seq_)
        {
            head_sptr head = get_a_head(0x2e, length, 0, cmd_, 0, seq_);
            ret_length = length + sizeof(protocol_head);

            std::memcpy(ret_block, head.get(), sizeof(protocol_head));
            std::memcpy(ret_block + sizeof(protocol_head), msg, length);
        }

        static void pack(unsigned char* ret_block, uint16_t& ret_length, const std::vector<unsigned char*>& msgs, const std::vector<size_t>& lens, uint16_t cmd_, uint32_t seq_)
        {
            assert(msgs.size() == lens.size());
            size_t total_length = 0;

            for (size_t i = 0; i < lens.size(); ++i)
            {
                std::memcpy(ret_block + sizeof(protocol_head) + total_length, msgs[i], lens[i]);
                total_length += lens[i];
            }

            head_sptr head = get_a_head(0x2e, (uint16_t)total_length, 0, cmd_, 0, seq_);
            ret_length = (uint16_t)(total_length + sizeof(protocol_head));
            std::memcpy(ret_block, head.get(), sizeof(protocol_head));
        }

        static head_sptr unpack(unsigned char* ret_msg, uint16_t& ret_length, unsigned char* origin, uint16_t length)
        {
            head_sptr head = HEAD_FROM_POOL;

            std::memcpy(head.get(), origin, sizeof(protocol_head));
            std::memcpy(ret_msg, origin + sizeof(protocol_head), length - sizeof(protocol_head));

            return head;
        }

        static head_sptr unpack_head(unsigned char* head_block)
        {
            head_sptr head = HEAD_FROM_POOL;

            std::memcpy(head.get(), head_block, sizeof(protocol_head));

            return head;
        }

        static void unpack_head(head_sptr head, unsigned char* head_block)
        {
            std::memcpy(head.get(), head_block, sizeof(protocol_head));
        }

        static void unpack_msg(unsigned char* ret_msg, unsigned char* msg_block, uint16_t length)
        {
            std::memcpy(ret_msg, msg_block, length);
        }
    };

    
    struct authentication_aaa_request
    {
        uint16_t		area_id;
        uint16_t		server_id;
        unsigned char	link_type;
        std::string		platform;
        uint16_t		platform_len;

        typedef std::shared_ptr<authentication_aaa_request> aaa_req_sptr;
#define AAA_REQ_FROM_POOL parallel_core::ThreadSafeObjectPool<authentication_aaa_request>::instance()->get_shared()

        static void pack(unsigned char* ret_block, uint16_t& ret_length, uint16_t area_id_, uint16_t server_id_, unsigned char link_type_,  unsigned char* platform_, uint16_t platform_len_)
        {
            write_uint16(ret_block, area_id_);
            write_uint16(ret_block + 2, server_id_);
            ret_block[4] = link_type_;
            unsigned char* msg_start = &ret_block[5];
            std::memcpy(msg_start, platform_, (size_t)platform_len_ + 1u);
            write_uint16(msg_start + platform_len_ + 1, platform_len_);

            ret_length = 2 + 2 + 1 + platform_len_ + 1 + 2;
        }

        static aaa_req_sptr unpack(unsigned char* block, uint16_t length)
        {
            auto msg = AAA_REQ_FROM_POOL;
            
            msg->platform_len = length - 8;

            msg->area_id = read_uint16(block);
            msg->server_id = read_uint16(block + 2);

            msg->link_type = block[4];
            msg->platform = std::string((char*)&block[5], (size_t)msg->platform_len + 1u);

            return msg;
        }
    };

    struct authentication_aaa_response
    {
        unsigned char	ec;
        unsigned char	subtract;
        unsigned char	mod_vt;
        std::string		key;
        uint16_t		key_len;
        uint32_t		link_no;
        unsigned char	link_type;

        static void pack(unsigned char* ret, uint16_t& ret_length,
            unsigned char	ec_,
            unsigned char	subtract_,
            unsigned char	mod_vt_,
            unsigned char*	key_,
            uint16_t		key_len_,
            uint32_t		link_no_,
            unsigned char	link_type_)
        {
            ret[0] = ec_;
            ret[1] = subtract_;
            ret[2] = mod_vt_;
            std::memcpy(&(ret[3]), key_, (size_t)key_len_);
            write_uint32(&(ret[key_len_ + 3]), link_no_);
            ret[key_len_ + 3 + 4] = link_type_;

            ret_length = 1 + 1 + 1 + key_len_ + 2 + 4 + 1;
        }

        typedef std::shared_ptr<authentication_aaa_response> aaa_res_sptr;
#define AAA_RES_FROM_POOL parallel_core::ThreadSafeObjectPool<authentication_aaa_response>::instance()->get_shared()

        static aaa_res_sptr unpack(unsigned char* block, uint16_t length)
        {
            auto msg = AAA_RES_FROM_POOL;

            msg->ec = block[0];
            msg->subtract = block[1];
            msg->mod_vt = block[2];
            msg->key_len = length - 11;
            msg->key = std::string((char*)&block[3], msg->key_len);
            msg->link_no = read_uint32(block + msg->key_len + 1 + 3);

            return msg;
        }
    };

    struct session_connect_confirm
    {
        uint32_t ip;

        typedef std::shared_ptr<session_connect_confirm> confirm_sptr;
#define CONFIRM_FROM_POOL parallel_core::ThreadSafeObjectPool<session_connect_confirm>::instance()->get_shared()

        static void pack(unsigned char* ret_block, uint16_t& ret_length,
            uint32_t ip
            )
        {
            write_uint32(ret_block, ip);
            ret_length = 4;
        }

        static confirm_sptr unpack(unsigned char* block, uint16_t length)
        {
            auto msg = CONFIRM_FROM_POOL;
            msg->ip = read_uint32(block);

            return msg;
        }
    };

#pragma pack(pop)
}

#define PROTO_HEAD_SIZE sizeof(net_middleware::protocol_head)

#define FRAME_KEY 0x2e

#define K_SIZE_COMPRESS 64