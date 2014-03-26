#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <macro.hpp>
#include "connection_param.hpp"

namespace Redis {
    class Connection {
    public:
        //Some empiric value after which library will reject commands
        static constexpr size_t max_key_count_per_command = 50000;
        static constexpr long default_scan_count = 10; //defaulted by redis (2.8 at least)
        typedef std::string Key;
        typedef const std::string& KeyRef;
        typedef std::vector<Key> KeyVec;
        typedef const std::vector<Key>& KeyVecRef;
        typedef unsigned long long Id;
        friend class Pool;
        friend class PoolWrapper;

        enum class Error {
            NONE = 0,
            CONTEXT_IS_NULL,
            REPLY_IS_NULL,
            FLOAT_OUT_OF_RANGE,
            DOUBLE_OUT_OF_RANGE,
            HIREDIS_IO,
            HIREDIS_EOF,
            HIREDIS_PROTOCOL,
            HIREDIS_OOM,
            HIREDIS_OTHER,
            HIREDIS_UNKNOWN,
            COMMAND_UNSUPPORTED,
            UNEXPECTED_INFO_RESULT,
            REPLY_ERR,
            TOO_LONG_COMMAND
        };
        enum class BitOperation { AND, OR, XOR, NOT };
        enum class Bit { ONE, ZERO };
        enum class ExpireType { NONE, SEC, MSEC };
        enum class SetType { ALWAYS, IF_EXIST, IF_NOT_EXIST };
        enum class ListInsertType { AFTER, BEFORE };



        Connection &operator=(const Connection &other) = delete;
        Connection(const Connection &other) = delete;
        Connection(Connection&& other) : d(nullptr) {
            std::swap(d, other.d);
        }
        Connection(const ConnectionParam &connection_param);
        Connection(const std::string &host = ConnectionParam::get_default_connection_param().host,
                unsigned int port = ConnectionParam::get_default_connection_param().port,
                const std::string& password = ConnectionParam::get_default_connection_param().password,
                unsigned int db_num = ConnectionParam::get_default_connection_param().db_num,
                const std::string &prefix = ConnectionParam::get_default_connection_param().prefix,
                unsigned int connect_timeout_ms = ConnectionParam::get_default_connection_param().connect_timeout_ms,
                unsigned int operation_timeout = ConnectionParam::get_default_connection_param().operation_timeout_ms,
                bool reconnect_on_failure = ConnectionParam::get_default_connection_param().reconnect_on_failure,
                bool throw_on_error = ConnectionParam::get_default_connection_param().throw_on_error
        );
        ~Connection();
        bool is_available();
        std::string get_error();
        Error get_errno();
        inline unsigned int get_version();
        Id get_id();

        //Redis commands

        /***************************************************************/
        /***************************************************************/
        /*********************** string commands ***********************/
        /***************************************************************/
        /***************************************************************/

        /* Append a value to a key */
        bool append(KeyRef key, KeyRef value, long long& result_length);

        /* Append a value to a key */
        bool append(KeyRef key, KeyRef value);

        /* Count set bits in a string */
        bool bitcount(KeyRef key, unsigned int start, unsigned int end, long long& result);

        /* Count set bits in a string */
        bool bitcount(KeyRef key, long long& result);

        /* Perform bitwise operations between strings */
        bool bitop(BitOperation operation, KeyRef destkey, KeyVecRef keys, long long& size_of_dest);

        /* Perform bitwise operations between strings */
        bool bitop(BitOperation operation, KeyRef destkey, KeyVecRef keys);

        /* Find first bit set or clear in a subsstring defined by start and end*/
        bool bitpos(KeyRef key, Bit bit, unsigned int start, unsigned int end, long long& result);

        /* Find first bit set or clear in a string */
        bool bitpos(KeyRef key, Bit bit, long long& result);

        /* Decrement the integer value of a key by one */
        bool decr(KeyRef key, long long& result_value);

        /* Decrement the integer value of a key by one */
        bool decr(KeyRef key);

        /* Decrement the integer value of a key by the given number */
        bool decrby(KeyRef key, long long decrement, long long& result_value);

        /* Decrement the integer value of a key by the given number */
        bool decrby(KeyRef key, long long decrement);

        /* Get the value of a key */
        bool get(KeyRef key, Key& result);

