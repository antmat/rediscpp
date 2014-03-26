#pragma once
#include <map>
#include <vector>
#include <memory>
#include "connection.hpp"
#include <mutex>
#include "pool_wrapper.hpp"
#include "connection.hpp"
#include "connection_param.hpp"
namespace Redis {
    class Pool {

    public:
        static Pool &instance();

        size_t get_connection_index_by_key(const std::string &key, const std::vector<ConnectionParam> &connection_params);
        PoolWrapper get_by_key(const std::string &key, const std::vector<ConnectionParam> &connection_params);
        PoolWrapper get(const std::string &host = ConnectionParam::get_default_connection_param().host,
                unsigned int port = ConnectionParam::get_default_connection_param().port,
                const std::string &password = ConnectionParam::get_default_connection_param().password,
                unsigned int db_num = ConnectionParam::get_default_connection_param().db_num,
                const std::string &prefix = ConnectionParam::get_default_connection_param().prefix,
                unsigned int connect_timeout_ms = ConnectionParam::get_default_connection_param().connect_timeout_ms,
                unsigned int operation_timeout = ConnectionParam::get_default_connection_param().operation_timeout_ms,
                bool reconnect_on_failure = ConnectionParam::get_default_connection_param().reconnect_on_failure,
                bool throw_on_error = ConnectionParam::get_default_connection_param().throw_on_error
        );
        PoolWrapper get(const ConnectionParam &connection_param);

    private:
        class Impl;
        Impl* d;
    };

}


