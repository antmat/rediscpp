#include <cstring>
#include "connection.hpp"
#include "exception.hpp"
#include <iostream>
#include <vector>
#include <map>
#include <atomic>
#include "macro.hpp"

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

namespace Redis {
    class Connection::Implementation {
        friend class Connection;
        friend class PoolWrapper;
        friend class Pool;
        typedef std::unique_ptr<redisReply, ReplyDeleter> Reply;
        Reply reply;
        ConnectionParam connection_param;
        bool available;
        bool used;
        std::unique_ptr<redisContext, ContextDeleter> context;
        std::hash<std::string> hash_fn;
        unsigned int redis_version;
        Error err;
        Error prev_err;
        Connection::Id id;
        static std::atomic_long id_counter;

        Implementation(const ConnectionParam &_connection_param) :
                reply(),
                connection_param(_connection_param),
                available(false),
                used(true),
                context(),
                hash_fn(),
                redis_version(),
                err(),
                prev_err(),
                id()
        {
            id = ++id_counter;
            reconnect();
            fetch_version();
            if (connection_param.db_num != 0) {
                select(connection_param.db_num);
            }
        }

        template<class Keys>
        void append_c_strings_with_prefixes_and_sizes(Keys keys, KeyVec& prefixed_keys, std::vector<const char*>& c_strs, std::vector<size_t>& sizes) {
            redis_assert(c_strs.size() == sizes.size());
            redis_assert(prefixed_keys.empty());
            size_t initial_size = c_strs.size();
            sizes.resize(sizes.size()+ keys.size());
            c_strs.resize(c_strs.size() + keys.size());
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
                    return "Reply returned error." + (context->err ? std::string("Context err is: ") + context->errstr : std::string()) + " Reply error is: " + (reply == nullptr ? "Reply is null. Please replort a bug." : reply->str);
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
            prev_err = (err == Error::NONE ? prev_err : err);
            struct timeval timeout = {static_cast<time_t>(connection_param.connect_timeout_ms % 1000), static_cast<suseconds_t>(connection_param.connect_timeout_ms * 1000)};
            context.reset(redisConnectWithTimeout(connection_param.host.c_str(), connection_param.port, timeout));
            if (context == nullptr) {
                err = Error::CONTEXT_IS_NULL;
                available = false;
            }
            else {
                set_error_from_context(true);
                available = (err == Error::NONE);
            }
            return available;
        }

