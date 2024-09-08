#include "http.hpp"

namespace index_stream {


    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Generate HTTP response string ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    std::string HTTPResponse::generate_response() const {
        std::ostringstream response {};
        response << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";
        response << "Content-Type: " << content_type << "\r\n";

        if (!this->location.empty())
            response << "Location: " << location << "\r\n";

        if (!this->cookies.first.empty() && !this->cookies.first.empty())
            response << "Set-Cookie: " << cookies.first << "=" << cookies.second << "; SameSite=None; Secure; HttpOnly\r\n";

        response << "Content-Length: " << body.length() << "\r\n";
        response << "\r\n";
        response << body;
        return response.str();
    }

}