        /* Get the value of a bunch of keys */
        bool get(KeyVecRef keys, KeyVec& vals);
#if __cplusplus > 199711L
        template <class PairIterator>
        bool get(PairIterator begin, PairIterator end) {
            std::vector<std::reference_wrapper<const Key>> keys;
            PairIterator begin_copy = begin;
            while(begin != end) {
                keys.emplace_back(begin->first);
                begin++;
            }
            if(!get(keys)) {
                return false;
            }
            Key result;
            size_t index = 0;
            while(begin_copy != end) {
                if (!fetch_get_result(begin_copy->second, index)) {
                    return false;
                }
                begin_copy++;
            }
        }
        template <class KeyIterator, class InsertIterator>
        bool get(KeyIterator key_begin, KeyIterator key_end, InsertIterator ins_it) {
            std::vector<std::reference_wrapper<const Key>> keys(key_begin, key_end);
            if(!get(keys)) {
                return false;
            }
            Key result;
            size_t index = 0;
            while(fetch_get_result(result, index)) {
                ins_it = result;
                index++;
            }
        }
#endif
        /* Returns the bit value at offset in the string value stored at key */
        bool getbit(KeyRef key, long long offset, Bit& result);

        /* Get a substring of the string stored at a key */
        bool getrange(KeyRef key, long long start, long long end, Key& result);

        /* Set the string value of a key and return its old value */
        bool getset(KeyRef key, KeyRef value, Key& old_value);

        /* Increment the integer value of a key by one */
        bool incr(KeyRef key, long long& result_value);

        /* Increment the integer value of a key by one */
        bool incr(KeyRef key);

        /* Increment the integer value of a key by the given amount */
        bool incrby(KeyRef key, long long increment, long long& result_value);

        /* Increment the integer value of a key by the given amount */
        bool incrby(KeyRef key, long long increment);

        /* Increment the float value of a key by the given amount */
        bool incrbyfloat(KeyRef key, float increment, float& result_value);

        /* Increment the float value of a key by the given amount */
        bool incrbyfloat(KeyRef key, double increment, double& result_value);

        /* Increment the float value of a key by the given amount */
        bool incrbyfloat(KeyRef key, float increment);

        /* Increment the float value of a key by the given amount */
        bool incrbyfloat(KeyRef key, double increment);

#if __cplusplus > 199711L
        template <class KeyIterator, class ValueIterator>
        bool set(KeyIterator key_begin, KeyIterator key_end, ValueIterator value_begin, ValueIterator value_end, SetType set_type = SetType::ALWAYS) {
            std::vector<std::reference_wrapper<const Key>> keys(key_begin, key_end);
            std::vector<std::reference_wrapper<const Key>> values(value_begin, value_end);
            return set(keys, values, set_type);
        }

        template <class PairIterator>
        bool set(PairIterator begin, PairIterator end, SetType set_type = SetType::ALWAYS) {
            std::vector<std::reference_wrapper<const Key>> keys, values;
            while(begin != end) {
                keys.emplace_back(begin->first);
                values.emplace_back(begin->second);
                begin++;
            }
            return set(keys, values, set_type);
        }

        bool set(const std::vector<std::reference_wrapper<const Key>>& keys, const std::vector<std::reference_wrapper<const Key>>values, SetType set_type = SetType::ALWAYS);
#endif
        bool set(KeyVecRef keys, KeyVecRef values, SetType set_type = SetType::ALWAYS);
        bool set(const std::map<Key, Key>& key_value_map, SetType set_type = SetType::ALWAYS);

        /* Set the string value of a key */
        bool set(const char* key, const char* value, SetType set_type = SetType::ALWAYS, long long expire = 0, ExpireType expire_type = ExpireType::NONE);
        bool set(const char* key, const char* value,  SetType set_type, bool& was_set, long long expire = 0, ExpireType expire_type = ExpireType::NONE);
        bool set(KeyRef key, KeyRef value, SetType set_type = SetType::ALWAYS, long long expire = 0, ExpireType expire_type = ExpireType::NONE);
        bool set(KeyRef key, KeyRef value,  SetType set_type, bool& was_set, long long expire = 0, ExpireType expire_type = ExpireType::NONE);

        /* Sets or clears the bit at offset in the string value stored at key */
        bool set_bit(KeyRef key,long long offset, Bit value, Bit& original_bit);

