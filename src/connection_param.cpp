#include "connection_param.hpp"
namespace Redis {
    ConnectionParam ConnectionParam::default_connection_param = {"127.0.0.1", 6379, 0, "", 1000, 1000, true, false};
    ConnectionParam::ConnectionParam(
            std::string const &_host,
            unsigned int _port,
            unsigned int _db_num,
            std::string const &_prefix,
            unsigned int _connect_timeout_ms,
            unsigned int _operation_timeout_ms,
            bool _reconnect_on_failure,
            bool _throw_on_error
    ) :
            host(_host),
            port(_port),
            db_num(_db_num),
            prefix(_prefix),
            connect_timeout_ms(_connect_timeout_ms),
            operation_timeout_ms(_operation_timeout_ms),
            reconnect_on_failure(_reconnect_on_failure),
            throw_on_error(_throw_on_error) {
    }

    ConnectionParam::ConnectionParam(ConnectionParam &&other) :
            host(std::move(other.host)),
            port(other.port),
            db_num(other.db_num),
            prefix(std::move(other.prefix)),
            connect_timeout_ms(other.connect_timeout_ms),
            operation_timeout_ms(other.operation_timeout_ms),
            reconnect_on_failure(other.reconnect_on_failure),
            throw_on_error(other.throw_on_error) {
    }
}