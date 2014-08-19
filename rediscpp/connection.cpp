#include <cstring>
#include "connection.hpp"
#include "exception.hpp"
#include <iostream>
#include <vector>
#include <map>
#include <atomic>
#include "macro.hpp"
#include "log.hpp"



namespace Redis {
    struct ReplyDeleter {
        void operator()(redisReply* r) {
            if(r != nullptr) {
                freeReplyObject(r);
            }
        }
    };

    struct ContextDeleter {
        void operator()(redisContext* c) {
            if (c != nullptr) {
                redisFree(c);
            }
        }
    };

    static constexpr size_t max_connection_count = 1000;
    class Connection::Implementation {
        friend class Connection;
        friend class PoolWrapper;
        friend class Pool;
        typedef std::unique_ptr<redisReply, ReplyDeleter> Reply;
        Reply reply;
        ConnectionParam connection_param;
        bool available;
        bool connected;
        bool used;
        std::unique_ptr<redisContext, ContextDeleter> context;
        std::hash<std::string> hash_fn;
        unsigned int redis_version;
        Error err;
        Error prev_err;
        Connection::Id id;
        static std::atomic_long id_counter;
        static std::atomic_ulong connection_count;

        Implementation(const ConnectionParam &_connection_param) :
                reply(),
                connection_param(_connection_param),
                available(false),
                connected(false),
                used(true),
                context(),
                hash_fn(),
                redis_version(),
                err(),
                prev_err(),
                id()
        {
            id = ++id_counter;
            unsigned long con_cnt = connection_count.load(std::memory_order_relaxed);
            if(con_cnt >= max_connection_count) {
                throw Redis::Exception("Maximum number of connections reached.");
            }
            connection_count++;

            rediscpp_debug(LogLevel::NOTICE, "Connection created. Est. current number of connections: " << con_cnt);
        }
        ~Implementation() {
            connection_count--;
            rediscpp_debug(LogLevel::NOTICE, "Connection destroyed. Est. current number of connections: " << connection_count.load(std::memory_order_relaxed));
        }

        template<class Keys>
        void append_c_strings_with_prefixes_and_sizes(const Keys& keys, KeyVec& prefixed_keys, std::vector<const char*>& c_strs, std::vector<size_t>& sizes) {
            redis_assert(c_strs.size() == sizes.size());
            redis_assert(prefixed_keys.empty());
            size_t initial_size = c_strs.size();
            sizes.resize(initial_size+ keys.size());
            c_strs.resize(initial_size + keys.size());
            if(has_prefix()) {
                prefixed_keys.reserve(keys.size());
                for (size_t i = 0; i < keys.size(); i++) {
                    prefixed_keys.push_back(add_prefix_to_key(keys[i]));
                    c_strs[i+initial_size] = prefixed_keys[i].c_str();
                    sizes[i+initial_size] = prefixed_keys[i].size();
                }
            }
            else {
                for (size_t i = 0; i < keys.size(); i++) {
                    c_strs[i+initial_size] = static_cast<const Key&>(keys[i]).c_str();
                    sizes[i+initial_size] = static_cast<const Key&>(keys[i]).size();
                }
            }
        }

        static std::string get_error_str(Error err, redisContext* context, redisReply* reply) {
            switch (err) {
                case Error::NONE:
                    return "";
                case Error::CONTEXT_IS_NULL:
                    return "hiredis context is null";
                case Error::REPLY_IS_NULL:
                    return "hiredis reply is null";
                case Error::FLOAT_OUT_OF_RANGE:
                    return std::string("Number can not be represented by float. ") + (context->err ? std::string("Hiredis err is: ") + context->errstr : std::string());
                case Error::DOUBLE_OUT_OF_RANGE:
                    return std::string("Double can not be represented by double. ") + (context->err ? std::string("Hiredis err is: ") + context->errstr : std::string());
                case Error::HIREDIS_IO:
                    return std::string("Hiredis io error. errno:"+std::to_string(errno)) + (context->err ? std::string(". Hiredis err is: ") + context->errstr : std::string());
                case Error::HIREDIS_EOF:
                    return std::string("Hiredis EOF error: ") + context->errstr;
                case Error::HIREDIS_PROTOCOL:
                    return std::string("Hiredis protocol error: ") + context->errstr;
                case Error::HIREDIS_OOM:
                    return std::string("Hiredis OOM error: ") + context->errstr;
                case Error::HIREDIS_OTHER:
                    return std::string("Hiredis error: ") + context->errstr;
                case Error::HIREDIS_UNKNOWN:
                    return std::string("Hiredis UNKNOWN error with code ") + std::to_string(context->err) + " :" + context->errstr;
                case Error::COMMAND_UNSUPPORTED:
                    return "Command is unsupported by this redis version" + (context->err ? std::string("Hiredis err is: ") + context->errstr : std::string());
                case Error::UNEXPECTED_INFO_RESULT:
                    return "Info command returned unexpected result. " + (context->err ? std::string("Hiredis err is: ") + context->errstr : std::string());
                case Error::REPLY_ERR:
                    return "Reply returned error." + (context->err ? std::string("Context err is: ") + context->errstr : std::string()) + " Reply error is: " + (reply == nullptr ? "Reply is null. Please replort a bug." : std::string(reply->str, reply->len));
                case Error::TOO_LONG_COMMAND :
                    return "Command was to long to perform. " + (context->err ? std::string("Context err is: ") + context->errstr : std::string()) + " Reply error is: " + (reply == nullptr ? "Reply is null. Please replort a bug." : std::string(reply->str, reply->len));
                default:
                    redis_assert_unreachable();
                    return "";
            }
        }

        bool has_prefix() { return !connection_param.prefix.empty(); }

        unsigned int get_version() { return redis_version; }

        Id get_id() { return id; }

        bool is_available() {
            return available;
        }

        std::string get_error() {
            if (err == Error::NONE) {
                return "";
            }
            if (prev_err == Error::NONE) {
                return get_error_str(get_errno(), context.get(), reply.get());
            }
            return get_error_str(get_errno(), context.get(), reply.get()) + " Previous error: " + get_error_str(prev_err, context.get(), reply.get());
        }

        Error get_errno() {
            return err;
        }

        bool reconnect() {
            rediscpp_debug(LL::NOTICE, "Reconnecting");
            struct timeval timeout;
            timeout.tv_sec = static_cast<time_t>(connection_param.connect_timeout_ms / 1000);
            timeout.tv_usec = static_cast<suseconds_t>((connection_param.connect_timeout_ms % 1000) * 1000);
            context.reset(redisConnectWithTimeout(connection_param.host.c_str(), connection_param.port, timeout));
            if (context == nullptr) {
                set_error(Error::CONTEXT_IS_NULL);
                available = false;
            }
            else {
                set_error_from_context(true);
                timeout.tv_sec = static_cast<time_t>(connection_param.operation_timeout_ms / 1000);
                timeout.tv_usec = static_cast<suseconds_t>((connection_param.operation_timeout_ms % 1000) * 1000);
                redisSetTimeout(context.get(), timeout);
                available = (err == Error::NONE);
            }
            if(!available) {
                rediscpp_debug(LL::WARNING, "Not available. reason:" << get_error());
            }
            if(available && !connection_param.password.empty()) {
                //TODO: AUTH
            }
            if(available && connection_param.db_num != 0) {
                bool old_val = connection_param.reconnect_on_failure;
                //as it's internally performs command we don't need reconnect
                connection_param.reconnect_on_failure = false;
                available = select(connection_param.db_num);
                connection_param.reconnect_on_failure = old_val;
                if(!available) {
                    rediscpp_debug(LL::WARNING, "Could not select DB: " << get_error());
                }
            }
            return available;
        }

        Key add_prefix_to_key(const Key& key) {
            return has_prefix() ? (connection_param.prefix + key) : key;
        }
        Key add_prefix_to_key(const char* key) {
            return has_prefix() ? (connection_param.prefix + key) : key;
        }
        inline void done() { used = false; }
        inline void set_used() { used = true; }
        inline bool is_used() { return used; }
        void update_param(const ConnectionParam& new_param) {
            assert(connection_param.host == new_param.host);
            assert(connection_param.port == new_param.port);
            if(new_param.db_num != connection_param.db_num) {
                select(new_param.db_num);
            }
            connection_param = new_param;
        }

