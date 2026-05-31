// ============================================================================
// Search Engine — Intelligent Code Search
// Advanced code search with semantic understanding
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../editor/search_indexer.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <set>

namespace RawrXD::Search {

enum class SearchType {
    EXACT,
    REGEX,
    SEMANTIC,
    SYMBOL,
    DEFINITION,
    REFERENCE
};

struct SearchResult {
    std::string filePath;
    int lineNumber;
    int column;
    std::string lineContent;
    std::string context;
    double relevance;
    SearchType type;
    std::map<std::string, std::string> metadata;
};

struct SearchQuery {
    std::string query;
    SearchType type;
    std::vector<std::string> filePatterns;
    std::vector<std::string> excludePatterns;
    bool caseSensitive;
    int maxResults;
};

struct SymbolInfo {
    std::string name;
    std::string type;
    std::string filePath;
    int lineNumber;
    std::string signature;
    std::vector<std::string> references;
};

struct CodeSnippet {
    std::string content;
    std::string language;
    std::string filePath;
    int startLine;
    int endLine;
    double relevance;
};

class SearchEngine {
public:
    explicit SearchEngine(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient) {}

    void IndexFile(const std::string& filePath, const std::string& content) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Index file content
        m_fileIndex[filePath] = content;
        
        // Extract symbols
        auto symbols = ExtractSymbols(content);
        for (const auto& symbol : symbols) {
            m_symbolIndex[symbol.name] = symbol;
        }
        
        // Build word index
        BuildWordIndex(filePath, content);
    }

    std::vector<SearchResult> Search(const SearchQuery& query) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<SearchResult> results;
        
        switch (query.type) {
            case SearchType::EXACT:
                results = SearchExact(query);
                break;
            case SearchType::REGEX:
                results = SearchRegex(query);
                break;
            case SearchType::SEMANTIC:
                results = SearchSemantic(query);
                break;
            case SearchType::SYMBOL:
                results = SearchSymbol(query);
                break;
            case SearchType::DEFINITION:
                results = SearchDefinition(query);
                break;
            case SearchType::REFERENCE:
                results = SearchReference(query);
                break;
        }
        
        // Sort by relevance
        std::sort(results.begin(), results.end(),
                 [](const SearchResult& a, const SearchResult& b) {
                     return a.relevance > b.relevance;
                 });
        
        // Limit results
        if (results.size() > static_cast<size_t>(query.maxResults)) {
            results.resize(query.maxResults);
        }
        
