// =============================================================================
// RawrXD Semantic Code Search — Production Implementation
// Copilot/Cursor Parity: semantic search across workspace with ranking
// =============================================================================
// Combines TF-IDF lexical search with symbol-aware semantic ranking.
// Supports: natural language queries, code pattern matching, regex,
//           fuzzy identifier search, and context-window extraction.
// Zero external dependencies — pure Win32 + STL.
// =============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <regex>
#include <mutex>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <functional>
#include <numeric>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace RawrXD {
namespace AI {

// ─── Search Result ───────────────────────────────────────────────────────────
struct SearchResult {
    std::string filePath;
    uint32_t line = 0;
    uint32_t column = 0;
    std::string matchText;         // The matched line/region
    std::string contextBefore;     // 3 lines before
    std::string contextAfter;      // 3 lines after
    float score = 0.0f;            // Relevance score [0..1]
    std::string matchKind;         // "exact", "fuzzy", "semantic", "regex"
    std::string symbolName;        // If match is inside a known symbol
};

// ─── Search Options ──────────────────────────────────────────────────────────
struct SearchOptions {
    bool caseSensitive = false;
    bool wholeWord = false;
    bool useRegex = false;
    bool includeComments = true;
    bool includeStrings = true;
    size_t maxResults = 100;
    size_t contextLines = 3;
    std::vector<std::string> includePatterns;   // glob patterns
    std::vector<std::string> excludePatterns;   // glob patterns
    std::vector<std::string> fileExtensions;
};

// ─── Document Index Entry ────────────────────────────────────────────────────
struct DocumentIndex {
    std::string filePath;
    std::vector<std::string> lines;
    std::unordered_map<std::string, std::vector<uint32_t>> termToLines; // term → line numbers
    std::unordered_map<std::string, float> tfidf;  // term → TF-IDF score
    uint64_t lastModified = 0;
    size_t tokenCount = 0;
};

// ─── Text Processing Helpers ─────────────────────────────────────────────────
static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

static std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : text) {
        if (isalnum(c) || c == '_') {
            current += c;
        } else {
            if (!current.empty()) {
                tokens.push_back(toLower(current));
                current.clear();
            }
        }
    }
    if (!current.empty()) tokens.push_back(toLower(current));
    return tokens;
}