        void set_error_from_context(bool never_thow = false) {
            prev_err = (err == Error::NONE ? prev_err : err);
            switch (context->err) {
                case 0:
                    err = Error::NONE;
                    break;
                case REDIS_ERR_EOF:
                    err = Error::HIREDIS_EOF;
                    break;
                case REDIS_ERR_IO:
                    err = Error::HIREDIS_IO;
                    break;
                case REDIS_ERR_OOM:
                    err = Error::HIREDIS_OOM;
                    break;
                case REDIS_ERR_PROTOCOL:
                    err = Error::HIREDIS_PROTOCOL;
                    break;
                case REDIS_ERR_OTHER:
                    err = Error::HIREDIS_OTHER;
                    break;
                default:
                    err = Error::HIREDIS_UNKNOWN;
                    break;
            }
            if(err != Error::NONE) {
                rediscpp_debug(LL::WARNING, __FUNCTION__ << ": error is " << get_error_str(err, context.get(), reply.get()));
                if (!never_thow && err != Error::NONE && connection_param.throw_on_error) {
                    throw Redis::Exception(get_error_str(err, context.get(), reply.get()));
                }
            }
        }

        inline void set_error(Error _err, bool never_throw = false) {
            prev_err = (err == Error::NONE ? prev_err : err);
            err = _err;
            if(! never_throw && err != Error::NONE && connection_param.throw_on_error) {
                throw Redis::Exception(get_error_str(err, context.get(), reply.get()));
            }
        }

        inline bool check_available() {
            if (!is_available() && connection_param.reconnect_on_failure) {
                reconnect();
            }
            if (!is_available()) {
                if (connection_param.throw_on_error) {
                    throw Redis::Exception(get_error());
                }
                return false;
            }
            return true;
        }


        bool run_command(std::function<void*()> callback) {
            if (!connected) {
                connected = true;
                if (reconnect()) {
                    rediscpp_debug(LogLevel::NOTICE, __FUNCTION__ << ": Reconnect done");
                    fetch_version();
                }
                else {
                    rediscpp_debug(LogLevel::NOTICE, __FUNCTION__ << ": Reconnect failed");
                }
            }
            if (!check_available()) {
                return false;
            }
            reply.reset(static_cast<redisReply*>(callback()));
            if(reply.get() == nullptr) {
                rediscpp_debug(LL::WARNING, "Got NULL reply");
            }
            else if(context->err) {
                rediscpp_debug(LL::WARNING, "Error on context: " << context->err);
            }
            if((reply.get() == nullptr || context->err) && connection_param.reconnect_on_failure) {
                rediscpp_debug(LL::NOTICE, "Reconnecting for command");
                reconnect();
                if (!is_available()) {
                    if(connection_param.throw_on_error) {
                        throw Redis::Exception(get_error());
                    }
                    return false;
                }
                reply.reset(static_cast<redisReply*>(callback()));
            }

            set_error_from_context();
            if(err != Error::NONE) {
                return false;
            }
            //This actually should never happen. But who knows.
            if(reply.get() == nullptr) {
                prev_err = (err == Error::NONE ? prev_err : err);
                err = Error::REPLY_IS_NULL;
                if(connection_param.throw_on_error) {
                    throw Redis::Exception(get_error());
                }
                return false;
            }
            if(reply.get()->type == REDIS_REPLY_ERROR) {
                set_error(Error::REPLY_ERR);
                return false;
            }
            return true;
        }

        bool run_command(const std::vector<const char*>& commands, const std::vector<size_t>& sizes) {
            auto callback = [&](){
                redis_assert(!commands.empty());
                rediscpp_debug(LL::NOTICE, connection_param.host << ":" << connection_param.port << ":" << connection_param.db_num << " : " << "Command(first member): " << commands[0] );
                return redisCommandArgv(context.get(), static_cast<int>(commands.size()), const_cast<const char**>(commands.data()), sizes.data());
            };
            return run_command(callback);
        }
        bool run_command(const char* format, va_list ap) {
            auto callback = [&]() {
                va_list copy;
                va_copy(copy, ap);
                rediscpp_debug(LL::NOTICE, connection_param.host << ":" << connection_param.port << ":" << connection_param.db_num << " : " << "Command: " << format );
                void* ret = redisvCommand(context.get(), format, ap);
                va_end(copy);
                return ret;
            };

            return run_command(callback);
        }
        bool run_command(const char* format, ...) {
            va_list ap;
            va_start(ap,format);
            bool ret = run_command(format,ap);
            va_end(ap);
            return ret;
        }
        bool info(const Key& section, Key& info_data) {
            if(!section.empty() && redis_version < 20600) {
                set_error(Error::COMMAND_UNSUPPORTED);
                return false;
            }
            if(section.empty()) {
                if(run_command("INFO", section.c_str())) {
                    info_data.assign(reply->str, reply->len);
                    return true;
                }
                return false;
            }
            if(run_command("INFO %s", section.c_str())) {
                info_data.assign(reply->str, reply->len);
                return true;
            }
            return false;
        }

        bool fetch_version() {
            Key info_data;
            if(!info("", info_data)) {
                return false;
            }
            auto pos = info_data.find("redis_version:");
            if(pos == std::string::npos) {
                prev_err = (err == Error::NONE ? prev_err : err);
                err = Error::UNEXPECTED_INFO_RESULT;
                return false;
            }
            auto major_pos = info_data.find('.', pos+14);// 14 = length("redis_version:")
            if(major_pos == std::string::npos) {
                prev_err = (err == Error::NONE ? prev_err : err);
                err = Error::UNEXPECTED_INFO_RESULT;
                return false;
            }
            unsigned int major = std::stoi(info_data.substr(pos+14, major_pos-pos-14));
            auto minor_pos = info_data.find('.', major_pos+1);
            if(minor_pos == std::string::npos) {
                prev_err = (err == Error::NONE ? prev_err : err);
                err = Error::UNEXPECTED_INFO_RESULT;
                return false;
            }
            unsigned int minor = std::stoi(info_data.substr(major_pos+1, minor_pos - major_pos -1));
            auto patch_level_pos = info_data.find('\n', minor_pos+1);
            if(patch_level_pos == std::string::npos) {
                prev_err = (err == Error::NONE ? prev_err : err);
                err = Error::UNEXPECTED_INFO_RESULT;
                return false;
            }
            unsigned int patch_level = std::stoi(info_data.substr(minor_pos+1, minor_pos - patch_level_pos -1));
            redis_version = major * 10000 + minor * 100 + patch_level;
            return true;
        }

        bool select(long long db_num) {
            return run_command("SELECT %lli", db_num);
        }

