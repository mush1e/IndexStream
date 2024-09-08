#include <vector>
#include <string>
#include <sstream>
#include <iostream>

#ifndef RFSS_HTTP_HPP
#define RFSS_HTTP_HPP

namespace index_stream {

    struct HTTPResponse {
        int status_code {};
        std::string status_message {};
        std::string content_type = "text/plain";
        std::string body {};
        std::string location {};
        std::pair<std::string, std::string> cookies {};
        std::string generate_response() const;
        void set_JSON_content(const std::string& json_data);
    };

    struct HTTPRequest {
        std::string method   {};
        std::string URI      {};
        std::string version  {};
        std::string multipart_boundary {};
        std::vector<std::pair<std::string, std::string>> headers {};
        std::vector<std::pair<std::string, std::string>> cookies {};
        std::string body     {};
    };

}

#endif