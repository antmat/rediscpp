#pragma once
#include <sstream>
#include <iostream>
#include <chrono>
namespace Redis {
    typedef std::chrono::high_resolution_clock Clock;

    inline double microtime() {
        Clock::time_point t = Clock::now();
        return std::chrono::duration_cast<std::chrono::duration<double>>(t.time_since_epoch()).count();
    }
}