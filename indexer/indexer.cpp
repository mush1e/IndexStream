#include "indexer.hpp"

namespace indexer {

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
                    const auto& [url, count] = queueCopy.top();
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
        if (sqlite3_exec(db_, query, nullptr, nullptr, &errmsg) != SQLITE_OK) {
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
                tf_idf REAL, -- Precomputed TF_IDF value
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
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ insert term in db if it doesnt exist and return the term id ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::get_or_insert_term(const std::string& term) -> long long {
        sqlite3_stmt* stmt;
        // Insert the term if it doesn't exist
        sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO terms (term, document_count) VALUES (?, 0);", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, term.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        // Select the term_id and document_count
        sqlite3_prepare_v2(db_, "SELECT term_id, document_count FROM terms WHERE term = ?;", -1, &stmt, nullptr);
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
        sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO documents (document_name) VALUES (?);", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, document.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        sqlite3_prepare_v2(db_, "SELECT document_id FROM documents WHERE document_name = ?;", -1, &stmt, nullptr);
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
        sqlite3_prepare_v2(db_, 
            "INSERT INTO term_document_matrix (term_id, document_id, frequency) "
            "VALUES (?, ?, ?);", 
            -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, term_id);
        sqlite3_bind_int64(stmt, 2, doc_id);
        sqlite3_bind_int64(stmt, 3, freq);  // Store the frequency
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }



    auto Indexer::update_idf() -> void {
        sqlite3_stmt* stmt;

        // Get the total number of documents (for IDF calculation)
        sqlite3_prepare_v2(db_, "SELECT total_documents FROM stats;", -1, &stmt, nullptr);
        sqlite3_step(stmt);
        long long total_documents = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);

        // Update IDF for each term in the term-document matrix
        const char* update_idf_query = R"(
            UPDATE term_document_matrix
            SET tf_idf = (SELECT frequency / CAST((SELECT total_terms FROM documents WHERE document_id = term_document_matrix.document_id) AS REAL)) 
                        * log( CAST(? AS REAL) / (SELECT document_count FROM terms WHERE term_id = term_document_matrix.term_id) )
        )";

        sqlite3_prepare_v2(db_, update_idf_query, -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, total_documents);  // Pass the total number of documents
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }



    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ insert in memory term document matrix to persistant store ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto Indexer::transform_to_persist() -> void {
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        for (const auto& [term, doc_queue] : term_document_matrix) {
            long long term_id = get_or_insert_term(term);
            auto term_docs = doc_queue;
            while (!term_docs.empty()) {
                const auto [url, count] = term_docs.top();
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
        sqlite3_prepare_v2(db_, "UPDATE documents SET term_count = ?, total_terms = ? WHERE document_id = ?;", -1, &stmt, nullptr);
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
    void Indexer::process_file(const std::string& f_name) {
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
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db_, "UPDATE stats SET total_documents = total_documents + 1;", -1, &stmt, nullptr);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ crawl documents in dump directory ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    void Indexer::directory_spider() {
        for (const auto& dir_entry : std::filesystem::directory_iterator(this->dump_dir)) {
            std::string f_name = dir_entry.path().string();
            std::cout << f_name << std::endl;
            process_file(f_name);
        }
        update_idf();
    }
}

int main() {
    indexer::Indexer idxr;
    idxr.directory_spider();
}
