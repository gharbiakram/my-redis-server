#pragma once

class Address {
public:
    Address() = default;
    virtual ~Address() = 0;
    [[nodiscard]]  sockaddr_in get() const {
        return address;
    }

    //Cleaner work with bind()
    [[nodiscard]] struct sockaddr* get_ptr() const {
        return reinterpret_cast<struct sockaddr*>(&address);
    }

protected:
   mutable sockaddr_in address{};
};

inline Address::~Address() {

}