        /* Sets or clears the bit at offset in the string value stored at key */
        bool set_bit(KeyRef key,long long offset, Bit value);

        /* Set the value and expiration of a key */
        bool setex(KeyRef key, KeyRef value, long long seconds);

        /* Set the value of a key, only if the key does not exist */
        bool setnx(KeyRef key, KeyRef value, bool& was_set);

        /* Set the value of a key, only if the key does not exist */
        bool setnx(KeyRef key, KeyRef value);

        /* Overwrite part of a string at key starting at the specified offset */
        bool setrange(KeyRef key,long long offset, KeyRef value, long long& result_length);

        /* Overwrite part of a string at key starting at the specified offset */
        bool setrange(KeyRef key,long long offset, KeyRef value);

        /* Get the length of the value stored in a key */
        bool strlen(KeyRef key, long long& key_length);


        /*******************************************************************/
        /*******************************************************************/
        /*********************** connection commands ***********************/
        /*******************************************************************/
        /*******************************************************************/

        //NOTE: Disable auth. It's dedicated to internals
        /* Authenticate to the server */
        //bool auth(KeyRef password);

        /* Echo the given string. Return message will contain a copy of message*/
        bool echo(KeyRef message, Key& return_message);

        /* Echo the given string. In fact does nothing. Just for a full interface. */
        bool echo(KeyRef message);

        /* Ping the server */
        bool ping();

        /* Close the connection */
        bool quit();

        /* Close the connection */
        bool disconnect();

        //NOTE: We passed db in connection. Disable db switching
        /* Change the selected database for the current connection */
        //bool select(long long db_num);

        /*******************************************************************/
        /*******************************************************************/
        /************************* server commands *************************/
        /*******************************************************************/
        /*******************************************************************/

        /* Asynchronously rewrite the append-only file */
        bool bgrewriteaof();

        /* Asynchronously save the dataset to disk */
        bool bgsave();

        /* Kill the connection of a client */
        bool client_kill(KeyRef ip_and_port);

        /* Get the list of client connections */
        //bool client_list(); //TODO : implement

        /* Get the current connection name */
        //bool client getname(); //TODO : implement

        /* Stop processing commands from clients for some time */
        //bool client pause(VAL timeout); //TODO : implement

        /* Set the current connection name */
        //bool client setname(VAL connection-name); //TODO : implement

        /* Get the value of a configuration parameter */
        bool config_get(KeyRef parameter, Key& result);

        /* Rewrite the configuration file with the in memory configuration */
        bool config_rewrite();

        /* Set a configuration parameter to the given value */
        bool config_set(KeyRef parameter, KeyRef value);

        /* Reset the stats returned by INFO */
        bool config_resetstat();

        /* Return the number of keys in the selected database */
        bool dbsize(long long* result);

        /* Get debugging information about a key */
        bool debug_object(KeyRef key, Key& info);

        /* Make the server crash */
        bool debug_segfault();

        /* Remove all keys from all databases */
        bool flushall();

        /* Remove all keys from the current database */
        bool flushdb();

        /* Get information and statistics about the server */
        bool info(KeyRef section, Key& info_data);

        /* Get information and statistics about the server */
        bool info(Key& info_data);

        /* Get the UNIX time stamp of the last successful save to disk */
        bool lastsave(time_t& result);

        /* Synchronously save the dataset to disk */
        bool save();

        /* Synchronously save the dataset to disk and then shut down the server */
        bool shutdown(bool save = true);

        /* Make the server a slave of another instance*/
        bool slaveof(KeyRef host, unsigned int port);

        /* Promote server as master */
        bool make_master();

        /* Manages the Redis slow queries log */
        //bool slowlog(VAL subcommand, bool argument = false); //TODO : implement

        /* Return the current server time */
        bool time(long long& seconds, long long& microseconds);


        /*******************************************************************/
        /*******************************************************************/
        /************************** list commands **************************/
        /*******************************************************************/
        /*******************************************************************/

        /* Remove and get the first element in a list, or block until one is available */
        bool blpop(KeyVecRef keys, long long timeout, Key& chosen_key, Key& value);

        /* Remove and get the last element in a list, or block until one is available */
        bool brpop(KeyVecRef keys, long long timeout, Key& chosen_key, Key& value);

