#include "http_request_handler.hpp"

namespace index_stream {


    auto handle_request(HTTPRequest& req, int client_socket) -> void {
        if (req.method == "GET") {
            if (req.URI == "/")     handle_get_home(req, client_socket);
        }
    }
}