        Key add_prefix_to_key(KeyRef &key) {
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
            if (!never_thow && err != Error::NONE && connection_param.throw_on_error) {
                throw Redis::Exception(get_error_str(err, context.get(), reply.get()));
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
            if (!check_available()) {
                return false;
            }
            reply.reset(static_cast<redisReply*>(callback()));
            if((reply.get() == nullptr || context->err) && connection_param.reconnect_on_failure) {
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
                prev_err = (err == Error::NONE ? prev_err : err);
                err = Error::REPLY_ERR;
                if(connection_param.throw_on_error) {
                    throw Redis::Exception(get_error());
                }
                return false;
            }
            return true;
        }

        bool run_command(const std::vector<const char*>& commands, const std::vector<size_t>& sizes) {
            auto callback = [&](){ return redisCommandArgv(context.get(), static_cast<int>(commands.size()), const_cast<const char**>(commands.data()), sizes.data());};
            return run_command(callback);
        }
        bool run_command(const char* format, va_list ap) {
            auto callback = [&]() {
                va_list copy;
                va_copy(copy, ap);
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
        bool info(KeyRef section, Key& info_data) {
            if(!section.empty() && redis_version < 20600) {
                set_error(Error::COMMAND_UNSUPPORTED);
                return false;
            }
            if(section.empty()) {
                if(run_command("INFO", section.c_str())) {
                    info_data = reply->str;
                    return true;
                }
                return false;
            }
            if(run_command("INFO %s", section.c_str())) {
                info_data = reply->str;
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
            auto major_pos = info_data.find('.', pos+14);
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
            return run_command("SELECT %d", db_num);
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
                else {
                    redis_assert_unreachable();
                }
                return false;
            }
        }
    };
    std::atomic_long Connection::Implementation::id_counter(0);

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
    inline unsigned int Connection::get_version() {
        return d->get_version();
    }
    Connection::Id Connection::get_id() {
        return d->get_id();
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
    bool Connection::append(Connection::KeyRef key, Connection::KeyRef value, long long& result_length) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("APPEND %b %b", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size())) {
            result_length = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Append a value to a key */
    bool Connection::append(Connection::KeyRef key, Connection::KeyRef value) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("APPEND %b %b", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size());
    }

    /* Count set bits in a string */
    bool Connection::bitcount(Connection::KeyRef key, unsigned int start, unsigned int end, long long& result) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("BITCOUNT %b %u %u", prefixed_key.c_str(), prefixed_key.size(), start, end)) {
            result = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Count set bits in a string */
    bool Connection::bitcount(Connection::KeyRef key, long long& result) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("BITCOUNT %b", prefixed_key.c_str(), prefixed_key.size())) {
            result = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Perform bitwise operations between strings */
    bool Connection::bitop(BitOperation operation, Connection::KeyRef destkey, Connection::KeyVecRef keys, long long& size_of_dest) {
        //TODO Preformance!
        std::vector<size_t> sizes;
        sizes.reserve(keys.size()+3); //command, key, operation and key_vector

        KeyVec commands;
        commands.reserve(keys.size()+3); //command, key, operation and key_vector

        commands.push_back("BITCOUNT");
        sizes.push_back(8);

        switch (operation) {
            case BitOperation::AND:
                commands.push_back("AND");
                sizes.push_back(3);
                break;
            case BitOperation::OR:
                commands.push_back("OR");
                sizes.push_back(2);
                break;
            case BitOperation::NOT:
                commands.push_back("NOT");
                sizes.push_back(3);
                redis_assert(keys.size() == 1);
                break;
            case BitOperation::XOR:
                commands.push_back("XOR");
                sizes.push_back(3);
                break;
            default: //Fool proof of explicit casting BitOperation to integer and asigning improper value;
                redis_assert_unreachable();
                break;
        }
        KeyRef prefixed_key = d->add_prefix_to_key(destkey);
        commands.push_back(prefixed_key);
        sizes.push_back(prefixed_key.size());
        for(size_t i=0; i < keys.size(); i++) {
            KeyRef p_key = d->add_prefix_to_key(keys[i]);
            commands.push_back(p_key);
            sizes.push_back(p_key.size());
        }
        std::vector<const char*> comm_c_strings(commands.size());
        for(size_t i=0; i<commands.size(); i++) {
            comm_c_strings[i] = commands[i].c_str();
        }
        if(d->run_command(comm_c_strings, sizes)) {
            size_of_dest = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Perform bitwise operations between strings */
    bool Connection::bitop(BitOperation operation, Connection::KeyRef destkey, Connection::KeyVecRef keys) {
        long long res;
        return bitop(operation, destkey, keys, res);
    }

    /* Find first bit set or clear in a subsstring defined by start and end*/
    bool Connection::bitpos(Connection::KeyRef key, Bit bit, unsigned int start, unsigned int end, long long& result) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("BITPOS %b %c %u %u", prefixed_key.c_str(), prefixed_key.size(), bit == Bit::ONE ? '1' : '0', start, end)) {
            result = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Find first bit set or clear in a string */
    bool Connection::bitpos(Connection::KeyRef key, Bit bit, long long& result) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("BITPOS %b %c", prefixed_key.c_str(), prefixed_key.size(), bit == Bit::ONE ? '1' : '0')) {
            result = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Decrement the integer value of a key by one */
    bool Connection::decr(Connection::KeyRef key, long long& result_value) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("DECR %b", prefixed_key.c_str(), prefixed_key.size())) {
            result_value = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Decrement the integer value of a key by one */
    bool Connection::decr(Connection::KeyRef key) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("DECR %b", prefixed_key.c_str(), prefixed_key.size());
    }

    /* Decrement the integer value of a key by the given number */
    bool Connection::decrby(Connection::KeyRef key, long long decrement, long long& result_value) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("DECRBY %b %lli", prefixed_key.c_str(), prefixed_key.size(), decrement)) {
            result_value = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Decrement the integer value of a key by the given number */
    bool Connection::decrby(Connection::KeyRef key, long long decrement) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("DECRBY %b %lli", prefixed_key.c_str(), prefixed_key.size(), decrement);
    }

    bool Connection::get(const std::vector<std::reference_wrapper<const Key>>& keys) {
        std::vector<size_t> sizes(keys.size()+1);
        std::vector<const char*> command_parts_c_strings(keys.size()+1);
        command_parts_c_strings[0] = "MGET";
        sizes[0] = 4;
        KeyVec prefixed_keys;
        d->append_c_strings_with_prefixes_and_sizes(keys, prefixed_keys, command_parts_c_strings, sizes);
        return d->run_command(command_parts_c_strings, sizes);
    }

    /* Get the value of a key */
    bool Connection::get(Connection::KeyRef key, Connection::Key& result) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("GET %b", prefixed_key.c_str(), prefixed_key.size())) {
            result = d->reply->str;
            return true;
        }
        return false;
    }

