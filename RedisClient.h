#pragma once
#include <memory>
#include <string>
#include <winsock2.h>
#include "ClientAddress.h"
#include "Connection.h"
#include "Connection.h"
#include "ServerAddress.h"

class RedisClient {
public:
    RedisClient(const char* ip , const int port) : address(ip,port){};
    explicit RedisClient(const sockaddr_in& addr) : address(addr){};
     RedisClient() = default;
     RedisClient(const sockaddr_in& addr,const SOCKET socket)
        :   conn(std::make_unique<Connection>(socket)),address(addr)
    {}
    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;
    RedisClient(RedisClient&&) noexcept = default;
    RedisClient& operator=(RedisClient&&) noexcept = default;
    ~RedisClient() = default;
    [[nodiscard]] SOCKET getSocket() const {
        return conn->getSocket();
    }
    [[nodiscard]] sockaddr_in getAddress() const {
        return address.get();
    }
    [[nodiscard]] struct sockaddr* getPointer() const {
        return address.get_ptr();
    }
    void assignSocket(const SOCKET socket) {
        conn->setSocket(socket);
    }
    void assignAddress(const sockaddr_in& addr) {
        address.set(addr);
    }
    void release() {
        address.nullify();
        conn->nullify();
    }
    Connection* getConnection() const {
        return conn.get();
    }
    utils::ConnectionState getState() const {
        return (conn->getState());
    }

    void setState(const utils::ConnectionState state) {
        conn->setState(state);
    }

private:
    std::unique_ptr<Connection> conn;
    ClientAddress address;
};
