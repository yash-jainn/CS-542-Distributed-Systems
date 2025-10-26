#pragma once
// include/network.h
#include <string>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using json = nlohmann::json;

class TCPConnection {
    int sockfd;
public:
    TCPConnection(int fd) : sockfd(fd) {}
    ~TCPConnection() { if (sockfd >= 0) close(sockfd); }

    bool send_json(const json &j);
    bool recv_json(json &j);

    int fd() const { return sockfd; }
};

int connect_to(const std::string &ip, int port);
int create_server_socket(int port, int backlog = 5);