    /* Get the value of multiple keys */
    bool Connection::get(Connection::KeyVecRef keys, Connection::KeyVec& result) {
        size_t sz = keys.size();
        std::vector<size_t> sizes(sz+1);
        std::vector<const char*> command_parts_c_strings(sz+1);
        command_parts_c_strings[0] = "MGET";
        sizes[0] = 4;
        KeyVec prefixed_keys;
        d->append_c_strings_with_prefixes_and_sizes(keys, prefixed_keys, command_parts_c_strings, sizes);
        result.resize(sz);
        if(d->run_command(command_parts_c_strings, sizes)) {
            size_t index=0;
            bool res;
            while(index < sz) {
                 res = fetch_get_result(result[index], index);
                 index++;
                 redis_assert(res);
                 if(!res) {
                    d->set_error(Error::HIREDIS_UNKNOWN);
                    return false;
                 }
            }
            return true;
        }
        return false;
    }

    /* Returns the bit value at offset in the string value stored at key */
    bool Connection::getbit(Connection::KeyRef key, long long offset, Bit& result) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("GETBIT %b %li", prefixed_key.c_str(), prefixed_key.size(), offset)) {
            result = d->reply->integer == 0 ? Bit::ZERO : Bit::ONE;
            return true;
        }
        return false;
    }

    /* Get a substring of the string stored at a key */
    bool Connection::getrange(Connection::KeyRef key, long long start, long long end, Connection::Key& result) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("GETRANGE %b %lli %lli", prefixed_key.c_str(), prefixed_key.size(), start, end)) {
            result = d->reply->str;
            return true;
        }
        return false;
    }

    /* Set the string value of a key and return its old value */
    bool Connection::getset(Connection::KeyRef key, Connection::KeyRef value, Connection::Key& old_value) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("GETSET %b %b", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size())) {
            old_value = d->reply->str;
            return true;
        }
        return false;
    }

    /* Increment the integer value of a key by one */
    bool Connection::incr(Connection::KeyRef key, long long& result_value) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("INCR %b", prefixed_key.c_str(), prefixed_key.size())) {
            result_value = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Increment the integer value of a key by one */
    bool Connection::incr(Connection::KeyRef key) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("INCR %b", prefixed_key.c_str(), prefixed_key.size());
    }

    /* Increment the integer value of a key by the given amount */
    bool Connection::incrby(Connection::KeyRef key, long long increment, long long& result_value) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("INCRBY %b %lli", prefixed_key.c_str(), prefixed_key.size(), increment)) {
            result_value = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Increment the integer value of a key by the given amount */
    bool Connection::incrby(Connection::KeyRef key, long long increment) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("INCRBY %b %lli", prefixed_key.c_str(), prefixed_key.size(), increment);
    }

    /* Increment the float value of a key by the given amount */
    bool Connection::incrbyfloat(Connection::KeyRef key, float increment, float& result_value) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
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
    bool Connection::incrbyfloat(Connection::KeyRef key, double increment, double& result_value) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
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
    bool Connection::incrbyfloat(Connection::KeyRef key, float increment) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("INCRBYFLOAT %b %f", prefixed_key.c_str(), prefixed_key.size(), increment);
    }

    /* Increment the float value of a key by the given amount */
    bool Connection::incrbyfloat(Connection::KeyRef key, double increment) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("INCRBYFLOAT %b %f", prefixed_key.c_str(), prefixed_key.size(), increment);
    }


    bool Connection::set(const std::vector<std::reference_wrapper<const Key>>& keys, const std::vector<std::reference_wrapper<const Key>>values, SetType set_type) {
        bool was_set;
        return d->set(keys, values, set_type, was_set);
    }

    /* Set multiple keys to multiple values */
    bool Connection::set(const std::map<Key, Key>& key_value_map, SetType set_type) {
        return set(key_value_map.begin(), key_value_map.end(), set_type);
    }

    /* Set multiple keys to multiple values */
    bool Connection::set(Connection::KeyVecRef keys, Connection::KeyVecRef values, SetType set_type) {
        bool was_set;
        return d->set(keys, values, set_type, was_set);
    }

    /* Set the string value of a key */
    bool Connection::set(Connection::KeyRef key, Connection::KeyRef value, SetType set_type, long long expire, ExpireType expire_type) {
        bool was_set;
        return d->set(key.c_str(),key.size(), value.c_str(), value.size(), set_type, was_set, expire, expire_type);
    }

    bool Connection::set(Connection::KeyRef key, Connection::KeyRef value, SetType set_type, bool& was_set, long long expire, ExpireType expire_type) {
        return d->set(key.c_str(),key.size(), value.c_str(), value.size(), set_type, was_set, expire, expire_type);
    }
    /* Set the string value of a key */
    bool Connection::set(const char* key, const char* value, SetType set_type, bool& was_set, long long expire, ExpireType expire_type) {
        return d->set(key, std::strlen(key), value, std::strlen(value), set_type, was_set, expire, expire_type);
    }
    bool Connection::set(const char* key, const char* value, SetType set_type, long long expire, ExpireType expire_type) {
        bool was_set;
        return d->set(key, std::strlen(key), value, std::strlen(value), set_type, was_set, expire, expire_type);
    }
