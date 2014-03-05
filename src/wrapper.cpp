#include "wrapper.hpp"
namespace Redis {
    Wrapper::Wrapper(Connection &_redis) :
            redis(&_redis) {
    }

    Wrapper::Wrapper(Wrapper &&other) :
            redis(other.redis) {
        other.redis = nullptr;
    }

    Wrapper::~Wrapper() {
        if (redis != nullptr) {
            redis->done();
        }
    }
}