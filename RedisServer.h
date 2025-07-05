#pragma once
#include <cstdint>
#include <map>
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
                remove_client(client_fd);
            }
        }
    }
    static ProtocolStatus parse_command(const char* r_buffer,uint32_t& len, const size_t buffer_size,std::vector<std::string>& cmds) {
        uint32_t nstr = 0;
        memcpy(&nstr, r_buffer, 4); // extract the current number of strings in the cmd
        if (nstr < 2 || nstr > 64) { // arbitrary sanity check
            Debug::error("Malformed protocol: invalid nstr");
            return ProtocolStatus::Malformed;
        }
        size_t offset = 4;
        for (uint32_t i = 0; i < nstr; i++) {
            if (offset + 4 >= buffer_size) {
                Debug::error("Malformed protocol : Bad command length");
                return ProtocolStatus::Malformed;
            }
            uint32_t current_len = 0;
            memcpy(&current_len, r_buffer + offset, 4);
            if (current_len > buffer_size - 4 || current_len <= 0) {
                Debug::error("Malformed protocol : Bad command length");
                return ProtocolStatus::Malformed;
            }
            offset += 4;
            if (offset + current_len > buffer_size) {
                Debug::error("Malformed protocol : Incomplete Command");
                return ProtocolStatus::Incomplete;
            }
            cmds.emplace_back(r_buffer + offset, current_len);
            offset += current_len;
        }
        len = offset - 4;
        std::cout << "nstr = " << nstr << std::endl;
        std::cout << "len = " << len << std::endl;
        return ProtocolStatus::Ok;
    }
    static bool send_response(Connection* conn ,CommandStatus status) {
        char* w_buffer = conn->getWriteBuffer();
        std::string statusStr = statusToString(status);
        int length = static_cast<int>(statusStr.length());
        if (length <= 0 || length > utils::MAX_LENGTH - 4) {
            conn->setState(ConnectionState::End);
            Debug::error("Inappropriate response length");
            return false;
        }
        // Copy status code (4 bytes)
        memcpy(w_buffer, &status, 4);
        // Copy string content
        memcpy(w_buffer + 4, statusStr.c_str(), length);
        // Send full response
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
        //assert(rbuff_size < capacity);
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
        std::cout << rv << std::endl;
        conn->extendBuffer(static_cast<size_t>(rv));
        assert(rbuff_size <= sizeof(rbuff));
        while (try_one_request(conn)) {/*DO NOTHING*/}
        return
        (conn->getState() == utils::ConnectionState::Req);
        //Continue Looping if the current state is REQ
    }
    bool try_one_request(Connection* conn) {
        auto r_buff_size = conn->getReadBufferSize();
        if (r_buff_size <= 0) {
            Debug::echo("No commands left , disconnecting...");
            conn->setState(utils::ConnectionState::End);
            return false;
        }
        char* r_buffer = conn->getReadBuffer();
        std::vector<std::string> cmds;
        uint32_t len = 0 ; // The command length
        auto protocol_status = parse_command(r_buffer,len,r_buff_size,cmds);
        std::cout << "Raw buffer (hex): ";
        for (size_t i = 0; i < r_buff_size; i++) {
            printf("%02x ", static_cast<unsigned char>(r_buffer[i]));
        }
        std::cout << "\n";
        for (size_t i = 0; i < r_buff_size; i++) {
            std::cout << r_buffer[i];
        }
        std::cout << "\n";
        if (protocol_status == ProtocolStatus::Malformed || protocol_status == ProtocolStatus::Incomplete ) {
            conn->setState(ConnectionState::End);
            return false;
        }
        if ((len + 4) > r_buff_size) {
            Debug::error("Not enough data in the buffer");
            return false;
        }
        if (8 + len > MAX_LENGTH) {
            Debug::error("Message too long");
            conn->setState(ConnectionState::End);
            return false;
        }
        CommandStatus cmd_state = do_cmd(cmds);
        const size_t total_request_size = 4 + len;
        const size_t remaining = r_buff_size - total_request_size;
        std::cout << "re " << remaining << std::endl;
        if (remaining >= 0) {
            std::cout << "remaining = " << remaining << std::endl;
            std::memmove(r_buffer, r_buffer + total_request_size, remaining);
            conn->setReadBufferSize(remaining);
        }

        for (size_t i = 0; i < remaining; i++) {
            std::cout << r_buffer[i];
        }
        std::cout << "\n";
        conn->setState(ConnectionState::Res);
        state_response(conn,cmd_state);
        return conn->getState() == ConnectionState::Req;
    }
    static void state_response(Connection* conn,CommandStatus status) {
        bool io_status = send_response(conn,status);
        if (io_status == false) {
            Debug::error("Failed to send response");
            conn->setState(ConnectionState::End);
            return;
        }
        conn->setState(ConnectionState::Req);
    }
    CommandStatus do_cmd(const std::vector<std::string>& cmds) {
        size_t cmd_size = cmds.size();
        if (cmd_size < 2 || cmd_size > 3) {
            Debug::echo("Invalid command size");
            return CommandStatus::InvalidCommand;
        }
        if (cmd_size == 2 && equalsIgnoreCase(cmds[0], "get")) {
            std::string val;
            const bool status = GET(cmds[1],val);
            if (status == false) {
                Debug::echo("KeyNotFound");
                return CommandStatus::KeyNotFound;
            }
            std::cout << "GOT : " << val << std::endl;
            Debug::echo("OK (get)");
            return CommandStatus::OK;
        }
        if (cmd_size == 3 && equalsIgnoreCase(cmds[0], "set")) {
            const bool status = SET(cmds[1],cmds[2]);
            if (status == false) {
                Debug::echo("ValAlreadyExists");
                return CommandStatus::ValAlreadyExists;
            }
            Debug::echo("OK (set)");
            return CommandStatus::OK;
        }
        if (cmd_size == 2 && equalsIgnoreCase(cmds[0], "del")) {
            const bool status =  DEL(cmds[1]);
            if (status == false) {
                Debug::echo("KeyNoExist");
                return CommandStatus::KeyNoExist;
            }
            Debug::echo("OK (del)");
            return CommandStatus::OK;
        }
        Debug::echo("Invalid command");
        return CommandStatus::SyntaxError;
    }
    bool GET(const std::string& key,std::string& val) {
        if (map.empty()) {
            return false;
        }
        if (!map.contains(key)) {
            return false;
        }
        val = map[key];
        return true;
    }
    bool SET(const std::string& key,const std::string& val) {
        if (map.contains(key)) {
            std::cerr << "Key already exists" << std::endl;
            return false;
        }
        map.insert({key,val});
        return true;
    }
    bool DEL(const std::string& key) {
        if (map.empty() || !map.contains(key)) {
            std::cerr << "Key doesn't exist" << std::endl;
            return false;
        }
        map.erase(key);
        return true;
    }
    static std::string statusToString(CommandStatus status) {
        switch (status) {
            case CommandStatus::InvalidCommand: return "InvCMD";
            case CommandStatus::KeyNoExist: return "KnX";
            case CommandStatus::ValAlreadyExists: return "VALX";
            case CommandStatus::SyntaxError: return "SynErr";
            case CommandStatus::KeyNotFound: return "KnF";
            case CommandStatus::OK: return "Ok";
            default: return "?";
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
    std::map<std::string,std::string> map{};
};
