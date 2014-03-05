#pragma once
#include <string>
#include <vector>
#include <memory>
#include <hiredis/hiredis.h>
#include <assert.h>
#include "connection_param.hpp"
#include "exception.hpp"
namespace Redis {
    class Connection {
    public:

        Connection &operator=(const Connection &other) = delete;
        Connection(const Connection &other) = delete;
        Connection(const ConnectionParam &connection_param);
        Connection(const std::string &host = ConnectionParam::get_default_connection_param().host,
                unsigned int port = ConnectionParam::get_default_connection_param().port,
                unsigned int db_num = ConnectionParam::get_default_connection_param().db_num,
                const std::string &prefix = ConnectionParam::get_default_connection_param().prefix,
                unsigned int connect_timeout_ms = ConnectionParam::get_default_connection_param().connect_timeout_ms,
                unsigned int operation_timeout = ConnectionParam::get_default_connection_param().operation_timeout_ms,
                bool reconnect_on_failure = ConnectionParam::get_default_connection_param().reconnect_on_failure,
                bool throw_on_error = ConnectionParam::get_default_connection_param().throw_on_error
        );

        bool switch_db(unsigned int db_num);
        ~Connection() {}
        inline bool is_available() { return available; }
        std::string get_error() { return context->errstr; }
        unsigned int get_errno() { return context->err; }
        inline bool has_prefix() { return !connection_param.prefix.empty(); }

        //Redis commands
//    bool del(const std::string& key);
//    bool del(const std::vector<std::string>& keys);
        bool get(const std::string &key, std::string &result);
        bool set(const std::string &key, const std::string &value);
        bool hincrby(const std::string &key, const std::string &field, long increment, long long &result);

        friend class Pool;
        friend class Wrapper;
    private:
        ConnectionParam connection_param;
        bool available;
        bool used;
        std::unique_ptr<redisContext> context;
        std::hash<std::string> hash_fn;
        typedef std::unique_ptr<redisReply> Reply;

        bool reconnect();
        std::string add_prefix_to_key(const std::string &key);
        inline void done() { used = false; }
        inline void set_used() { used = true; }
        inline bool is_used() { return used; }
        void update_param(const ConnectionParam& new_param);
        bool run_command(Reply& reply, const char* format, va_list ap);
        inline bool run_command(Reply& reply, const char* format, ...) {
            va_list ap;
            va_start(ap,format);
            bool ret = run_command(reply, format,ap);
            va_end(ap);
            return ret;
        }
        inline bool run_command(const char* format, ...) {
            Reply reply;
            va_list ap;
            va_start(ap,format);
            bool ret = run_command(reply, format,ap);
            va_end(ap);
            return ret;
        }
    };
}