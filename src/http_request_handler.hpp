#include "http.hpp"
#include "controller.hpp"

#ifndef HTTP_REQUEST_HANDLER_HPP
#define HTTP_REQUEST_HANDLER_HPP

namespace rfss {

    void handle_request(HTTPRequest& req, int client_socket);

}

#endif