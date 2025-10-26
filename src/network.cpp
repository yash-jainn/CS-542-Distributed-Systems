// src/network.cpp
#include "network.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <cstring>

using namespace std;

bool TCPConnection::send_json(const json &j) {
    string s = j.dump();
    s.push_back('\n'); // line-delimited
    const char *buf = s.c_str();
    size_t left = s.size();
    while (left > 0) {
        ssize_t n = ::send(sockfd, buf, left, 0);
        if (n <= 0) return false;
        buf += n; left -= n;
    }
    return true;
}

bool TCPConnection::recv_json(json &j) {
    static thread_local std::string buffer;
    char tmp[1024];
    while (true) {
        // check if we already have a newline
        auto pos = buffer.find('\n');
        if (pos != string::npos) {
            string line = buffer.substr(0, pos);
            buffer.erase(0, pos+1);
            try {
                j = json::parse(line);
                return true;
            } catch(...) {
                cerr << "[network] parse error for line: " << line << "\n";
                return false;
            }
        }
        ssize_t rec = ::recv(sockfd, tmp, sizeof(tmp), 0);
        if (rec <= 0) return false;
        buffer.append(tmp, rec);
    }
}

int connect_to(const std::string &ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    if (connect(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) { close(sockfd); return -1; }
    return sockfd;
}

int create_server_socket(int port, int backlog) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) { close(sockfd); return -1; }
    if (listen(sockfd, backlog) < 0) { close(sockfd); return -1; }
    return sockfd;
}
