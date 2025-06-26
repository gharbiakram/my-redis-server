#pragma once
#include <winsock2.h>
#include "Address.h"

class ClientAddress : public Address {
public:
    ClientAddress(const char* ip,const int port) {
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = inet_addr(ip);
        address.sin_port = htons(port);
    }
    explicit ClientAddress(const sockaddr_in addr) {
        address = addr;
    }
    ClientAddress() = default;
    void set(const sockaddr_in& addr) const {
        address = addr;
    }
    void nullify() {
        address.sin_addr.s_addr = 0;
    }
};
