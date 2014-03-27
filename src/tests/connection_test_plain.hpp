#pragma once
#include "connection_test_abstract.hpp"
class ConnectionTestPlain : public ConnectionTestAbstract {
    CPPUNIT_TEST_SUB_SUITE(ConnectionTestPlain, ConnectionTestAbstract);
    CPPUNIT_TEST_SUITE_END();
    protected:
        virtual Redis::Connection get_connection();
};