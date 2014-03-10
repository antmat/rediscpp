#include "redis.hpp"
#define get_prefixed_key const std::string prefixed_key = has_prefix() ? add_prefix_to_key(key) : key
namespace Redis {

    Connection::Connection(const ConnectionParam &_connection_param) :
            connection_param(_connection_param),
            available(false),
            used(true),
            context(),
            hash_fn()
    {
        reconnect();
        if (connection_param.db_num != 0) {
            switch_db(connection_param.db_num);
        }
    }

    Connection::Connection(
            const std::string &host,
            unsigned int port,
            const std::string& _password,
            unsigned int db_num,
            const std::string &prefix,
            unsigned int connect_timeout_ms,
            unsigned int operation_timeout_ms,
            bool reconnect_on_failure,
            bool throw_on_error
    ) :
            Connection(ConnectionParam(host, port, _password, db_num, prefix, connect_timeout_ms, operation_timeout_ms, reconnect_on_failure, throw_on_error))
    {}

    bool Connection::switch_db(long db_num) {
        return run_command("SELECT %d", db_num);
    }

    bool Connection::reconnect() {
        struct timeval timeout = {static_cast<time_t>(connection_param.connect_timeout_ms % 1000), static_cast<suseconds_t>(connection_param.connect_timeout_ms * 1000)};
        context.reset(redisConnectWithTimeout(connection_param.host.c_str(), connection_param.port, timeout));
        if (context == nullptr) {
            available = false;
        }
        else available = context->err ? false : true;
        return available;
    }

    bool Connection::run_command(Reply& reply, const char* format, va_list ap) {
        if (!is_available() && connection_param.reconnect_on_failure) {
            reconnect();
            if (!is_available()) {
                if (connection_param.throw_on_error) {
                    throw Redis::Exception(context.get());
                }
                return false;
            }
        }
        va_list ap2;
        va_copy(ap2,ap);
        reply.reset(static_cast<redisReply*>(redisvCommand(context.get(), format, ap2)));
        va_end(ap2);
        if((reply.get() == nullptr || context->err) && connection_param.reconnect_on_failure) {
            reconnect();
            if (!is_available()) {
                if(connection_param.throw_on_error) {
                    throw Redis::Exception(context.get());
                }
                return false;
            }
            reply.reset(static_cast<redisReply*>(redisvCommand(context.get(), format, ap)));
        }
        if(reply.get() == nullptr || context->err) {
            if(connection_param.throw_on_error) {
                throw Redis::Exception(context.get(), reply.get());
            }
            return false;
        }
        return true;
    }

    bool Connection::get(const std::string &key, std::string &result) {
        Reply reply;
        get_prefixed_key;
        if(run_command(reply, "GET \"%b\"", prefixed_key.c_str(), prefixed_key.size())) {
            result.assign(reply->str, reply->len);
            return true;
        }
        return false;
    }

    /*bool Connection::hincrby(const std::string &key, const std::string &field, long increment, long long &result) {
        Reply reply;
        get_prefixed_key;
        if(run_command("HINCRBY \"%b\" \"%b\" %l", prefixed_key.c_str(), prefixed_key.size(), field.c_str(), field.size(), increment)) {
            result = reply->integer;
            return true;
        }
        return false;
    }*/

    /*bool Connection::set(KeyRef &key, const KeyRef &value) {
        get_prefixed_key;
        return run_command("SET \"%b\" %b", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size());
    }*/

    std::string Connection::add_prefix_to_key(const std::string &key) {
        return connection_param.prefix + key;
    }

    void Connection::update_param(const ConnectionParam& new_param) {
        assert(connection_param.host == new_param.host);
        assert(connection_param.port == new_param.port);
        if(new_param.db_num != connection_param.db_num) {
            switch_db(new_param.db_num);
        }
        connection_param = new_param;
    }

}
