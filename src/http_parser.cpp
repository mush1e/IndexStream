#include "http_parser.hpp"

namespace index_stream {

    // Size for BUFFER to read incoming http requests
    const size_t BUFFER_SIZE = 4016;


    // ~~~~~~~~~~~~~~~~~~~~~~~ Helper Function to trim white spaces from strings ~~~~~~~~~~~~~~~~~~~~~~~
    std::string trim(const std::string& str) {
        auto start = str.find_first_not_of(" \t\n\r\f\v");
        
        if (start == std::string::npos) {
            return ""; 
        }

        auto end = str.find_last_not_of(" \t\n\r\f\v");
        return str.substr(start, end - start + 1);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Function to parse form data incoming with request ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto parse_form_data(const std::string& form_data, HTTPRequest& req) -> void {
        std::istringstream iss(form_data);
        std::string pair;
        while (std::getline(iss, pair, '&')) {
            size_t pos = pair.find('=');
            if (pos != std::string::npos) {
                std::string key = pair.substr(0, pos);
                std::string value = pair.substr(pos + 1);
                key = url_decode(key);
                value = url_decode(value);
                req.body.append(key + ": " + value + "\n");
            }
        }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Function to parse header data from incoming request ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    void parse_headers(HTTPRequest& req, const std::string& header_str) {
        std::istringstream iss(header_str);
        std::string line;

        std::getline(iss, line);
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.URI >> req.version;
        req.method = trim(req.method);
        req.URI = trim(req.URI);
        req.version = trim(req.version);

        while (std::getline(iss, line) && line != "\r") {
            if (line.back() == '\r') {
                line.pop_back(); 
            }
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string key = trim(line.substr(0, pos));
                std::string value = trim(line.substr(pos + 1));

                if (key == "Cookie") {
                    std::istringstream cookie_stream(value);
                    std::string cookie_pair;
                    while (std::getline(cookie_stream, cookie_pair, ';')) {
                        size_t eq_pos = cookie_pair.find('=');
                        if (eq_pos != std::string::npos) {
                            std::string cookie_name = trim(cookie_pair.substr(0, eq_pos));
                            std::string cookie_value = trim(cookie_pair.substr(eq_pos + 1));
                            req.cookies.emplace_back(cookie_name, cookie_value);
                        }
                    }
                } else {
                    req.headers.emplace_back(key, value);
                }
            }
        }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Function to parse body data from incoming request ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    void parse_body(HTTPRequest& req, const std::string& body_content) {
        bool is_form_data = false;
        bool is_multipart_data = false;

        for (const auto& header : req.headers) {
            if (header.first == "Content-Type") {
                if (header.second.find("application/x-www-form-urlencoded") != std::string::npos) {
                    is_form_data = true;
                } else if (header.second.find("multipart/form-data") != std::string::npos) {
                    is_multipart_data = true;
                    const std::string boundaryPrefix = "boundary=";
                    size_t pos = header.second.find(boundaryPrefix);
                    if (pos != std::string::npos) {
                        size_t start = pos + boundaryPrefix.length();
                        size_t end = header.second.find(';', start);
                        if (end == std::string::npos) {
                            end = header.second.length();
                        }
                        req.multipart_boundary = header.second.substr(start, end - start);
                    }
                }
                break;
            }
        }

        if (is_form_data) {
            parse_form_data(body_content, req);
        } else {
            req.body = body_content;
        }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Function to parse incoming requests ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    void handle_client(int client_socket) {
        char buffer[BUFFER_SIZE];
        HTTPRequest request;
        std::string http_request_string;
        bool headers_received = false;
        size_t content_length = 0;

        auto start_time = std::chrono::steady_clock::now();

        while (true) {
            ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_read <= 0) {
                if (bytes_read < 0) {
                    std::cerr << "Error: Client disconnected or no data received!\n";
                }
                close(client_socket);
                return;
            }

            http_request_string.append(buffer, bytes_read);

            if (!headers_received) {
                size_t pos = http_request_string.find("\r\n\r\n");
                if (pos != std::string::npos) {
                    headers_received = true;
                    std::string header_str = http_request_string.substr(0, pos);
                    parse_headers(request, header_str);
                    
                    for (const auto& header : request.headers) {
                        if (header.first == "Content-Length") {
                            content_length = std::stoi(header.second);
                            break;
                        }
                    }
                    
                    http_request_string.erase(0, pos + 4); 
                }
            }

            if (headers_received && http_request_string.size() >= content_length) {
                parse_body(request, http_request_string.substr(0, content_length));
                break;
            }

            auto current_time = std::chrono::steady_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
            if (elapsed_time > 30) {
                std::cerr << "Error: Timeout while reading from client socket\n";
                close(client_socket);
                return;
            }
        }
        handle_request(request, client_socket);
        close(client_socket);
    }
}