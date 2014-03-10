#pragma once
#include <string>
namespace Redis {
    class ConnectionParam {
    private:
        static ConnectionParam default_connection_param;

    public:
        std::string host;
        unsigned int port;
        std::string password;
        unsigned int db_num;
        std::string prefix;
        unsigned int connect_timeout_ms;
        unsigned int operation_timeout_ms;
        bool reconnect_on_failure;
        bool throw_on_error;


        //TODO: thread safety. This one is not thread safe. It cannot be used while other operations with library are in progress
        inline static void set_default_host(const std::string& default_host) {
            default_connection_param.host.assign(default_host);
        }

        inline static void set_default_port(unsigned int default_port) {
            default_connection_param.port = default_port;
        }

        //TODO: thread safety. This one is not thread safe. It cannot be used while other operations with library are in progress
        inline static void set_default_password(const std::string& _password) {
            default_connection_param.password = _password;
        }

        inline static void set_default_db_num(unsigned int default_db_num) {
            default_connection_param.db_num = default_db_num;
        }

        //TODO: thread safety. This one is not thread safe. It cannot be used while other operations with library are in progress
        inline static void set_default_prefix(const std::string &default_prefix) {
            default_connection_param.prefix = default_prefix;
        }

        inline static void set_default_connect_timeout_ms(unsigned int default_connect_timeout_ms) {
            default_connection_param.connect_timeout_ms = default_connect_timeout_ms;
        }

        inline static void set_default_operation_timeout_ms(unsigned int default_operation_timeout_ms) {
            default_connection_param.operation_timeout_ms = default_operation_timeout_ms;
        }

        inline static void set_reconnect_on_failure(bool reconnect_on_failure) {
            default_connection_param.reconnect_on_failure = reconnect_on_failure;
        }

        inline static void set_throw_on_error(bool throw_on_error) {
            default_connection_param.throw_on_error = throw_on_error;
        }

        inline static const ConnectionParam &get_default_connection_param() {
            return default_connection_param;
        }

        ConnectionParam(
                const std::string &_host = default_connection_param.host,
                unsigned int _port = default_connection_param.port,
                const std::string& password = default_connection_param.password,
                unsigned int _db_num = default_connection_param.db_num,
                const std::string &_prefix = default_connection_param.prefix,
                unsigned int connect_timeout_ms = default_connection_param.connect_timeout_ms,
                unsigned int operation_timeout_ms = default_connection_param.operation_timeout_ms,
                bool try_reconnect_on_failure = default_connection_param.reconnect_on_failure,
                bool throw_on_error = default_connection_param.throw_on_error
        );
        ConnectionParam(ConnectionParam &&other);
        ConnectionParam(const ConnectionParam &) = default;
        ConnectionParam &operator=(const ConnectionParam &) = default;
    };
}