        template <class KeyContainer>
        bool set(const KeyContainer& keys, const KeyContainer& values, Connection::SetType set_type, bool& was_set) {
            size_t sz = keys.size();
            redis_assert(sz == values.size());
            std::vector<size_t> sizes(sz*2+1);
            std::vector<const char*> command_parts_c_strings(sz*2+1);
            if(set_type == Connection::SetType::IF_EXIST) {
                set_error(Error::COMMAND_UNSUPPORTED);
            }
            else if(set_type == Connection::SetType::IF_NOT_EXIST) {
                command_parts_c_strings[0] = "MSETNX";
                sizes[0] = 6;
            }
            else {
                command_parts_c_strings[0] = "MSET";
                sizes[0] = 4;
            }
            KeyVec prefixed_keys;
            if(has_prefix()) {
                for(size_t i=0; i<keys.size(); i++) {
                    command_parts_c_strings[2*i+1] = static_cast<const Key&>(keys[i]).c_str();
                    command_parts_c_strings[2*i+2] = static_cast<const Key&>(values[i]).c_str();
                    sizes[2*i+1] = static_cast<const Key&>(keys[i]).size();
                    sizes[2*i+2] = static_cast<const Key&>(values[i]).size();
                }
            }
            else {
                for(size_t i=0; i<keys.size(); i++) {
                    prefixed_keys.push_back(add_prefix_to_key(keys[i]));
                    command_parts_c_strings[2*i+1] = prefixed_keys[i].c_str();
                    command_parts_c_strings[2*i+2] = static_cast<const Key&>(values[i]).c_str();
                    sizes[2*i+1] = prefixed_keys[i].size();
                    sizes[2*i+2] = static_cast<const Key&>(values[i]).size();
                }
            }
            if(run_command(command_parts_c_strings, sizes)) {
                was_set = (reply->integer == 1);
                return true;
            }
            return false;
        }
        /* Set the string value of a key */
        bool set(const char* key, size_t key_size, const char* value, size_t value_size, SetType set_type, bool& was_set, long long expire, ExpireType expire_type) {
            Key key_s;
            if(has_prefix()) {
                key_s = add_prefix_to_key(key);
                key = key_s.c_str();
                key_size = key_s.size();
            }
            if(redis_version >= 20612) {
                bool ret;
                if(expire_type == ExpireType::SEC) {
                    if(set_type == SetType::IF_EXIST) {
                        ret = run_command("SET %b %b EX %lli XX", key, key_size, value, value_size, expire);
                    }
                    else if(set_type == SetType::IF_NOT_EXIST) {
                        ret = run_command("SET %b %b EX %lli NX", key, key_size, value, value_size, expire);
                    }
                    else {
                        ret = run_command("SET %b %b EX %lli", key, key_size, value, value_size, expire);
                    }
                }
                else if(expire_type == ExpireType::MSEC) {
                    if(set_type == SetType::IF_EXIST) {
                        ret = run_command("SET %b %b PX %lli XX", key, key_size, value, value_size, expire);
                    }
                    else if(set_type == SetType::IF_NOT_EXIST) {
                        ret = run_command("SET %b %b PX %lli NX", key, key_size, value, value_size, expire);
                    }
                    else {
                        ret = run_command("SET %b %b PX %lli", key, key_size, value, value_size, expire);
                    }
                }
                else {
                    if(set_type == SetType::IF_EXIST) {
                        ret = run_command("SET %b %b XX", key, key_size, value, value_size);
                    }
                    else if(set_type == SetType::IF_NOT_EXIST) {
                        ret = run_command("SET %b %b NX", key, key_size, value, value_size);
                    }
                    else {
                        ret = run_command("SET %b %b", key, key_size, value, value_size);
                    }
                }
                if(ret) {
                    was_set = reply->type != REDIS_REPLY_NIL;
                    return true;
                }
                return false;
            }
            else {
                if(
                        ((expire_type == ExpireType::SEC ||
                                expire_type == ExpireType::MSEC)
                                &&
                                set_type == SetType::IF_NOT_EXIST)
                                ||
                                set_type == SetType::IF_EXIST
                        ) {
                    //TODO: We can make a multi analogue
                    set_error(Error::COMMAND_UNSUPPORTED);
                    return false;
                }
                else if(expire_type == ExpireType::MSEC) {
                    if(run_command("PSETEX %b %lli %b", key, key_size, expire, value, value_size)) {
                        was_set = true;
                        return true;
                    }
                    return false;
                }
                else if(expire_type == ExpireType::SEC) {
                    if(run_command("SETEX %b %lli %b", key, key_size, expire, value, value_size)) {
                        was_set = true;
                        return true;
                    }
                    return false;
                }
                else if(set_type == SetType::IF_NOT_EXIST) {
                    if(run_command("SETNX %b %b", key, key_size, value, value_size)) {
                        was_set = reply->integer != 0;
                        return true;
                    }
                    return false;
                }
                else if(set_type == SetType::ALWAYS) {
                    if(run_command("SET %b %b", key, key_size, value, value_size)) {
                        was_set = true;
                        return true;
                    }
                    return false;
                }
                else {
                    redis_assert_unreachable();
                }
                return false;
            }
        }
        template <class KeyContainer>
        bool bitop(BitOperation operation, const Key& destkey, const KeyContainer& keys, long long& size_of_dest) {
            std::vector<size_t> sizes(3);
            std::vector<const char*> command_parts_c_strings(3);
            command_parts_c_strings[0] = "BITOP";
            sizes[0] = 5;
            switch (operation) {
                case BitOperation::AND:
                    command_parts_c_strings[1] = "AND";
                    sizes[1] = 3;
                    break;
                case BitOperation::OR:
                    command_parts_c_strings[1] = "OR";
                    sizes[1] = 2;
                    break;
                case BitOperation::NOT:
                    command_parts_c_strings[1] = "NOT";
                    sizes[1] = 3;
                    redis_assert(keys.size() == 1);
                    break;
                case BitOperation::XOR:
                    command_parts_c_strings[1] = "XOR";
                    sizes[1] = 3;
                    break;
                default: //Fool proof of explicit casting BitOperation to integer and asigning improper value;
                    redis_assert_unreachable();
                    break;
            }
            const Key& prefixed_destkey = add_prefix_to_key(destkey);
            command_parts_c_strings[2] = prefixed_destkey.c_str();
            sizes[2] = prefixed_destkey.size();

            KeyVec prefixed_keys;
            append_c_strings_with_prefixes_and_sizes(keys, prefixed_keys, command_parts_c_strings, sizes);
            if(run_command(command_parts_c_strings, sizes)) {
                size_of_dest = reply->integer;
                return true;
            }
            return false;
        }
        bool run_set_command(const char* comm, size_t comm_sz, const KeyVec& keys, KeyVec& result) {
            std::vector<size_t> sizes(1);
            std::vector<const char*> command_parts_c_strings(1);
            command_parts_c_strings[0] = comm;
            sizes[0] = comm_sz;
            KeyVec prefixed_keys;
            append_c_strings_with_prefixes_and_sizes(keys, prefixed_keys, command_parts_c_strings, sizes);
            if(run_command(command_parts_c_strings, sizes)) {
                result.clear();
                redis_assert(reply->type == REDIS_REPLY_ARRAY);
                for(size_t i=0; i < reply->elements; i++) {
                    redis_assert(reply->element[i]->type == REDIS_REPLY_STRING);
                    result.push_back(std::string(reply->element[i]->str, reply->element[i]->len));
                }
                return true;
            }
            return false;
        }

        bool run_set_store_command(const char* comm, size_t comm_sz, const Key& destination, const KeyVec& keys, long long& number_of_elements) {
            std::vector<size_t> sizes(2);
            std::vector<const char*> command_parts_c_strings(2);
            command_parts_c_strings[0] = comm;
            sizes[0] = comm_sz;
            const Key& prefixed_dest = add_prefix_to_key(destination);
            command_parts_c_strings[1] = prefixed_dest.c_str();
            sizes[1] = prefixed_dest.size();
            KeyVec prefixed_keys;
            append_c_strings_with_prefixes_and_sizes(keys, prefixed_keys, command_parts_c_strings, sizes);
            if(run_command(command_parts_c_strings, sizes)) {
                redis_assert(reply->type == REDIS_REPLY_INTEGER);
                number_of_elements = reply->integer;
            }
            return false;
        }
    };
    std::atomic_long Connection::Implementation::id_counter(0);
    std::atomic_ulong Connection::Implementation::connection_count(0);

    Connection::Connection(const ConnectionParam &connection_param) :
        d(new Connection::Implementation(connection_param))
    {}

    Connection::Connection(const std::string &host,
            unsigned int port,
            const std::string& password,
            unsigned int db_num,
            const std::string &prefix,
            unsigned int connect_timeout_ms,
            unsigned int operation_timeout_ms,
            bool reconnect_on_failure,
            bool throw_on_error
    ) :
        d(new Connection::Implementation(ConnectionParam(host, port, password, db_num, prefix, connect_timeout_ms, operation_timeout_ms, reconnect_on_failure, throw_on_error)))
    {}
    Connection::~Connection() {
        if(d != nullptr) {
            delete d;
        }
    }
    bool Connection::is_available() {
        return d->is_available();
    }
    std::string Connection::get_error() {
        return d->get_error();
    }
    Connection::Error Connection::get_errno() {
        return d->get_errno();
    }
    unsigned int Connection::get_version() {
        return d->get_version();
    }
    Connection::Id Connection::get_id() {
        return d->get_id();
    }
    size_t Connection::get_connection_count() {
        return Implementation::connection_count.load(std::memory_order_relaxed);
    }
    bool Connection::fetch_get_result(Key& result, size_t index) {
        if( index >= d->reply->elements ) {
            return false;
        }
        if(d->reply->element[index]->type == REDIS_REPLY_STRING) {
            result.assign(d->reply->element[index]->str);
        }
        else if (d->reply->element[index]->type == REDIS_REPLY_NIL) {
            result.clear();
        }
        else {
            redis_assert_unreachable();
        }
        return true;
    }

    /*********************** string commands ***********************/

