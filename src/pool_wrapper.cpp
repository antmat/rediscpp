#include "pool_wrapper.hpp"
namespace Redis {
    PoolWrapper::PoolWrapper(Connection &_redis) :
            redis(&_redis) {
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