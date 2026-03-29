// Semantic Index — inverted trigram index for fast code search
// Production implementation: trigram-based indexing with TF-IDF ranking

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace RawrXD {
namespace Indexing {

class SemanticIndex {
public:
    static SemanticIndex& instance() {
        static SemanticIndex inst;
        return inst;
    }

    void indexFile(const std::string& filePath, const std::string& content) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Remove old index entries for this file
        removeFileEntries(filePath);

        // Store raw content for result extraction
        m_fileContents[filePath] = content;

        // Build trigram index
        if (content.size() < 3) return;
        std::string lower = toLower(content);
        for (size_t i = 0; i + 3 <= lower.size(); ++i) {
            std::string tri = lower.substr(i, 3);
            m_trigramIndex[tri].insert(filePath);
        }

        // Build word index for whole-word search
        auto words = tokenize(lower);
        for (const auto& w : words) {
            m_wordIndex[w].insert(filePath);
        }

        m_totalDocs = m_fileContents.size();
    }

    void removeFile(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        removeFileEntries(filePath);
        m_fileContents.erase(filePath);
        m_totalDocs = m_fileContents.size();
    }

    std::vector<std::string> search(const std::string& query, size_t maxResults = 20) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (query.empty() || m_totalDocs == 0) return {};

        std::string lq = toLower(query);

        // Collect candidate files from trigram matches
        std::unordered_map<std::string, double> scores;

        // Trigram scoring
        if (lq.size() >= 3) {
            for (size_t i = 0; i + 3 <= lq.size(); ++i) {
                std::string tri = lq.substr(i, 3);
                auto it = m_trigramIndex.find(tri);
                if (it != m_trigramIndex.end()) {
                    double idf = std::log(static_cast<double>(m_totalDocs) /
                                          (1.0 + it->second.size()));
                    for (const auto& fp : it->second) {
                        scores[fp] += idf;
                    }
                }
            }
        }

        // Word-level boost
        auto queryWords = tokenize(lq);
        for (const auto& w : queryWords) {
            auto it = m_wordIndex.find(w);
            if (it != m_wordIndex.end()) {
                double idf = std::log(static_cast<double>(m_totalDocs) /
                                      (1.0 + it->second.size()));
                for (const auto& fp : it->second) {
                    scores[fp] += idf * 2.0; // boost exact word matches
                }
            }
        }

        // Rank by score, descending
        std::vector<std::pair<double, std::string>> ranked;
        ranked.reserve(scores.size());
        for (const auto& [fp, sc] : scores) {
            ranked.push_back({sc, fp});
        }
        std::sort(ranked.begin(), ranked.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });

        std::vector<std::string> results;
        for (size_t i = 0; i < ranked.size() && i < maxResults; ++i) {
            results.push_back(ranked[i].second);
        }
        return results;
    }

    size_t indexedFileCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_fileContents.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_trigramIndex.clear();
        m_wordIndex.clear();
        m_fileContents.clear();
        m_totalDocs = 0;
    }

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_trigramIndex;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_wordIndex;
    std::unordered_map<std::string, std::string> m_fileContents;
    size_t m_totalDocs = 0;

    void removeFileEntries(const std::string& filePath) {
        for (auto& [tri, files] : m_trigramIndex) {
            files.erase(filePath);
        }
        for (auto& [word, files] : m_wordIndex) {
            files.erase(filePath);
        }
    }

    static std::string toLower(const std::string& s) {
        std::string r = s;
        for (auto& c : r) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        return r;
    }

    static std::vector<std::string> tokenize(const std::string& s) {
        std::vector<std::string> tokens;
        std::string tok;
        for (char c : s) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                tok += c;
            } else {
                if (tok.size() >= 2) tokens.push_back(tok);
                tok.clear();
            }
        }
        if (tok.size() >= 2) tokens.push_back(tok);
        return tokens;
    }
};

} // namespace Indexing
} // namespace RawrXD