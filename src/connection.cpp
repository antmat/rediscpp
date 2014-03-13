#include <cstring>
#include "connection.hpp"
#include "exception.hpp"
#include <iostream>
#define get_prefixed_key(key) const std::string prefixed_key = has_prefix() ? add_prefix_to_key(key) : key
#define get_prefixed_key_with_p(key, prefixed_key) const std::string prefixed_key = has_prefix() ? add_prefix_to_key(key) : key
#define get_prefixed_keys(keys) \
const KeyVec* key_vec_ptr;\
std::vector<std::string> prefixed_keys;\
if(has_prefix()) {\
    key_vec_ptr = &prefixed_keys;\
    prefixed_keys.reserve(keys.size());\
    for (size_t i = 0; i < keys.size(); i++) {\
        prefixed_keys.push_back(add_prefix_to_key(keys[i]));\
    }\
}\
else {\
    key_vec_ptr = &keys;\
}

namespace Redis {


    Connection::Connection(const ConnectionParam &_connection_param) :
            connection_param(_connection_param),
            available(false),
            used(true),
            context(),
            hash_fn(),
            redis_version(),
            err()
    {
        reconnect();
        fetch_version();
        if (connection_param.db_num != 0) {
            switch_db(connection_param.db_num);
        }
    }

    Connection::Connection(
            const std::string &host,
            unsigned int port,
            const std::string& _password,
            unsigned int db_num,
            const std::string &prefix,
            unsigned int connect_timeout_ms,
            unsigned int operation_timeout_ms,
            bool reconnect_on_failure,
            bool throw_on_error
    ) :
            Connection(ConnectionParam(host, port, _password, db_num, prefix, connect_timeout_ms, operation_timeout_ms, reconnect_on_failure, throw_on_error))
    {}

    bool Connection::switch_db(long long db_num) {
        return run_command("SELECT %d", db_num);
    }

    bool Connection::select(long long db_num) {
        return run_command("SELECT %d", db_num);
    }

    bool Connection::reconnect() {
        struct timeval timeout = {static_cast<time_t>(connection_param.connect_timeout_ms % 1000), static_cast<suseconds_t>(connection_param.connect_timeout_ms * 1000)};
        context.reset(redisConnectWithTimeout(connection_param.host.c_str(), connection_param.port, timeout));
        if (context == nullptr) {
            err = Error::CONTEXT_IS_NULL;
            available = false;
        }
        else {
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
            available = (err == Error::NONE);
        }
        return available;
    }

    bool Connection::run_command(Reply& reply, const std::vector<const char*>& commands, const std::vector<size_t>& sizes) {
        if (!is_available() && connection_param.reconnect_on_failure) {
            reconnect();
            if (!is_available()) {
                if (connection_param.throw_on_error) {
                    throw Redis::Exception(get_error());
                }
                return false;
            }
        }
        redis_assert(commands.size() == sizes.size());
        reply.reset(static_cast<redisReply*>(redisCommandArgv(context.get(), static_cast<int>(commands.size()), const_cast<const char**>(commands.data()), sizes.data())));
        if((reply.get() == nullptr || context->err) && connection_param.reconnect_on_failure) {
            reconnect();
            if (!is_available()) {
                if(connection_param.throw_on_error) {
                    throw Redis::Exception(get_error());
                }
                return false;
            }
            reply.reset(static_cast<redisReply*>(redisCommandArgv(context.get(), static_cast<int>(commands.size()), const_cast<const char**>(commands.data()), sizes.data())));
        }
        if(context->err) {
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
            if(connection_param.throw_on_error) {
                throw Redis::Exception(get_error());
            }
            return false;
        }
        if(reply.get() == nullptr) {
            err = Error::REPLY_IS_NULL;
            if(connection_param.throw_on_error) {
                throw Redis::Exception(get_error());
            }
            return false;
        }
        return true;
    }

