#include "named_pool.hpp"
#include "pool.hpp"
#include "log.hpp"
#include <map>
#include <mutex>
#include <array>
#include <iostream>
#include "exception.hpp"
static std::hash<std::string> hash_fn;
static constexpr size_t bucket_count = 48;
namespace Redis {
    class NamedPool::Implementation {
        friend class NamedPool;
        Pool pool;
        std::vector<ConnectionParam> connection_params;

        Implementation() : pool(), connection_params() {}
        void assign_connection_param(const std::vector<ConnectionParam> conn_params) {
            connection_params = conn_params;
            for(size_t i=0; i< conn_params.size(); i++) {
                //Preinitialization;
                pool.get(conn_params[i]);
            }
        }
        static bool is_created_no_lock(const std::string& name) {
            size_t bucket = hash_fn(name) % bucket_count;
            auto it = Implementation::instances[bucket].find(name);
            return it != Implementation::instances[bucket].end() && it->second.use_count() != 0;
        }
        static std::array<std::map<std::string, std::shared_ptr<NamedPool>>, bucket_count> instances;
        static std::array<std::mutex, bucket_count> mutexes;
    };
    std::array<std::map<std::string, std::shared_ptr<NamedPool>>, bucket_count> NamedPool::Implementation::instances;
    std::array<std::mutex, bucket_count> NamedPool::Implementation::mutexes;


    NamedPool::NamedPool() :
            d(new Implementation())
    {}

    NamedPool::~NamedPool() {
        if (d != nullptr) {
            delete d;
        }
    }

    bool NamedPool::is_created(const std::string& name) {
        size_t bucket = hash_fn(name) % bucket_count;
        std::lock_guard<std::mutex> guard(Implementation::mutexes[bucket]);
        auto it = Implementation::instances[bucket].find(name);
        return it != Implementation::instances[bucket].end() && it->second.use_count() != 0;
    }



    void NamedPool::create(const std::string& name, const std::vector<Redis::ConnectionParam> connection_params) {
        size_t bucket = hash_fn(name) % bucket_count;
        std::lock_guard<std::mutex> guard(Implementation::mutexes[bucket]);

        auto& ptr = Implementation::instances[bucket][name];
        rediscpp_debug(LL::NOTICE, "Going to create Named Pool");

        if(Implementation::is_created_no_lock(name)) {
            redis_assert(ptr != nullptr);
            auto& c_param = Implementation::instances[bucket][name]->d->connection_params;
            if(c_param.size() != connection_params.size()) {
                throw Redis::Exception("Trying to create named pool with different connections params");
            }
            for(size_t i=0; i<c_param.size(); i++) {
                if(c_param[i] != connection_params[i]) {
                    throw Redis::Exception("Trying to create named pool with different connections params");
                }
            }
            return;
        }
        else {
            redis_assert(ptr == nullptr);
            ptr = std::shared_ptr<NamedPool>(new NamedPool);
            ptr->d->assign_connection_param(connection_params);
        }
    }
    NamedPool& NamedPool::get_pool(const std::string& name) {
        if(!is_created(name)) {
            throw Redis::Exception("Requested uncreated item from Named Pool");
        }
        size_t bucket = hash_fn(name) % bucket_count;
        std::lock_guard<std::mutex> guard(Implementation::mutexes[bucket]);
        return *(Implementation::instances[bucket][name]);
    }
    PoolWrapper NamedPool::get(const std::string& key) {
        size_t index = d->pool.get_connection_index_by_key(key, d->connection_params);
        return d->pool.get(d->connection_params[index]);
    }
}