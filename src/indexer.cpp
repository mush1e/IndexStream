#include "indexer.hpp"

namespace indexer {

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Singleton static instance ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    Indexer& Indexer::get_instance() {
        static Indexer instance{};
        return instance;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Extract URL from first line of file and remove delim ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::url_extractor(std::string file_name) -> std::string {
        std::string line;
        std::string url;
        std::ifstream file(file_name);
        const std::string delimiter = "---URL---";

        if (file) {
            while (std::getline(file, line)) {
                if (line == delimiter) 
                    break;
                url += line;
            }
        }
        file.close();
        return url;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Parse Document and remove the html tags ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::document_parser(const std::string& file_name, std::string& document) -> void {
        std::ifstream file(file_name);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << file_name << std::endl;
            return;
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        std::regex scriptStyleRegex("<(script|style)[^>]*>[\\s\\S]*?</\\1>", std::regex::icase);
        content = std::regex_replace(content, scriptStyleRegex, "");
        std::regex bodyRegex("<body[^>]*>([\\s\\S]*?)</body>", std::regex::icase);
        std::smatch bodyMatch;
        if (std::regex_search(content, bodyMatch, bodyRegex)) {
            std::string bodyContent = bodyMatch[1].str();
            std::regex commentRegex("<!--.*?-->", std::regex::icase);
            bodyContent = std::regex_replace(bodyContent, commentRegex, "");
            std::regex tagRegex("<[^>]*>");
            document = std::regex_replace(bodyContent, tagRegex, " ");
            std::unordered_map<std::string, std::string> htmlEntities = {
                {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}
            };

            for (const auto& [entity, character] : htmlEntities) {
                size_t pos = 0;
                while ((pos = document.find(entity, pos)) != std::string::npos) {
                    document.replace(pos, entity.length(), character);
                    pos += character.length();
                }
            }

            std::regex whitespaceRegex("\\s+");
            document = std::regex_replace(document, whitespaceRegex, " ");
            std::transform(document.begin(), document.end(), document.begin(), [](unsigned char c) {
                return std::tolower(c);
            });
        }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Helper function to display the term document frequency matrix ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::print_term_document_matrix() const -> void {
        if (term_document_matrix.empty()) {
            std::cout << "The term-document matrix is empty." << std::endl;
            return;
        }

        std::cout << "=== Term-Document Matrix ===" << std::endl;
        std::cout << std::string(30, '-') << std::endl; 

        for (const auto& [term, docQueue] : term_document_matrix) {
            std::cout << "Term: " << term << std::endl;

            auto queueCopy = docQueue; 
            if (queueCopy.empty()) {
                std::cout << "  No documents found for this term." << std::endl;
            } else {
                while (!queueCopy.empty()) {
                    const auto& [url, count] = queueCopy.front();
                    std::cout << "  URL: " << url << " | Count: " << count << std::endl;
                    queueCopy.pop();
                }
            }
            std::cout << std::string(30, '-') << std::endl; 
        }
        std::cout << "============================" << std::endl; 
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Once document is stored in the DB delete it ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::delete_file(const std::string& file_name) -> bool {
        try {
            if (std::filesystem::remove(file_name)) {
                std::cout << "File '" << file_name << "' successfully deleted." << std::endl;
                this->indexed_documents.erase(file_name);
                return true;
            } else {
                std::cerr << "File '" << file_name << "' not found or could not be deleted." << std::endl;
                return false;
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error deleting file '" << file_name << "': " << e.what() << std::endl;
            return false;
        }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Helper function to execute SQL queries ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::execute_sql(const char* query) -> void {
        char* errmsg = nullptr;
        if (sqlite3_exec(safe_check_cpy() ? temp_db_ : db_, query, nullptr, nullptr, &errmsg) != SQLITE_OK) {
            std::cerr << "SQL error: " << errmsg << std::endl;
            sqlite3_free(errmsg);
        }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Create tables if they dont exist ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::create_tables() -> void {

        const char* create_terms_table = R"(
            CREATE TABLE IF NOT EXISTS terms (
                term_id INTEGER PRIMARY KEY AUTOINCREMENT,
                term TEXT UNIQUE,
                document_count INTEGER -- stores the number of documents where this term appears
            );
        )";

        const char* create_documents_table = R"(
            CREATE TABLE IF NOT EXISTS documents (
                document_id INTEGER PRIMARY KEY AUTOINCREMENT,
                document_name TEXT UNIQUE,
                term_count INTEGER, -- Number of unique terms in document
                total_terms INTEGER -- Total number of terms in document
            );
        )";

        const char* create_matrix_table = R"(
            CREATE TABLE IF NOT EXISTS term_document_matrix (
                term_id INTEGER,
                document_id INTEGER,
                frequency INTEGER, -- Number of occurrences of the term in the document (TF numerator)
                tf_idf REAL DEFAULT 0.0, -- Precomputed TF_IDF value
                PRIMARY KEY (term_id, document_id),
                FOREIGN KEY (term_id) REFERENCES terms(term_id),
                FOREIGN KEY (document_id) REFERENCES documents(document_id)
            );
        )";

        const char* create_stats_table = R"(
            CREATE TABLE IF NOT EXISTS stats (
                total_documents INTEGER DEFAULT 0  -- Total number of documents in the entire corpus
            );
        )";

        const char* create_term_index = R"(
            CREATE INDEX IF NOT EXISTS idx_term ON terms(term);
        )";

        const char* create_tdm_index = R"(
            CREATE INDEX IF NOT EXISTS idx_term_document ON term_document_matrix(term_id, document_id);
        )";

        execute_sql(create_terms_table);
        execute_sql(create_documents_table);
        execute_sql(create_matrix_table);
        execute_sql(create_stats_table);
        execute_sql(create_term_index);
        execute_sql(create_tdm_index);

        const char* init_stats_table = R"(
            INSERT INTO stats (total_documents) VALUES (0);
        )";
        sqlite3_exec(safe_check_cpy() ? temp_db_ : db_, init_stats_table, nullptr, nullptr, nullptr);

    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ insert term in db if it doesnt exist and return the term id ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::get_or_insert_term(const std::string& term) -> long long {
        sqlite3_stmt* stmt;
        // Insert the term if it doesn't exist
        sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, "INSERT OR IGNORE INTO terms (term, document_count) VALUES (?, 0);", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, term.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        // Select the term_id and document_count
        sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, "SELECT term_id, document_count FROM terms WHERE term = ?;", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, term.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        long long term_id = sqlite3_column_int64(stmt, 0);
        long long doc_count = sqlite3_column_int64(stmt, 1);
        sqlite3_finalize(stmt);

        return term_id;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ insert document in db if it doesnt exist and return the term id ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::get_or_insert_document(const std::string& document) -> long long {
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, "INSERT OR IGNORE INTO documents (document_name) VALUES (?);", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, document.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, "SELECT document_id FROM documents WHERE document_name = ?;", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, document.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        long long doc_id = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);

        return doc_id;
    }
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ insert TDFM in db ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::insert_term_document_matrix(long long term_id, long long doc_id, long long freq) -> void {
        sqlite3_stmt* stmt;

        // Insert the term into the term-document matrix
        sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, 
            "INSERT OR IGNORE INTO term_document_matrix (term_id, document_id, frequency, tf_idf) "
            "VALUES (?, ?, ?, ?);", 
            -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, term_id);
        sqlite3_bind_int64(stmt, 2, doc_id);
        sqlite3_bind_int64(stmt, 3, freq);  // Store the frequency
        sqlite3_bind_double(stmt, 4, 0.0);  // Store temp tf-idf
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ update tf-idf for all terms ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    auto Indexer::update_idf() -> void {
        sqlite3_stmt* stmt;

        // Get the total number of documents (for IDF calculation)
        sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, "SELECT total_documents FROM stats;", -1, &stmt, nullptr);
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            std::cerr << "Failed to get total_documents: " << sqlite3_errmsg(safe_check_cpy() ? temp_db_ : db_) << std::endl;
            sqlite3_finalize(stmt);
            return;
        }
        long long total_documents = sqlite3_column_int64(stmt, 0);
        std::cout << "Total Documents : " << total_documents << std::endl;
        sqlite3_finalize(stmt);

