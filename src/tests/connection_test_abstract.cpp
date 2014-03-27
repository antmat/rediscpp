#include "connection_test_abstract.hpp"
#define RUN(command) if(!command) {CPPUNIT_FAIL(connection.get_error());}
#define VERSION_REQUIRED(version) if(connection.get_version() < version) {CPPUNIT_FAIL(std::string("Redis version:")+std::to_string(connection.get_version())+" is not enough for performing test");}

void ConnectionTestAbstract::setUp() {
    connection = std::move(get_connection());
}
void ConnectionTestAbstract::tearDown() {
}
void ConnectionTestAbstract::test_append(){
    std::string key = "test_append";
    RUN(connection.set(key, "v"));
    RUN(connection.append(key, "v"));
    std::string val;
    RUN(connection.get(key, val));
    CPPUNIT_ASSERT(val == "vv");
    long long r_l;
    RUN(connection.append(key, "v", r_l));
    CPPUNIT_ASSERT(r_l == 3);
    RUN(connection.get(key, val));
    CPPUNIT_ASSERT(val == "vvv");
}
void ConnectionTestAbstract::test_bitcount(){
    VERSION_REQUIRED(20600);
    std::string key = "test_bitcount";
    long long bc;
    RUN(connection.set(key, "\xff\xff\xff"));
    RUN(connection.bitcount(key,bc));
    CPPUNIT_ASSERT_MESSAGE(std::to_string(bc), bc == 24);
    RUN(connection.bitcount(key, 1, 2, bc));
    CPPUNIT_ASSERT_MESSAGE(std::to_string(bc), bc == 16);
    RUN(connection.set(key, "\x01\x01\x01"));
    RUN(connection.bitcount(key,bc));
    CPPUNIT_ASSERT_MESSAGE(std::to_string(bc), bc == 3);
    RUN(connection.bitcount(key, 1, 1, bc));
    CPPUNIT_ASSERT_MESSAGE(std::to_string(bc), bc == 1);
    std::string val_zero ("\x00\x00\x00", 3);
    RUN(connection.set(key, val_zero));
    RUN(connection.bitcount(key,bc));
    CPPUNIT_ASSERT_MESSAGE(std::to_string(bc), bc == 0);
    RUN(connection.bitcount(key, 1, 2, bc));
    CPPUNIT_ASSERT_MESSAGE(std::to_string(bc), bc == 0);
}
void ConnectionTestAbstract::test_bitop(){
    VERSION_REQUIRED(20600);
    std::string key = "test_bitop";
    std::string result;
    long long result_len;
    std::vector<std::string> keys(3);
    std::string zero_val("\x00\x00\x00", 3);
    std::string val2("\x00\x01", 2);
    std::string val3("\x11\x11\x11", 3);
    RUN(connection.set("test_bitop_key1", zero_val));
    RUN(connection.set("test_bitop_key2", val2));
    RUN(connection.set("test_bitop_key3", val3));
    keys[0] = "test_bitop_key1";
    keys[1] = "test_bitop_key2";
    keys[2] = "test_bitop_key3";

    /*** OR tests ***/
    RUN(connection.bitop(Redis::Connection::BitOperation::OR, key, keys));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x11\x11");

    RUN(connection.bitop(Redis::Connection::BitOperation::OR, key, keys, result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x11\x11");
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bitop(Redis::Connection::BitOperation::OR, key, keys.begin(), keys.end()));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x11\x11");

    RUN(connection.bitop(Redis::Connection::BitOperation::OR, key, keys.begin(), keys.end(), result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x11\x11");
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bit_or(key, keys, result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x11\x11");
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bit_or(key, keys));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x11\x11");

    RUN(connection.bit_or(key, keys.begin(), keys.end()));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x11\x11");

    RUN(connection.bit_or(key, keys.begin(), keys.end(), result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x11\x11");
    CPPUNIT_ASSERT(result_len == 3);

    /*** AND tests ***/
    RUN(connection.bitop(Redis::Connection::BitOperation::AND, key, keys));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == zero_val);

    RUN(connection.bitop(Redis::Connection::BitOperation::AND, key, keys, result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == zero_val);
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bitop(Redis::Connection::BitOperation::AND, key, keys.begin(), keys.end()));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == zero_val);

    RUN(connection.bitop(Redis::Connection::BitOperation::AND, key, keys.begin(), keys.end(), result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == zero_val);
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bit_and(key, keys, result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == zero_val);
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bit_and(key, keys));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == zero_val);

    RUN(connection.bit_and(key, keys.begin(), keys.end()));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == zero_val);

    RUN(connection.bit_and(key, keys.begin(), keys.end(), result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == zero_val);
    CPPUNIT_ASSERT(result_len == 3);

    /*** XOR tests ***/
    RUN(connection.bitop(Redis::Connection::BitOperation::XOR, key, keys));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x10\x11");

    RUN(connection.bitop(Redis::Connection::BitOperation::XOR, key, keys, result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x10\x11");
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bitop(Redis::Connection::BitOperation::XOR, key, keys.begin(), keys.end()));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x10\x11");

    RUN(connection.bitop(Redis::Connection::BitOperation::XOR, key, keys.begin(), keys.end(), result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x10\x11");
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bit_xor(key, keys, result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x10\x11");
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bit_xor(key, keys));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x10\x11");

    RUN(connection.bit_xor(key, keys.begin(), keys.end()));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x10\x11");

    RUN(connection.bit_xor(key, keys.begin(), keys.end(), result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x10\x11");
    CPPUNIT_ASSERT(result_len == 3);

    /*** NOT tests ***/
    keys.resize(1);
    RUN(connection.bitop(Redis::Connection::BitOperation::NOT, key, keys));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\xff\xff\xff");

    RUN(connection.bitop(Redis::Connection::BitOperation::NOT, key, keys, result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\xff\xff\xff");
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bitop(Redis::Connection::BitOperation::NOT, key, keys.begin(), keys.end()));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\xff\xff\xff");

    RUN(connection.bitop(Redis::Connection::BitOperation::NOT, key, keys.begin(), keys.end(), result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\xff\xff\xff");
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bit_not(key, keys[0], result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\xff\xff\xff");
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bit_not(key, keys[0]));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\xff\xff\xff");
}
void ConnectionTestAbstract::test_bitpos(){
    std::string val("\x00\x0f\x00\xf0", 4);
    std::string val_zero("\x00\x00\x00", 3);
    std::string val_one("\xff\xff\xff", 3);
    RUN(connection.set("test_bitpos_key1", val));
    RUN(connection.set("test_bitpos_key2", val_zero));
    RUN(connection.set("test_bitpos_key3", val_one));
    long long pos;
    RUN(connection.bitpos("test_bitpos_key1", Redis::Connection::Bit::ONE, pos));
    CPPUNIT_ASSERT(pos == 12);
    RUN(connection.bitpos("test_bitpos_key1", Redis::Connection::Bit::ZERO, pos));
    CPPUNIT_ASSERT(pos == 0);
    RUN(connection.bitpos("test_bitpos_key2", Redis::Connection::Bit::ONE, pos));
    CPPUNIT_ASSERT(pos == -1);
    RUN(connection.bitpos("test_bitpos_key3", Redis::Connection::Bit::ZERO, pos));
    CPPUNIT_ASSERT(pos == 24);
    RUN(connection.bitpos("test_bitpos_key3", Redis::Connection::Bit::ZERO, 1, pos));
    CPPUNIT_ASSERT(pos == 24);
    RUN(connection.bitpos("test_bitpos_key3", Redis::Connection::Bit::ZERO, 0, 2,pos));
    CPPUNIT_ASSERT(pos == -1);
    RUN(connection.bitpos("test_bitpos_key1", Redis::Connection::Bit::ONE, 1, pos));
    CPPUNIT_ASSERT(pos == 12);
}
void ConnectionTestAbstract::test_decr(){
    std::string val("100500");
    std::string result;
    long long res_value;
    RUN(connection.set("test_decr", val));
    RUN(connection.decr("test_decr"));
    RUN(connection.get("test_decr", result));
    CPPUNIT_ASSERT(result == "100499");
    RUN(connection.decr("test_decr", res_value));
    CPPUNIT_ASSERT(res_value == 100498);
}
void ConnectionTestAbstract::test_decrby(){
    std::string val("100500");
    std::string result;
    long long res_value;
    RUN(connection.set("test_decr", val));
    RUN(connection.decrby("test_decr", 2));
    RUN(connection.get("test_decr", result));
    CPPUNIT_ASSERT(result == "100498");
    RUN(connection.decrby("test_decr", 2,res_value));
    CPPUNIT_ASSERT(res_value == 100496);
}
void ConnectionTestAbstract::test_get(){
}
void ConnectionTestAbstract::test_getbit(){
}
void ConnectionTestAbstract::test_getrange(){
}
void ConnectionTestAbstract::test_getset(){
}
void ConnectionTestAbstract::test_incr(){
}
void ConnectionTestAbstract::test_incrby(){
}
void ConnectionTestAbstract::test_incrbyfloat(){
}
void ConnectionTestAbstract::test_set(){
}