    /* Append a value to a key */
    bool Connection::append(const Key& key, const Key& value, long long& result_length) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("APPEND %b %b", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size())) {
            result_length = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Append a value to a key */
    bool Connection::append(const Key& key, const Key& value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("APPEND %b %b", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size());
    }

    /* Count set bits in a string */
    bool Connection::bitcount(const Key& key, unsigned int start, unsigned int end, long long& result) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("BITCOUNT %b %u %u", prefixed_key.c_str(), prefixed_key.size(), start, end)) {
            result = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Count set bits in a string */
    bool Connection::bitcount(const Key& key, long long& result) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("BITCOUNT %b", prefixed_key.c_str(), prefixed_key.size())) {
            result = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Perform bitwise operations between strings */
    bool Connection::bitop(BitOperation operation, const Key& destkey, const StringKeyHolder& keys, long long& size_of_dest) {
        return d->bitop(operation, destkey, keys, size_of_dest);
    }

    /* Perform bitwise operations between strings */
    bool Connection::bitop(BitOperation operation, const Key& destkey, const StringKeyHolder& keys) {
        long long res;
        return bitop(operation, destkey, keys, res);
    }

    bool Connection::bit_not(const Key& destkey, const Key& key, long long& size_of_dest) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        const Key& prefixed_destkey = d->add_prefix_to_key(destkey);
        if(d->run_command("BITOP NOT %b %b", prefixed_destkey.c_str(), prefixed_destkey.size(), prefixed_key.c_str(), prefixed_key.size())) {
            size_of_dest = d->reply->integer;
            return true;
        }
        return false;
    }

    bool Connection::bit_not(const Key& destkey, const Key& key) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        const Key& prefixed_destkey = d->add_prefix_to_key(destkey);
        return d->run_command("BITOP NOT %b %b", prefixed_destkey.c_str(), prefixed_destkey.size(), prefixed_key.c_str(), prefixed_key.size());
    }

    /* Find first bit set or clear in a subsstring defined by start and end*/
    bool Connection::bitpos(const Key& key, Bit bit, unsigned int start, unsigned int end, long long& result) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("BITPOS %b %u %u %u", prefixed_key.c_str(), prefixed_key.size(), bit == Bit::ONE ? 1 : 0, start, end)) {
            result = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Find first bit set or clear in a subsstring defined by start and end*/
    bool Connection::bitpos(const Key& key, Bit bit, unsigned int start, long long& result) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("BITPOS %b %u %u", prefixed_key.c_str(), prefixed_key.size(), bit == Bit::ONE ? 1 : 0, start)) {
            result = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Find first bit set or clear in a string */
    bool Connection::bitpos(const Key& key, Bit bit, long long& result) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("BITPOS %b %u", prefixed_key.c_str(), prefixed_key.size(), bit == Bit::ONE ? 1 : 0)) {
            result = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Decrement the integer value of a key by one */
    bool Connection::decr(const Key& key, long long& result_value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("DECR %b", prefixed_key.c_str(), prefixed_key.size())) {
            result_value = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Decrement the integer value of a key by one */
    bool Connection::decr(const Key& key) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("DECR %b", prefixed_key.c_str(), prefixed_key.size());
    }

    /* Decrement the integer value of a key by the given number */
    bool Connection::decrby(const Key& key, long long decrement, long long& result_value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("DECRBY %b %lli", prefixed_key.c_str(), prefixed_key.size(), decrement)) {
            result_value = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Decrement the integer value of a key by the given number */
    bool Connection::decrby(const Key& key, long long decrement) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("DECRBY %b %lli", prefixed_key.c_str(), prefixed_key.size(), decrement);
    }

    /* Get the value of a key */
    bool Connection::get(const Key& key, Connection::Key& result) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("GET %b", prefixed_key.c_str(), prefixed_key.size())) {
            result.assign(d->reply->str, d->reply->len);
            return true;
        }
        return false;
    }

    /* Get the value of multiple keys */
    bool Connection::get(const StringKeyHolder& keys, StringValueHolder&& result) {
        std::vector<size_t> sizes(1);
        std::vector<const char*> command_parts_c_strings(1);
        command_parts_c_strings[0] = "MGET";
        sizes[0] = 4;
        KeyVec prefixed_keys;
        d->append_c_strings_with_prefixes_and_sizes(keys, prefixed_keys, command_parts_c_strings, sizes);
        if(d->run_command(command_parts_c_strings, sizes)) {
            redis_assert(d->reply->elements == keys.size());
            for(size_t index=0; index < d->reply->elements; index++) {
                if(d->reply->element[index]->type == REDIS_REPLY_STRING) {
                    result.push_back(d->reply->element[index]->str);
                }
                else if (d->reply->element[index]->type == REDIS_REPLY_NIL) {
                    result.push_back("");
                }
                else {
                    redis_assert_unreachable();
                }
            }
            return true;
        }
        return false;
    }

    bool Connection::get(StringKVHolder&& vals) {
        return get(vals.k, std::move(vals.v));
    }

    /* Returns the bit value at offset in the string value stored at key */
    bool Connection::getbit(const Key& key, long long offset, Bit& result) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("GETBIT %b %li", prefixed_key.c_str(), prefixed_key.size(), offset)) {
            result = d->reply->integer == 0 ? Bit::ZERO : Bit::ONE;
            return true;
        }
        return false;
    }

    /* Get a substring of the string stored at a key */
    bool Connection::getrange(const Key& key, long long start, long long end, Connection::Key& result) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("GETRANGE %b %lli %lli", prefixed_key.c_str(), prefixed_key.size(), start, end)) {
            result.assign(d->reply->str, d->reply->len);
            return true;
        }
        return false;
    }

    /* Set the string value of a key and return its old value */
    bool Connection::getset(const Key& key, const Key& value, Connection::Key& old_value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("GETSET %b %b", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size())) {
            old_value.assign(d->reply->str, d->reply->len);
            return true;
        }
        return false;
    }

    /* Increment the integer value of a key by one */
    bool Connection::incr(const Key& key, long long& result_value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("INCR %b", prefixed_key.c_str(), prefixed_key.size())) {
            result_value = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Increment the integer value of a key by one */
    bool Connection::incr(const Key& key) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("INCR %b", prefixed_key.c_str(), prefixed_key.size());
    }

    /* Increment the integer value of a key by the given amount */
    bool Connection::incrby(const Key& key, long long increment, long long& result_value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("INCRBY %b %lli", prefixed_key.c_str(), prefixed_key.size(), increment)) {
            result_value = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Increment the integer value of a key by the given amount */
    bool Connection::incrby(const Key& key, long long increment) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("INCRBY %b %lli", prefixed_key.c_str(), prefixed_key.size(), increment);
    }

    /* Increment the float value of a key by the given amount */
    bool Connection::incrbyfloat(const Key& key, float increment, float& result_value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("INCRBYFLOAT %b %f", prefixed_key.c_str(), prefixed_key.size(), increment)) {
            result_value = strtof(d->reply->str, nullptr);
            if(result_value == 0.0 && errno == ERANGE) {
                d->set_error(Error::FLOAT_OUT_OF_RANGE);
                return false;
            }
            return true;
        }
        return false;
    }

    /* Increment the float value of a key by the given amount */
    bool Connection::incrbyfloat(const Key& key, double increment, double& result_value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("INCRBYFLOAT %b %f", prefixed_key.c_str(), prefixed_key.size(), increment)) {
            result_value = strtod(d->reply->str, nullptr);
            if(result_value == 0.0 && errno == ERANGE) {
                d->set_error(Error::DOUBLE_OUT_OF_RANGE);
                return false;
            }
            return true;
        }
        return false;
    }

    /* Increment the float value of a key by the given amount */
    bool Connection::incrbyfloat(const Key& key, float increment) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("INCRBYFLOAT %b %f", prefixed_key.c_str(), prefixed_key.size(), increment);
    }

    /* Increment the float value of a key by the given amount */
    bool Connection::incrbyfloat(const Key& key, double increment) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("INCRBYFLOAT %b %f", prefixed_key.c_str(), prefixed_key.size(), increment);
    }


    bool Connection::set(const StringKeyHolder& keys, const StringKeyHolder& values, SetType set_type) {
        bool was_set;
        return d->set(keys, values, set_type, was_set);
    }

    bool Connection::set(StringKKHolder&& kk, SetType set_type) {
        bool was_set;
        return d->set(kk.k1, kk.k2, set_type, was_set);
    }
    /* Set the string value of a key */
    bool Connection::set(const Key& key, const Key& value, SetType set_type, long long expire_val, ExpireType expire_type) {
        bool was_set;
        return d->set(key.c_str(),key.size(), value.c_str(), value.size(), set_type, was_set, expire_val, expire_type);
    }

    bool Connection::set(const Key& key, const Key& value, SetType set_type, bool& was_set, long long expire_val, ExpireType expire_type) {
        return d->set(key.c_str(),key.size(), value.c_str(), value.size(), set_type, was_set, expire_val, expire_type);
    }
    /* Set the string value of a key */
    bool Connection::set(const char* key, const char* value, SetType set_type, bool& was_set, long long expire_val, ExpireType expire_type) {
        return d->set(key, std::strlen(key), value, std::strlen(value), set_type, was_set, expire_val, expire_type);
    }
    bool Connection::set(const char* key, const char* value, SetType set_type, long long expire_val, ExpireType expire_type) {
        bool was_set;
        return d->set(key, std::strlen(key), value, std::strlen(value), set_type, was_set, expire_val, expire_type);
    }

    /* Sets or clears the bit at offset in the string value stored at key */
    bool Connection::set_bit(const Key& key,long long offset, Bit value, Bit& original_bit) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if (d->run_command("SETBIT %b %lli %u", prefixed_key.c_str(), prefixed_key.size(), offset, value == Bit::ZERO ? 0 : 1)) {
            original_bit = d->reply->integer == 0 ? Bit::ZERO : Bit::ONE;
            return true;
        }
        return false;
    }

    /* Sets or clears the bit at offset in the string value stored at key */
    bool Connection::set_bit(const Key& key,long long offset, Bit value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("SETBIT %b %lli %u", prefixed_key.c_str(), prefixed_key.size(), offset, value == Bit::ZERO ? 0 : 1);
    }

    /* Overwrite part of a string at key starting at the specified offset */
    bool Connection::setrange(const Key& key,long long offset, const Key& value, long long& result_length) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if (d->run_command("SETRANGE %b %lli %b", prefixed_key.c_str(), prefixed_key.size(), offset, value.c_str(), value.size())) {
            result_length = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Overwrite part of a string at key starting at the specified offset */
    bool Connection::setrange(const Key& key,long long offset, const Key& value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("SETRANGE %b %lli %b", prefixed_key.c_str(), prefixed_key.size(), offset, value.c_str(), value.size());
    }

    /* Get the length of the value stored in a key */
    bool Connection::strlen(const Key& key, long long& key_length) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if (d->run_command("STRLEN %b", prefixed_key.c_str(), prefixed_key.size())) {
            key_length = d->reply->integer;
            return true;
        }
        return false;
    }

    /*********************** server commands ***********************/
    /* Asynchronously rewrite the append-only file */
    bool Connection::bgrewriteaof() {
        return d->run_command("BGREWRITEAOF");
    }

    /* Asynchronously save the dataset to disk */
    bool Connection::bgsave() {
        return d->run_command("BGSAVE");
    }

    /* Kill the connection of a client */
    bool Connection::client_kill(const Key& ip, long long port) {
        std::string ip_and_port(ip+':'+std::to_string(port));
        return d->run_command("CLIENT KILL %b", ip_and_port.c_str(), ip_and_port.size());
    }

    /* Get the list of client connections */
    //bool Connection::client_list(); //TODO : implement

