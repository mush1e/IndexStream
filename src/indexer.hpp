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
#include <thread>
#include <future>
#include <mutex>
#include <sqlite3.h>


namespace fs = std::filesystem;

namespace indexer {
    class Indexer {
    public:
        static Indexer& get_instance();
        Indexer(const Indexer&) = delete;
        Indexer& operator=(const Indexer&) = delete;

        void document_parser(const std::string& file_name, std::string& document);
        void directory_spider();
        void index_updater(std::string& document, std::string& url);
        std::string url_extractor(std::string file_name);
        std::vector<std::pair<std::string, double>> search(const std::string& query_term);

    private:
        sqlite3* db_; 
        std::string dump_dir {};
        std::mutex file_mutex;
        std::unordered_map<std::string, std::queue<std::pair<std::string, long long>>> term_document_matrix;
        std::unordered_set<std::string> indexed_documents;
        std::vector<std::string> tokenize_query(const std::string& query);
        void create_tables();
        void execute_sql(const char* query);
        void process_file(const std::string& f_name);
        void print_term_document_matrix() const;
        void insert_term_document_matrix(long long term_id, long long doc_id, long long frequency);
        void transform_to_persist();
        void compute_tf_idf();
        void update_idf();
        bool delete_file(const std::string& file_name);
        long long get_or_insert_term(const std::string& term);
        long long get_or_insert_document(const std::string& document);  


        Indexer(std::string dump_dir  = "../raw_dump", std::string index_file  = "./index.csv") : dump_dir(dump_dir) {
            std::cout << "Initializing DB....\n";
            if (sqlite3_open("../db/document_store.db", &db_) != SQLITE_OK) {
                std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
                exit(1);
            }
            create_tables();
            std::cout << "Indexer Initiated...." << std::endl;
        };

    };

}