#pragma once

#include <functional>
#include <vector>

#include "parallel_core/SafeSingleton.h"
#include "parallel_core/RingBuffer.h"
#include "basic_async_session.h"
#include "NetUtils.hpp"
#include "JsonUtils.hpp"
#include "active_server_session_logic.h"

namespace net_middleware
{
    class cluster_config
    {
    public:
        std::string proxy_ip_;
        uint16_t proxy_port_;
        uint16_t sid_;
        uint16_t area_id_;
        std::string platform_;
        uint32_t tick_interval_;
        uint32_t keep_alive_timeout_;

        cluster_config(const std::string& filepath = "../../../../cluster_config.json")
        {
            if (json_utils::open_file(filepath.c_str(), _dom))
            {
                try
                {
                    proxy_ip_ = _dom["proxy_ip"].GetString();
                    proxy_port_ = _dom["proxy_port"].GetInt();
                    sid_ = _dom["sid"].GetInt();
                    area_id_ = _dom["area_id"].GetInt();
                    platform_ = _dom["platform"].GetString();
                    tick_interval_ = _dom["tick_interval"].GetInt();
                    keep_alive_timeout_ = _dom["keep_alive_timeout"].GetInt();

                    return;
                }
                catch (std::exception e)
                {
                    LOG("failed to load cluster config: %s", e.what());
                }
            }

            abort();
        }

    private:
        rapidjson::Document _dom;
    };
    
    // actively connect to proxy
    // settled into server
    class active_server_session_mgr : public parallel_core::SafeSingleton<active_server_session_mgr>
    {
    public:
        using session_sptr = std::shared_ptr<basic_async_session>;

    public:
        active_server_session_mgr();
        ~active_server_session_mgr();

        void start();

        void stop();

        void send(session_uid target_id, unsigned char* data_block, uint16_t len);

        void broadcast(const std::vector<session_uid>& targets, unsigned char* data_block, uint16_t len);

        bool pick_msg(session_uid* from_id, unsigned char* ret_cmd, unsigned char* ret_block, uint16_t* ret_len);

        uint32_t get_session_remote_ip(session_uid s_uid);

        void set_session_remote_ip(session_uid s_uid, uint32_t ip);

    private:
        cluster_config _cluster_config;

        job_excutor_sptr _session_excutor;

        lockfree_buffer_sptr _storage;

        session_sptr _session;

        session_uid _already_read_uid;
        uint16_t _already_read_cmd;
        uint16_t _already_read_length;

        struct extra_socket_info
        {
            uint32_t remote_ip;
        };
        std::unordered_map<session_uid, extra_socket_info> _sock_info_map;
    };
}

#define ACTIVE_SERVER_MGR net_middleware::active_server_session_mgr::instance()