#include "pool_wrapper.hpp"
namespace Redis {
    PoolWrapper::PoolWrapper() :
        redis(nullptr)
    {
    }
    PoolWrapper::PoolWrapper(Connection &_redis) :
            redis(&_redis) {
    }

    PoolWrapper& PoolWrapper::operator=(PoolWrapper&& other) {
        std::swap(redis,other.redis);
        return *this;
    }
    PoolWrapper::PoolWrapper(PoolWrapper &&other) :
            redis(other.redis) {
        other.redis = nullptr;
    }

    PoolWrapper::~PoolWrapper() {
        if (redis != nullptr) {
            redis->done();
        }
    }
}