#pragma once
#include <winsock2.h>

#include "Address.h"

class ServerAddress : public Address{
public:
    ServerAddress() {
        //Hard-Coded for now
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(6379);
    };
};