//
//    /* Sets or clears the bit at offset in the string value stored at key */
//    bool Connection::setbit(Connection::KeyRef key,long long offset, Bit value, Bit& original_bit);
//
//    /* Sets or clears the bit at offset in the string value stored at key */
//    bool Connection::setbit(Connection::KeyRef key,long long offset, Bit value);
//
//    /* Set the value and expiration of a key */
//    bool Connection::setex(Connection::KeyRef key, Connection::KeyRef value, long long seconds);
//
//    /* Set the value of a key, only if the key does not exist */
//    bool Connection::setnx(Connection::KeyRef key, Connection::KeyRef value, bool& was_set);
//
//    /* Set the value of a key, only if the key does not exist */
//    bool Connection::setnx(Connection::KeyRef key, Connection::KeyRef value);
//
//    /* Overwrite part of a string at key starting at the specified offset */
//    bool Connection::setrange(Connection::KeyRef key,long long offset, Connection::KeyRef value, long long& result_length);
//
//    /* Overwrite part of a string at key starting at the specified offset */
//    bool Connection::setrange(Connection::KeyRef key,long long offset, Connection::KeyRef value);
//
//    /* Get the length of the value stored in a key */
//    bool Connection::strlen(Connection::KeyRef key, long long& key_length);
//
//
//    /*********************** connection commands ***********************/
//    /* Authenticate to the server */
//    bool Connection::auth(Connection::KeyRef password);
//
//    /* Echo the given string. Return message will contain a copy of message*/
//    bool Connection::echo(Connection::KeyRef message, Connection::Key& return_message);
//
//    /* Echo the given string. In fact does nothing. Just for a full interface. */
//    bool Connection::echo(Connection::KeyRef message);
//
//    /* Ping the server */
//    bool Connection::ping();
//
//    /* Close the connection */
//    bool Connection::quit();
//    bool Connection::disconnect();
//
//    /* Change the selected database for the current connection */
//    bool Connection::select(long long db_num);
//
//
//    /*********************** server commands ***********************/
//    /* Asynchronously rewrite the append-only file */
//    bool Connection::bgrewriteaof();
//
//    /* Asynchronously save the dataset to disk */
//    bool Connection::bgsave();
//
//    /* Kill the connection of a client */
//    bool Connection::client_kill(Connection::KeyRef ip_and_port);
//
//    /* Get the list of client connections */
//    //bool Connection::client_list(); //TODO : implement
//
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
//    bool Connection::config_get(Connection::KeyRef parameter, Connection::Key& result);
//
//    /* Rewrite the configuration file with the in memory configuration */
//    bool Connection::config_rewrite();
//
//    /* Set a configuration parameter to the given value */
//    bool Connection::config_set(Connection::KeyRef parameter, Connection::KeyRef value);
//
//    /* Reset the stats returned by INFO */
//    bool Connection::config_resetstat();
//
//    /* Return the number of keys in the selected database */
//    bool Connection::dbsize(long long* result);
//
//    /* Get debugging information about a key */
//    bool Connection::debug_object(Connection::KeyRef key, Connection::Key& info);
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
    bool Connection::info(Connection::KeyRef section, Connection::Key& info_data) {
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
//    bool Connection::slaveof(Connection::KeyRef host, unsigned int port);
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
//    bool Connection::blpop(Connection::KeyVecRef keys, long long timeout, Connection::Key& chosen_key, Connection::Key& value);
//
//    /* Remove and get the last element in a list, or block until one is available */
//    bool Connection::brpop(Connection::KeyVecRef keys, long long timeout, Connection::Key& chosen_key, Connection::Key& value);
//
//    /* Pop a value from a list, push it to another list and return it; or block until one is available */
//    bool Connection::brpoplpush(Connection::KeyRef source, Connection::KeyRef destination, long long timeout, Connection::Key& result);
//
//    /* Pop a value from a list, push it to another list and return it; or block until one is available */
//    bool Connection::brpoplpush(Connection::KeyRef source, Connection::KeyRef destination, long long timeout);
//
//    /* Get an element from a list by its index */
//    bool Connection::lindex(Connection::KeyRef key, long long index);
//
//    enum class ListInsertType {
//        AFTER,
//        BEFORE
//    };
//
//    /* Insert an element before or after another element in a list */
//    bool Connection::linsert(Connection::KeyRef key, ListInsertType insert_type, Connection::KeyRef pivot, Connection::KeyRef value);
//
//    /* Insert an element before or after another element in a list */
//    bool Connection::linsert(Connection::KeyRef key, ListInsertType insert_type, Connection::KeyRef pivot, Connection::KeyRef value, long long& list_size);
//
//    /* Get the length of a list */
//    bool Connection::llen(Connection::KeyRef key, long long& length);
//
//    /* Remove and get the first element in a list */
//    bool Connection::lpop(Connection::KeyRef key, Connection::Key& value);
//
//    /* Remove and get the first element in a list */
//    bool Connection::lpop(Connection::KeyRef key);
//
//    /* Prepend one or multiple values to a list */
//    bool Connection::lpush(Connection::KeyRef key, Connection::KeyRef value, long long& list_length);
//
//    /* Prepend one or multiple values to a list */
//    bool Connection::lpush(Connection::KeyRef key, Connection::KeyRef value);
//
//    /* Prepend one or multiple values to a list */
//    bool Connection::lpush(Connection::KeyRef key, Connection::KeyVecRef values, long long& list_length);
//
//    /* Prepend one or multiple values to a list */
//    bool Connection::lpush(Connection::KeyRef key, Connection::KeyVecRef values);
//
//    /* Prepend a value to a list, only if the list exists */
//    bool Connection::lpushx(Connection::KeyRef key, Connection::KeyRef value, long long& list_length);
//
//    /* Prepend a value to a list, only if the list exists */
//    bool Connection::lpushx(Connection::KeyRef key, Connection::KeyRef value);


    /* Get a range of elements from a list */
//        bool lrange(KeyRef key, VAL start, VAL stop);

    /* Remove elements from a list */
//        bool lrem(KeyRef key, VAL count, VAL value);

    /* Set the value of an element in a list by its index */
//        bool lset(KeyRef key, VAL index, VAL value);

    /* Trim a list to the specified range */
//        bool ltrim(KeyRef key, VAL start, VAL stop);

    /* Remove and get the last element in a list */
//        bool rpop(KeyRef key);

    /* Remove the last element in a list, append it to another list and return it */
//        bool rpoplpush(VAL source, VAL destination);

    /* Append one or multiple values to a list */
//        bool rpush(KeyRef key, VAL value [value ...]);

    /* Append a value to a list, only if the list exists */
//        bool rpushx(KeyRef key, VAL value);


    /*********************** generic commands ***********************/
    /* Delete a key */
//        bool del(KeyVecRef keys);

    /* Return a serialized version of the value stored at the specified key. */
//        bool dump(KeyRef key);

    /* Determine if a key exists */
//        bool exists(KeyRef key);

    /* Set a key's time to live in seconds */
//        bool expire(KeyRef key, VAL seconds);

    /* Set the expiration for a key as a UNIX timestamp */
//        bool expireat(KeyRef key, VAL timestamp);

    /* Find all keys matching the given pattern */
//        bool keys(VAL pattern);

    /* Atomically transfer a key from a Redis instance to another one. */
//        bool migrate(VAL host, VAL port, KeyRef key, VAL destination-db, VAL timeout, bool copy = false, bool replace = false);

    /* Move a key to another database */
//        bool move(KeyRef key, VAL db);

    /* Inspect the internals of Redis objects */
//        bool object(VAL subcommand /*, [arguments [arguments ...]] */);

    /* Remove the expiration from a key */
//        bool persist(KeyRef key);

    /* Set a key's time to live in milliseconds */
//        bool pexpire(KeyRef key, VAL milliseconds);

    /* Set the expiration for a key as a UNIX timestamp specified in milliseconds */
//        bool pexpireat(KeyRef key, VAL milliseconds-timestamp);

    /* Get the time to live for a key in milliseconds */
//        bool pttl(KeyRef key);

    /* Return a random key from the keyspace */
//        bool randomkey();

    /* Rename a key */
//        bool rename(KeyRef key, VAL newkey);

    /* Rename a key, only if the new key does not exist */
//        bool renamenx(KeyRef key, VAL newkey);

    /* Create a key using the provided serialized value, previously obtained using DUMP. */
//        bool restore(KeyRef key, VAL ttl, VAL serialized-value);

    /* Sort the elements in a list, set or sorted set */
//        bool sort(KeyRef key /*, [BY pattern] */ /*, [LIMIT offset count] */ /*, [GET pattern [GET pattern ...]] */ /*, [ASC|DESC] */, bool alpha = false /*, [STORE destination] */);

    /* Get the time to live for a key */
//        bool ttl(KeyRef key);

    /* Determine the type stored at key */
//        bool type(KeyRef key);

    /* Incrementally iterate the keys space */
    bool Connection::scan(unsigned long long& cursor, KeyVec& result_keys, KeyRef pattern, long count) {
        bool ret;
        KeyRef prefixed_pattern = d->add_prefix_to_key(pattern);
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
            result_keys.clear();
            redis_assert(d->reply->elements == 2);
            redis_assert(d->reply->element[0]->type == REDIS_REPLY_STRING);
            redis_assert(d->reply->element[1]->type == REDIS_REPLY_ARRAY);
            cursor = std::stoull(d->reply->element[0]->str);
            for(size_t i=0; i < d->reply->element[1]->elements; i++) {
                redis_assert(d->reply->element[1]->element[i]->type == REDIS_REPLY_STRING);
                redis_assert(static_cast<size_t>(d->reply->element[1]->element[i]->len) >= prefixed_pattern.size());
                result_keys.push_back(d->reply->element[1]->element[i]->str+prefixed_pattern.size());
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
//        bool watch(KeyVecRef keys);


    /*********************** scripting commands ***********************/
    /* Execute a Lua script server side */
//        bool eval(VAL script, VAL numkeys, KeyVecRef keys, VAL arg [arg ...]);

    /* Execute a Lua script server side */
//        bool evalsha(VAL sha1, VAL numkeys, KeyVecRef keys, VAL arg [arg ...]);

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
//        bool hdel(KeyRef key, VAL field [field ...]);

    /* Determine if a hash field exists */
//        bool hexists(KeyRef key, VAL field);

    /* Get the value of a hash field */
//        bool hget(KeyRef key, VAL field);

    /* Get all the fields and values in a hash */
//        bool hgetall(KeyRef key);

    /* Increment the integer value of a hash field by the given number */
//        bool hincrby(KeyRef key, VAL field, VAL increment);

    /* Increment the float value of a hash field by the given amount */
//        bool hincrbyfloat(KeyRef key, VAL field, VAL increment);

    /* Get all the fields in a hash */
//        bool hkeys(KeyRef key);

    /* Get the number of fields in a hash */
//        bool hlen(KeyRef key);

    /* Get the values of all the given hash fields */
//        bool hmget(KeyRef key, VAL field [field ...]);

    /* Set multiple hash fields to multiple values */
//        bool hmset(KeyRef key, VAL field value [field value ...]);

    /* Set the string value of a hash field */
//        bool hset(KeyRef key, VAL field, VAL value);

    /* Set the value of a hash field, only if the field does not exist */
//        bool hsetnx(KeyRef key, VAL field, VAL value);

    /* Get all the values in a hash */
//        bool hvals(KeyRef key);

    /* Incrementally iterate hash fields and associated values */
//        bool hscan(KeyRef key, VAL cursor /*, [MATCH pattern] */ /*, [COUNT count] */);


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
    bool Connection::sadd(KeyRef key, KeyRef member) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        return d->run_command("SADD %b %b", prefixed_key.c_str(), prefixed_key.size(), member.c_str(), member.size());
    }

    /* Add one or more members to a set */
    bool Connection::sadd(KeyRef key, KeyRef member, bool& was_added) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("SADD %b %b", prefixed_key.c_str(), prefixed_key.size(), member.c_str(), member.size())) {
            was_added = d->reply->integer != 0;
            return true;
        }
        return false;
    }

    /* Add one or more members to a set */
    bool Connection::sadd(KeyRef key, KeyVecRef members) {
        if(d->redis_version < 20400) {
            bool res = true;
            for(size_t i=0; i<members.size(); i++) {
                res = sadd(key, members[i]) && res;
            }
            return res;
        }
        std::vector<size_t> sizes(members.size()+2);
        std::vector<const char*> commands(members.size()+2);
        KeyRef prefixed_key = d->add_prefix_to_key(key);
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
    bool Connection::scard(KeyRef key, long long& result_size) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
        if(d->run_command("SCARD %b", prefixed_key.c_str(), prefixed_key.size())) {
            result_size = d->reply->integer;
            return true;
        }
        return false;
    }

    /* Subtract multiple sets */
