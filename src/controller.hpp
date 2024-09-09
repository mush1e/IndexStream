#include <regex>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <unordered_map>
#include <ctime>

#include "http.hpp"

#ifndef RFSS_CONTROLLER_HPP
#define RFSS_CONTRILLER_HPP

namespace index_stream {

    // helpers
    void serveStaticFile(const std::string& file_path, int client_socket);
    std::unordered_map<std::string, std::string> parse_parameters(std::string uri);
    std::ostream& operator<<(std::ostream& os, const HTTPRequest& req);
    std::string get_form_field(const std::string& body, const std::string& field_name);
    std::string url_decode(const std::string& str);
    // controllers
    void handle_get_home(HTTPRequest& req, int client_socket);
    void handle_post_search(HTTPRequest& req, int client_socket);
}

#endif