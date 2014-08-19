#pragma once
#include <exception>
#include <hiredis/hiredis.h>
#include <string>
namespace Redis {
    class Exception : public std::exception {
    private:
        std::string err_str;
    public:
        Exception(const std::string& message);
        Exception(std::string&& message);
        Exception(redisContext* context);
        Exception(redisContext* context, redisReply* reply);
        virtual const char *what() const noexcept(true) override ;
    };
}