//        bool sdiff(KeyVecRef keys);

    /* Subtract multiple sets and store the resulting set in a key */
//        bool sdiffstore(VAL destination, KeyVecRef keys);

    /* Intersect multiple sets */
    bool Connection::sinter(KeyVecRef keys, KeyVec& result) {
        std::vector<size_t> sizes(keys.size()+1);
        std::vector<const char*> command_parts_c_strings(keys.size()+1);
        command_parts_c_strings[0] = "SINTER";
        sizes[0] = 6;
        KeyVec prefixed_keys;
        d->append_c_strings_with_prefixes_and_sizes(keys, prefixed_keys, command_parts_c_strings, sizes);
        if(d->run_command(command_parts_c_strings, sizes)) {
            redis_assert(d->reply->type == REDIS_REPLY_ARRAY);
            for(size_t i=0; i < d->reply->elements; i++) {
                redis_assert(d->reply->element[i]->type == REDIS_REPLY_STRING);
                result.push_back(d->reply->element[i]->str);
            }
            return true;
        }
        return false;
    }

    /* Intersect multiple sets and store the resulting set in a key */
//        bool sinterstore(VAL destination, KeyVecRef keys);

    /* Determine if a given value is a member of a set */
//        bool sismember(KeyRef key, VAL member);

    /* Get all the members in a set */
    bool Connection::smembers(KeyRef key, KeyVec& result) {
        KeyRef prefixed_key = d->add_prefix_to_key(key);
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
//        bool spop(KeyRef key);

    /* Get one or multiple random members from a set */
//        bool srandmember(KeyRef key, bool count = false);

    /* Remove one or more members from a set */
//        bool srem(KeyRef key, VAL member [member ...]);

    /* Add multiple sets */
//        bool sunion(KeyVecRef keys);

    /* Add multiple sets and store the resulting set in a key */
//        bool sunionstore(VAL destination, KeyVecRef keys);

    /* Incrementally iterate Set elements */
//        bool sscan(KeyRef key, VAL cursor /*, [MATCH pattern] */ /*, [COUNT count] */);


    /*********************** sorted_set commands ***********************/
    /* Add one or more members to a sorted set, or update its score if it already exists */
//        bool zadd(KeyRef key, VAL score member [score member ...]);

    /* Get the number of members in a sorted set */
//        bool zcard(KeyRef key);

    /* Count the members in a sorted set with scores within the given values */
//        bool zcount(KeyRef key, VAL min, VAL max);

    /* Increment the score of a member in a sorted set */
//        bool zincrby(KeyRef key, VAL increment, VAL member);

    /* Intersect multiple sorted sets and store the resulting sorted set in a new key */
//        bool zinterstore(VAL destination, VAL numkeys, KeyVecRef keys /*, [WEIGHTS weight [weight ...]] */ /*, [AGGREGATE SUM|MIN|MAX] */);

    /* Return a range of members in a sorted set, by index */
//        bool zrange(KeyRef key, VAL start, VAL stop, bool withscores = false);

    /* Return a range of members in a sorted set, by score */
//        bool zrangebyscore(KeyRef key, VAL min, VAL max, bool withscores = false /*, [LIMIT offset count] */);

    /* Determine the index of a member in a sorted set */
//        bool zrank(KeyRef key, VAL member);

    /* Remove one or more members from a sorted set */
//        bool zrem(KeyRef key, VAL member [member ...]);

    /* Remove all members in a sorted set within the given indexes */
//        bool zremrangebyrank(KeyRef key, VAL start, VAL stop);

    /* Remove all members in a sorted set within the given scores */
//        bool zremrangebyscore(KeyRef key, VAL min, VAL max);

    /* Return a range of members in a sorted set, by index, with scores ordered from high to low */
//        bool zrevrange(KeyRef key, VAL start, VAL stop, bool withscores = false);

    /* Return a range of members in a sorted set, by score, with scores ordered from high to low */
//        bool zrevrangebyscore(KeyRef key, VAL max, VAL min, bool withscores = false /*, [LIMIT offset count] */);

    /* Determine the index of a member in a sorted set, with scores ordered from high to low */
//        bool zrevrank(KeyRef key, VAL member);

    /* Get the score associated with the given member in a sorted set */
//        bool zscore(KeyRef key, VAL member);

    /* Add multiple sorted sets and store the resulting sorted set in a new key */
//        bool zunionstore(VAL destination, VAL numkeys, KeyVecRef keys /*, [WEIGHTS weight [weight ...]] */ /*, [AGGREGATE SUM|MIN|MAX] */);

    /* Incrementally iterate sorted sets elements and associated scores */
//        bool zscan(KeyRef key, VAL cursor /*, [MATCH pattern] */ /*, [COUNT count] */);


}
