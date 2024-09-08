#include "server.hpp"

auto main() -> int {
    index_stream::HTTP_Server server(8080);
    server.start();
    return 0;
}