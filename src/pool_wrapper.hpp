#pragma once
#include "connection.hpp"

namespace Redis {
    class PoolWrapper {
    private:
        Connection* redis;
    public:
        PoolWrapper(Connection& _redis);
        ~PoolWrapper();
        PoolWrapper(const PoolWrapper& other) = delete;
        PoolWrapper& operator=(const PoolWrapper& other) = delete;
        PoolWrapper(PoolWrapper &&other);

        Connection& operator*() {
            return *redis;
        }

        Connection* operator->() {
            return redis;
        }
    };
}