#include "server.h"
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <vector>

RedisServer::RedisServer(const int port) {
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    // allow the socket to reuse the address (avoids "address already in use" error)
    constexpr int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // set the file descriptor to non-blocking mode
    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // Listen on all available network interfaces
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
    for (char& i : tokens[0]) {
        i = toupper(i);
    }
    const std::string command_type = tokens[0];
    if (command_type.length() < 2) {
        send_response(client_fd, "Unknown command " + command_type);
    }
    if (command_type[0] == 'L') {
        // List
        if (command_type == "LPUSH") {
            if (tokens.size() == 3) {
                if (const auto it = kv_store.find(tokens[1]); it == kv_store.end()) {
                    auto ro = RedisObject(RedisObject::Type::LIST);
                    auto res = ro.l_push(tokens[2]);
                    kv_store.emplace(tokens[1], std::move(ro));
                    send_response(client_fd, res);
                } else {
                    auto res = it->second.l_push(tokens[2]);
                    send_response(client_fd, res);
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else if (command_type == "LPOP") {
            if (tokens.size() == 2) {
                if (const auto it = kv_store.find(tokens[1]); it != kv_store.end()) {
                    auto res = it->second.l_pop();
                    send_response(client_fd, res);
                } else {
                    send_response(client_fd, "(nil)");
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else if (command_type == "LRANGE") {
            if (tokens.size() == 4) {
                if (const auto it = kv_store.find(tokens[1]); it != kv_store.end()) {
                    try {
                        int idx1 = std::stoi(tokens[2]);
                        int idx2 = std::stoi(tokens[3]);
                        auto res = it->second.l_range(idx1, idx2);
                        send_response(client_fd, res);
                    } catch (const std::invalid_argument &e) {
                        send_response(client_fd, "Index should be an integer");
                    } catch (const std::out_of_range &e) {
                        send_response(client_fd, "Index should be an integer");
                    }
                } else {
                    send_response(client_fd, "(nil)");
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else if (command_type == "LLEN") {
            if (tokens.size() == 2) {
                if (const auto it = kv_store.find(tokens[1]); it != kv_store.end()) {
                    auto res = it->second.l_len();
                    send_response(client_fd, res);
                } else {
                    send_response(client_fd, "(nil)");
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else {
            send_response(client_fd, "Unknown command " + command_type);
        }
    } else if (command_type[0] == 'R') {
        // List
        if (command_type == "RPUSH") {
            if (tokens.size() == 3) {
                if (const auto it = kv_store.find(tokens[1]); it == kv_store.end()) {
                    auto ro = RedisObject(RedisObject::Type::LIST);
                    auto res = ro.r_push(tokens[2]);
                    kv_store.emplace(tokens[1], std::move(ro));
                    send_response(client_fd, res);
                } else {
                    auto res = it->second.r_push(tokens[2]);
                    send_response(client_fd, res);
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else if (command_type == "RPOP") {
            if (tokens.size() == 2) {
                if (const auto it = kv_store.find(tokens[1]); it != kv_store.end()) {
                    auto res = it->second.r_pop();
                    send_response(client_fd, res);
                } else {
                    send_response(client_fd, "(nil)");
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else {
            send_response(client_fd, "Unknown command " + command_type);
        }
    } else if (command_type[0] == 'H') {
        // Hash
    } else if (command_type[0] == 'S' && command_type[1] != 'E') {
        // Set
    } else if (command_type[0] == 'Z') {
        // ZSet
    } else {
        // String
        if (command_type == "GET") {
            if (tokens.size() == 2) {
                if (const auto it = kv_store.find(tokens[1]); it != kv_store.end())
                    send_response(client_fd, it->second.get());
                else
                    send_response(client_fd, "(nil)");
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else if (command_type == "SET") {
            if (tokens.size() == 3) {
                if (const auto it = kv_store.find(tokens[1]); it == kv_store.end()) {
                    auto ro = RedisObject(RedisObject::Type::STRING);
                    auto res = ro.set(tokens[2]);
                    kv_store.emplace(tokens[1], std::move(ro));
                    send_response(client_fd, res);
                } else {
                    auto res = it->second.set(tokens[2]);
                    send_response(client_fd, res);
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else if (command_type == "SETNX") {
            if (tokens.size() == 3) {
                if (const auto it = kv_store.find(tokens[1]); it == kv_store.end()) {
                    auto ro = RedisObject(RedisObject::Type::STRING);
                    auto res = ro.set(tokens[2]);
                    kv_store.emplace(tokens[1], std::move(ro));
                    send_response(client_fd, res);
                } else {
                    send_response(client_fd, "(nil)");
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else if (command_type == "INCR") {
            if (tokens.size() == 2) {
                if (const auto it = kv_store.find(tokens[1]); it != kv_store.end()) {
                    auto res = it->second.incr();
                    send_response(client_fd, res);
                } else {
                    send_response(client_fd, "(nil)");
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else if (command_type == "INCRBY") {
            if (tokens.size() == 3) {
                if (const auto it = kv_store.find(tokens[1]); it != kv_store.end()) {
                    try {
                        int increment = std::stoi(tokens[2]);
                        auto res = it->second.incr_by(increment);
                        send_response(client_fd, res);
                    } catch (...) {
                        send_response(client_fd, "Increment should be an integer");
                    }
                } else {
                    send_response(client_fd, "(nil)");
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else if (command_type == "INCRBYFLOAT") {
            if (tokens.size() == 3) {
                if (const auto it = kv_store.find(tokens[1]); it != kv_store.end()) {
                    try {
                        double increment = std::stod(tokens[2]);
                        auto res = it->second.incr_by_float(increment);
                        send_response(client_fd, res);
                    } catch (...) {
                        send_response(client_fd, "Increment should be a float number");
                    }
                } else {
                    send_response(client_fd, "(nil)");
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else if (command_type == "EXISTS") {
            if (tokens.size() == 2) {
                if (const auto it = kv_store.find(tokens[1]); it != kv_store.end()) {
                    send_response(client_fd, "true");
                } else {
                    send_response(client_fd, "false");
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else if (command_type == "DEL") {
            if (tokens.size() == 2) {
                if (const auto it = kv_store.find(tokens[1]); it != kv_store.end()) {
                    kv_store.erase(tokens[1]);
                    send_response(client_fd, "OK");
                } else {
                    send_response(client_fd, "(nil)");
                }
            } else {
                send_response(client_fd, "Incorrect argument number");
            }
        } else {
            send_response(client_fd, "Unknown command " + command_type);
        }
    }
}