//    /* Get the current connection name */
//    //bool Connection::client getname(); //TODO : implement
//
//    /* Stop processing commands from clients for some time */
//    //bool Connection::client pause(VAL timeout); //TODO : implement
//
//    /* Set the current connection name */
//    //bool Connection::client setname(VAL connection-name); //TODO : implement
//
//    /* Get the value of a configuration parameter */
//    bool Connection::config_get(const Key& parameter, Connection::Key& result);
//
//    /* Rewrite the configuration file with the in memory configuration */
//    bool Connection::config_rewrite();
//
//    /* Set a configuration parameter to the given value */
//    bool Connection::config_set(const Key& parameter, const Key& value);
//
//    /* Reset the stats returned by INFO */
//    bool Connection::config_resetstat();
//
//    /* Return the number of keys in the selected database */
//    bool Connection::dbsize(long long* result);
//
//    /* Get debugging information about a key */
//    bool Connection::debug_object(const Key& key, Connection::Key& info);
//
//    /* Make the server crash */
//    bool Connection::debug_segfault();
//
//    /* Remove all keys from all databases */
//    bool Connection::flushall();
//
//    /* Remove all keys from the current database */
//    bool Connection::flushdb();
//
    /* Get information and statistics about the server */
    bool Connection::info(const Key& section, Connection::Key& info_data) {
        return d->info(section, info_data);
    }

    /* Get information and statistics about the server */
    bool Connection::info(Key& info_data) {
        return d->info("", info_data);
    }

//    /* Get the UNIX time stamp of the last successful save to disk */
//    bool Connection::lastsave(time_t& result);
//
//    /* Synchronously save the dataset to disk */
//    bool Connection::save();
//
//    /* Synchronously save the dataset to disk and then shut down the server */
//    bool Connection::shutdown(bool save = true);
//
//    /* Make the server a slave of another instance*/
//    bool Connection::slaveof(const Key& host, unsigned int port);
//
//    /* Promote server as master */
//    bool Connection::make_master();
//
//    /* Manages the Redis slow queries log */
//    //bool Connection::slowlog(VAL subcommand, bool argument = false); //TODO : implement
//
//    /* Return the current server time */
//    bool Connection::time(long long& seconds, long long& microseconds);
//
//
//    /*********************** list commands ***********************/
//    /* Remove and get the first element in a list, or block until one is available */
//    bool Connection::blpop(const KeyVec& keys, long long timeout, Connection::Key& chosen_key, Connection::Key& value);
//
//    /* Remove and get the last element in a list, or block until one is available */
//    bool Connection::brpop(const KeyVec& keys, long long timeout, Connection::Key& chosen_key, Connection::Key& value);
//
//    /* Pop a value from a list, push it to another list and return it; or block until one is available */
//    bool Connection::brpoplpush(const Key& source, const Key& destination, long long timeout, Connection::Key& result);
//
//    /* Pop a value from a list, push it to another list and return it; or block until one is available */
//    bool Connection::brpoplpush(const Key& source, const Key& destination, long long timeout);
//
//    /* Get an element from a list by its index */
//    bool Connection::lindex(const Key& key, long long index);
//
//    enum class ListInsertType {
//        AFTER,
//        BEFORE
//    };
//
//    /* Insert an element before or after another element in a list */
//    bool Connection::linsert(const Key& key, ListInsertType insert_type, const Key& pivot, const Key& value);
//
//    /* Insert an element before or after another element in a list */
//    bool Connection::linsert(const Key& key, ListInsertType insert_type, const Key& pivot, const Key& value, long long& list_size);
//
//    /* Get the length of a list */
//    bool Connection::llen(const Key& key, long long& length);
//
//    /* Remove and get the first element in a list */
//    bool Connection::lpop(const Key& key, Connection::Key& value);
//
//    /* Remove and get the first element in a list */
//    bool Connection::lpop(const Key& key);
//
//    /* Prepend one or multiple values to a list */
//    bool Connection::lpush(const Key& key, const Key& value, long long& list_length);
//
//    /* Prepend one or multiple values to a list */
//    bool Connection::lpush(const Key& key, const Key& value);
//
//    /* Prepend one or multiple values to a list */
//    bool Connection::lpush(const Key& key, const KeyVec& values, long long& list_length);
//
//    /* Prepend one or multiple values to a list */
//    bool Connection::lpush(const Key& key, const KeyVec& values);
//
//    /* Prepend a value to a list, only if the list exists */
//    bool Connection::lpushx(const Key& key, const Key& value, long long& list_length);
//
//    /* Prepend a value to a list, only if the list exists */
//    bool Connection::lpushx(const Key& key, const Key& value);


    /* Get a range of elements from a list */
//        bool lrange(const Key& key, VAL start, VAL stop);

    /* Remove elements from a list */
//        bool lrem(const Key& key, VAL count, VAL value);

    /* Set the value of an element in a list by its index */
//        bool lset(const Key& key, VAL index, VAL value);

    /* Trim a list to the specified range */
//        bool ltrim(const Key& key, VAL start, VAL stop);

    /* Remove and get the last element in a list */
//        bool rpop(const Key& key);

    /* Remove the last element in a list, append it to another list and return it */
//        bool rpoplpush(VAL source, VAL destination);

    /* Append one or multiple values to a list */
//        bool rpush(const Key& key, VAL value [value ...]);

    /* Append a value to a list, only if the list exists */
//        bool rpushx(const Key& key, VAL value);


    /*********************** generic commands ***********************/
    /* Delete a key */
        bool Connection::del(const Key& key) {
            const Key& prefixed_key = d->add_prefix_to_key(key);
            return d->run_command("DEL %b", prefixed_key.c_str(), prefixed_key.size());
        }

    /* Return a serialized version of the value stored at the specified key. */