    bool Connection::run_command(Reply& reply, const char* format, va_list ap) {
        if (!is_available() && connection_param.reconnect_on_failure) {
            reconnect();
            if (!is_available()) {
                if (connection_param.throw_on_error) {
                    throw Redis::Exception(get_error());
                }
                return false;
            }
        }
        va_list ap2;
        va_copy(ap2,ap);
        reply.reset(static_cast<redisReply*>(redisvCommand(context.get(), format, ap2)));
        va_end(ap2);
        if((reply.get() == nullptr || context->err) && connection_param.reconnect_on_failure) {
            reconnect();
            if (!is_available()) {

                if(connection_param.throw_on_error) {
                    throw Redis::Exception(get_error());
                }
                return false;
            }
            reply.reset(static_cast<redisReply*>(redisvCommand(context.get(), format, ap)));
        }
        if(context->err) {
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
            if(connection_param.throw_on_error) {
                throw Redis::Exception(get_error());
            }
            return false;
        }
        if(reply.get() == nullptr) {
            err = Error::REPLY_IS_NULL;
            if(connection_param.throw_on_error) {
                throw Redis::Exception(get_error());
            }
            return false;
        }
        return true;
    }

    bool Connection::fetch_version() {
        Key info_data;
        if(!info(info_data)) {
            return false;
        }
        auto pos = info_data.find("redis_version:");
        if(pos == std::string::npos) {
            err = Error::UNEXPECTED_INFO_RESULT;
            return false;
        }
        auto major_pos = info_data.find('.', pos+14);
        if(major_pos == std::string::npos) {
            err = Error::UNEXPECTED_INFO_RESULT;
            return false;
        }
        unsigned int major = std::stoi(info_data.substr(pos+14, major_pos-pos-14));
        auto minor_pos = info_data.find('.', major_pos+1);
        if(minor_pos == std::string::npos) {
            err = Error::UNEXPECTED_INFO_RESULT;
            return false;
        }
        unsigned int minor = std::stoi(info_data.substr(major_pos+1, minor_pos - major_pos -1));
        auto patch_level_pos = info_data.find('\n', minor_pos+1);
        if(patch_level_pos == std::string::npos) {
            err = Error::UNEXPECTED_INFO_RESULT;
            return false;
        }
        unsigned int patch_level = std::stoi(info_data.substr(minor_pos+1, minor_pos - patch_level_pos -1));
        redis_version = major * 10000 + minor * 100 + patch_level;
        return true;
    }

    std::string Connection::add_prefix_to_key(const std::string &key) {
        return connection_param.prefix + key;
    }

    void Connection::update_param(const ConnectionParam& new_param) {
        assert(connection_param.host == new_param.host);
        assert(connection_param.port == new_param.port);
        if(new_param.db_num != connection_param.db_num) {
            switch_db(new_param.db_num);
        }
        connection_param = new_param;
    }


    /*********************** string commands ***********************/

