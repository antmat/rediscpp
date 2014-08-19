#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include "redis.hpp"
int main(int argc, const char** argv)
{
    if(argc == 3) {
        Redis::ConnectionParam::set_default_host(argv[1]);
        Redis::ConnectionParam::set_default_port(std::atoi(argv[2]));
    }
    CppUnit::TextUi::TestRunner runner;
    CppUnit::TestFactoryRegistry &registry
            = CppUnit::TestFactoryRegistry::getRegistry();
    runner.addTest( registry.makeTest() );
    runner.run();
    return 0;
}