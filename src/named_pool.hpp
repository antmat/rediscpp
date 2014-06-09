#include "connection_param.hpp"
#include "pool_wrapper.hpp"
#pragma once
namespace Redis {
    class NamedPool {
    public:
        class Implementation;
        static bool is_created(const std::string& name);
        static void create(const std::string& name, const std::vector<Redis::ConnectionParam> connection_params);
        static NamedPool& get_pool(const std::string& name);
        PoolWrapper get(const std::string& key);
        ~NamedPool();
        NamedPool(const NamedPool& other) = delete;
        NamedPool& operator=(const NamedPool& other) = delete;
    private:
        NamedPool();

        Implementation* d;
    };
}