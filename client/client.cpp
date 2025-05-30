#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>

int main() {
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6379);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));

    std::string line;
    char buffer[1024];
    while (std::getline(std::cin, line)) {
        line += '\n';
        send(sock, line.c_str(), line.size(), 0);
        if (const int n = read(sock, buffer, sizeof(buffer) - 1); n > 0) {
            buffer[n] = '\0';
            std::cout << "Response: " << buffer;
        }
    }
    close(sock);
    return 0;
}
