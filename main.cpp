#include <iostream>
#include "RedisServer.h"
int main() {
    try {
        std::cout << "POLLIN:" << POLLIN << std::endl;
        RedisServer server;
        server.Start();
        server.Serve();
    } catch (const std::exception& e) {
        // Handle the exception
        std::cerr << "Error: " << e.what() << "\n";
    }
}
