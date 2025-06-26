#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <winsock2.h>
#include "Poller.h"
#include "RedisClient.h"
#include "RedisSocket.h"
#include "ServerAddress.h"
#include "WindowsSockets.h"
#include "Utils.h"
using namespace utils;

class RedisServer {
public:
    void Start() {
        bind_socket_addr();
        register_listener_fd(listener_socket.get(),POLLIN);
        start_listening();
    }
    void Serve() {
      while (true) {
          //poller.log_arr();
          PollerStatus status = poller.POLL();
          if (status == utils::PollerStatus::Failed) {
              utils::Debug::die("Polling failure");
          }
          for (pollfd poll_fd : poller.array()) {
              const SOCKET fd = poll_fd.fd;
              if (poll_fd.revents == POLLERR || poll_fd.revents == POLLHUP) {
                  remove_client(fd);
                  continue; // Skip the removed poll_fd
              }
              if (fd == listener_socket.get() && (poll_fd.revents & POLLIN)) {
                  const SOCKET skc = accept_connection();
                  ConnectionState state = connections[skc].getState();
                  register_nonblocking_fd(skc,state);
              }
              if (fd != listener_socket.get() && poll_fd.revents) {
                  connection_io(fd);
              }
          }
      }
    }
private:
    void bind_socket_addr() const {
        int binding_status = bind(listener_socket.get(),address.get_ptr(),sizeof(sockaddr_in));
        if (binding_status == SOCKET_ERROR) {
            throw std::runtime_error("bind failed");
        }
        std::cout << "Bind Success" << std::endl;
    }
    void start_listening() const {
      int rv = listen(listener_socket.get(),SOMAXCONN);
        if (rv) {
            throw std::runtime_error("listening failed");
        }
        std::cout << "Listening on port 6379 \n";
    }
    SOCKET accept_connection()  {
        sockaddr_in client_address{};
        int addr_len = sizeof(struct sockaddr_in);
        const SOCKET sck = accept(listener_socket.get(),reinterpret_cast<sockaddr *>(&client_address),&addr_len);
        if (sck == INVALID_SOCKET)
            throw std::runtime_error("accepting client failed :" +  WSAGetLastError());
        connections.emplace(std::piecewise_construct,
        std::forward_as_tuple(sck),
        std::forward_as_tuple(client_address, sck));
        std::cout << "Client accepted : "  << sck << "\n";
        return sck;
    }
    void connection_io(const SOCKET& client_fd) {
        if (connections[client_fd].getState() == ConnectionState::Req) {
            state_request(client_fd);
            if (connections[client_fd].getState() == ConnectionState::End) {
                Debug::echo("Client disconnected");
                remove_client(client_fd);
            }
        }
    }
    bool receive_request(const SOCKET& client) {
        Connection* conn = connections[client].getConnection();
        char* r_buffer = conn->getReadBuffer();
        IOStatus io_status = ProtocolParser::recv_full(client, r_buffer, 4);
        switch (io_status) {
            case IOStatus::Error: {
                Debug::error("Socket Error");
                conn->setState(ConnectionState::End);
                break;
            }
            case IOStatus::Disconnected: {
                Debug::error("Client Disconnected");
                conn->setState(ConnectionState::End);
                break;
            }
            case IOStatus::Ok: {
                uint32_t len;
                ProtocolStatus ps = try_parse(r_buffer,len,4);
                if (ps == ProtocolStatus::Malformed) {
                    conn->setState(ConnectionState::End);
                }
                io_status = ProtocolParser::recv_full(client, r_buffer + 4, (int)len);
                if (io_status == IOStatus::Error || io_status == IOStatus::Disconnected) {
                    conn->setState(ConnectionState::End);
                }
                r_buffer[4 + len] = '\0'; //Add null termination
                std::cout << "[Client:"<< client << "] sent -> " << &r_buffer[4] << std::endl;
                //Ready to respond
                conn->setState(ConnectionState::Res);
                return true;
            }
            case IOStatus::WouldBlock: {
                return true;
            }
            default:
                Debug::error("Invalid Status");
        }
        return false;
    }
    static ProtocolStatus try_parse(const char* r_buffer,uint32_t& len , const size_t MaxCount) {
        memcpy(&len, r_buffer, MaxCount);
        if (len <= 0 || len > MAX_LENGTH) {
            Debug::error("Malformed protocol : Bad message length");
            return ProtocolStatus::Malformed;
        }
        return ProtocolStatus::Ok;
    }
    static bool send_response(Connection* conn) {
        char* w_buffer = conn->getWriteBuffer();
        const char reply[] = "Acknowledged";
        int length = static_cast<int>(strlen(reply));
        if (length <= 0 || length > utils::MAX_LENGTH) {
            conn->setState(ConnectionState::End);
            Debug::error("Inappropriate response length");
            return false;
        }
        memcpy(w_buffer, &length, 4);
        memcpy(&w_buffer[4], reply, length);
        IOStatus io_status = utils::ProtocolParser::send_full(conn->getSocket(), w_buffer, 4 + length);
        if (io_status == IOStatus::Error || io_status == IOStatus::Disconnected) {
            conn->setState(ConnectionState::End);
            return false;
        }
        return true;
    }
    void register_listener_fd(const SOCKET socket, const SHORT event) {
       // Poller::set_socket_nonblock(socket);
        poller.register_fd(socket, event);
    }
    void register_nonblocking_fd(const SOCKET socket, ConnectionState state) {
        int event = (state == ConnectionState::Req) ? POLLIN : POLLOUT;
        Poller::set_socket_nonblock(socket);
        poller.register_fd(socket, event);
    }
    void unregister_fd(const SOCKET socket) {
        poller.unregister_fd(socket);
    }
    void disconnect_client(const SOCKET socket) {
        connections.erase(socket);
        utils::Debug::echo("Client disconnected");
    }
    void remove_client(const SOCKET socket) {
        disconnect_client(socket);
        poller.unregister_fd(socket);
    }
    void state_request(const SOCKET socket) {
        while (try_fill_buffer(socket)) {
            // Keep filling the buffer with as much as possible
        }
    }
    bool try_fill_buffer(const SOCKET socket) {
        Connection* conn = connections[socket].getConnection();
        char *rbuff = conn->getReadBuffer();
        auto rbuff_size = conn->getReadBufferSize();
        const auto capacity = conn->getReadBufferCapacity();
        assert(rbuff_size < capacity);
        int rv = 0;
        //Try again if there's an error or an interruption
        do {
            const size_t remaining  = capacity - rbuff_size;
            rv = recv(socket,rbuff,(int)remaining ,0);
            // rv is the state of recv , and it changes errno in case of errors
            // EINTR means that the reading got interrupted by a signal
        } while (rv < 0 && errno == EINTR);
        if (rv < 0 && errno == EAGAIN) {
            //Non-fatal Error , try again in the next Poll() iteration
            utils::Debug::echo("Non-Fatal error: EAGAIN , Try again at the next iteration of Poll()");
            return false;
        }
        if (rv < 0) {
            //A fatal error occurred , aborting connection
            utils::Debug::echo("Fatal error , disconnecting...");
            conn->setState(utils::ConnectionState::End);
            return false;
        }
        if (rv == 0) {
            if (rbuff_size > 0) {
                utils::Debug::echo("WARNING: partial data detected on a disconnected client");
            }
            conn->setState(utils::ConnectionState::End);
            utils::Debug::error("disconnecting...");
            return false;
        }
        conn->extendBuffer(static_cast<size_t>(rv));
        assert(rbuff_size <= sizeof(rbuff));
        while (try_one_request(conn)) {/*DO NOTHING*/}
        return conn->getState() == utils::ConnectionState::Req;
        //Continue Looping if the current state is REQ
    }
    static bool try_one_request(Connection* conn) {
        auto r_buff_size = conn->getReadBufferSize();
        char* r_buffer = conn->getReadBuffer();
        if (r_buff_size < 4) {
            Debug::error("Not enough data in the buffer");
            return false;
        }
        uint32_t len = 0 ;
        if (try_parse(r_buffer,len, 4) == ProtocolStatus::Malformed) {
            Debug::error("Malformed request");
            conn->setState(ConnectionState::End);
            return false;
        }
        if (4 + len > r_buff_size) {
            Debug::error("Not enough data in the buffer");
            return false;
        }
        if (4 + len > MAX_LENGTH) {
            Debug::error("Message too long");
            conn->setState(ConnectionState::End);
            return false;
        }
        std::cout << "Client " << conn->getSocket() << " sent: " << std::string(&r_buffer[4], len) << "\n";
        size_t remaining = r_buff_size - (4 + len);
        if (remaining > 0) {
            //Remove the request
            std::memmove(&r_buffer,&r_buffer[4+len],remaining);
        }
        r_buff_size = remaining;
        conn->setState(ConnectionState::Res);
        state_response(conn);
        return conn->getState() == ConnectionState::Req;
    }
    static void state_response(Connection* conn) {
        bool io_status = send_response(conn);
        if (io_status == false) {
            Debug::error("Failed to send response");
            conn->setState(ConnectionState::End);
            return;
        }
        conn->setState(ConnectionState::Req);
    }
    void log_conn() {
        for (auto& conn : connections) {
            std::cout << conn.first << "->" << "Client.." <<std::endl;
        }
    }
public:
    RedisServer() {
        std::cout << "Server Running on port 6379" << std::endl;
    };
    ~RedisServer() = default;
    RedisServer(const RedisServer &) = delete;
    RedisServer &operator=(const RedisServer &) = delete;
private:
    WindowsSocket wsa;
    RedisSocket listener_socket;
    ServerAddress address;
    Poller poller;
    std::unordered_map<SOCKET,RedisClient> connections{};
};
