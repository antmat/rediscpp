#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include "macro.hpp"
#include "connection_param.hpp"

namespace Redis {
    class Connection {
    public:
        //Some empiric value after which library will reject commands
        static constexpr size_t max_key_count_per_command = 50000;
        static constexpr long default_scan_count = 10; //defaulted by redis (2.8 at least)
        typedef std::string Key;
        typedef std::vector<Key> KeyVec;
        typedef std::vector<std::reference_wrapper<const Key>> KeyRefVec;
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
        enum class Bit { ZERO, ONE };
        enum class ExpireType { NONE, SEC, MSEC };
        enum class SetType { ALWAYS, IF_EXIST, IF_NOT_EXIST };
        enum class ListInsertType { AFTER, BEFORE };



        Connection &operator=(const Connection &other) = delete;
        Connection(const Connection &other) = delete;
        Connection(Connection&& other) : d(nullptr) {
            std::swap(d, other.d);
        }
        Connection& operator=(Connection&& other) {
            std::swap(d, other.d);
            return *this;
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
        unsigned int get_version();
        Id get_id();

        //Redis commands

        /***************************************************************/
        /***************************************************************/
        /*********************** string commands ***********************/
        /***************************************************************/
        /***************************************************************/

        /* Append a value to a key */
        bool append(const Key& key, const Key& value, long long& result_length);

        /* Append a value to a key */
        bool append(const Key& key, const Key& value);

        /* Count set bits in a string */
        bool bitcount(const Key& key, unsigned int start, unsigned int end, long long& result);

        /* Count set bits in a string */
        bool bitcount(const Key& key, long long& result);

        /* Perform bitwise operations between strings */
        bool bitop(BitOperation operation, const Key& destkey, const KeyVec& keys, long long& size_of_dest);

        /* Perform bitwise operations between strings */
        bool bitop(BitOperation operation, const Key& destkey, const KeyVec& keys);

        /* perform and operation between strings and store result in destkey */
        inline bool bit_and(const Key& destkey, const KeyVec& keys, long long& size_of_dest) {
            return bitop(BitOperation::AND, destkey, keys, size_of_dest);
        }

        /* perform and operation between strings and store result in destkey */
        inline bool bit_or(const Key& destkey, const KeyVec& keys, long long& size_of_dest) {
            return bitop(BitOperation::OR, destkey, keys, size_of_dest);
        }

        /* perform and operation between strings and store result in destkey */
        inline bool bit_xor(const Key& destkey, const KeyVec& keys, long long& size_of_dest) {
            return bitop(BitOperation::XOR, destkey, keys, size_of_dest);
        }

        /* perform and operation between strings and store result in destkey */
        bool bit_not(const Key& destkey, const Key& key, long long& size_of_dest);

        /* perform and operation between strings and store result in destkey */
        inline bool bit_and(const Key& destkey, const KeyVec& keys) {
            return bitop(BitOperation::AND, destkey, keys);
        }

        /* perform and operation between strings and store result in destkey */
        inline bool bit_or(const Key& destkey, const KeyVec& keys) {
            return bitop(BitOperation::OR, destkey, keys);
        }

        /* perform and operation between strings and store result in destkey */
        inline bool bit_xor(const Key& destkey, const KeyVec& keys) {
            return bitop(BitOperation::XOR, destkey, keys);
        }

        /* perform and operation between strings and store result in destkey */
        bool bit_not(const Key& destkey, const Key& key);

#if __cplusplus > 199711L
        /* Perform bitwise operations between strings */
        bool bitop(BitOperation operation, const Key& destkey, const KeyRefVec& keys, long long& size_of_dest);

        /* Perform bitwise operations between strings */
        bool bitop(BitOperation operation, const Key& destkey, const KeyRefVec& keys);

        /* Perform bitwise operations between strings */
        template <class KeyIterator>
        inline bool bitop(BitOperation operation, const Key& destkey, KeyIterator begin, KeyIterator end, long long& size_of_dest) {
            KeyRefVec keys(begin, end);
            return bitop(operation, destkey, keys, size_of_dest);
        }

        template <class KeyIterator>
        inline bool bitop(BitOperation operation, const Key& destkey, KeyIterator begin, KeyIterator end) {
            KeyRefVec keys(begin, end);
            return bitop(operation, destkey, keys);
        }


        /* perform and operation between strings and store result in destkey */
        template <class KeyIterator>
        inline bool bit_and(const Key& destkey, KeyIterator begin, KeyIterator end, long long& size_of_dest) {
            KeyRefVec keys(begin, end);
            return bitop(BitOperation::AND, destkey, keys, size_of_dest);
        }

        /* perform and operation between strings and store result in destkey */
        template <class KeyIterator>
        inline bool bit_or(const Key& destkey, KeyIterator begin, KeyIterator end, long long& size_of_dest) {
            KeyRefVec keys(begin, end);
            return bitop(BitOperation::OR, destkey, keys, size_of_dest);
        }

        /* perform and operation between strings and store result in destkey */
        template <class KeyIterator>
        inline bool bit_xor(const Key& destkey, KeyIterator begin, KeyIterator end, long long& size_of_dest) {
            KeyRefVec keys(begin, end);
            return bitop(BitOperation::XOR, destkey, keys, size_of_dest);
        }

        /* perform and operation between strings and store result in destkey */
        template <class KeyIterator>
        inline bool bit_and(const Key& destkey, KeyIterator begin, KeyIterator end) {
            KeyRefVec keys(begin, end);
            return bitop(BitOperation::AND, destkey, keys);
        }

        /* perform and operation between strings and store result in destkey */
        template <class KeyIterator>
        inline bool bit_or(const Key& destkey, KeyIterator begin, KeyIterator end) {
            KeyRefVec keys(begin, end);
            return bitop(BitOperation::OR, destkey, keys);
        }

        /* perform and operation between strings and store result in destkey */
        template <class KeyIterator>
        inline bool bit_xor(const Key& destkey, KeyIterator begin, KeyIterator end) {
            KeyRefVec keys(begin, end);
            return bitop(BitOperation::XOR, destkey, keys);
        }

#endif
        /* Find first bit set or clear in a subsstring defined by start and end*/
        bool bitpos(const Key& key, Bit bit, unsigned int start, long long& result);

        /* Find first bit set or clear in a subsstring defined by start and end*/
        bool bitpos(const Key& key, Bit bit, unsigned int start, unsigned int end, long long& result);

        /* Find first bit set or clear in a string */
        bool bitpos(const Key& key, Bit bit, long long& result);

        /* Decrement the integer value of a key by one */
        bool decr(const Key& key, long long& result_value);

        /* Decrement the integer value of a key by one */
        bool decr(const Key& key);

        /* Decrement the integer value of a key by the given number */
        bool decrby(const Key& key, long long decrement, long long& result_value);

        /* Decrement the integer value of a key by the given number */
        bool decrby(const Key& key, long long decrement);

        /* Get the value of a key */
        bool get(const Key& key, Key& result);

        /* Get the value of a bunch of keys */
        bool get(const KeyVec& keys, KeyVec& vals);
#if __cplusplus > 199711L
        template <class PairIterator>
        bool get(PairIterator begin, PairIterator end) {
            KeyRefVec keys;
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
                index++;
            }
            redis_assert(index == keys.size());
            return true;
        }
        template <class KeyIterator, class InsertIterator>
        bool get(KeyIterator key_begin, KeyIterator key_end, InsertIterator ins_it) {
            KeyRefVec keys(key_begin, key_end);
            if(!get(keys)) {
                return false;
            }
            Key result;
            size_t index = 0;
            while(fetch_get_result(result, index)) {
                ins_it = result;
                ins_it++;
                index++;
            }
            redis_assert(index == keys.size());
            return true;
        }
#endif
        /* Returns the bit value at offset in the string value stored at key */
        bool getbit(const Key& key, long long offset, Bit& result);

