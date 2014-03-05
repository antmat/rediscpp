#include "redis.hpp"
#include <thread>

namespace Redis {
    Wrapper Pool::get_by_key(const std::string &key, const std::vector<ConnectionParam> &connection_params) {
        static std::hash<std::string> hash_fn;
        return get(connection_params[hash_fn(key) % connection_params.size()]);
    }

    Wrapper Pool::get(const ConnectionParam &connection_param) {
        static std::hash<std::string> hash_fn;
        unsigned long hash = hash_fn(connection_param.host) + connection_param.port;
        std::lock_guard<std::mutex> guard(lock);
        std::vector<std::unique_ptr<Connection>> &vec = instances[hash];
        for (size_t i = 0; vec.size() > i; i++) {
            if (!vec[i]->is_used()) {
                vec[i]->set_used();
                vec[i]->update_param(connection_param);
                return *vec[i];
            }
        }
        vec.push_back(std::unique_ptr<Connection>(new Connection(connection_param)));
        return Wrapper(*(vec.back()));
    }

    Wrapper Pool::get(const std::string &host,
            unsigned int port,
            unsigned int db_num,
            const std::string &prefix,
            unsigned int connect_timeout_ms,
            unsigned int operation_timeout_ms,
            bool reconnect_on_failure,
            bool throw_on_error
    ) {
        ConnectionParam param(host, port, db_num, prefix, connect_timeout_ms, operation_timeout_ms, reconnect_on_failure, throw_on_error);
        return get(param);
    }
}