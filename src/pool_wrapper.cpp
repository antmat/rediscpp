#include "pool_wrapper.hpp"
namespace Redis {

    PoolWrapper::PoolWrapper() :
        redis(nullptr),
        used(nullptr)
    {
    }

    PoolWrapper::PoolWrapper(Connection &_redis, bool& _used) :
            redis(&_redis),
            used(&_used)
    {
    }

    PoolWrapper& PoolWrapper::operator=(PoolWrapper&& other) {
        std::swap(redis,other.redis);
        std::swap(used, other.used);
        return *this;
    }
    PoolWrapper::PoolWrapper(PoolWrapper &&other) :
            redis(other.redis),
            used(other.used)
    {
        other.redis = nullptr;
        other.used = nullptr;
    }

    PoolWrapper::~PoolWrapper() {
        if(used != nullptr) {
            *used = false;
        }
    }
}