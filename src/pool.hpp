#pragma once
#include <map>
#include <vector>
#include <memory>
#include "connection.hpp"
#include <mutex>
#include "wrapper.hpp"
#include "connection_param.hpp"
namespace Redis {
    class Pool {
    private:
        Pool() :
                instances(),
                lock() {
        }

        Pool(const Pool &other) = delete;
        Pool &operator=(const Pool &other) = delete;
        std::map<unsigned long, std::vector<std::unique_ptr<Connection> > > instances;
        std::mutex lock;
    public:
        static Pool &instance() {
            static Pool inst;
            return inst;
        }

        Wrapper get_by_key(const std::string &key, const std::vector<ConnectionParam> &connection_params);
        Wrapper get(const std::string &host = ConnectionParam::get_default_connection_param().host,
                unsigned int port = ConnectionParam::get_default_connection_param().port,
                unsigned int db_num = ConnectionParam::get_default_connection_param().db_num,
                const std::string &prefix = ConnectionParam::get_default_connection_param().prefix,
                unsigned int connect_timeout_ms = ConnectionParam::get_default_connection_param().connect_timeout_ms,
                unsigned int operation_timeout = ConnectionParam::get_default_connection_param().operation_timeout_ms,
                bool reconnect_on_failure = ConnectionParam::get_default_connection_param().reconnect_on_failure,
                bool throw_on_error = ConnectionParam::get_default_connection_param().throw_on_error
        );
        Wrapper get(const ConnectionParam &connection_param);
    };
}