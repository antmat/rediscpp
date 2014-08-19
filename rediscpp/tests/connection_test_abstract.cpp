#include <algorithm>
#include <thread>
#include <chrono>
#include "connection_test_abstract.hpp"
#define RUN(command) if(!command) {CPPUNIT_FAIL(connection.get_error());}
#define VERSION_REQUIRED(version) if(connection.get_version() < version) {CPPUNIT_FAIL(std::string("Redis version:")+std::to_string(connection.get_version())+" is not enough for performing test");}
#define CHECK_KEY(key, val) {std::string result_UNMEANING_SUFFIX; RUN(connection.get(key, result_UNMEANING_SUFFIX)); CPPUNIT_ASSERT_MESSAGE(result_UNMEANING_SUFFIX, result_UNMEANING_SUFFIX == val);}
void ConnectionTestAbstract::setUp() {
    connection = std::move(get_connection());
}
void ConnectionTestAbstract::tearDown() {
}

void ConnectionTestAbstract::test_append() {
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
void ConnectionTestAbstract::test_bitcount() {
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
void ConnectionTestAbstract::test_bitop() {
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

    RUN(connection.bit_or(key, keys, result_len));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x11\x11");
    CPPUNIT_ASSERT(result_len == 3);

    RUN(connection.bit_or(key, keys));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x11\x11");

    /*** AND tests ***/
    RUN(connection.bitop(Redis::Connection::BitOperation::AND, key, keys));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == zero_val);

    RUN(connection.bitop(Redis::Connection::BitOperation::AND, key, keys, result_len));
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


    /*** XOR tests ***/
    RUN(connection.bitop(Redis::Connection::BitOperation::XOR, key, keys));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\x11\x10\x11");

    RUN(connection.bitop(Redis::Connection::BitOperation::XOR, key, keys, result_len));
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

    /*** NOT tests ***/
    keys.resize(1);
    RUN(connection.bitop(Redis::Connection::BitOperation::NOT, key, keys));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "\xff\xff\xff");

    RUN(connection.bitop(Redis::Connection::BitOperation::NOT, key, keys, result_len));
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
void ConnectionTestAbstract::test_bitpos() {
    VERSION_REQUIRED(20807);
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
void ConnectionTestAbstract::test_decr() {
    std::string val("100500");
    std::string invalid_val("UPCHK");
    std::string key("test_decr");
    std::string invalid_key("test_invalid_decr");
    std::string result;
    long long res_value;
    RUN(connection.set(key, val));
    RUN(connection.set(invalid_key, invalid_val));
    RUN(connection.decr(key));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "100499");
    RUN(connection.decr(key, res_value));
    CPPUNIT_ASSERT(res_value == 100498);
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.decr(invalid_key)));
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.decr(invalid_key, res_value)));
    CPPUNIT_ASSERT(res_value == 100498);
    RUN(connection.decr(key, res_value));
    CPPUNIT_ASSERT(res_value == 100497);
}
void ConnectionTestAbstract::test_decrby() {
    std::string val("100500");
    std::string invalid_val("UPCHK");
    std::string key("test_decr");
    std::string invalid_key("test_invalid_decr");
    std::string result;
    long long res_value;
    RUN(connection.set(key, val));
    RUN(connection.set(invalid_key, invalid_val));
    RUN(connection.decrby(key, 2));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "100498");
    RUN(connection.decrby(key, 2, res_value));
    CPPUNIT_ASSERT(res_value == 100496);
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.decr(invalid_key)));
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.decr(invalid_key, res_value)));
    CPPUNIT_ASSERT(res_value == 100496);
    RUN(connection.decrby(key, 2, res_value));
    CPPUNIT_ASSERT(res_value == 100494);
}
void ConnectionTestAbstract::test_get() {
    std::vector<std::string> vals = {"UPCHK", "UP", "CHK"};
    std::vector<std::string> keys = {"test_get1", "test_get2", "test_get3"};
    std::map<std::string, std::string> kv_pairs = {{"test_get1",""}, {"test_get2",""}, {"test_get3",""}};
    std::vector<std::string> ret_vals(1);
    RUN(connection.set(keys, vals));
    RUN(connection.get(keys[0], ret_vals[0]));
    CPPUNIT_ASSERT(ret_vals[0] == "UPCHK");
    RUN(connection.get(keys, ret_vals));
    CPPUNIT_ASSERT(ret_vals.size() == 3);
    CPPUNIT_ASSERT(ret_vals[0] == vals[0]);
    CPPUNIT_ASSERT(ret_vals[1] == vals[1]);
    CPPUNIT_ASSERT(ret_vals[2] == vals[2]);
    ret_vals.clear();
    //RUN(connection.get(keys.begin(), keys.end(), std::insert_iterator<std::vector<std::string>>(ret_vals, ret_vals.begin())));
    //CPPUNIT_ASSERT(ret_vals.size() == 3);
    //CPPUNIT_ASSERT(ret_vals[0] == vals[0]);
    //CPPUNIT_ASSERT(ret_vals[1] == vals[1]);
    //CPPUNIT_ASSERT(ret_vals[2] == vals[2]);
    RUN(connection.get(kv_pairs));
    CPPUNIT_ASSERT(kv_pairs.size() == 3);
    CPPUNIT_ASSERT(kv_pairs["test_get1"] == vals[0]);
    CPPUNIT_ASSERT(kv_pairs["test_get2"] == vals[1]);
    CPPUNIT_ASSERT(kv_pairs["test_get3"] == vals[2]);
}
void ConnectionTestAbstract::test_getbit() {
    std::string key("test_get_bit");
    RUN(connection.set(key, "\xff\x0f\xff"));
    Redis::Connection::Bit bit;
    RUN(connection.getbit(key, 3, bit));
    CPPUNIT_ASSERT(bit == Redis::Connection::Bit::ONE);
    RUN(connection.getbit(key, 8, bit));
    CPPUNIT_ASSERT(bit == Redis::Connection::Bit::ZERO);
    RUN(connection.getbit(key, 32, bit));
    CPPUNIT_ASSERT(bit == Redis::Connection::Bit::ZERO);
}
void ConnectionTestAbstract::test_getrange(){
    std::string key("test_getrange");
    std::string result;
    RUN(connection.set(key, "foobazbar"));
    RUN(connection.getrange(key, 3, 5, result));
    CPPUNIT_ASSERT_MESSAGE(result, result == "baz");
    RUN(connection.getrange(key, -3, -1, result));
    CPPUNIT_ASSERT_MESSAGE(result, result == "bar");
}
void ConnectionTestAbstract::test_getset(){
    std::string key("test_getset");
    std::string result;
    RUN(connection.set(key, "foo"));
    RUN(connection.getset(key, "bar", result));
    CPPUNIT_ASSERT(result == "foo");
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "bar");
}
void ConnectionTestAbstract::test_incr(){
    std::string val("100500");
    std::string invalid_val("UPCHK");
    std::string key("test_incr");
    std::string invalid_key("test_invalid_incr");
    std::string result;
    long long res_value;
    RUN(connection.set(key, val));
    RUN(connection.set(invalid_key, invalid_val));
    RUN(connection.incr(key));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "100501");
    RUN(connection.incr(key, res_value));
    CPPUNIT_ASSERT(res_value == 100502);
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.incr(invalid_key)));
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.incr(invalid_key, res_value)));
    CPPUNIT_ASSERT(res_value == 100502);
    RUN(connection.incr(key, res_value));
    CPPUNIT_ASSERT(res_value == 100503);
}
void ConnectionTestAbstract::test_incrby(){
    std::string val("100500");
    std::string invalid_val("UPCHK");
    std::string key("test_incr");
    std::string invalid_key("test_invalid_incr");
    std::string result;
    long long res_value;
    RUN(connection.set(key, val));
    RUN(connection.set(invalid_key, invalid_val));
    RUN(connection.incrby(key, 2));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(result == "100502");
    RUN(connection.incrby(key, 2, res_value));
    CPPUNIT_ASSERT(res_value == 100504);
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.incr(invalid_key)));
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.incr(invalid_key, res_value)));
    CPPUNIT_ASSERT(res_value == 100504);
    RUN(connection.incrby(key, 2, res_value));
    CPPUNIT_ASSERT(res_value == 100506);
}
void ConnectionTestAbstract::test_incrbyfloat(){
    VERSION_REQUIRED(20600);
    std::string val("10.0");
    std::string invalid_val("UPCHK");
    std::string key("test_decr");
    std::string invalid_key("test_invalid_decr");
    std::string result;
    double d_incr = 0.2;
    float f_incr = static_cast<float>(d_incr);
    double res_d_value;
    float res_f_value;
    RUN(connection.set(key, val));
    RUN(connection.set(invalid_key, invalid_val));
    RUN(connection.incrbyfloat(key, f_incr));
    RUN(connection.incrbyfloat(key, d_incr));
    RUN(connection.get(key, result));
    CPPUNIT_ASSERT(fabs(std::stod(result) - 10.4) < 0.0000001);
    RUN(connection.incrbyfloat(key, d_incr, res_d_value));
    CPPUNIT_ASSERT(fabs(res_d_value - 10.6) < 0.0000001);
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.incrbyfloat(invalid_key, d_incr)));
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.incrbyfloat(invalid_key, d_incr, res_d_value)));
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.incrbyfloat(invalid_key, f_incr)));
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.incrbyfloat(invalid_key, f_incr, res_f_value)));
    RUN(connection.incrbyfloat(key, 2.0, res_d_value));
    CPPUNIT_ASSERT(fabs(res_d_value - 12.6) < 0.0000001);
}

