#pragma once
#include <cassert>
#define redis_assert assert
#define redis_assert_unreachable() assert(false)

//#define REDISCPP_DEBUG 1
#ifdef REDISCPP_DEBUG
 #include "debug.hpp"
 #define rediscpp_debug(...) {std::ostringstream ss; ss  << microtime() << " : " << __VA_ARGS__ << std::endl; std::clog << ss.str();}
#else
 #define rediscpp_debug(...) ((void)0)
#endif