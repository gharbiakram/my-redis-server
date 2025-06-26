#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <Winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#pragma comment(lib, "ws2_32.lib")// Tell the Linker to Link the socket library

inline void start_wsa(WSADATA& wsa) {
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        throw std::runtime_error("WSAStartup failed");
    std::cout << std::endl << "WSAStartup succeeded" << std::endl;
};
// Call of WindowsSocket's constructor MUST be wrapped in try/catch
class WindowsSocket {
public:
    explicit WindowsSocket() {
            start_wsa(wsa_data);
    }
    WindowsSocket(const WindowsSocket&) = delete;
    WindowsSocket& operator=(const WindowsSocket&) = delete;
    ~WindowsSocket() {
        WSACleanup();
    }
private:
      WSADATA wsa_data;
};