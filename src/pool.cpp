#include "redis.hpp"
#include <thread>
#include <unordered_map>

namespace Redis {

    class Pool::Impl {
    public:
        static constexpr size_t bucket_count = 100;
        Impl() :
                instances(bucket_count),
                locks(bucket_count) {
        }
        struct ConnectionParamHasher {
            unsigned long operator()(const ConnectionParam& c_param) const {
                return c_param.get_hash();
            }
        };
        typedef std::pair<Connection, bool> ConnectionWithUsage;
        typedef std::vector<std::unique_ptr<ConnectionWithUsage>> ConnectionVector;
        typedef std::unordered_map<ConnectionParam,  ConnectionVector, ConnectionParamHasher> ConnectionVectorMap;
        std::vector<ConnectionVectorMap> instances;
        std::vector<std::mutex> locks;
    };

    Pool::Pool() :
        d(new Pool::Impl())
    {}
    Pool::~Pool() {
        if(d != nullptr) {
            delete d;
        }
    }
    //Should be thread safe in c++11 standart
    Pool& Pool::instance() {
        static Pool inst;
        return inst;
    }

    size_t Pool::get_connection_index_by_key(const std::string &key, const std::vector<ConnectionParam> &connection_params) {
        static std::hash<std::string> hash_fn;
        return hash_fn(key) % connection_params.size();
    }

    PoolWrapper Pool::get_by_key(const std::string &key, const std::vector<ConnectionParam> &connection_params) {
        static std::hash<std::string> hash_fn;
        return get(connection_params[hash_fn(key) % connection_params.size()]);
    }

    PoolWrapper Pool::get(const ConnectionParam &connection_param) {
        size_t bucket = connection_param.get_hash() % d->bucket_count;
        std::lock_guard<std::mutex> guard(d->locks[bucket]);
        Impl::ConnectionVector &vec = d->instances[bucket][connection_param];
        for (size_t i = 0; vec.size() > i; i++) {
            if (!vec[i]->second) {
                vec[i]->second = true;
                return PoolWrapper(vec[i]->first, vec[i]->second);
            }
        }
        vec.emplace_back(new Impl::ConnectionWithUsage(connection_param, true));
        return PoolWrapper(vec.back()->first, vec.back()->second);
    }

    PoolWrapper Pool::get(const std::string& host,
            unsigned int port,
            const std::string& password,
            unsigned int db_num,
            const std::string& prefix,
            unsigned int connect_timeout_ms,
            unsigned int operation_timeout_ms,
            bool reconnect_on_failure,
            bool throw_on_error
    ) {
        ConnectionParam param(host, port, password, db_num, prefix, connect_timeout_ms, operation_timeout_ms, reconnect_on_failure, throw_on_error);
        return get(param);
    }
}