        /* Get a substring of the string stored at a key */
        bool getrange(const Key& key, long long start, long long end, Key& result);

        /* Set the string value of a key and return its old value */
        bool getset(const Key& key, const Key& value, Key& old_value);

        /* Increment the integer value of a key by one */
        bool incr(const Key& key, long long& result_value);

        /* Increment the integer value of a key by one */
        bool incr(const Key& key);

        /* Increment the integer value of a key by the given amount */
        bool incrby(const Key& key, long long increment, long long& result_value);

        /* Increment the integer value of a key by the given amount */
        bool incrby(const Key& key, long long increment);

        /* Increment the float value of a key by the given amount */
        bool incrbyfloat(const Key& key, float increment, float& result_value);

        /* Increment the float value of a key by the given amount */
        bool incrbyfloat(const Key& key, double increment, double& result_value);

        /* Increment the float value of a key by the given amount */
        bool incrbyfloat(const Key& key, float increment);

        /* Increment the float value of a key by the given amount */
        bool incrbyfloat(const Key& key, double increment);

#if __cplusplus > 199711L
        template <class KeyIterator, class ValueIterator>
        bool set(KeyIterator key_begin, KeyIterator key_end, ValueIterator value_begin, ValueIterator value_end, SetType set_type = SetType::ALWAYS) {
            KeyRefVec keys(key_begin, key_end);
            KeyRefVec values(value_begin, value_end);
            return set(keys, values, set_type);
        }

