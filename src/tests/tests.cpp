#include "tests.hpp"
#include <iostream>
#define test_fail std::cerr << "Test in " << __FUNCTION__ << " on line: " <<__LINE__ << " Failed." << std::endl;
#define test_ok std::cout << "Test " << __FUNCTION__ << " completed" << std::endl; return true;
#define test_assert(...) if(!(__VA_ARGS__)) {test_fail; return false;}
static unsigned int redis_port = 0;
bool run_tests(unsigned int port) {
    redis_port = port;
    return test_simple() &&
            test_simple_fail();
}

bool test_simple() {
    Redis::ConnectionParam param;
    param.port = redis_port;
    Redis::Connection conn(param);
    test_assert(conn.set("testkey", "testvalue"));
    std::string res;
    test_assert(conn.get("testkey", res));
    test_assert(res == "testvalue");
    test_ok;
}

bool test_simple_fail() {
    Redis::ConnectionParam param;
    param.port = 9000;
    Redis::Connection conn(param);
    test_assert(!conn.set("testkey", "testvalue"));
    param.throw_on_error = true;
    Redis::Connection conn2(param);
    try {
        conn2.set("testkey", "testvalue");
        test_assert(false);
    }
    catch (const Redis::Exception &e) {
        std::cerr << e.what();
        test_assert(true);
    }
    catch (const std::exception &e) {
        test_assert(false);
    }
    test_ok;
}