//        bool dump(const Key& key);

    /* Determine if a key exists */
//        bool exists(const Key& key);

    /* Set a key's time to live in seconds */
    bool Connection::expire(const Key& key, long long expire_time, ExpireType expire_type) {
        bool was_set;
        return expire(key, expire_time, was_set, expire_type);
    }
    bool Connection::expire(const Key& key, long long expire_time, bool& was_set, ExpireType expire_type) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(expire_type == ExpireType::SEC) {
            if(d->run_command("EXPIRE %b %lli", prefixed_key.c_str(), prefixed_key.size(), expire_time)) {
                was_set = d->reply->integer != 0;
                return true;
            }
            return false;
        }
        else if (expire_type == ExpireType::MSEC) {
            if(d->run_command("PEXPIRE %b %lli", prefixed_key.c_str(), prefixed_key.size(), expire_time)) {
                was_set = d->reply->integer != 0;
                return true;
            }
            return false;
        }
        else {
            redis_assert_unreachable();
        }
        return false;
    }

    /* Set the expiration for a key as a UNIX timestamp */
    bool Connection::expireat(const Key& key, long long expire_time, ExpireType expire_type) {
        bool was_set;
        return expireat(key, expire_time, was_set, expire_type);
    }

    bool Connection::expireat(const Key& key, long long expire_time, bool& was_set, ExpireType expire_type) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(expire_type == ExpireType::SEC) {
            if(d->run_command("EXPIREAT %b %lli", prefixed_key.c_str(), prefixed_key.size(), expire_time)) {
                was_set = d->reply->integer != 0;
                return true;
            }
            return false;
        }
        else if (expire_type == ExpireType::MSEC) {
            if(d->run_command("PEXPIRE %b %lli", prefixed_key.c_str(), prefixed_key.size(), expire_time)) {
                was_set = d->reply->integer != 0;
                return true;
            }
            return false;
        }
        else {
            redis_assert_unreachable();
        }
        return false;
    }

    /* Find all keys matching the given pattern */
//        bool keys(VAL pattern);

    /* Atomically transfer a key from a Redis instance to another one. */
//        bool migrate(VAL host, VAL port, const Key& key, VAL destination-db, VAL timeout, bool copy = false, bool replace = false);

    /* Move a key to another database */
//        bool move(const Key& key, VAL db);

    /* Inspect the internals of Redis objects */
//        bool object(VAL subcommand /*, [arguments [arguments ...]] */);

    /* Remove the expiration from a key */
//        bool persist(const Key& key);

    /* Get the time to live for a key in milliseconds */
//        bool pttl(const Key& key);

    /* Return a random key from the keyspace */
//        bool randomkey();

    /* Rename a key */
//        bool rename(const Key& key, VAL newkey);

    /* Rename a key, only if the new key does not exist */
//        bool renamenx(const Key& key, VAL newkey);

    /* Create a key using the provided serialized value, previously obtained using DUMP. */
//        bool restore(const Key& key, VAL ttl, VAL serialized-value);

    /* Sort the elements in a list, set or sorted set */
//        bool sort(const Key& key /*, [BY pattern] */ /*, [LIMIT offset count] */ /*, [GET pattern [GET pattern ...]] */ /*, [ASC|DESC] */, bool alpha = false /*, [STORE destination] */);

    /* Get the time to live for a key */
    bool Connection::ttl(const Key& key, long long& ttl_value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if (d->run_command("TTL %b", prefixed_key.c_str(), prefixed_key.size())) {
            ttl_value = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Determine the type stored at key */
    bool Connection::type(const Key& key, KeyType& key_type) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if (d->run_command("TYPE %b", prefixed_key.c_str(), prefixed_key.size())) {
            if(strcmp(d->reply->str, "none") == 0) {
                key_type = KeyType::NONE;
            }
            else if(strcmp(d->reply->str, "string") == 0) {
                key_type = KeyType::STRING;
            }
            else if(strcmp(d->reply->str, "list") == 0) {
                key_type = KeyType::LIST;
            }
            else if(strcmp(d->reply->str, "set") == 0) {
                key_type = KeyType::SET;
            }
            else if(strcmp(d->reply->str, "zset") == 0) {
                key_type = KeyType::ZSET;
            }
            else if(strcmp(d->reply->str, "hash") == 0) {
                key_type = KeyType::HASH;
            }
            else {
                redis_assert_unreachable();
            }
            return true;
        }
        return false;
    }

    /* Incrementally iterate the keys space */
    bool Connection::scan(unsigned long long& cursor, StringValueHolder&& result_keys, const Key& pattern, long count) {
        bool ret;
        const Key& prefixed_pattern = d->add_prefix_to_key(pattern);
        if(prefixed_pattern != "*") {
            if(count != default_scan_count) {
                ret = d->run_command("SCAN %lli MATCH %b COUNT %lli", cursor, prefixed_pattern.c_str(), prefixed_pattern.size(), count);
            }
            else {
                ret = d->run_command("SCAN %lli MATCH %b", cursor, prefixed_pattern.c_str(), prefixed_pattern.size());
            }
        }
        else if(count != default_scan_count) {
            ret = d->run_command("SCAN %lli COUNT %lli", cursor, count);
        }
        else {
            ret = d->run_command("SCAN %lli", cursor);
        }
        if(ret) {
            redis_assert(d->reply->elements == 2);
            redis_assert(d->reply->element[0]->type == REDIS_REPLY_STRING);
            redis_assert(d->reply->element[1]->type == REDIS_REPLY_ARRAY);
            cursor = std::stoull(d->reply->element[0]->str);
            for(size_t i=0; i < d->reply->element[1]->elements; i++) {
                redis_assert(d->reply->element[1]->element[i]->type == REDIS_REPLY_STRING);
                redis_assert(static_cast<size_t>(d->reply->element[1]->element[i]->len) >= prefixed_pattern.size());
                result_keys.push_back(d->reply->element[1]->element[i]->str+prefixed_pattern.size()-1);
            }
            return true;
        }
        return false;
    }



    /*********************** transactions commands ***********************/
    /* Discard all commands issued after MULTI */
//        bool discard();

    /* Execute all commands issued after MULTI */
//        bool exec();

    /* Mark the start of a transaction block */
//        bool multi();

    /* Forget about all watched keys */
//        bool unwatch();

    /* Watch the given keys to determine execution of the MULTI/EXEC block */
//        bool watch(const KeyVec& keys);


    /*********************** scripting commands ***********************/
    /* Execute a Lua script server side */
//        bool eval(VAL script, VAL numkeys, const KeyVec& keys, VAL arg [arg ...]);

    /* Execute a Lua script server side */
//        bool evalsha(VAL sha1, VAL numkeys, const KeyVec& keys, VAL arg [arg ...]);

    /* Check existence of scripts in the script cache. */
//        bool script exists(VAL script [script ...]);

    /* Remove all the scripts from the script cache. */
//        bool script flush();

    /* Kill the script currently in execution. */
//        bool script kill();

    /* Load the specified Lua script into the script cache. */
//        bool script load(VAL script);


    /*********************** hash commands ***********************/
    /* Delete one or more hash fields */
    bool Connection::hdel(const Key& key) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("HDEL %b", prefixed_key.c_str(), prefixed_key.size());
    }

    bool Connection::hdel(const Key& key, bool& was_removed) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("HDEL %b", prefixed_key.c_str(), prefixed_key.size())) {
            redis_assert(d->reply->type == REDIS_REPLY_INTEGER);
            was_removed = d->reply->integer != 0;
            return true;
        }
        return false;
    }


    /* Determine if a hash field exists */
