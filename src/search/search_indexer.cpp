#include "search_indexer.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cctype>

namespace RawrXD::Search {
    void SearchIndexer::indexFile(const std::string& path) {
        std::ifstream in(path);
        if (!in.is_open()) return;

        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::istringstream stream(content);
        std::string token;
        size_t lineNum = 1;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_fileTokens[path].clear();

        while (std::getline(stream, token, '\n')) {
            std::istringstream wordStream(token);
            std::string word;
            while (wordStream >> word) {
                // Normalize
                std::string normalized;
                for (char c : word) {
                    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
                        normalized += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                if (normalized.size() < 2) continue;

                m_index[normalized].push_back({path, lineNum});
                m_fileTokens[path].insert(normalized);
            }
            ++lineNum;
        }
    }

    void SearchIndexer::removeFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_fileTokens.find(path);
        if (it == m_fileTokens.end()) return;

        for (const auto& token : it->second) {
            auto& postings = m_index[token];
            postings.erase(std::remove_if(postings.begin(), postings.end(),
                [&path](const Posting& p) { return p.filePath == path; }), postings.end());
            if (postings.empty()) m_index.erase(token);
        }
        m_fileTokens.erase(it);
    }

    std::vector<SearchResult> SearchIndexer::search(const std::string& query, size_t maxResults) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string normalized;
        for (char c : query) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
                normalized += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        auto it = m_index.find(normalized);
        if (it == m_index.end()) return {};

        std::vector<SearchResult> results;
        for (const auto& posting : it->second) {
            results.push_back({posting.filePath, posting.lineNumber});
            if (results.size() >= maxResults) break;
        }
        return results;
    }

    void SearchIndexer::indexDirectory(const std::string& dir, const std::vector<std::string>& extensions) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            if (extensions.empty() ||
                std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
                indexFile(entry.path().string());
            }
        }
    }
}
