#pragma once

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
#include <sqlite3.h>


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
            std::cout << "Initializing DB....\n";
            if (sqlite3_open("../db/document_store.db", &db_) != SQLITE_OK) {
                std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
                exit(1);
            }
            create_tables();
            std::cout << "Indexer Initiated...." << std::endl;
        };

        void document_parser(std::string& file_name, std::string& document);
        void directory_spider();
        void index_updater(std::string& document, std::string& url);
        std::string url_extractor(std::string file_name);

    private:
        sqlite3* db_; // DB instance
        std::string dump_dir {};
        std::string index_file {};
        // Inverted Index; 
        std::unordered_map<std::string, std::priority_queue<std::pair<std::string, long long>, std::vector<std::pair<std::string, long long>>, CompareSize>> term_document_matrix;
        std::unordered_set<std::string> indexed_documents;
        void create_tables();
        void execute_sql(const char* query);
        void print_term_document_matrix() const;
        long long get_or_insert_term(const std::string& term);
        long long get_or_insert_document(const std::string& document);  
        void insert_term_document_matrix(long long term_id, long long doc_id, long long frequency);
        bool delete_file(const std::string& file_name);
    };

}