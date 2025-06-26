#pragma once
#include <winsock2.h>
#include <iostream>

inline SOCKET generate_socket() {
    auto skt = socket(AF_INET, SOCK_STREAM, 0);
    if (skt == INVALID_SOCKET)
        throw std::runtime_error("Socket creation failed");
    else {
        std::cout << "Server Socket created successfully" << std::endl;
        return skt;
    }

}
// Call of RedisSocket's constructor MUST be wrapped in try/catch
class RedisSocket {
public:

    RedisSocket() {
            socket = generate_socket();
    }

    RedisSocket(const RedisSocket&) = delete;

    RedisSocket& operator=(const RedisSocket&) = delete;

    [[nodiscard]] SOCKET get() const { return socket; }

    ~RedisSocket() {
        if (socket != INVALID_SOCKET)
            closesocket(socket);
        std::cout << "Listener Socket destroyed" << std::endl;
    }
private:
    SOCKET socket = INVALID_SOCKET;
};