        /* Pop a value from a list, push it to another list and return it; or block until one is available */
        bool brpoplpush(KeyRef source, KeyRef destination, long long timeout, Key& result);

        /* Pop a value from a list, push it to another list and return it; or block until one is available */
        bool brpoplpush(KeyRef source, KeyRef destination, long long timeout);

        /* Get an element from a list by its index */
        bool lindex(KeyRef key, long long index);

        /* Insert an element before or after another element in a list */
        bool linsert(KeyRef key, ListInsertType insert_type, KeyRef pivot, KeyRef value);

        /* Insert an element before or after another element in a list */
        bool linsert(KeyRef key, ListInsertType insert_type, KeyRef pivot, KeyRef value, long long& list_size);

        /* Get the length of a list */
        bool llen(KeyRef key, long long& length);

        /* Remove and get the first element in a list */
        bool lpop(KeyRef key, Key& value);

        /* Remove and get the first element in a list */
        bool lpop(KeyRef key);

        /* Prepend one or multiple values to a list */
        bool lpush(KeyRef key, KeyRef value, long long& list_length);

        /* Prepend one or multiple values to a list */
        bool lpush(KeyRef key, KeyRef value);

        /* Prepend one or multiple values to a list */
        bool lpush(KeyRef key, KeyVecRef values, long long& list_length);

        /* Prepend one or multiple values to a list */
        bool lpush(KeyRef key, KeyVecRef values);

        /* Prepend a value to a list, only if the list exists */
        bool lpushx(KeyRef key, KeyRef value, long long& list_length);

        /* Prepend a value to a list, only if the list exists */
        bool lpushx(KeyRef key, KeyRef value);

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


        /*******************************************************************/
        /*******************************************************************/
        /************************ generic commands *************************/
        /*******************************************************************/
        /*******************************************************************/

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
        bool scan(unsigned long long& cursor, KeyVec& result_keys, KeyRef pattern = "*", long count = default_scan_count);


        /*********************************************************************/
        /*********************************************************************/
        /*********************** transactions commands ***********************/
        /*********************************************************************/
        /*********************************************************************/

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


        /*******************************************************************/
        /*******************************************************************/
        /*********************** scripting commands ************************/
        /*******************************************************************/
        /*******************************************************************/

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


        /*******************************************************************/
        /*******************************************************************/
        /************************** hash commands **************************/
        /*******************************************************************/
        /*******************************************************************/

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


        /*******************************************************************/
        /*******************************************************************/
        /************************* pubsub commands *************************/
        /*******************************************************************/
        /*******************************************************************/

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


        /*******************************************************************/
        /*******************************************************************/
        /*************************** set commands **************************/
        /*******************************************************************/
        /*******************************************************************/

        /* Add one or more members to a set */
        bool sadd(KeyRef key, KeyRef member);

        /* Add one or more members to a set */
        bool sadd(KeyRef key, KeyVecRef members);

        /* Add one or more members to a set */
        bool sadd(KeyRef key, KeyRef member, bool& was_added);

        /* Add one or more members to a set */
        bool sadd(KeyRef key, KeyVecRef members, long& num_of_added);

        /* Get the number of members in a set */
        bool scard(KeyRef key, long long& result_size);

        /* Subtract multiple sets */
//        bool sdiff(KeyVecRef keys);

        /* Subtract multiple sets and store the resulting set in a key */
//        bool sdiffstore(VAL destination, KeyVecRef keys);

        /* Intersect multiple sets */
        bool sinter(KeyVecRef keys, KeyVec& result);

        /* Intersect multiple sets and store the resulting set in a key */
//        bool sinterstore(VAL destination, KeyVecRef keys);

        /* Determine if a given value is a member of a set */
//        bool sismember(KeyRef key, VAL member);

        /* Get all the members in a set */
        bool smembers(KeyRef key, KeyVec& resul);

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


        /*******************************************************************/
        /*******************************************************************/
        /*********************** sorted_set commands ***********************/
        /*******************************************************************/
        /*******************************************************************/

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
    private:
        //Pimpl
        class Implementation;
        Implementation* d;

        //Only methods used by template public functions
        bool fetch_get_result(Key& result, size_t index);
        bool get(const std::vector<std::reference_wrapper<const Key>>& keys);
    };
}