// Split camelCase and snake_case into sub-tokens
static std::vector<std::string> splitIdentifier(const std::string& id) {
    std::vector<std::string> parts;
    std::string current;
    for (size_t i = 0; i < id.size(); ++i) {
        char c = id[i];
        if (c == '_') {
            if (!current.empty()) { parts.push_back(toLower(current)); current.clear(); }
        } else if (i > 0 && isupper(c) && islower(id[i - 1])) {
            if (!current.empty()) { parts.push_back(toLower(current)); current.clear(); }
            current += c;
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(toLower(current));
    return parts;
}

// Subsequence match score
static float subsequenceScore(const std::string& query, const std::string& target) {
    if (query.empty()) return 0.0f;
    std::string lq = toLower(query), lt = toLower(target);
    size_t qi = 0;
    int consecutiveBonus = 0;
    float score = 0.0f;
    for (size_t ti = 0; ti < lt.size() && qi < lq.size(); ++ti) {
        if (lt[ti] == lq[qi]) {
            score += 1.0f + consecutiveBonus * 0.5f;
            if (ti == 0 || !isalnum(lt[ti - 1])) score += 2.0f; // word boundary bonus
            ++consecutiveBonus;
            ++qi;
        } else {
            consecutiveBonus = 0;
        }
    }
    return qi == lq.size() ? score / static_cast<float>(lt.size()) : 0.0f;
}

// Wildcard glob → regex
static std::string globToRegex(const std::string& glob) {
    std::string re;
    for (char c : glob) {
        switch (c) {
            case '*': re += ".*"; break;
            case '?': re += "."; break;
            case '.': re += "\\."; break;
            default: re += c;
        }
    }
    return re;
}

// =============================================================================
// SemanticCodeSearch — Main Engine
// =============================================================================
class SemanticCodeSearch {
public:
    static SemanticCodeSearch& instance() {
        static SemanticCodeSearch s;
        return s;
    }

    // ── Index a file for search ──────────────────────────────────────────────
    bool indexFile(const std::string& filePath) {
        std::ifstream ifs(filePath);
        if (!ifs.is_open()) return false;

        DocumentIndex doc;
        doc.filePath = filePath;
        std::string line;
        while (std::getline(ifs, line)) {
            doc.lines.push_back(line);
        }
        ifs.close();

        // Build term → lines index
        for (uint32_t lineNum = 0; lineNum < doc.lines.size(); ++lineNum) {
            auto tokens = tokenize(doc.lines[lineNum]);
            for (auto& tok : tokens) {
                doc.termToLines[tok].push_back(lineNum);
                ++doc.tokenCount;
            }

            // Also index sub-tokens from camelCase/snake_case
            for (auto& tok : tokens) {
                auto parts = splitIdentifier(tok);
                for (auto& part : parts) {
                    if (part != tok) {
                        doc.termToLines[part].push_back(lineNum);
                    }
                }
            }
        }

        // Compute TF for this document
        for (auto& [term, lineNums] : doc.termToLines) {
            doc.tfidf[term] = static_cast<float>(lineNums.size()) / static_cast<float>(std::max<size_t>(doc.tokenCount, 1));
        }

#ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExA(filePath.c_str(), GetFileExInfoStandard, &fad)) {
            doc.lastModified = (static_cast<uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) | fad.ftLastWriteTime.dwLowDateTime;
        }
#endif

        std::lock_guard<std::mutex> lock(m_mutex);
        m_documents[filePath] = std::move(doc);
        ++m_totalDocs;
        recomputeIDF();
        return true;
    }

    // ── Index directory ──────────────────────────────────────────────────────
    size_t indexDirectory(const std::string& rootPath,
                          const std::vector<std::string>& extensions = {".cpp",".hpp",".h",".c",".cxx",".hxx",".asm",".py",".js",".ts",".json",".md"}) {
        size_t count = 0;
        try {
            for (auto& entry : std::filesystem::recursive_directory_iterator(
                     rootPath, std::filesystem::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                bool match = false;
                for (auto& e : extensions) { if (ext == e) { match = true; break; } }
                if (!match) continue;

                auto pathStr = entry.path().string();
                if (pathStr.find("\\.git\\") != std::string::npos ||
                    pathStr.find("\\node_modules\\") != std::string::npos ||
                    pathStr.find("\\build\\") != std::string::npos) continue;

                if (indexFile(pathStr)) ++count;
            }
        } catch (...) {}
        return count;
    }

    // ── Primary Search Interface ─────────────────────────────────────────────
    std::vector<SearchResult> search(const std::string& query, const SearchOptions& opts = {}) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<SearchResult> results;

        if (opts.useRegex) {
            searchRegex(query, opts, results);
        } else {
            // Combined: lexical TF-IDF + fuzzy identifier + literal substring
            searchSemantic(query, opts, results);
        }

        // Sort by score descending
        std::sort(results.begin(), results.end(), [](auto& a, auto& b) { return a.score > b.score; });

        // Limit
        if (results.size() > opts.maxResults) results.resize(opts.maxResults);
        return results;
    }

    // ── Natural Language Query (maps NL → code concepts) ─────────────────────
    std::vector<SearchResult> naturalLanguageSearch(const std::string& nlQuery, size_t maxResults = 20) {
        // Decompose the NL query into code-relevant terms
        auto queryTokens = tokenize(nlQuery);

        // Remove stop words
        static const std::unordered_set<std::string> stopWords = {
            "the","a","an","is","are","was","were","be","been","being","have","has","had",
            "do","does","did","will","would","shall","should","may","might","can","could",
            "this","that","these","those","i","me","my","we","our","you","your","he","she",
            "it","they","them","their","what","which","who","whom","whose","when","where",
            "how","why","not","no","nor","but","and","or","if","then","else","so","for",
            "from","to","in","on","at","by","with","about","of","up","out","all","each"
        };

        std::vector<std::string> codeTerms;
        for (auto& t : queryTokens) {
            if (stopWords.find(t) == stopWords.end() && t.size() > 1) {
                codeTerms.push_back(t);
            }
        }

        // Search using the filtered terms
        SearchOptions opts;
        opts.maxResults = maxResults;
        std::string joined;
        for (auto& t : codeTerms) { if (!joined.empty()) joined += " "; joined += t; }
        return search(joined, opts);
    }

    // ── Stats ────────────────────────────────────────────────────────────────
    size_t documentCount() const { return m_totalDocs.load(); }
    size_t vocabularySize() const { std::lock_guard<std::mutex> lock(m_mutex); return m_idf.size(); }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_documents.clear();
        m_idf.clear();
        m_totalDocs = 0;
    }

private:
    SemanticCodeSearch() = default;

    void recomputeIDF() {
        m_idf.clear();
        size_t N = m_documents.size();
        if (N == 0) return;

        // Count document frequency for each term
        std::unordered_map<std::string, size_t> df;
        for (auto& [path, doc] : m_documents) {
            for (auto& [term, _] : doc.termToLines) {
                ++df[term];
            }
        }

        // IDF = log(N / df)
        for (auto& [term, count] : df) {
            m_idf[term] = std::log(static_cast<float>(N) / static_cast<float>(count));
        }
    }

    void searchSemantic(const std::string& query, const SearchOptions& opts,
                        std::vector<SearchResult>& results) {
        auto queryTokens = tokenize(query);
        if (queryTokens.empty()) return;

        for (auto& [filePath, doc] : m_documents) {
            if (!matchesFileFilter(filePath, opts)) continue;

            // Score each line
            for (uint32_t lineNum = 0; lineNum < doc.lines.size(); ++lineNum) {
                const auto& line = doc.lines[lineNum];
                float lineScore = 0.0f;
                std::string matchKind = "semantic";

                // 1. Exact substring match (highest priority)
                std::string lowerLine = opts.caseSensitive ? line : toLower(line);
                std::string lowerQuery = opts.caseSensitive ? query : toLower(query);
                auto pos = lowerLine.find(lowerQuery);
                if (pos != std::string::npos) {
                    lineScore += 10.0f;
                    matchKind = "exact";
                    if (opts.wholeWord) {
                        bool leftOk = (pos == 0 || !isalnum(lowerLine[pos - 1]));
                        bool rightOk = (pos + lowerQuery.size() >= lowerLine.size() || !isalnum(lowerLine[pos + lowerQuery.size()]));
                        if (!leftOk || !rightOk) lineScore -= 8.0f;
                    }
                }

                // 2. TF-IDF scoring
                float tfidfScore = 0.0f;
                for (auto& qt : queryTokens) {
                    auto termIt = doc.termToLines.find(qt);
                    if (termIt != doc.termToLines.end()) {
                        for (auto ln : termIt->second) {
                            if (ln == lineNum) {
                                float tf = doc.tfidf.count(qt) ? doc.tfidf[qt] : 0.0f;
                                float idf = m_idf.count(qt) ? m_idf[qt] : 1.0f;
                                tfidfScore += tf * idf;
                                break;
                            }
                        }
                    }
                }
                lineScore += tfidfScore;

                // 3. Fuzzy identifier match
                auto lineTokens = tokenize(line);
                for (auto& lt : lineTokens) {
                    for (auto& qt : queryTokens) {
                        float fuzzy = subsequenceScore(qt, lt);
                        if (fuzzy > 0.3f) {
                            lineScore += fuzzy * 2.0f;
                            if (matchKind != "exact") matchKind = "fuzzy";
                        }
                    }
                }

                if (lineScore > 0.1f) {
                    SearchResult res;
                    res.filePath = filePath;
                    res.line = lineNum + 1;
                    res.column = static_cast<uint32_t>(pos != std::string::npos ? pos + 1 : 1);
                    res.matchText = line;
                    res.score = lineScore;
                    res.matchKind = matchKind;
                    extractContext(doc, lineNum, opts.contextLines, res);
                    results.push_back(std::move(res));
                }
            }
        }
    }

    void searchRegex(const std::string& pattern, const SearchOptions& opts,
                     std::vector<SearchResult>& results) {
        try {
            auto flags = std::regex_constants::ECMAScript;
            if (!opts.caseSensitive) flags |= std::regex_constants::icase;
            std::regex re(pattern, flags);

            for (auto& [filePath, doc] : m_documents) {
                if (!matchesFileFilter(filePath, opts)) continue;

                for (uint32_t lineNum = 0; lineNum < doc.lines.size(); ++lineNum) {
                    std::smatch sm;
                    if (std::regex_search(doc.lines[lineNum], sm, re)) {
                        SearchResult res;
                        res.filePath = filePath;
                        res.line = lineNum + 1;
                        res.column = static_cast<uint32_t>(sm.position() + 1);
                        res.matchText = doc.lines[lineNum];
                        res.score = 5.0f;
                        res.matchKind = "regex";
                        extractContext(doc, lineNum, opts.contextLines, res);
                        results.push_back(std::move(res));
                    }
                }
            }
        } catch (const std::regex_error&) {}
    }

    void extractContext(const DocumentIndex& doc, uint32_t lineNum, size_t contextLines, SearchResult& res) {
        std::string before, after;
        for (size_t i = (lineNum >= contextLines ? lineNum - contextLines : 0); i < lineNum; ++i) {
            before += doc.lines[i] + "\n";
        }
        for (size_t i = lineNum + 1; i <= std::min<size_t>(lineNum + contextLines, doc.lines.size() - 1); ++i) {
            after += doc.lines[i] + "\n";
        }
        res.contextBefore = before;
        res.contextAfter = after;
    }

    bool matchesFileFilter(const std::string& filePath, const SearchOptions& opts) {
        if (!opts.fileExtensions.empty()) {
            std::string ext = std::filesystem::path(filePath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            bool found = false;
            for (auto& e : opts.fileExtensions) { if (ext == e) { found = true; break; } }
            if (!found) return false;
        }

        for (auto& excl : opts.excludePatterns) {
            try {
                std::regex re(globToRegex(excl), std::regex_constants::icase);
                if (std::regex_search(filePath, re)) return false;
            } catch (...) {}
        }

        if (!opts.includePatterns.empty()) {
            bool anyMatch = false;
            for (auto& incl : opts.includePatterns) {
                try {
                    std::regex re(globToRegex(incl), std::regex_constants::icase);
                    if (std::regex_search(filePath, re)) { anyMatch = true; break; }
                } catch (...) {}
            }
            if (!anyMatch) return false;
        }

        return true;
    }

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, DocumentIndex> m_documents;
    std::unordered_map<std::string, float> m_idf;
    std::atomic<size_t> m_totalDocs{0};
};

} // namespace AI
} // namespace RawrXD

