#include "connection_param.hpp"
namespace Redis {
    static std::hash<std::string> hash_fn;
    ConnectionParam ConnectionParam::default_connection_param = {"127.0.0.1", 6379, "", 0, "", 1000, 1000, true, false};
    ConnectionParam::ConnectionParam(
            const std::string &_host,
            unsigned int _port,
            const std::string& _password,
            unsigned int _db_num,
            const std::string &_prefix,
            unsigned int _connect_timeout_ms,
            unsigned int _operation_timeout_ms,
            bool _reconnect_on_failure,
            bool _throw_on_error
    ) :
            host(_host),
            port(_port),
            password(_password),
            db_num(_db_num),
            prefix(_prefix),
            connect_timeout_ms(_connect_timeout_ms),
            operation_timeout_ms(_operation_timeout_ms),
            reconnect_on_failure(_reconnect_on_failure),
            throw_on_error(_throw_on_error)
    {}
    unsigned long long ConnectionParam::get_hash() const {
            return
                    hash_fn(host)                                           +   //We don't care about overflow. Only about unique values
                    (static_cast<unsigned long>(port) << 48)                +   //64 - 16 bit
                    hash_fn(password)                                       +
                    (static_cast<unsigned long>(db_num) << 44)              +   //48 - 4  bit
                    hash_fn(prefix)                                         +
                    (static_cast<unsigned long>(connect_timeout_ms) << 28)  +   //44 - 16 bit
                    (static_cast<unsigned long>(operation_timeout_ms) << 12)+   //28 - 16 bit
                    (static_cast<unsigned long>(reconnect_on_failure) << 11)+   //12 - 1  bit
                    (static_cast<unsigned long>(throw_on_error) << 10)          //11 - 1  bit
            ;
    }

    ConnectionParam::ConnectionParam(ConnectionParam &&other) :
            host(std::move(other.host)),
            port(other.port),
            password(std::move(other.password)),
            db_num(other.db_num),
            prefix(std::move(other.prefix)),
            connect_timeout_ms(other.connect_timeout_ms),
            operation_timeout_ms(other.operation_timeout_ms),
            reconnect_on_failure(other.reconnect_on_failure),
            throw_on_error(other.throw_on_error)
    {
    }
}