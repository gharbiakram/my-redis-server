#pragma once
#include <cstdio>
#include <iostream>
#include <Winsock2.h>


namespace utils {

    constexpr size_t MAX_LENGTH = 1024;

    enum class IOStatus {
        Ok,
        WouldBlock,
        Disconnected,
        Error
    };
    enum class PollerStatus {
        Ok,
        Failed
    };
    enum class ProtocolStatus {
        Ok,
        Malformed,
        Incomplete,
    };
    enum class CommandStatus {
        OK,
        KeyNotFound,
        ValAlreadyExists,
        KeyNoExist,
        SyntaxError,
        InvalidCommand
    };
    enum class ConnectionState {
        Req,
        Res,
        End
    };

    class ProtocolParser {
    public:
        static IOStatus send_full(const SOCKET socket,char *buffer,int len) {
            while (len > 0) {
                errno = 0;
                auto rv = send(socket,buffer,len,0);
                if (rv == SOCKET_ERROR) {
                    int err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK) return IOStatus::WouldBlock;
                    return IOStatus::Error;
                }
                buffer += rv;
                len -= rv;
            }
            return IOStatus::Ok;
        }
        static IOStatus recv_full(const SOCKET socket,char *buffer,int len) {
            while (len > 0) {
                const auto rv = recv(socket,buffer,len,0);
                if (rv == 0) {
                    std::cerr << "ERROR:Client disconnected.\n" ;
                    return IOStatus::Disconnected;
                }
                if (rv == SOCKET_ERROR) {
                    auto err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK) {
                        std::cerr << "No More Data."<< std::endl;
                        return IOStatus::WouldBlock;
                    }else {
                        std::cerr << "ERROR:recv_full() failed: " << err << "\n";
                        return IOStatus::Error;
                    }
                }
                buffer += rv;
                len -= rv;
            }
            return IOStatus::Ok;
        }
    };

    static bool equalsIgnoreCase(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;

        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
                }
        }

        return true;
    }

    class Debug {
    public:
        static void echo(const char* msg) {
            std::cout << msg << std::endl;
        }
        static void echo(const int val) {
            printf("%d\n",val);
        }

        static void error(const int val) {
            printf("%d\n",val);
        }
        static void error(const std::string& msg) {
            std::cerr << "[ERROR] " << msg << std::endl;
        }
        static void die(const char* err) {
           throw std::runtime_error(err);
        }
    };



}

