#pragma once
#include "connection.hpp"

namespace Redis {
    class Wrapper {
    private:
        Connection *redis;
    public:
        Wrapper(Connection &_redis);
        ~Wrapper();
        Wrapper(const Wrapper &other) = delete;
        Wrapper &operator=(const Wrapper &other) = delete;
        Wrapper(Wrapper &&other);

        Connection &operator*() {
            return *redis;
        }

        Connection *operator->() {
            return redis;
        }
    };
}