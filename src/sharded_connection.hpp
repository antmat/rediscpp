#pragma once
#include <string>
namespace Redis {
    class ConnectionParam;
    class Connection;

    /**
    * Lightweight pool of connections to sharded cluster of redis.
    * NOT thread safe.
    */
    class ShardedConnection {
    public:
        ShardedConnection();
        ShardedConnection(const ShardedConnection& other) = delete;
        ShardedConnection& operator=(const ShardedConnection& other) = delete;
        ShardedConnection(ShardedConnection&& other);
        ShardedConnection& operator=(ShardedConnection&& other);
        ~ShardedConnection();
        void add_connection(const ConnectionParam& conn_param);
        Connection& get(const std::string& key);
        class Impl;
        Impl* d;
    };
}