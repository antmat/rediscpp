#include "sharded_connection.hpp"
#include "connection.hpp"
#include "connection_param.hpp"
#include "exception.hpp"
#include "pool.hpp"
namespace Redis {
    class ShardedConnection::Impl {
        friend class ShardedConnection;
        std::vector<Connection> connections;
        bool locked;
        Impl() :
            connections(),
            locked(false)
        {}
    };

    ShardedConnection::ShardedConnection() :
        d(new ShardedConnection::Impl)
    {
    }

    ShardedConnection::ShardedConnection(ShardedConnection&& other) :
        d(other.d)
    {
        other.d = nullptr;
    }

    ShardedConnection& ShardedConnection::operator=(ShardedConnection&& other) {
        d = other.d;
        other.d = nullptr;
        return *this;
    }

    ShardedConnection::~ShardedConnection() {
        if(d != nullptr) {
            delete d;
        }
    }

    void ShardedConnection::add_connection(const ConnectionParam& conn_param) {
        if(d->locked) {
            throw Redis::Exception("Tried to add connection to local pool while existing connections could be used before. Hashing function would return different index for key.");
        }
        d->connections.emplace_back(conn_param);
    }

    Connection& ShardedConnection::get(const std::string& key) {
        d->locked = true;
        size_t index = Pool::get_connection_index_by_key_and_shard_size(key, d->connections.size());
        return d->connections[index];
    }

}


