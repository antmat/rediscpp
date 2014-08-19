#include "exception.hpp"

namespace Redis {
    const char* Exception::what() const noexcept(true) {
        return err_str.c_str();
    }

    Exception::Exception(const std::string& message) :
            err_str(message)
    {
    }

    Exception::Exception(std::string&& message) :
            err_str(message)
    {
    }

    Exception::Exception(redisContext* context) :
            err_str(context == nullptr ?
                    std::string("Redis Exception: nullptr passed as context. Please report a bug.") :
                    std::string("Redis Exception(") + std::to_string(context->err) + "): " + context->errstr )
    {
    }

    Exception::Exception(redisContext* context, redisReply* reply) :
            err_str(context == nullptr ?
                    std::string("Redis Exception: nullptr passed as context. Please report a bug.") :
                    std::string("Redis Exception(") + std::to_string(context->err) + "): " + context->errstr + ". Reply:" + (reply == nullptr? "nullptr" : reply->str))
    {}
}