    /* Append a value to a key */
    bool Connection::append(Connection::KeyRef key, Connection::KeyRef value, long long& result_length) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "APPEND %b %b", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size())) {
            result_length = reply->integer;
            return true;
        }
        return false;
    }

    /* Append a value to a key */
    bool Connection::append(Connection::KeyRef key, Connection::KeyRef value) {
        get_prefixed_key(key);
        return run_command("APPEND %b %b", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size());
    }

    /* Count set bits in a string */
    bool Connection::bitcount(Connection::KeyRef key, unsigned int start, unsigned int end, long long& result) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "BITCOUNT %b %u %u", prefixed_key.c_str(), prefixed_key.size(), start, end)) {
            result = reply->integer;
            return true;
        }
        return false;
    }

    /* Count set bits in a string */
    bool Connection::bitcount(Connection::KeyRef key, long long& result) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "BITCOUNT %b", prefixed_key.c_str(), prefixed_key.size())) {
            result = reply->integer;
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
        get_prefixed_key(destkey);
        commands.push_back(prefixed_key);
        sizes.push_back(prefixed_key.size());
        for(size_t i=0; i < keys.size(); i++) {
            get_prefixed_key_with_p(keys[i], p_key);
            commands.push_back(p_key);
            sizes.push_back(p_key.size());
        }
        Reply reply;
        std::vector<const char*> comm_c_strings(commands.size());
        for(size_t i=0; i<commands.size(); i++) {
            comm_c_strings[i] = commands[i].c_str();
        }
        if(run_command(reply, comm_c_strings, sizes)) {
            size_of_dest = reply->integer;
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
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "BITPOS %b %c %u %u", prefixed_key.c_str(), prefixed_key.size(), bit == Bit::ONE ? '1' : '0', start, end)) {
            result = reply->integer;
            return true;
        }
        return false;
    }

    /* Find first bit set or clear in a string */
    bool Connection::bitpos(Connection::KeyRef key, Bit bit, long long& result) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command("BITPOS %b %c", prefixed_key.c_str(), prefixed_key.size(), bit == Bit::ONE ? '1' : '0')) {
            result = reply->integer;
            return true;
        }
        return false;
    }

    /* Decrement the integer value of a key by one */
    bool Connection::decr(Connection::KeyRef key, long long& result_value) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "DECR %b", prefixed_key.c_str(), prefixed_key.size())) {
            result_value = reply->integer;
            return true;
        }
        return false;
    }

    /* Decrement the integer value of a key by one */
    bool Connection::decr(Connection::KeyRef key) {
        get_prefixed_key(key);
        return run_command("DECR %b", prefixed_key.c_str(), prefixed_key.size());
    }

    /* Decrement the integer value of a key by the given number */
    bool Connection::decrby(Connection::KeyRef key, long long decrement, long long& result_value) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "DECRBY %b %lli", prefixed_key.c_str(), prefixed_key.size(), decrement)) {
            result_value = reply->integer;
            return true;
        }
        return false;
    }

    /* Decrement the integer value of a key by the given number */
    bool Connection::decrby(Connection::KeyRef key, long long decrement) {
        get_prefixed_key(key);
        return run_command("DECRBY %b %lli", prefixed_key.c_str(), prefixed_key.size(), decrement);
    }

    /* Get the value of a key */
    bool Connection::get(Connection::KeyRef key, Connection::Key& result) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "GET %b", prefixed_key.c_str(), prefixed_key.size())) {
            result = reply->str;
            return true;
        }
        return false;
    }

    /* Get the value of multiple keys */
    bool Connection::get(Connection::KeyVecRef keys, Connection::KeyVec& result) {
        std::vector<size_t> sizes(keys.size()+1);
        std::vector<const char*> command_parts_c_strings(keys.size()+1);
        command_parts_c_strings[0] = "MGET";
        sizes[0] = 4;
        Reply reply;
        get_prefixed_keys(keys);
        for(size_t i=0; i<key_vec_ptr->size(); i++) {
            command_parts_c_strings[i+1] = (*key_vec_ptr)[i].c_str();
            sizes[i+1] = (*key_vec_ptr)[i].size();
        }
        if(run_command(reply, command_parts_c_strings, sizes)) {
            redis_assert(reply->type == REDIS_REPLY_ARRAY);
            for(size_t i=0; i < reply->elements; i++) {
                if(reply->element[i]->type == REDIS_REPLY_STRING) {
                    result.push_back(reply->element[i]->str);
                }
                else if (reply->element[i]->type == REDIS_REPLY_NIL) {
                    result.push_back(std::string());
                }
                else {
                    redis_assert_unreachable();
                }
            }
            return true;
        }
        return false;
    }

    /* Returns the bit value at offset in the string value stored at key */
    bool Connection::getbit(Connection::KeyRef key, long long offset, Bit& result) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "GETBIT %b %li", prefixed_key.c_str(), prefixed_key.size(), offset)) {
            result = reply->integer == 0 ? Bit::ZERO : Bit::ONE;
            return true;
        }
        return false;
    }

    /* Get a substring of the string stored at a key */
    bool Connection::getrange(Connection::KeyRef key, long long start, long long end, Connection::Key& result) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "GETRANGE %b %lli %lli", prefixed_key.c_str(), prefixed_key.size(), start, end)) {
            result = reply->str;
            return true;
        }
        return false;
    }

    /* Set the string value of a key and return its old value */
    bool Connection::getset(Connection::KeyRef key, Connection::KeyRef value, Connection::Key& old_value) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "GETSET %b %b", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size())) {
            old_value = reply->str;
            return true;
        }
        return false;
    }

    /* Increment the integer value of a key by one */
    bool Connection::incr(Connection::KeyRef key, long long& result_value) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "INCR %b", prefixed_key.c_str(), prefixed_key.size())) {
            result_value = reply->integer;
            return true;
        }
        return false;
    }

    /* Increment the integer value of a key by one */
    bool Connection::incr(Connection::KeyRef key) {
        get_prefixed_key(key);
        return run_command("INCR %b", prefixed_key.c_str(), prefixed_key.size());
    }

    /* Increment the integer value of a key by the given amount */
    bool Connection::incrby(Connection::KeyRef key, long long increment, long long& result_value) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "INCRBY %b %lli", prefixed_key.c_str(), prefixed_key.size(), increment)) {
            result_value = reply->integer;
            return true;
        }
        return false;
    }

    /* Increment the integer value of a key by the given amount */
    bool Connection::incrby(Connection::KeyRef key, long long increment) {
        get_prefixed_key(key);
        return run_command("INCRBY %b %lli", prefixed_key.c_str(), prefixed_key.size(), increment);
    }

    /* Increment the float value of a key by the given amount */
    bool Connection::incrbyfloat(Connection::KeyRef key, float increment, float& result_value) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "INCRBYFLOAT %b %f", prefixed_key.c_str(), prefixed_key.size(), increment)) {
            result_value = strtof(reply->str, nullptr);
            if(result_value == 0.0 && errno == ERANGE) {
                err = Error::FLOAT_OUT_OF_RANGE;
                return false;
            }
            return true;
        }
        return false;
    }

    /* Increment the float value of a key by the given amount */
    bool Connection::incrbyfloat(Connection::KeyRef key, double increment, double& result_value) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "INCRBYFLOAT %b %f", prefixed_key.c_str(), prefixed_key.size(), increment)) {
            result_value = strtod(reply->str, nullptr);
            if(result_value == 0.0 && errno == ERANGE) {
                err = Error::DOUBLE_OUT_OF_RANGE;
                return false;
            }
            return true;
        }
        return false;
    }

    /* Increment the float value of a key by the given amount */
    bool Connection::incrbyfloat(Connection::KeyRef key, float increment) {
        get_prefixed_key(key);
        return run_command("INCRBYFLOAT %b %f", prefixed_key.c_str(), prefixed_key.size(), increment);
    }

    /* Increment the float value of a key by the given amount */
    bool Connection::incrbyfloat(Connection::KeyRef key, double increment) {
        get_prefixed_key(key);
        return run_command("INCRBYFLOAT %b %f", prefixed_key.c_str(), prefixed_key.size(), increment);
    }

    /* Get the values of all the given keys */
    bool Connection::mget(Connection::KeyVecRef keys, Connection::KeyVec& result) {
        return get(keys, result);
    }
