#include <cppunit/extensions/HelperMacros.h>
#include "../redis.hpp"
/*
* A test case that is designed to produce
* example errors and failures
*
*/
class ConnectionTestAbstract : public CPPUNIT_NS::TestFixture
{
    //establish the test suit of TestComplexNumber
    CPPUNIT_TEST_SUITE(ConnectionTestAbstract);
        CPPUNIT_TEST( test_append );
        CPPUNIT_TEST( test_bitcount );
        CPPUNIT_TEST( test_bitop );
        CPPUNIT_TEST( test_bitpos );
        CPPUNIT_TEST( test_decr );
        CPPUNIT_TEST( test_decrby );
        CPPUNIT_TEST( test_get );
        CPPUNIT_TEST( test_getbit );
        CPPUNIT_TEST( test_getrange );
        CPPUNIT_TEST( test_getset );
        CPPUNIT_TEST( test_incr );
        CPPUNIT_TEST( test_incrby );
        CPPUNIT_TEST( test_incrbyfloat );
        CPPUNIT_TEST( test_set );
        CPPUNIT_TEST( test_set_bit );
        CPPUNIT_TEST( test_setrange );
        CPPUNIT_TEST( test_strlen );


        CPPUNIT_TEST( test_sinter );


    CPPUNIT_TEST_SUITE_END_ABSTRACT();
public:
    void setUp();
    void tearDown();
protected:
    virtual Redis::Connection get_connection() = 0;
    void test_append();
    void test_bitcount();
    void test_bitop();
    void test_bitpos();
    void test_decr();
    void test_decrby();
    void test_get();
    void test_getbit();
    void test_getrange();
    void test_getset();
    void test_incr();
    void test_incrby();
    void test_incrbyfloat();
    void test_set();
    void test_set_bit();
    void test_setrange();
    void test_strlen();

    void test_sinter();



private:
    Redis::Connection connection;
};
