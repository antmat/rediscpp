#include "connection_test_plain.hpp"
CPPUNIT_TEST_SUITE_REGISTRATION( ConnectionTestPlain );
Redis::Connection ConnectionTestPlain::get_connection() {
    return Redis::Connection();
}