void ConnectionTestAbstract::test_set(){
    bool was_set;
    std::string key = "test_set";
    connection.set(key, "test_val");
    CHECK_KEY(key, "test_val");
    connection.set(key, "test_val2", Redis::Connection::SetType::IF_EXIST, was_set, 1, Redis::Connection::ExpireType::SEC);
    CPPUNIT_ASSERT(was_set == true);
    CHECK_KEY(key, "test_val2");
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    CHECK_KEY(key, "");
    connection.set(key, "test_val2", Redis::Connection::SetType::IF_EXIST, was_set);
    CPPUNIT_ASSERT(was_set == false);
    CHECK_KEY(key, "");
    connection.set(key, "test_val3", Redis::Connection::SetType::IF_NOT_EXIST, was_set, 1000, Redis::Connection::ExpireType::MSEC);
    CPPUNIT_ASSERT(was_set == true);
    CHECK_KEY(key, "test_val3");
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    CHECK_KEY(key, "");

    std::vector<std::string> keys = { "test_set1", "test_set2", "test_set3"};
    std::vector<std::string> values = {"val", "val", "val"};
    std::vector<std::reference_wrapper<const std::string>> key_refs(keys.begin(), keys.end());
    std::vector<std::reference_wrapper<const std::string>> val_refs(values.begin(), values.end());
    std::vector<std::pair<std::string, std::string>> kv_pairs;
    std::map<std::string, std::string> kv_map;
    for(size_t i=0; i<keys.size(); i++) {
        kv_pairs.push_back(std::make_pair(keys[i], values[i]));
        kv_map.insert(std::make_pair(keys[i], values[i]));
    }
    RUN(connection.set(keys, values, Redis::Connection::SetType::ALWAYS));
    for(size_t i=0; i<keys.size(); i++) {
        CHECK_KEY(keys[i], "val");
        RUN(connection.del(keys[i]));
        CHECK_KEY(keys[i], "");
    }
    CPPUNIT_ASSERT_ASSERTION_FAIL(RUN(connection.set(keys, values, Redis::Connection::SetType::IF_EXIST)));
    RUN(connection.set(keys, values, Redis::Connection::SetType::IF_NOT_EXIST));
    for(size_t i=0; i<keys.size(); i++) {
        CHECK_KEY(keys[i], "val");
        RUN(connection.del(keys[i]));
        CHECK_KEY(keys[i], "");
    }

    RUN(connection.set(key_refs, val_refs, Redis::Connection::SetType::ALWAYS));
    for(size_t i=0; i<keys.size(); i++) {
        CHECK_KEY(keys[i], "val");
        RUN(connection.del(keys[i]));
        CHECK_KEY(keys[i], "");
    }
    RUN(connection.set(kv_pairs, Redis::Connection::SetType::ALWAYS));
    for(size_t i=0; i<keys.size(); i++) {
        CHECK_KEY(keys[i], "val");
        RUN(connection.del(keys[i]));
        CHECK_KEY(keys[i], "");
    }
    RUN(connection.set(kv_map, Redis::Connection::SetType::ALWAYS));
    for(size_t i=0; i<keys.size(); i++) {
        CHECK_KEY(keys[i], "val");
        RUN(connection.del(keys[i]));
        CHECK_KEY(keys[i], "");
    }

}
void ConnectionTestAbstract::test_set_bit() {
    std::string key("test_set_bit");
    Redis::Connection::Bit bit;
    Redis::Connection::Bit original_bit;
    RUN(connection.set_bit(key, 32, Redis::Connection::Bit::ONE));
    RUN(connection.getbit(key, 32, bit));
    CPPUNIT_ASSERT(bit == Redis::Connection::Bit::ONE);
    RUN(connection.set_bit(key, 64, Redis::Connection::Bit::ZERO));
    RUN(connection.getbit(key, 64, bit));
    CPPUNIT_ASSERT(bit == Redis::Connection::Bit::ZERO);
    RUN(connection.set_bit(key, 64, Redis::Connection::Bit::ONE, original_bit ));
    RUN(connection.getbit(key, 64, bit));
    CPPUNIT_ASSERT(bit == Redis::Connection::Bit::ONE);
    CPPUNIT_ASSERT(original_bit == Redis::Connection::Bit::ZERO);
}

