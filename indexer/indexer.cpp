#include "indexer.hpp"

namespace indexer {


    void Indexer::document_parser(std::string& file_name) {
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
            std::string plainText = std::regex_replace(bodyContent, tagRegex, " ");
            std::unordered_map<std::string, std::string> htmlEntities = {
                {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}
            };

            for (const auto& [entity, character] : htmlEntities) {
                size_t pos = 0;
                while ((pos = plainText.find(entity, pos)) != std::string::npos) {
                    plainText.replace(pos, entity.length(), character);
                    pos += character.length();
                }
            }

            std::regex whitespaceRegex("\\s+");
            plainText = std::regex_replace(plainText, whitespaceRegex, " ");

            std::cout << "Extracted Text from <body>: " << plainText << std::endl;
        } else {
            std::cerr << "No <body> content found in file: " << file_name << std::endl;
        }
    }

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
        return url;
    }

    auto Indexer::directory_spider() -> void {
        // TODO
        for (const auto& dir_entry : fs::directory_iterator(this->dump_dir)) {
            std::string f_name = dir_entry.path().string();
            if (this->indexed_documents.find(f_name) == this->indexed_documents.end()) { 
                this->indexed_documents.insert(f_name);
                std::cout << url_extractor(f_name) << std::endl;
                document_parser(f_name);
                index_updater(f_name);
            }
        }
    }
}


int main() {
    indexer::Indexer idxr;
    idxr.directory_spider();
}