        // Fetch term-document matrix data
        const char* select_query = R"(
            SELECT term_id, document_id, frequency
            FROM term_document_matrix;
        )";
        if (sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, select_query, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare select_query: " << sqlite3_errmsg(safe_check_cpy() ? temp_db_ : db_) << std::endl;
            return;
        }

        std::unordered_map<long long, long long> document_term_counts;
        std::unordered_map<long long, double> term_idfs;

        // Iterate through the term-document matrix
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            long long term_id = sqlite3_column_int64(stmt, 0);
            long long doc_id = sqlite3_column_int64(stmt, 1);
            long long frequency = sqlite3_column_int64(stmt, 2);

            // Fetch total terms for the document
            sqlite3_stmt* doc_stmt;
            sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, "SELECT total_terms FROM documents WHERE document_id = ?;", -1, &doc_stmt, nullptr);
            sqlite3_bind_int64(doc_stmt, 1, doc_id);
            if (sqlite3_step(doc_stmt) == SQLITE_ROW) {
                long long total_terms = sqlite3_column_int64(doc_stmt, 0);
                double tf = static_cast<double>(frequency) / total_terms;
                if (term_idfs.find(term_id) == term_idfs.end()) {
                    // Calculate IDF
                    sqlite3_stmt* term_stmt;
                    sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, "SELECT document_count FROM terms WHERE term_id = ?;", -1, &term_stmt, nullptr);
                    sqlite3_bind_int64(term_stmt, 1, term_id);
                    if (sqlite3_step(term_stmt) == SQLITE_ROW) {
                        long long document_count = sqlite3_column_int64(term_stmt, 0);
                        double idf = log(static_cast<double>(total_documents) / (document_count + 1)); // Add 1 to avoid division by zero
                        term_idfs[term_id] = idf;
                    }
                    sqlite3_finalize(term_stmt);
                }

                // Calculate TF-IDF
                double tf_idf = tf * term_idfs[term_id];

                // Update TF-IDF in the database
                sqlite3_stmt* update_stmt;
                sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, "UPDATE term_document_matrix SET tf_idf = ? WHERE term_id = ? AND document_id = ?;", -1, &update_stmt, nullptr);
                sqlite3_bind_double(update_stmt, 1, tf_idf);
                sqlite3_bind_int64(update_stmt, 2, term_id);
                sqlite3_bind_int64(update_stmt, 3, doc_id);
                if (sqlite3_step(update_stmt) != SQLITE_DONE) {
                    std::cerr << "Failed to update TF-IDF: " << sqlite3_errmsg(safe_check_cpy() ? temp_db_ : db_) << std::endl;
                }
                sqlite3_finalize(update_stmt);
            }
            sqlite3_finalize(doc_stmt);
        }
        sqlite3_finalize(stmt);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ insert in memory term document matrix to persistant store ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::transform_to_persist() -> void {
        sqlite3_exec(safe_check_cpy() ? temp_db_ : db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        for (const auto& [term, doc_queue] : term_document_matrix) {
            long long term_id = get_or_insert_term(term);
            auto term_docs = doc_queue;
            while (!term_docs.empty()) {
                const auto [url, count] = term_docs.front();
                term_docs.pop();
                long long doc_id = get_or_insert_document(url);
                insert_term_document_matrix(term_id, doc_id, count);
            }
        }
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ create TDFM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::index_updater(std::string& document, std::string& url) -> void {
        if (document.empty()) return;

        std::istringstream stream(document);
        std::string word;
        std::unordered_map<std::string, long long> wordCount;
        std::transform(document.begin(), document.end(), document.begin(), ::tolower);

        long long total_terms = 0;  // Track total number of terms
        long long unique_terms = 0;  // Track unique terms

        // Count word frequencies and total terms
        while (stream >> word) {
            word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
            if (!word.empty()) {
                if (wordCount[word] == 0) {
                    unique_terms++;  // Increment unique term count
                }
                wordCount[word]++;
                total_terms++;  // Increment total terms count
            }
        }

        long long doc_id = get_or_insert_document(url);

        // Update the term_count and total_terms in the documents table
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, "UPDATE documents SET term_count = ?, total_terms = ? WHERE document_id = ?;", -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, unique_terms);
        sqlite3_bind_int64(stmt, 2, total_terms);
        sqlite3_bind_int64(stmt, 3, doc_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        // Insert the term frequencies into the term-document matrix
        for (const auto& [term, count] : wordCount) {
            long long term_id = get_or_insert_term(term);
            insert_term_document_matrix(term_id, doc_id, count);
        }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ parse document ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::process_file(const std::string& f_name) -> void {
        if (f_name.find(".gitkeep") != std::string::npos)
            return;

        std::string document{};
        if (this->indexed_documents.find(f_name) == this->indexed_documents.end()) {
            this->indexed_documents.insert(f_name);

            std::string url = url_extractor(f_name);
            document_parser(f_name, document);
            index_updater(document, url);  // Update index, including frequencies
            transform_to_persist();  // Move memory matrix to persistent storage

            term_document_matrix.clear();
            delete_file(f_name);

            // Update total_documents in stats table
            std::cout << "Updating total_documents" << std::endl;
            sqlite3_stmt* stmt;
            size_t document_count {};
            const char* count_query = "SELECT COUNT(*) FROM documents;";

            if (sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, count_query, -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    document_count = sqlite3_column_int(stmt, 0);
                } else {
                    std::cerr << "Failed to count documents: " << sqlite3_errmsg(db_) << std::endl;
                }
            } else {
                std::cerr << "Failed to prepare count query: " << sqlite3_errmsg(db_) << std::endl;
            }
            sqlite3_finalize(stmt);

            const char* update_query = "UPDATE stats SET total_documents = ?;";
            if (sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, update_query, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, document_count);
                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    std::cerr << "Failed to update total_documents: " << sqlite3_errmsg(db_) << std::endl;
                }
            } else {
                std::cerr << "Failed to prepare update query: " << sqlite3_errmsg(db_) << std::endl;
            }
            sqlite3_finalize(stmt);
        }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ crawl documents in dump directory ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::directory_spider() -> void {
        for (const auto& dir_entry : std::filesystem::directory_iterator(this->dump_dir)) {
            std::string f_name = dir_entry.path().string();
            std::cout << f_name << std::endl;
            process_file(f_name);
        }
        update_idf();
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ helper to safely set cpy ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::safe_check_cpy() -> bool {
        std::lock_guard<std::mutex> lock(cpy_mutex);
        return cpy;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ helper to safely set cpy ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::set_safe_copy(bool cpy_status) -> void {
        std::lock_guard<std::mutex> lock(cpy_mutex);
        cpy = cpy_status;
    }

    auto Indexer::merge_db() -> void {
        std::string src = "../db/document_store.db";
        std::string dest = "../db/temp_document_store.db";
        
        std::cout << "Init DB Merge...\n";

        if (std::remove(src.c_str()) == 0) {
            std::cout << "Original db removed" << std::endl;
            if (std::rename(dest.c_str(), src.c_str()) == 0) {
                std::cout << "Temporary file renamed to original file name." << std::endl;
            } else {
                std::cerr << "Error renaming temporary file!" << std::endl;
            }
        }

        std::cout << "DB updated sucessfully!\n";
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ insert create new write buffer db ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::update_db() -> void {
        std::string src = "../db/document_store.db";
        std::string dest = "../db/temp_document_store.db";

        std::cout << "Initializing write buffer db....\n";
        std::ifstream read_db(src, std::ios::binary);
        std::ofstream write_db(dest, std::ios::binary);


        if (!read_db || !write_db)
            std::cerr << "Error accessing file" << std::endl;

        write_db << read_db.rdbuf();
        read_db.close();
        write_db.close();

        if (sqlite3_open(dest.c_str(), &temp_db_) != SQLITE_OK) {
            std::cerr << "Cannot open database: " << sqlite3_errmsg(temp_db_) << std::endl;
            exit(1);
        }
        std::cout << "Init document parsing...\n";

        set_safe_copy(true);
        directory_spider();
        set_safe_copy(false);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Helper function to tokenize query ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::tokenize_query(const std::string& query) -> std::vector<std::string> {
        std::vector<std::string> terms;
        std::stringstream ss(query);
        std::string term;
        
        while (ss >> term) 
            terms.push_back(term);

        return terms;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Basic Search Function to test my stuff ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::search(const std::string& query) -> std::vector<std::pair<std::string, double>> {
        std::vector<std::pair<std::string, double>> final_results;
        std::unordered_map<std::string, double> document_scores;
        
        std::vector<std::string> terms = tokenize_query(query);

        const char* search_query = R"(
            SELECT d.document_name, td.tf_idf
            FROM term_document_matrix td
            JOIN documents d ON td.document_id = d.document_id
            WHERE td.term_id = (SELECT term_id FROM terms WHERE term = ?)
        )";

        for (const auto& query_term : terms) {
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(safe_check_cpy() ? temp_db_ : db_, search_query, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(safe_check_cpy() ? temp_db_ : db_) << std::endl;
                continue;  // Skip this term and continue with the next one
            }

            if (sqlite3_bind_text(stmt, 1, query_term.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                std::cerr << "Failed to bind query term: " << sqlite3_errmsg(safe_check_cpy() ? temp_db_ : db_) << std::endl;
                sqlite3_finalize(stmt);
                continue;  // Skip this term and continue with the next one
            }

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string document_name(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
                double tf_idf = sqlite3_column_double(stmt, 1);
                document_scores[document_name] += tf_idf;
            }

            sqlite3_finalize(stmt);
        }

        for (const auto& entry : document_scores) {
            final_results.emplace_back(entry.first, entry.second);
        }

        std::sort(final_results.begin(), final_results.end(), 
                [](const auto& a, const auto& b) { return a.second > b.second; });

        return final_results;
    }
}