        template <class PairIterator>
        bool set(PairIterator begin, PairIterator end, SetType set_type = SetType::ALWAYS) {
            KeyRefVec keys, values;
            while(begin != end) {
                keys.emplace_back(begin->first);
                values.emplace_back(begin->second);
                begin++;
            }
            return set(keys, values, set_type);
        }

        bool set(const KeyRefVec& keys, const KeyRefVec& values, SetType set_type = SetType::ALWAYS);
#endif
        bool set(const KeyVec& keys, const KeyVec& values, SetType set_type = SetType::ALWAYS);
        bool set(const std::map<Key, Key>& key_value_map, SetType set_type = SetType::ALWAYS);

        /* Set the string value of a key */
        bool set(const char* key, const char* value, SetType set_type = SetType::ALWAYS, long long expire = 0, ExpireType expire_type = ExpireType::NONE);
        bool set(const char* key, const char* value,  SetType set_type, bool& was_set, long long expire = 0, ExpireType expire_type = ExpireType::NONE);
        bool set(const Key& key, const Key& value, SetType set_type = SetType::ALWAYS, long long expire = 0, ExpireType expire_type = ExpireType::NONE);
        bool set(const Key& key, const Key& value,  SetType set_type, bool& was_set, long long expire = 0, ExpireType expire_type = ExpireType::NONE);

        /* Sets or clears the bit at offset in the string value stored at key */
        bool set_bit(const Key& key,long long offset, Bit value, Bit& original_bit);

        /* Sets or clears the bit at offset in the string value stored at key */
        bool set_bit(const Key& key,long long offset, Bit value);

        /* Set the value and expiration of a key */
        bool setex(const Key& key, const Key& value, long long seconds);

        /* Set the value of a key, only if the key does not exist */
        bool setnx(const Key& key, const Key& value, bool& was_set);

        /* Set the value of a key, only if the key does not exist */
        bool setnx(const Key& key, const Key& value);

        /* Overwrite part of a string at key starting at the specified offset */
        bool setrange(const Key& key,long long offset, const Key& value, long long& result_length);

        /* Overwrite part of a string at key starting at the specified offset */
        bool setrange(const Key& key,long long offset, const Key& value);

        /* Get the length of the value stored in a key */
        bool strlen(const Key& key, long long& key_length);


        /*******************************************************************/
        /*******************************************************************/
        /*********************** connection commands ***********************/
        /*******************************************************************/
        /*******************************************************************/

        //NOTE: Disable auth. It's dedicated to internals
        /* Authenticate to the server */
        //bool auth(const Key& password);

        /* Echo the given string. Return message will contain a copy of message*/
        bool echo(const Key& message, Key& return_message);

        /* Echo the given string. In fact does nothing. Just for a full interface. */
        bool echo(const Key& message);

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
        bool client_kill(const Key& ip_and_port);

        /* Get the list of client connections */
        //bool client_list(); //TODO : implement

        /* Get the current connection name */
        //bool client getname(); //TODO : implement

        /* Stop processing commands from clients for some time */
        //bool client pause(VAL timeout); //TODO : implement

        /* Set the current connection name */
        //bool client setname(VAL connection-name); //TODO : implement

        /* Get the value of a configuration parameter */
        bool config_get(const Key& parameter, Key& result);

        /* Rewrite the configuration file with the in memory configuration */
        bool config_rewrite();

        /* Set a configuration parameter to the given value */
        bool config_set(const Key& parameter, const Key& value);

        /* Reset the stats returned by INFO */
        bool config_resetstat();

        /* Return the number of keys in the selected database */
        bool dbsize(long long* result);

        /* Get debugging information about a key */
        bool debug_object(const Key& key, Key& info);

        /* Make the server crash */
        bool debug_segfault();

        /* Remove all keys from all databases */
        bool flushall();

        /* Remove all keys from the current database */
        bool flushdb();

        /* Get information and statistics about the server */
        bool info(const Key& section, Key& info_data);

