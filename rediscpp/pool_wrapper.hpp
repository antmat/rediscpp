#pragma once
#include "connection.hpp"
#include "macro.hpp"
namespace Redis {
    class PoolWrapper {
    private:
        Connection* redis;
        bool* used;
    public:
        PoolWrapper();
        PoolWrapper(Connection& _redis, bool& used);
        ~PoolWrapper();
        PoolWrapper(const PoolWrapper& other) = delete;
        PoolWrapper& operator=(const PoolWrapper& other) = delete;
        PoolWrapper& operator=(PoolWrapper&& other);
        PoolWrapper(PoolWrapper &&other);

        Connection& operator*() {
            redis_assert(redis != nullptr);
            return *redis;
        }

        Connection* operator->() {
            redis_assert(redis != nullptr);
            return redis;
        }
    };
}