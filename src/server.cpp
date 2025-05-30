#include "server.hpp"
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <vector>

RedisServer::RedisServer(const int port) {
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    constexpr int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    listen(listen_fd, SOMAXCONN);

    epoll_fd = epoll_create1(0);
    epoll_event ev { EPOLLIN, { .fd = listen_fd } };
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);
}

void RedisServer::run() {
    while (true) {
        epoll_event events[1024];
        const int nfds = epoll_wait(epoll_fd, events, 1024, -1);
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == listen_fd) {
                accept_connection();
            } else {
                handle_client(events[i].data.fd);
            }
        }
    }
}

void RedisServer::accept_connection() const {
    sockaddr_in client_addr {};
    socklen_t client_len = sizeof(client_addr);
    const int client_fd = accept(listen_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    epoll_event ev { EPOLLIN, { .fd = client_fd } };
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
}

void RedisServer::handle_client(const int client_fd) {
    char buf[1024];
    const int n = read(client_fd, buf, sizeof(buf));
    if (n <= 0) {
        close(client_fd);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
        client_buffers.erase(client_fd);
        return;
    }

    client_buffers[client_fd] += std::string(buf, n);
    size_t pos;
    while ((pos = client_buffers[client_fd].find('\n')) != std::string::npos) {
        std::string command = client_buffers[client_fd].substr(0, pos);
        client_buffers[client_fd].erase(0, pos + 1);
        parse_and_execute(client_fd, command);
    }
}

void RedisServer::send_response(const int client_fd, const std::string& response) {
    const std::string resp = response + "\n";
    send(client_fd, resp.c_str(), resp.size(), 0);
}

void RedisServer::parse_and_execute(const int client_fd, const std::string& command) {
    auto tokens = std::vector<std::string>();
    std::istringstream iss(command);
    std::string token;
    while (iss >> token) tokens.push_back(token);

    if (tokens.empty()) return;
    if (tokens[0] == "SET" && tokens.size() == 3) {
        kv_store[tokens[1]] = tokens[2];
        send_response(client_fd, "OK");
    } else if (tokens[0] == "GET" && tokens.size() == 2) {
        if (const auto it = kv_store.find(tokens[1]); it != kv_store.end())
            send_response(client_fd, it->second);
        else
            send_response(client_fd, "(nil)");
    } else {
        send_response(client_fd, "ERR unknown command");
    }
}
