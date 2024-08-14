#include "indexer.hpp"

namespace indexer {

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

    auto Indexer::document_parser(std::string& file_name, std::string& document) -> void {
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

    auto Indexer::index_updater(std::string& document, std::string& url) -> void {
        if (document.empty())
            return;
        
        std::istringstream stream(document);
        std::string word;
        std::unordered_map<std::string, long long> wordCount;
        std::transform(document.begin(), document.end(), document.begin(), ::tolower);

        while (stream >> word) {
            word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
            if (!word.empty()) {
                wordCount[word]++;
            }
        }
        for (const auto& [term, count] : wordCount) 
            term_document_matrix[term].emplace(url, count);
    }

    auto Indexer::directory_spider() -> void {
        for (const auto& dir_entry : fs::directory_iterator(this->dump_dir)) {
            std::string f_name = dir_entry.path().string();
            if (f_name.find(".gitkeep") != std::string::npos)
                continue;
            std::string document {};
            if (this->indexed_documents.find(f_name) == this->indexed_documents.end()) { 
                this->indexed_documents.insert(f_name);
                std::string url = url_extractor(f_name);
                document_parser(f_name, document);
                index_updater(document, url);
                delete_file(f_name);
            }
        }
        // insert_term_document_matrix();
    }

    auto Indexer::execute_sql(const char* query) -> void {
        char* errmsg = nullptr;
        if (sqlite3_exec(db_, query, nullptr, nullptr, &errmsg) != SQLITE_OK) {
            std::cerr << "SQL error: " << errmsg << std::endl;
            sqlite3_free(errmsg);
        }
    }

    auto Indexer::create_tables() -> void {

        const char* create_terms_table = R"(
            CREATE TABLE IF NOT EXISTS terms (
                term_id INTEGER PRIMARY KEY AUTOINCREMENT,
                term TEXT UNIQUE
            );
        )";

        const char* create_documents_table = R"(
            CREATE TABLE IF NOT EXISTS documents (
                document_id INTEGER PRIMARY KEY AUTOINCREMENT,
                document_name TEXT UNIQUE
            );
        )";

        const char* create_matrix_table = R"(
            CREATE TABLE IF NOT EXISTS term_document_matrix (
                term_id INTEGER,
                document_id INTEGER,
                frequency INTEGER,
                PRIMARY KEY (term_id, document_id),
                FOREIGN KEY (term_id) REFERENCES terms(term_id),
                FOREIGN KEY (document_id) REFERENCES documents(document_id)
            );
        )";

        execute_sql(create_terms_table);
        execute_sql(create_documents_table);
        execute_sql(create_matrix_table);
    }

    auto Indexer::get_or_insert_term(const std::string& term) -> long long {
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO terms (term) VALUES (?);", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, term.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        sqlite3_prepare_v2(db_, "SELECT term_id FROM terms WHERE term = ?;", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, term.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        long long term_id = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);

        return term_id;
    }

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

    auto Indexer::insert_term_document_matrix(long long term_id, long long doc_id, long long freq) -> void {
        sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db_, "INSERT INTO term_document_matrix (term_id, document_id, size) VALUES (?, ?, ?);", -1, &stmt, nullptr);
            sqlite3_bind_int64(stmt, 1, term_id);
            sqlite3_bind_int64(stmt, 2, doc_id);
            sqlite3_bind_int64(stmt, 3, size);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
    }

}


int main() {
    indexer::Indexer idxr;
    idxr.directory_spider();
}