        /* Get information and statistics about the server */
        bool info(Key& info_data);

        /* Get the UNIX time stamp of the last successful save to disk */
        bool lastsave(time_t& result);

        /* Synchronously save the dataset to disk */
        bool save();

        /* Synchronously save the dataset to disk and then shut down the server */
        bool shutdown(bool save = true);

        /* Make the server a slave of another instance*/
        bool slaveof(const Key& host, unsigned int port);

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
        bool blpop(const KeyVec& keys, long long timeout, Key& chosen_key, Key& value);

        /* Remove and get the last element in a list, or block until one is available */
        bool brpop(const KeyVec& keys, long long timeout, Key& chosen_key, Key& value);

        /* Pop a value from a list, push it to another list and return it; or block until one is available */
        bool brpoplpush(const Key& source, const Key& destination, long long timeout, Key& result);

        /* Pop a value from a list, push it to another list and return it; or block until one is available */
        bool brpoplpush(const Key& source, const Key& destination, long long timeout);

        /* Get an element from a list by its index */
        bool lindex(const Key& key, long long index);

        /* Insert an element before or after another element in a list */
        bool linsert(const Key& key, ListInsertType insert_type, const Key& pivot, const Key& value);

        /* Insert an element before or after another element in a list */
        bool linsert(const Key& key, ListInsertType insert_type, const Key& pivot, const Key& value, long long& list_size);

        /* Get the length of a list */
        bool llen(const Key& key, long long& length);

        /* Remove and get the first element in a list */
        bool lpop(const Key& key, Key& value);

        /* Remove and get the first element in a list */
        bool lpop(const Key& key);

        /* Prepend one or multiple values to a list */
        bool lpush(const Key& key, const Key& value, long long& list_length);

        /* Prepend one or multiple values to a list */
        bool lpush(const Key& key, const Key& value);

        /* Prepend one or multiple values to a list */
        bool lpush(const Key& key, const KeyVec& values, long long& list_length);

        /* Prepend one or multiple values to a list */
        bool lpush(const Key& key, const KeyVec& values);

        /* Prepend a value to a list, only if the list exists */
        bool lpushx(const Key& key, const Key& value, long long& list_length);

        /* Prepend a value to a list, only if the list exists */
        bool lpushx(const Key& key, const Key& value);

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


        /*******************************************************************/
        /*******************************************************************/
        /************************ generic commands *************************/
        /*******************************************************************/
        /*******************************************************************/

        /* Delete a key */
        bool del(const Key& key);
        /* Delete a key */
        bool del(const Key& key, bool& was_deleted);


        /* Return a serialized version of the value stored at the specified key. */
//        bool dump(const Key& key);

        /* Determine if a key exists */
//        bool exists(const Key& key);

        /* Set a key's time to live in seconds */
//        bool expire(const Key& key, VAL seconds);

        /* Set the expiration for a key as a UNIX timestamp */
//        bool expireat(const Key& key, VAL timestamp);

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

        /* Set a key's time to live in milliseconds */
//        bool pexpire(const Key& key, VAL milliseconds);

        /* Set the expiration for a key as a UNIX timestamp specified in milliseconds */
//        bool pexpireat(const Key& key, VAL milliseconds-timestamp);

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
//        bool ttl(const Key& key);

        /* Determine the type stored at key */
//        bool type(const Key& key);

        /* Incrementally iterate the keys space */
        bool scan(unsigned long long& cursor, KeyVec& result_keys, const Key& pattern = "*", long count = default_scan_count);


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
//        bool watch(const KeyVec& keys);


        /*******************************************************************/
        /*******************************************************************/
        /*********************** scripting commands ************************/
        /*******************************************************************/
        /*******************************************************************/

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


        /*******************************************************************/
        /*******************************************************************/
        /************************** hash commands **************************/
        /*******************************************************************/
        /*******************************************************************/

        /* Delete one or more hash fields */
//        bool hdel(const Key& key, VAL field [field ...]);

        /* Determine if a hash field exists */
//        bool hexists(const Key& key, VAL field);

        /* Get the value of a hash field */
//        bool hget(const Key& key, VAL field);

        /* Get all the fields and values in a hash */
//        bool hgetall(const Key& key);

        /* Increment the integer value of a hash field by the given number */
//        bool hincrby(const Key& key, VAL field, VAL increment);

        /* Increment the float value of a hash field by the given amount */
//        bool hincrbyfloat(const Key& key, VAL field, VAL increment);