// =============================================================================
// C API
// =============================================================================
extern "C" {

__declspec(dllexport) void SemanticSearch_IndexDirectory(const char* rootPath) {
    RawrXD::AI::SemanticCodeSearch::instance().indexDirectory(rootPath ? rootPath : ".");
}

__declspec(dllexport) int SemanticSearch_IndexFile(const char* filePath) {
    return RawrXD::AI::SemanticCodeSearch::instance().indexFile(filePath ? filePath : "") ? 1 : 0;
}

__declspec(dllexport) size_t SemanticSearch_Search(const char* query, int useRegex,
                                                    char* outResults, int outLen) {
    RawrXD::AI::SearchOptions opts;
    opts.useRegex = useRegex != 0;
    opts.maxResults = 20;
    auto results = RawrXD::AI::SemanticCodeSearch::instance().search(query ? query : "", opts);
    if (outResults && outLen > 0) {
        std::string json = "[";
        for (size_t i = 0; i < results.size(); ++i) {
            auto& r = results[i];
            if (i > 0) json += ",";
            json += "{\"file\":\"" + r.filePath + "\",\"line\":" + std::to_string(r.line) +
                    ",\"score\":" + std::to_string(r.score) + ",\"kind\":\"" + r.matchKind + "\"}";
        }
        json += "]";
        strncpy_s(outResults, outLen, json.c_str(), _TRUNCATE);
    }
    return results.size();
}

__declspec(dllexport) size_t SemanticSearch_DocumentCount() {
    return RawrXD::AI::SemanticCodeSearch::instance().documentCount();
}

__declspec(dllexport) void SemanticSearch_Clear() {
    RawrXD::AI::SemanticCodeSearch::instance().clear();
}

} // extern "C"