//        bool hexists(const Key& key, VAL field);

    /* Get the value of a hash field */
    bool Connection::hget(const Key& key, const Key& field, Key& value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("HGET %b %b", prefixed_key.c_str(), prefixed_key.size(), field.c_str(), field.size())) {
            redis_assert(d->reply->type == REDIS_REPLY_STRING || d->reply->type == REDIS_REPLY_NIL);
            if(d->reply->type == REDIS_REPLY_NIL) {
                value.clear();
            }
            else {
                value = std::string(d->reply->str, d->reply->len);
            }
            return true;
        }
        return false;
    }

    /* Get all the fields and values in a hash */
    bool Connection::hgetall(const Key& key, PairHolder<std::string, std::string>&& result) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("HGETALL %b", prefixed_key.c_str(), prefixed_key.size())) {
            redis_assert(d->reply->type == REDIS_REPLY_ARRAY);
            redis_assert(d->reply->elements % 2 ==0);
            for(size_t i=0; i < d->reply->elements; i+=2) {
                redis_assert(d->reply->element[i]->type == REDIS_REPLY_STRING);
                redis_assert(d->reply->element[i+1]->type == REDIS_REPLY_STRING);
                std::string k(d->reply->element[i]->str, d->reply->element[i]->len);
                std::string v(d->reply->element[i+1]->str, d->reply->element[i+1]->len);
                result.push_back(std::make_pair(std::move(k), std::move(v)));
            }
            return true;
        }
        return false;
    }

    /* Increment the integer value of a hash field by the given number */
    bool Connection::hincrby(const Key& key, const Key& field, long long increment) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("HINCRBY %b %b %lli", prefixed_key.c_str(), prefixed_key.size(), field.c_str(), field.size(), increment);
    }

    bool Connection::hincrby(const Key& key, const Key& field, long long increment, long long& result_value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("HINCRBY %b %b %lli", prefixed_key.c_str(), prefixed_key.size(), field.c_str(), field.size(), increment)) {
            redis_assert(d->reply->type == REDIS_REPLY_INTEGER);
            result_value = d->reply->integer;
            return true;
        }
        return false;

    }
    /* Increment the float value of a hash field by the given amount */
    bool Connection::hincrby(const Key& key, const Key& field, double increment) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("HINCRBYFLOAT %b %b %f", prefixed_key.c_str(), prefixed_key.size(), field.c_str(), field.size(), increment);
    }

    bool Connection::hincrby(const Key& key, const Key& field, double increment, double& result_value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("HINCRBYFLOAT %b %b %f", prefixed_key.c_str(), prefixed_key.size(), field.c_str(), field.size(), increment)) {
            redis_assert(d->reply->type == REDIS_REPLY_STRING);
            result_value = std::stod(d->reply->str);
            return true;
        }
        return false;

    }

    /* Get all the fields in a hash */
//        bool hkeys(const Key& key);

    /* Get the number of fields in a hash */
//        bool hlen(const Key& key);

    /* Get the values of all the given hash fields */
//        bool hmget(const Key& key, VAL field [field ...]);

    /* Set multiple hash fields to multiple values */
//        bool hmset(const Key& key, VAL field value [field value ...]);

    /* Set the string value of a hash field */
        bool Connection::hset(const Key& key, const Key& field, const Key& value) {
            const Key& prefixed_key = d->add_prefix_to_key(key);
            return d->run_command("HSET %b %b %b", prefixed_key.c_str(), prefixed_key.size(), field.c_str(), field.size(), value.c_str(), value.size());
        }

    bool Connection::hset(const Key& key, const Key& field, const Key& value, bool& was_created) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("HSET %b %b %b", prefixed_key.c_str(), prefixed_key.size(), field.c_str(), field.size(), value.c_str(), value.size())) {
            redis_assert(d->reply->type == REDIS_REPLY_INTEGER);
            was_created = d->reply->integer !=0;
            return true;
        }
        return false;
    }

    /* Set the value of a hash field, only if the field does not exist */
    bool Connection::hsetnx(const Key& key, const Key& field, const Key& value) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("HSETNX %b %b %b", prefixed_key.c_str(), prefixed_key.size(), field.c_str(), field.size(), value.c_str(), value.size());
    }

    bool Connection::hsetnx(const Key& key, const Key& field, const Key& value, bool& was_set) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("HSETNX %b %b %b", prefixed_key.c_str(), prefixed_key.size(), field.c_str(), field.size(), value.c_str(), value.size())) {
            redis_assert(d->reply->type == REDIS_REPLY_INTEGER);
            was_set = d->reply->integer !=0;
            return true;
        }
        return false;
    }

    /* Get all the values in a hash */
//        bool hvals(const Key& key);

    /* Incrementally iterate hash fields and associated values */
//        bool hscan(const Key& key, VAL cursor /*, [MATCH pattern] */ /*, [COUNT count] */);


    /*********************** pubsub commands ***********************/
    /* Listen for messages published to channels matching the given patterns */
//        bool psubscribe(VAL pattern [pattern ...]);

    /* Inspect the state of the Pub/Sub subsystem */
//        bool pubsub(VAL subcommand /*, [argument [argument ...]] */);

    /* Post a message to a channel */
//        bool publish(VAL channel, VAL message);

    /* Stop listening for messages posted to channels matching the given patterns */
//        bool punsubscribe( /* [pattern [pattern ...]] */);

    /* Listen for messages published to the given channels */
//        bool subscribe(VAL channel [channel ...]);

    /* Stop listening for messages posted to the given channels */
//        bool unsubscribe( /* [channel [channel ...]] */);


    /*********************** set commands ***********************/
    /* Add one or more members to a set */
    bool Connection::sadd(const Key& key, const Key& member) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("SADD %b %b", prefixed_key.c_str(), prefixed_key.size(), member.c_str(), member.size());
    }

    /* Add one or more members to a set */
    bool Connection::sadd(const Key& key, const Key& member, bool& was_added) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("SADD %b %b", prefixed_key.c_str(), prefixed_key.size(), member.c_str(), member.size())) {
            was_added = d->reply->integer != 0;
            return true;
        }
        return false;
    }

    /* Add one or more members to a set */
    bool Connection::sadd(const Key& key, const KeyVec& members) {
        if(d->redis_version < 20400) {
            bool res = true;
            for(size_t i=0; i<members.size(); i++) {
                res = sadd(key, members[i]) && res;
            }
            return res;
        }
        std::vector<size_t> sizes(members.size()+2);
        std::vector<const char*> commands(members.size()+2);
        const Key& prefixed_key = d->add_prefix_to_key(key);
        commands[0] = "SADD";
        sizes[0] = 4;
        commands[1] = prefixed_key.c_str();
        sizes[1] = prefixed_key.size();
        for(size_t i=0; i<members.size(); i++) {
            commands[i+2] = members[i].c_str();
            sizes[i+2] = members[i].size();
        }
        return d->run_command(commands, sizes);
    }

    /* Get the number of members in a set */
    bool Connection::scard(const Key& key, long long& result_size) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("SCARD %b", prefixed_key.c_str(), prefixed_key.size())) {
            result_size = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Subtract multiple sets */
    bool Connection::sdiff(const KeyVec& keys, KeyVec& result) {
        return d->run_set_command("SDIFF", 5, keys, result);
    }

    /* Subtract multiple sets and store the resulting set in a key */
    bool Connection::sdiffstore(const Key& destination, const KeyVec& keys) {
        long long num_of_elements;
        return d->run_set_store_command("SDIFFSTORE", 10, destination, keys, num_of_elements);
    }

    bool Connection::sdiffstore(const Key& destination, const KeyVec& keys, long long& num_of_elements) {
        return d->run_set_store_command("SDIFFSTORE", 10, destination, keys, num_of_elements);
    }

    /* Intersect multiple sets */
    bool Connection::sinter(const KeyVec& keys, KeyVec& result) {
        return d->run_set_command("SINTER", 6, keys, result);
    }

    /* Intersect multiple sets and store the resulting set in a key */
    bool Connection::sinterstore(const Key& destination, const KeyVec& keys) {
        long long num_of_elements;
        return d->run_set_store_command("SINTERSTORE", 11, destination, keys, num_of_elements);
    }
    bool Connection::sinterstore(const Key& destination, const KeyVec& keys, long long& number_of_elements) {
        return d->run_set_store_command("SINTERSTORE", 11, destination, keys, number_of_elements);
    }

    /* Determine if a given value is a member of a set */
//        bool sismember(const Key& key, VAL member);

    /* Get all the members in a set */
    bool Connection::smembers(const Key& key, KeyVec& result) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("SMEMBERS %b", prefixed_key.c_str(), prefixed_key.size())) {
            result.clear();
            redis_assert(d->reply->type == REDIS_REPLY_ARRAY);
            for(size_t i=0; i < d->reply->elements; i++) {
                redis_assert(d->reply->element[i]->type == REDIS_REPLY_STRING);
                result.push_back(d->reply->element[i]->str);
            }
            return true;
        }
        return false;
    }

    /* Move a member from one set to another */
//        bool smove(VAL source, VAL destination, VAL member);

    /* Remove and return a random member from a set */
//        bool spop(const Key& key);

    /* Get one or multiple random members from a set */
//        bool srandmember(const Key& key, bool count = false);

    /* Remove one or more members from a set */
