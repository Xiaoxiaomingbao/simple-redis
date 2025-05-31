#pragma once
#include <object.h>
#include <unordered_map>
#include <string>
#include <sys/epoll.h>

class RedisServer {
public:
    explicit RedisServer(int port);
    void run();

private:
    int listen_fd;
    int epoll_fd;
    std::unordered_map<int, std::string> client_buffers;
    std::unordered_map<std::string, RedisObject> kv_store;

    void accept_connection() const;
    void handle_client(int client_fd);
    static void send_response(int client_fd, const std::string& response);
    void parse_and_execute(int client_fd, const std::string& command);
};
