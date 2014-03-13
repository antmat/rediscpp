#include <cstring>
#include "connection.hpp"
#include "exception.hpp"
#define get_prefixed_key(key) const std::string prefixed_key = has_prefix() ? add_prefix_to_key(key) : key
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
        std::string command("BITCOUNT ");
        switch (operation) {
            case BitOperation::AND:
                command += "AND \"";
                break;
            case BitOperation::OR:
                command += "OR \"";
                break;
            case BitOperation::NOT:
                command += "NOT \"";
                redis_assert(keys.size() == 1);
                break;
            case BitOperation::XOR:
                command += "XOR \"";
                break;
            default: //Fool proof of explicit casting BitOperation to integer and asigning improper value;
                redis_assert_unreachable();
                break;
        }
        get_prefixed_key(destkey);
        command += prefixed_key+"\" ";
        for(size_t i=0; i < keys.size(); i++) {
            command += '"'+keys[i]+"\" ";
        }
        Reply reply;
        if(run_command(reply, command.c_str())) {
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

//    /* Get the values of all the given keys */
//    bool Connection::mget(Connection::KeyVecRef keys, Connection::KeyVec& result) {
//        return get(keys, result);
//    }
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
            std::string command("SET \""+prefixed_key+"\" \""+value+"\" ");
            if(expire_type == ExpireType::SEC) {
                command += "EX " +std::to_string(expire)+' ';
            }
            else if(expire_type == ExpireType::MSEC) {
                command += "PX " +std::to_string(expire)+' ';
            }
            if(set_type == SetType::IF_EXIST) {
                command += "XX ";
            }
            else if(set_type == SetType::IF_NOT_EXIST) {
                command += "NX ";
            }
            if(run_command(command.c_str())) {
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


}
