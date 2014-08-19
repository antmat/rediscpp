#pragma once

#include <sstream>
#include <iostream>
#include <chrono>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace Redis {
    typedef std::chrono::high_resolution_clock Clock;

    enum class LogLevel {
        NONE,
        CRIT,
        WARNING,
        NOTICE,
        ALL
    };
    typedef LogLevel LL;
    double microtime();

    class Log {
    private:
        static LogLevel log_level;
    public:
        static void log(LogLevel, const std::string& data);
        static void set_log_level(LogLevel new_log_level);
        static LogLevel get_log_level();
    };
}

#define rediscpp_debug(log_lev, ...) {\
    if(log_lev <= ::Redis::Log::get_log_level()) {\
        std::ostringstream ss;\
        ss  << '[' << getpid() << ']' << ' ' << '[' << syscall(SYS_gettid) << ']' << ' ' << std::to_string(microtime()) << " rediscpp: " << __VA_ARGS__ << std::endl;\
        ::Redis::Log::log(log_lev, ss.str());\
    }\
}

