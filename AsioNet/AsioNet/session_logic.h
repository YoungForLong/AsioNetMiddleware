#pragma once

#include <memory>
#include <tuple>
#include "NetUtils.hpp"
#include "protocol.hpp"
#include "parallel_core/SafeRandom.hpp"

namespace net_middleware
{
    class basic_async_session;

    class session_logic_interface
    {
    public:
        // 自定义会话类型
        // define types by yourself
        enum class SessionType
        {
            SINGLE_SESSION_BEGIN = 0x0,
            CLIENT_PROXY, // 网关上用于监听客户端连接的会话

            MULTI_SESSION_BEGIN = 0x8,
            GAMESVR_PROXY, // 网关上用于监听gamesvr的会话
            CHATSVR_PROXY,
            DBSVR_PROXY,
            
            ACTIVE_SEESION_BEGIN = 0x20,
            ACTIVE_GAMESVR, // 服务器本地主动发起连接的会话

            SESSION_PAIR,
            ALL_SESSION_END = 0xFF
        };

        enum class AuthenticationState
        {
            BEFORE_VERIFY,
            VERIFY_SUCC,
            VERIFY_FAILED
        };

    public:
        // avoid circle reference using weak_ptr
        virtual void apply_session(std::weak_ptr<basic_async_session> session_holder) = 0;

        virtual bool is_free_session() = 0;

        virtual SessionType get_session_type() = 0;

        virtual bool unwrap_received_data(once_buffer_sptr buffer, protocol_head::head_sptr head, once_buffer_sptr ret_block) = 0;

        virtual size_t prefix_size() = 0;

        virtual void wrap_to_send_data(once_buffer_sptr& buffer) = 0;

        virtual bool try_copy_to_storage(once_buffer_sptr data, protocol_head::head_sptr head) = 0;

        virtual void kick_peer() = 0;

        template <class _SessionType>
        static std::shared_ptr<_SessionType> session_cast(std::shared_ptr<session_logic_interface> basic_session)
        {
            return std::dynamic_pointer_cast<_SessionType>(basic_session);
        }

        template <class _SessionType>
        static std::shared_ptr<session_logic_interface> session_cast(std::shared_ptr<_SessionType> drived_session)
        {
            return std::dynamic_pointer_cast<session_logic_interface>(drived_session);
        }

        static server_id cal_server_uid(uint16_t area_id, uint16_t server_id_, const std::string& platform)
        {
            uint32_t str_hash = 0;
            uint32_t len = 0;
            for (; *(platform.c_str() + len) != '\0'; len++)
            {
                str_hash = str_hash * 31 + *(platform.c_str());
            }

            return (((server_id)area_id) << 48) + (((server_id)server_id_) << 32) + str_hash;
        }
    };

    struct rc4_info
    {
        int           rc4_modvt_;
        unsigned char rc4_key_[RC4_KEY_LEN];
        int           rc4_subtract_;

        // do data copy when inherit
        rc4_info()
        {
            uint64_t rand = SAFE_RAND;
            rc4_modvt_ = rand % 255 + 1;

            for (size_t i = 0; i < RC4_KEY_LEN; i++) {
                rand = SAFE_RAND;
                rc4_key_[i] = (char)(rand % 255 + 1);
            }

            rand = SAFE_RAND;
            rc4_subtract_ = rand % 255 + 1;
        }
    };

    struct server_info
    {
        uint16_t        server_id_;
        uint16_t        area_id_;
        std::string     platform_;
        unsigned char   link_type_;
    };
}