        /* Get all the fields in a hash */
//        bool hkeys(const Key& key);

        /* Get the number of fields in a hash */
//        bool hlen(const Key& key);

        /* Get the values of all the given hash fields */
//        bool hmget(const Key& key, VAL field [field ...]);

        /* Set multiple hash fields to multiple values */
//        bool hmset(const Key& key, VAL field value [field value ...]);

        /* Set the string value of a hash field */
//        bool hset(const Key& key, VAL field, VAL value);

        /* Set the value of a hash field, only if the field does not exist */
//        bool hsetnx(const Key& key, VAL field, VAL value);

        /* Get all the values in a hash */
//        bool hvals(const Key& key);

        /* Incrementally iterate hash fields and associated values */
//        bool hscan(const Key& key, VAL cursor /*, [MATCH pattern] */ /*, [COUNT count] */);


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
        bool sadd(const Key& key, const Key& member);

        /* Add one or more members to a set */
        bool sadd(const Key& key, const KeyVec& members);

        /* Add one or more members to a set */
        bool sadd(const Key& key, const Key& member, bool& was_added);

        /* Add one or more members to a set */
        bool sadd(const Key& key, const KeyVec& members, long& num_of_added);

        /* Get the number of members in a set */
        bool scard(const Key& key, long long& result_size);

        /* Subtract multiple sets */
//        bool sdiff(const KeyVec& keys);

        /* Subtract multiple sets and store the resulting set in a key */
//        bool sdiffstore(VAL destination, const KeyVec& keys);

        /* Intersect multiple sets */
        bool sinter(const KeyVec& keys, KeyVec& result);

        /* Intersect multiple sets and store the resulting set in a key */
//        bool sinterstore(VAL destination, const KeyVec& keys);

        /* Determine if a given value is a member of a set */
//        bool sismember(const Key& key, VAL member);

        /* Get all the members in a set */
        bool smembers(const Key& key, KeyVec& resul);

        /* Move a member from one set to another */
//        bool smove(VAL source, VAL destination, VAL member);

        /* Remove and return a random member from a set */
//        bool spop(const Key& key);

        /* Get one or multiple random members from a set */
//        bool srandmember(const Key& key, bool count = false);

        /* Remove one or more members from a set */
//        bool srem(const Key& key, VAL member [member ...]);

        /* Add multiple sets */
//        bool sunion(const KeyVec& keys);

        /* Add multiple sets and store the resulting set in a key */
//        bool sunionstore(VAL destination, const KeyVec& keys);

        /* Incrementally iterate Set elements */
//        bool sscan(const Key& key, VAL cursor /*, [MATCH pattern] */ /*, [COUNT count] */);


        /*******************************************************************/
        /*******************************************************************/
        /*********************** sorted_set commands ***********************/
        /*******************************************************************/
        /*******************************************************************/

        /* Add one or more members to a sorted set, or update its score if it already exists */
//        bool zadd(const Key& key, VAL score member [score member ...]);

        /* Get the number of members in a sorted set */
//        bool zcard(const Key& key);

        /* Count the members in a sorted set with scores within the given values */
//        bool zcount(const Key& key, VAL min, VAL max);

        /* Increment the score of a member in a sorted set */
//        bool zincrby(const Key& key, VAL increment, VAL member);

        /* Intersect multiple sorted sets and store the resulting sorted set in a new key */
//        bool zinterstore(VAL destination, VAL numkeys, const KeyVec& keys /*, [WEIGHTS weight [weight ...]] */ /*, [AGGREGATE SUM|MIN|MAX] */);

        /* Return a range of members in a sorted set, by index */
//        bool zrange(const Key& key, VAL start, VAL stop, bool withscores = false);

        /* Return a range of members in a sorted set, by score */
//        bool zrangebyscore(const Key& key, VAL min, VAL max, bool withscores = false /*, [LIMIT offset count] */);

        /* Determine the index of a member in a sorted set */
//        bool zrank(const Key& key, VAL member);

        /* Remove one or more members from a sorted set */
//        bool zrem(const Key& key, VAL member [member ...]);

        /* Remove all members in a sorted set within the given indexes */
//        bool zremrangebyrank(const Key& key, VAL start, VAL stop);

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
    private:
        //Pimpl
        class Implementation;
        Implementation* d;

        //Only methods used by template public functions
        bool fetch_get_result(Key& result, size_t index);
        bool get(const std::vector<std::reference_wrapper<const Key>>& keys);
    };
}