//        bool srem(const Key& key, VAL member [member ...]);

    /* Add multiple sets */
    bool Connection::sunion(const KeyVec& keys, KeyVec& result) {
        return d->run_set_command("SUNION", 6, keys, result);
    }

    /* Add multiple sets and store the resulting set in a key */
    bool Connection::sunionstore(const Key& destination, const KeyVec& keys) {
        long long num_of_elements;
        return d->run_set_store_command("SUNIONSTORE", 11, destination, keys, num_of_elements);
    }

    bool Connection::sunionstore(const Key& destination, const KeyVec& keys, long long& num_of_elements) {
        return d->run_set_store_command("SUNIONSTORE", 11, destination, keys, num_of_elements);
    }

    /* Incrementally iterate Set elements */
//        bool sscan(const Key& key, VAL cursor /*, [MATCH pattern] */ /*, [COUNT count] */);


    /*********************** sorted_set commands ***********************/
    /* Add one or more members to a sorted set, or update its score if it already exists */
    bool Connection::zadd(const Key& key, const  KKHolder<std::string, double>& members_with_scores) {
        long long num_of_inserted_elements;
        return zadd(key, members_with_scores, num_of_inserted_elements);
    }
    bool Connection::zadd(const Key& key, const KKHolder<std::string, double>& members_with_scores, long long& num_of_inserted_elements) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        size_t sz = members_with_scores.size();
        std::vector<size_t> sizes(2+sz*2);
        std::vector<const char*> command_parts_c_strings(2+sz*2);
        command_parts_c_strings[0] = "ZADD";
        sizes[0] = 4;
        command_parts_c_strings[1] = prefixed_key.c_str();
        sizes[1] = prefixed_key.size();
        std::vector<std::string> scores_only;
        for (size_t i = 0; i < members_with_scores.size(); i++) {
            scores_only.emplace_back(std::to_string(members_with_scores.k2[i]));
        }
        for (size_t i = 0; i < members_with_scores.size(); i++) {
            command_parts_c_strings[2*i+2] = scores_only[i].c_str();
            sizes[2*i+2] = scores_only[i].size();

            command_parts_c_strings[2*i+3] = members_with_scores.k1[i].c_str();
            sizes[2*i+3] = members_with_scores.k1[i].size();
        }
        if(d->run_command(command_parts_c_strings, sizes)) {
            redis_assert(d->reply->type == REDIS_REPLY_INTEGER);
            num_of_inserted_elements = d->reply->integer;
            return true;
        }
        return false;
    }
    bool Connection::zadd(const Key& key, const Key& member, double score) {
        bool was_inserted;
        return zadd(key, member, score, was_inserted);
    }
    bool Connection::zadd(const Key& key, const Key& member, double score, bool& was_inserted) {
        std::string score_s = std::to_string(score);
        const Key& prefixed_key = d->add_prefix_to_key(key);
        //TODO https://github.com/redis/hiredis/issues/244 Update when issue is resolved.
        if(d->run_command("ZADD %b %b %b", prefixed_key.c_str(), prefixed_key.size(), score_s.c_str(), score_s.size(), member.c_str(), member.size())) {
            redis_assert(d->reply->type == REDIS_REPLY_INTEGER);
            was_inserted = d->reply->integer != 0;
            return true;
        }
        return false;
    }

    /* Get the number of members in a sorted set */
//        bool zcard(const Key& key);

    /* Count the members in a sorted set with scores within the given values */
//        bool zcount(const Key& key, VAL min, VAL max);

    /* Increment the score of a member in a sorted set */
    bool Connection::zincrby(const Key& key, double increment, const Key& member) {
        double new_score;
        return zincrby(key, increment, member, new_score);
    }

    bool Connection::zincrby(const Key& key, double increment, const Key& member, double& new_score) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        std::string increment_s = std::to_string(increment);
        //TODO https://github.com/redis/hiredis/issues/244 Update when issue is resolved.
        if(d->run_command("ZINCRBY %b %b %b", prefixed_key.c_str(), prefixed_key.size(), increment_s.c_str(), increment_s.size(), member.c_str(), member.size())) {
            redis_assert(d->reply->type == REDIS_REPLY_STRING);
            new_score = std::stod(std::string(d->reply->str, d->reply->len));
            return true;
        }
        return false;
    }

    /* Intersect multiple sorted sets and store the resulting sorted set in a new key */
//        bool zinterstore(VAL destination, VAL numkeys, const KeyVec& keys /*, [WEIGHTS weight [weight ...]] */ /*, [AGGREGATE SUM|MIN|MAX] */);

    /* Return a range of members in a sorted set, by index */
    bool Connection::zrange(const Key& key, long long start, long long stop, StringValueHolder&& values, Order) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("ZRANGE %b %lli %lli", prefixed_key.c_str(), prefixed_key.size(), start, stop)) {
            redis_assert(d->reply->type == REDIS_REPLY_ARRAY);
            for(size_t i=0; i < d->reply->elements; i++) {
                redis_assert(d->reply->element[i]->type == REDIS_REPLY_STRING);
                values.push_back(d->reply->element[i]->str);
            }
            return true;
        }
        return false;
    }
    bool Connection::zrange_with_scores(const Key& key, long long start, long long stop, PairHolder<std::string, double>&& values, Order order) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        const char* command;
        if(order == Order::ASC) {
            command = "ZRANGE";
        }
        else {
            command = "ZREVRANGE";
        }
        if(d->run_command("%s %b %lli %lli WITHSCORES", command, prefixed_key.c_str(), prefixed_key.size(), start, stop)) {
            redis_assert(d->reply->type == REDIS_REPLY_ARRAY);
            redis_assert(d->reply->elements % 2 ==0);
            for(size_t i=0; i < d->reply->elements; i+=2) {
                redis_assert(d->reply->element[i]->type == REDIS_REPLY_STRING);
                redis_assert(d->reply->element[i+1]->type == REDIS_REPLY_STRING);
                values.push_back(std::make_pair(std::string(d->reply->element[i]->str, d->reply->element[i]->len), std::stod(d->reply->element[i+1]->str)));
            }
            return true;
        }
        return false;
    }

    /* Return a range of members in a sorted set, by score */
//        bool zrangebyscore(const Key& key, VAL min, VAL max, bool withscores = false /*, [LIMIT offset count] */);

    /* Determine the index of a member in a sorted set */
//        bool zrank(const Key& key, VAL member);

    /* Remove one or more members from a sorted set */
//        bool zrem(const Key& key, VAL member [member ...]);

    /* Remove all members in a sorted set within the given indexes */
    bool Connection::zremrangebyrank(const Key& key, long long start, long long stop, Order order) {
        long long elements_removed_cnt;
        return zremrangebyrank(key, start, stop, elements_removed_cnt, order);
    }

    bool Connection::zremrangebyrank(const Key& key, long long start, long long stop, long long& elements_removed_cnt, Order order) {
        const Key& prefixed_key = d->add_prefix_to_key(key);
        if(order == Order::DESC) {
            long long temp;
            temp = (start+1)*-1;
            start = (stop+1)*-1;
            stop = temp;
        }
        if(d->run_command("ZREMRANGEBYRANK %b %lli %lli", prefixed_key.c_str(), prefixed_key.size(), start, stop)) {
            redis_assert(d->reply->type == REDIS_REPLY_INTEGER);
            elements_removed_cnt = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Remove all members in a sorted set within the given scores */
//        bool zremrangebyscore(const Key& key, VAL min, VAL max);

    /* Return a range of members in a sorted set, by index, with scores ordered from high to low */
//        bool zrevrange(const Key& key, VAL start, VAL stop, bool withscores = false);

    /* Return a range of members in a sorted set, by score, with scores ordered from high to low */
//        bool zrevrangebyscore(const Key& key, VAL max, VAL min, bool withscores = false /*, [LIMIT offset count] */);

    /* Determine the index of a member in a sorted set, with scores ordered from high to low */
//        bool zrevrank(const Key& key, VAL member);

    /* Get the score associated with the given member in a sorted set */
//        bool zscore(const Key& key, VAL member);

    /* Add multiple sorted sets and store the resulting sorted set in a new key */
//        bool zunionstore(VAL destination, VAL numkeys, const KeyVec& keys /*, [WEIGHTS weight [weight ...]] */ /*, [AGGREGATE SUM|MIN|MAX] */);

    /* Incrementally iterate sorted sets elements and associated scores */
//        bool zscan(const Key& key, VAL cursor /*, [MATCH pattern] */ /*, [COUNT count] */);


}