void ConnectionTestAbstract::test_setrange() {
    std::string key("test_setrange");
    std::string result;
    long long str_len;

    RUN( connection.set(key, "test string one" ) );
    RUN( connection.setrange(key, 5, "STRING" )  );
    RUN( connection.get(key, result) );
    CPPUNIT_ASSERT( result == "test STRING one" );

    RUN( connection.setrange(key, 5, "a different string", str_len )  );
    RUN( connection.get(key, result) );
    CPPUNIT_ASSERT( result ==  "test a different string" );
    CPPUNIT_ASSERT( str_len == 23 );

    CPPUNIT_ASSERT_ASSERTION_FAIL( RUN( connection.setrange(key, -1, "Should be an error" ) ) );
}

void ConnectionTestAbstract::test_strlen() {
    std::string key("test_strlen");
    std::string sample_str("The Quick Brown Fox Jumps Over Lazy Dog");
    long long str_len;
    RUN( connection.set( key, sample_str ) );
    RUN( connection.strlen( key, str_len ) );
    CPPUNIT_ASSERT( str_len - sample_str.size() == 0 );
    RUN( connection.del(key) );
    RUN( connection.strlen(key, str_len) );
    CPPUNIT_ASSERT( str_len == 0 );
}

void ConnectionTestAbstract::test_expire() {
    // for redis 2.4, error between 0 and 1 second
    // for redis 2.6, error between 0 and 1 milisecond
    std::string key("test_expire");
    std::string test_val("The Quick Brown Fox Jumps Over Lazy Dog");
    RUN( connection.set(key, test_val));
    RUN( connection.expire(key, 2,  Redis::Connection::ExpireType::SEC ));
    CHECK_KEY( key, test_val );
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    CHECK_KEY( key, "");
    VERSION_REQUIRED(20600);
    RUN( connection.set(key, test_val));
    RUN( connection.expire(key, 2000,  Redis::Connection::ExpireType::MSEC ));
    CHECK_KEY( key, test_val );
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    CHECK_KEY( key, "");
}

