#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>

std::string host = "127.0.0.1";
int port = 6379;

void parse_args(const int argc, char* argv[]) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string arg = argv[i]; arg == "-host") {
            host = argv[++i];
        } else if (arg == "-port") {
            port = std::stoi(argv[++i]);
        }
    }
}

int main(const int argc, char* argv[]) {
    parse_args(argc, argv);

    // Create a socket using IPv4 (AF_INET) and TCP (SOCK_STREAM)
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    // host-to-network short: converts a 16-bit integer to network byte order (big-endian)
    addr.sin_port = htons(port);
    // converts IP address from text to binary form
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << host << "\n";
        return 1;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Connection failed to " << host << ":" << port << "\n";
        return 1;
    }

    std::cout << "Connected to " << host << ":" << port << "\n";

    std::string line;
    char buffer[1024];
    while (std::getline(std::cin, line)) {
        line += '\n';
        send(sock, line.c_str(), line.size(), 0);
        if (const int n = read(sock, buffer, sizeof(buffer) - 1); n > 0) {
            buffer[n] = '\0';
            std::cout << buffer;
        } else if (n == 0) {
            std::cout << "Connection closed by server\n";
            break;
        } else {
            std::cerr << "Read error\n";
            break;
        }
    }
    close(sock);
    return 0;
}
