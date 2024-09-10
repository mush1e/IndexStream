#include "controller.hpp"

namespace index_stream {

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Helper to send 400 response ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto send_bad_request = [](int client_socket) {
        HTTPResponse response;
        std::string http_response;
        response.status_code = 400;
        response.status_message = "Bad Request";
        http_response = response.generate_response();
        send(client_socket, http_response.c_str(), http_response.length(), 0);
    };

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Helper to send 500 response ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto send_internal_server_error = [](int client_socket) {
        HTTPResponse response;
        std::string http_response;
        response.status_code = 500;
        response.status_message = "Internal Server Error";
        http_response = response.generate_response();
        send(client_socket, http_response.c_str(), http_response.length(), 0);
    };

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Helper to send 404 response ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto send_not_found_request = [](int client_socket) {
        HTTPResponse response;
        std::string http_response;
        response.status_code = 404;
        response.status_message = "Unable to locate resource";
        http_response = response.generate_response();
        send(client_socket, http_response.c_str(), http_response.length(), 0);
    };

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Helper to print request ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    std::ostream& operator<<(std::ostream& os, const HTTPRequest& req) {
        os << "Method: " << req.method << "\n";
        os << "URI: " << req.URI << "\n";
        os << "Version: " << req.version << "\n";
        os << "Headers:\n";

        for (const auto& header : req.headers)
            os << "  " << std::setw(20) << std::left << header.first << ": " << header.second << "\n";

        os << "Cookies:\n";

        for (const auto& cookie : req.cookies)
            os << "  " << std::setw(20) << std::left << cookie.first << ": " << cookie.second << "\n";

        os << "Body: " << req.body << "\n";

        return os;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Helper to decode URL ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto url_decode(const std::string& str) -> std::string {
        int i = 0;
        std::stringstream decoded;

        while (i < str.length()) {
            if (str[i] == '%') {
                if (i + 2 < str.length()) {
                    int hexValue;
                    std::istringstream(str.substr(i + 1, 2)) >> std::hex >> hexValue;
                    decoded << static_cast<char>(hexValue);
                    i += 3;
                } else {
                    // If '%' is at the end of the string, leave it unchanged
                    decoded << '%';
                    i++;
                }
            } else if (str[i] == '+') {
                decoded << ' ';
                i++;
            } else {
                decoded << str[i];
                i++;
            }
        }
        return decoded.str();
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~ Helper to get form values for field name ~~~~~~~~~~~~~~~~~~~~~~~
    auto get_form_field(const std::string& body, const std::string& field_name) -> std::string {
        std::string field_value;
        size_t pos = body.find(field_name + ": ");
        if (pos != std::string::npos) {
            pos += field_name.length() + 1;
            size_t end_pos = body.find("\n", pos);
            end_pos = (end_pos == std::string::npos) ? body.length() : end_pos;
            field_value = body.substr(pos, end_pos - pos);
        }
        return url_decode(field_value);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~ Helper to serve static HTML ~~~~~~~~~~~~~~~~~~~~~~~
    auto serveStaticFile(const std::string& file_path, int client_socket) -> void {
        std::ifstream file(file_path);

        if (file.good()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();

            std::string response = "HTTP/1.1 200 OK\r\nContent-Length: "
                                    + std::to_string(content.length())
                                    + "\r\n\r\n"
                                    + content;

            send(client_socket, response.c_str(), response.length(), 0);
        }
        else
            send_not_found_request(client_socket);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~ Helper to parse query parameters ~~~~~~~~~~~~~~~~~~~~~~~
    auto parse_query_params(const std::string& query, std::unordered_map<std::string, std::string>& query_params) -> void {
         
        size_t start = query.find('?') + 1;
        while (start < query.length()) {
            size_t end = query.find('&', start);  // Find the next '&'
            std::string pair = (end == std::string::npos) ? query.substr(start) : query.substr(start, end - start);
            
            size_t delimiterPos = pair.find('=');
            if (delimiterPos != std::string::npos) {
                std::string key = pair.substr(0, delimiterPos);
                std::string value = pair.substr(delimiterPos + 1);
                query_params[key] = value;  // Store key-value pair
            }
            start = (end == std::string::npos) ? query.length() : end + 1;
        }
    } 

    // ~~~~~~~~~~~~~~~~~~~~~~~ GET controller for home route ~~~~~~~~~~~~~~~~~~~~~~~
    auto handle_get_home(HTTPRequest& req, int client_socket) -> void {
        serveStaticFile("../public/index.html", client_socket);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~ GET controller for home route ~~~~~~~~~~~~~~~~~~~~~~~
    auto handle_get_search(HTTPRequest& req, int client_socket) -> void {
        HTTPResponse response {};
        std::string http_response {};
        std::unordered_map<std::string, std::string> query_params;
        parse_query_params(req.URI, query_params);
        for (const auto& q : query_params)
            std::cout << q.first << " - " << q.second << std::endl;

        serveStaticFile("../public/under_construction.html", client_socket);
    }

}