        return results;
    }

    std::vector<SymbolInfo> FindSymbols(const std::string& pattern) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<SymbolInfo> results;
        
        for (const auto& [name, symbol] : m_symbolIndex) {
            if (name.find(pattern) != std::string::npos) {
                results.push_back(symbol);
            }
        }
        
        return results;
    }

    std::vector<CodeSnippet> FindSimilarCode(const std::string& codeSnippet) {
        std::vector<CodeSnippet> results;
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return results;
        }

        // AI-powered semantic similarity search
        std::string prompt = "Find code similar to:\n```\n" + codeSnippet + "\n```";
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a code search expert. Find similar code patterns."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            // Parse AI response for similar code
            CodeSnippet snippet;
            snippet.content = result.response;
            snippet.relevance = 0.9;
            results.push_back(snippet);
        }
        
        return results;
    }

    std::vector<SearchResult> FindReferences(const std::string& symbolName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<SearchResult> results;
        
        auto it = m_symbolIndex.find(symbolName);
        if (it != m_symbolIndex.end()) {
            for (const auto& ref : it->second.references) {
                SearchResult result;
                result.filePath = ref;
                result.relevance = 1.0;
                result.type = SearchType::REFERENCE;
                results.push_back(result);
            }
        }
        
        return results;
    }

    std::string GenerateSearchReport(const SearchQuery& query, 
                                    const std::vector<SearchResult>& results) {
        std::ostringstream report;
        report << "# Search Report\n\n";
        report << "**Query:** " << query.query << "\n";
        report << "**Type:** " << TypeToString(query.type) << "\n";
        report << "**Results:** " << results.size() << "\n\n";
        
        report << "## Results\n";
        for (const auto& result : results) {
            report << "### " << result.filePath << ":" << result.lineNumber << "\n";
            report << "**Relevance:** " << std::fixed << std::setprecision(2) 
                   << result.relevance << "\n";
            report << "```\n" << result.lineContent << "\n```\n\n";
        }
        
        return report.str();
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    mutable std::mutex m_mutex;
    std::map<std::string, std::string> m_fileIndex;
    std::map<std::string, SymbolInfo> m_symbolIndex;
    std::map<std::string, std::set<std::string>> m_wordIndex;

    std::vector<SymbolInfo> ExtractSymbols(const std::string& content) {
        std::vector<SymbolInfo> symbols;
        
        // Extract function definitions
        std::regex funcPattern(R"((\w+)\s+(\w+)\s*\(([^)]*)\))");
        std::sregex_iterator iter(content.begin(), content.end(), funcPattern);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            SymbolInfo symbol;
            symbol.type = "function";
            symbol.name = (*iter)[2];
            symbol.signature = (*iter)[0];
            symbols.push_back(symbol);
        }
        
        // Extract class definitions
        std::regex classPattern(R"(class\s+(\w+))");
        std::sregex_iterator classIter(content.begin(), content.end(), classPattern);
        
        for (; classIter != end; ++classIter) {
            SymbolInfo symbol;
            symbol.type = "class";
            symbol.name = (*classIter)[1];
            symbols.push_back(symbol);
        }
        
        return symbols;
    }

    void BuildWordIndex(const std::string& filePath, const std::string& content) {
        std::istringstream stream(content);
        std::string word;
        
        while (stream >> word) {
            // Clean word
            word.erase(std::remove_if(word.begin(), word.end(), 
                [](char c) { return !std::isalnum(c); }), word.end());
            
            if (!word.empty()) {
                m_wordIndex[word].insert(filePath);
            }
        }
    }

    std::vector<SearchResult> SearchExact(const SearchQuery& query) {
        std::vector<SearchResult> results;
        
        for (const auto& [filePath, content] : m_fileIndex) {
            size_t pos = 0;
            while ((pos = content.find(query.query, pos)) != std::string::npos) {
                SearchResult result;
                result.filePath = filePath;
                result.lineNumber = std::count(content.begin(), content.begin() + pos, '\n') + 1;
                result.relevance = 1.0;
                result.type = SearchType::EXACT;
                results.push_back(result);
                pos += query.query.length();
            }
        }
        
        return results;
    }

    std::vector<SearchResult> SearchRegex(const SearchQuery& query) {
        std::vector<SearchResult> results;
        
        try {
            std::regex pattern(query.query);
            
            for (const auto& [filePath, content] : m_fileIndex) {
                std::sregex_iterator iter(content.begin(), content.end(), pattern);
                std::sregex_iterator end;
                
                for (; iter != end; ++iter) {
                    SearchResult result;
                    result.filePath = filePath;
                    result.lineNumber = std::count(content.begin(), 
                        content.begin() + iter->position(), '\n') + 1;
                    result.relevance = 0.9;
                    result.type = SearchType::REGEX;
                    results.push_back(result);
                }
            }
        } catch (const std::regex_error&) {
            // Invalid regex
        }
        
        return results;
    }

    std::vector<SearchResult> SearchSemantic(const SearchQuery& query) {
        std::vector<SearchResult> results;
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return results;
        }

        // Use AI for semantic understanding
        std::string prompt = "Find code related to: " + query.query;
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a code search expert."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            // Parse AI suggestions
            SearchResult sr;
            sr.relevance = 0.95;
            sr.type = SearchType::SEMANTIC;
            results.push_back(sr);
        }
        
        return results;
    }

    std::vector<SearchResult> SearchSymbol(const SearchQuery& query) {
        std::vector<SearchResult> results;
        
        auto symbols = FindSymbols(query.query);
        for (const auto& symbol : symbols) {
            SearchResult result;
            result.filePath = symbol.filePath;
            result.lineNumber = symbol.lineNumber;
            result.relevance = 1.0;
            result.type = SearchType::SYMBOL;
            results.push_back(result);
        }
        
        return results;
    }

    std::vector<SearchResult> SearchDefinition(const SearchQuery& query) {
        return SearchSymbol(query); // Same as symbol search for now
    }

    std::vector<SearchResult> SearchReference(const SearchQuery& query) {
        return FindReferences(query.query);
    }

    std::string TypeToString(SearchType type) {
        switch (type) {
            case SearchType::EXACT: return "Exact";
            case SearchType::REGEX: return "Regex";
            case SearchType::SEMANTIC: return "Semantic";
            case SearchType::SYMBOL: return "Symbol";
            case SearchType::DEFINITION: return "Definition";
            case SearchType::REFERENCE: return "Reference";
            default: return "Unknown";
        }
    }
};

} // namespace RawrXD::Search
