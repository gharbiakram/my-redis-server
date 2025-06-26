#pragma once
#include <stdexcept>
#include <winsock2.h>
#include "Utils.h"
#include "Poller.h"
class Connection {
public:
    Connection(): socket(INVALID_SOCKET){}
    explicit Connection(SOCKET s)
        : socket(s), state(utils::ConnectionState::Req), rbuf_size(0), wbuf_size(0), wbuf_sent(0) {}
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&& other) noexcept {
        socket = other.socket;
        other.socket = INVALID_SOCKET;
    }
    Connection& operator=(Connection&& other) noexcept {
        if (this != &other) {
            if (socket != INVALID_SOCKET) {
                closesocket(socket);
            }
            socket = other.socket;
            other.socket = INVALID_SOCKET;
        }
        return *this;
    }
    ~Connection() {
        std::cout << "Closing connection..." << std::endl;
        if (closesocket(socket) == 0) {
            std::cerr << "Socket closed : " <<socket<< std::endl;
            socket = INVALID_SOCKET;
        }
    };
    [[nodiscard]] SOCKET getSocket() const {
        if (socket == INVALID_SOCKET) {
            throw std::runtime_error("socket is invalid");
        }
        return socket;
    }
    //Socket Assigned upon Acceptance
    void setSocket(const SOCKET socket) {
        this->socket = socket;
    }
    [[nodiscard]] utils::ConnectionState getState() const {
        return state;
    }
    void nullify() {
        this->socket = INVALID_SOCKET;
        this->state = utils::ConnectionState::End;
    }

    // --- Getters ---
    size_t getReadBufferSize() const { return rbuf_size; }
    size_t getWriteBufferSize() const { return wbuf_size; }
    size_t getWriteBufferSent() const { return wbuf_sent; }

    [[nodiscard]]  char* getReadBuffer()  { return rbuf; }
    [[nodiscard]]  char* getWriteBuffer() { return wbuf; }

    // --- Setters ---
    void setReadBufferSize(size_t size) {
        if (size <= sizeof(rbuf)) rbuf_size = size;
    }

    void setWriteBufferSize(size_t size) {
        if (size <= sizeof(wbuf)) wbuf_size = size;
    }

    void setWriteBufferSent(size_t sent) {
        if (sent <= wbuf_size) wbuf_sent = sent;
    }

    // --- Access Buffer Contents ---
    char* getReadBufferData() { return rbuf; }
    char* getWriteBufferData() { return wbuf; }

    // --- Buffer Manipulation ---
    void clearReadBuffer() {
        rbuf_size = 0;
        std::fill(std::begin(rbuf), std::end(rbuf), 0);
    }

    void clearWriteBuffer() {
        wbuf_size = 0;
        wbuf_sent = 0;
        std::fill(std::begin(wbuf), std::end(wbuf), 0);
    }

    void appendToWriteBuffer(const uint8_t* data, size_t len) {
        if (len + wbuf_size > sizeof(wbuf)) return;  // Prevent overflow
        memcpy(wbuf + wbuf_size, data, len);
        wbuf_size += len;
    }

    void appendToReadBuffer(const uint8_t* data, size_t len) {
        if (len + rbuf_size > sizeof(rbuf)) return;
        memcpy(rbuf + rbuf_size, data, len);
        rbuf_size += len;
    }
    void setState(const utils::ConnectionState state) {
        this->state = state;
    }
    void extendBuffer(const size_t len) {
        this->rbuf_size += len;
    }
    size_t getReadBufferCapacity() const {
        return rbuf_size;
    }

private:
    utils::ConnectionState state;
    size_t rbuf_size;
    char rbuf[4 + utils::MAX_LENGTH+1]{};
    size_t r_buffer_capacity = 4 + utils::MAX_LENGTH + 1;
    size_t wbuf_size;
    size_t wbuf_sent;
    char wbuf[4 + utils::MAX_LENGTH+1]{};
    SOCKET socket;
};