void ConnectionTestAbstract::test_ttl() {
    std::string key("test_ttl");
    long long sec_to_live;
    RUN( connection.set(key,"The Quick Brown Fox Jumps Over Lazy Dog"));
    RUN( connection.ttl(key, sec_to_live) );
    CPPUNIT_ASSERT( sec_to_live == -1 );    // no expire, return code = -1
    RUN( connection.expire(key, 4,  Redis::Connection::ExpireType::SEC ));// can be 3 - 5 sec due to error in redis 2.4
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    RUN( connection.ttl(key, sec_to_live) );
    CPPUNIT_ASSERT( sec_to_live < 3 && sec_to_live > 1 );// after 2 seconds, should be between 3 and 1
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));// additional 1 sec due to errors in redis 2.4
    RUN( connection.ttl(key, sec_to_live) );
    CPPUNIT_ASSERT( sec_to_live == -2 );    // key should be vanished, return code = -2
}

void ConnectionTestAbstract::test_sadd() {
    std::string key("test_sadd");
    std::vector < std::string > result;

    RUN(connection.del(key));// make sure key is empty

    RUN(connection.sadd(key, "Moscow"));
    RUN(connection.sadd(key, "Hanoi"));
    RUN(connection.sadd(key, "Maryland"));

    RUN(connection.smembers(key, result));
    std::sort( result.begin(), result.end() );

    CPPUNIT_ASSERT( result[0] == "Hanoi" );
    CPPUNIT_ASSERT( result[1] == "Maryland" );
    CPPUNIT_ASSERT( result[2] == "Moscow" );
}

