#include <cstdlib>
#include <stdio.h>
#include <string>
#include <iostream>
#include "tests.hpp"
FILE* redis_handle;
bool run_redis (unsigned int port) {
    std::string command("redis-server --port "+std::to_string(port));
     redis_handle = popen(command.c_str(), "r");
    setvbuf(redis_handle, NULL, _IONBF, 0);
    if (redis_handle == nullptr) {
        std::cerr << "Could not start redis" << std::endl;
        exit(EXIT_FAILURE);
    }
    char path[256];
    while (fgets(path, 256, redis_handle) != NULL) {
        std::string path_s(path);
        if(path_s.find("The server is now ready to accept connections") != std::string::npos) {
            return true;
        }
    }
    return false;
}
int main(int argc, char** argv) {
    if(argc != 2) {
        std::cerr << "Usage: ./test port" <<std::endl;
        exit(EXIT_FAILURE);
    }
    run_tests(std::stoi(argv[1]));
    return 0;
}