//
//    /* Set multiple keys to multiple values */
//    bool Connection::mset(Connection::KeyVecRef keys, Connection::KeyVecRef values);
//
//    /* Set multiple keys to multiple values, only if none of the keys exist */
//    bool Connection::msetnx(Connection::KeyVecRef keys, Connection::KeyVecRef values, bool& all_new_keys);
//
//    /* Set multiple keys to multiple values, only if none of the keys exist */
//    bool Connection::msetnx(Connection::KeyVecRef keys, Connection::KeyVecRef values);
//
//    /* Set the value and expiration in milliseconds of a key */
//    bool Connection::psetex(Connection::KeyRef key, Connection::KeyRef value, long long milliseconds);
//
//    enum class ExpireType {
//        NONE,
//        SEC,
//        MSEC
//    };
//
//    enum class SetType {
//        ALWAYS,
//        IF_EXIST,
//        IF_NOT_EXIST
//    };
    /* Set the string value of a key */
    bool Connection::set(Connection::KeyRef key, Connection::KeyRef value, long long expire, ExpireType expire_type,  SetType set_type) {
        bool was_set;
        return set(key, value, expire, expire_type, set_type, was_set);
    }

    /* Set the string value of a key */
    bool Connection::set(Connection::KeyRef key, Connection::KeyRef value, long long expire, ExpireType expire_type,  SetType set_type, bool& was_set) {
        if(redis_version >= 20612) {
            Reply reply;
            get_prefixed_key(key);
            bool ret;
            if(expire_type == ExpireType::SEC) {
                if(set_type == SetType::IF_EXIST) {
                    ret = run_command(reply, "SET %b %b EX %lli XX", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size(), expire);
                }
                else if(set_type == SetType::IF_NOT_EXIST) {
                    ret = run_command(reply, "SET %b %b EX %lli NX", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size(), expire);
                }
                else {
                    ret = run_command(reply, "SET %b %b EX %lli", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size(), expire);
                }
            }
            else if(expire_type == ExpireType::MSEC) {
                if(set_type == SetType::IF_EXIST) {
                    ret = run_command(reply, "SET %b %b PX %lli XX", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size(), expire);
                }
                else if(set_type == SetType::IF_NOT_EXIST) {
                    ret = run_command(reply, "SET %b %b PX %lli NX", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size(), expire);
                }
                else {
                    ret = run_command(reply, "SET %b %b PX %lli", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size(), expire);
                }
            }
            else {
                if(set_type == SetType::IF_EXIST) {
                    ret = run_command(reply, "SET %b %b XX", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size());
                }
                else if(set_type == SetType::IF_NOT_EXIST) {
                    ret = run_command(reply, "SET %b %b NX", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size());
                }
                else {
                    ret = run_command(reply, "SET %b %b", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size());
                }
            }
            if(ret) {
                was_set = reply->type != REDIS_REPLY_NIL;
                return true;
            }
            return false;
        }
        else {
            Reply reply;
            get_prefixed_key(key);
            if(
                    ((expire_type == ExpireType::SEC ||
                    expire_type == ExpireType::MSEC)
                    &&
                    set_type == SetType::IF_NOT_EXIST)
                    ||
                    set_type == SetType::IF_EXIST
            ) {
                //TODO: We can make a multi analogue
                err = Error::COMMAND_UNSUPPORTED;
                return false;
            }
            else if(expire_type == ExpireType::MSEC) {
                if(run_command(reply, "PSETEX %b %lli %b", prefixed_key.c_str(), prefixed_key.size(), expire, value.c_str(), value.size())) {
                    was_set = true;
                    return true;
                }
                return false;
            }
            else if(expire_type == ExpireType::SEC) {
                if(run_command(reply, "SETEX %b %lli %b", prefixed_key.c_str(), prefixed_key.size(), expire, value.c_str(), value.size())) {
                    was_set = true;
                    return true;
                }
                return false;
            }
            else if(set_type == SetType::IF_NOT_EXIST) {
                if(run_command(reply, "SETNX %b %b", prefixed_key.c_str(), prefixed_key.size(), value.c_str(), value.size())) {
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
//    bool Connection::switch_db(long long db_num);
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
        if(!section.empty() && redis_version < 20600) {
            return false;
            //TODO : Pretty error message
        }
        Reply reply;
        if(run_command(reply, "INFO %s", section.c_str())) {
            info_data = reply->str;
            return true;
        }
        return false;
    }

    /* Get information and statistics about the server */
    bool Connection::info(Key& info_data) {
        return info("", info_data);
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
//        bool scan(VAL cursor /*, [MATCH pattern] */ /*, [COUNT count] */);


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
        get_prefixed_key(key);
        return run_command("SADD %b %b", prefixed_key.c_str(), prefixed_key.size(), member.c_str(), member.size());
    }

    /* Add one or more members to a set */
    bool Connection::sadd(KeyRef key, KeyRef member, bool& was_added) {
        Reply reply;
        get_prefixed_key(key);
        if(run_command(reply, "SADD %b %b", prefixed_key.c_str(), prefixed_key.size(), member.c_str(), member.size())) {
            was_added = reply->integer != 0;
            return true;
        }
        return false;
    }

    /* Add one or more members to a set */
    bool Connection::sadd(KeyRef key, KeyVecRef members) {
        if(redis_version < 20400) {
            bool res = true;
            for(size_t i=0; i<members.size(); i++) {
                res = sadd(key, members[i]) && res;
            }
            return res;
        }
        std::vector<size_t> sizes(members.size()+2);
        std::vector<const char*> commands(members.size()+2);
        get_prefixed_key(key);
        commands[0] = "SADD";
        sizes[0] = 4;
        commands[1] = prefixed_key.c_str();
        sizes[1] = prefixed_key.size();
        for(size_t i=0; i<members.size(); i++) {
            commands[i+2] = members[i].c_str();
            sizes[i+2] = members[i].size();
        }
        return run_command(commands, sizes);
    }

    /* Get the number of members in a set */
//        bool scard(KeyRef key);

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
        Reply reply;
        get_prefixed_keys(keys);
        for(size_t i=0; i<key_vec_ptr->size(); i++) {
            command_parts_c_strings[i+1] = (*key_vec_ptr)[i].c_str();
            sizes[i+1] = (*key_vec_ptr)[i].size();
        }
        if(run_command(reply, command_parts_c_strings, sizes)) {
            redis_assert(reply->type == REDIS_REPLY_ARRAY);
            for(size_t i=0; i < reply->elements; i++) {
                redis_assert(reply->element[i]->type == REDIS_REPLY_STRING);
                result.push_back(reply->element[i]->str);
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
//        bool smembers(KeyRef key);

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