void ConnectionTestAbstract::test_scard() {
    std::string key("test_scard");
    RUN( connection.del(key) ) // make sure key is empty
    long long res_size, cnt = 0;

    RUN(connection.scard(key, res_size));
    CPPUNIT_ASSERT( res_size == 0 );

    RUN(connection.sadd(key, "Moscow"));
    cnt++;
    RUN(connection.sadd(key, "Hanoi"));
    cnt++;
    RUN(connection.sadd(key, "Maryland"));
    cnt++;
    RUN(connection.scard(key, res_size));
    CPPUNIT_ASSERT( res_size == cnt );

}

void ConnectionTestAbstract::test_sinter(){
    std::string key1("test_sinter1");
    std::string key2("test_sinter2");
    std::string bin_val("1\x001", 3);
    RUN(connection.del(key1));
    RUN(connection.del(key2));
    RUN(connection.sadd(key1, bin_val));
    RUN(connection.sadd(key1, "2"));
    RUN(connection.sadd(key1, "3"));
    RUN(connection.sadd(key2, bin_val));
    RUN(connection.sadd(key2, "2"));
    RUN(connection.sadd(key2, "5"));
    std::vector<std::string> keys;
    keys.push_back(key1);
    std::vector<std::string> result;
    RUN(connection.sinter(keys, result));
    CPPUNIT_ASSERT(result.size() == 3);
    std::sort(result.begin(), result.end());
    CPPUNIT_ASSERT(result[0] == bin_val);
    CPPUNIT_ASSERT(result[1] == "2");
    CPPUNIT_ASSERT(result[2] == "3");

    keys.push_back(key2);
    RUN(connection.sinter(keys, result));
    CPPUNIT_ASSERT(result.size() == 2);
    std::sort(result.begin(), result.end());
    CPPUNIT_ASSERT(result[0] == bin_val);
    CPPUNIT_ASSERT(result[1] == "2");
}

void ConnectionTestAbstract::test_smembers() {
    std::string key("test_smembers");
    std::vector < std::string > result;

    RUN(connection.del(key));// make sure key is empty

    RUN(connection.sadd(key, "Moscow"));
    RUN(connection.sadd(key, "Hanoi"));
    RUN(connection.sadd(key, "Maryland"));

    RUN(connection.smembers(key, result));
    std::sort( result.begin(), result.end() );

    CPPUNIT_ASSERT( result[0] == "Hanoi" );
    CPPUNIT_ASSERT( result[1] == "Maryland" );
    CPPUNIT_ASSERT( result[2] == "Moscow" );
}

void ConnectionTestAbstract::test_zincrby() {

}