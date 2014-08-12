#include "log.hpp"

namespace Redis {
    double microtime() {
        Clock::time_point t = Clock::now();
        return std::chrono::duration_cast<std::chrono::duration<double>>(t.time_since_epoch()).count();
    }
    LogLevel Log::log_level = LogLevel::WARNING;

    void Log::log(LogLevel, const std::string& data) {
        std::cerr << data;
    }

    void Log::set_log_level(LogLevel new_log_level) {
        log_level = new_log_level;
    }

    LogLevel Log::get_log_level() {
        return log_level;
    }
}