#pragma once

#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <vector>
#include <string>
#include <regex>
#include <sstream>
#include <algorithm>


namespace fs = std::filesystem;

namespace indexer {

    struct CompareSize {
        bool operator()(const std::pair<std::string, long long>& a, const std::pair<std::string, long long>& b) {
            return a.second < b.second;
        }
    };

    class Indexer {
    public:
        Indexer(std::string dump_dir  = "../raw_dump", std::string index_file  = "./index.csv") : dump_dir(dump_dir), index_file(index_file) {
            std::cout << "Indexer Initiated...." << std::endl;
        };

        void document_parser(std::string& file_name, std::string& document);
        void directory_spider();
        // void directory_spider(std::string& file_name);
        void index_updater(std::string& document, std::string& url);
        std::string url_extractor(std::string file_name);

    private:
        std::string dump_dir {};
        std::string index_file {};
        // Inverted Index; 
        std::unordered_map<std::string, std::priority_queue<std::pair<std::string, long long>, std::vector<std::pair<std::string, long long>>, CompareSize>> term_document_matrix;
        std::unordered_set<std::string> indexed_documents;
        void print_term_document_matrix() const;
        void insert_term_document_matrix();
        bool delete_file(const std::string& file_name);
    };

}