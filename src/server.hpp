#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <fcntl.h>
#include <sys/select.h>

#include "threadpool.hpp"
#include "http_parser.hpp"

#ifndef RFSS_SERVER_HPP
#define RFSS_SERVER_HPP

namespace index_stream {
    class HTTP_Server {
    private:
        int server_socket {};
        int port{};
        sockaddr_in server_address {};
        ThreadPool thread_pool{4};

    public:
        HTTP_Server(int port);
        ~HTTP_Server();
        void start();
    };
}

#endif