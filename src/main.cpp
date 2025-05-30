#include "server.hpp"

int main() {
    RedisServer server(6379);
    server.run();
    return 0;
}
