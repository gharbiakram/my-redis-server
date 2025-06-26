#pragma once
#include <algorithm>
#include <winsock2.h>
#include <vector>
#include "Utils.h"
#include <cassert>
class Poller {
public:
    Poller() = default;
    ~Poller() = default;
    utils::PollerStatus POLL() {

        int pfd = WSAPoll(fd_array.data(), fd_array.size(), 3000);
        if (pfd == SOCKET_ERROR) {
            int err = WSAGetLastError();
            std::cerr << "WSAPoll failed with error: " << err << "\n";
            return utils::PollerStatus::Failed;
        }
        return utils::PollerStatus::Ok;
    };
    void register_fd(const SOCKET socket, const int event) {
        if (socket == INVALID_SOCKET) {
            utils::Debug::error("Trying to register an INVALID_SOCKET!");
        }
        pollfd fd = {socket,static_cast<SHORT>(event),0};
        fd_array.emplace_back(fd);
    }
    std::vector <pollfd> array() {
        return fd_array;
    }
    void clear() {
        fd_array.clear();
    }
    void unregister_fd(const SOCKET socket) {
        fd_array.erase(remove_if(fd_array.begin(),fd_array.end(),[socket](const pollfd& fd) {
            return fd.fd == socket;
        }), fd_array.end());
    }
    static void set_socket_nonblock(const SOCKET& socket) {
        u_long mode = 1;
        int status = ioctlsocket(socket, FIONBIO, &mode);
        if (status != NO_ERROR)
            utils::Debug::die("set_socket_nonblock():ioctlsocket() failed.");
    }
    void log_arr() {
        for (const auto& fd : fd_array) {
            std::cout << "[Poll] fd: " << fd.fd
                      << ", valid: " << (fd.fd != INVALID_SOCKET)
                      << ", events: " << fd.events << "\n";
            assert(fd.fd != INVALID_SOCKET && "INVALID SOCKET ??"); // Important
        }
    }
private:
    std::vector<pollfd> fd_array;
};

