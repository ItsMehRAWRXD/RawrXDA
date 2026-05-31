#include "ai_digestion_engine.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

AIDigestionEngine::AIDigestionEngine() = default;
AIDigestionEngine::~AIDigestionEngine() = default;

std::vector<FileAnalysisResult> AIDigestionEngine::digest(const std::vector<std::string>& files, const DigestionConfig& config) {
    std::vector<FileAnalysisResult> results;
    m_totalTokens = 0;
    
    for (const auto& f : files) {
        if (!fs::exists(f)) continue;
        // Basic exclusion check
        bool excluded = false;
        for(const auto& pat : config.excludePatterns) {
            if(f.find(pat) != std::string::npos) {
                excluded = true;
                break;
            }
        }
        if(excluded) continue;

        auto res = analyzeFile(f, config);
        results.push_back(res);
        m_totalTokens += res.tokenCount;
    }
    return results;
}

FileAnalysisResult AIDigestionEngine::analyzeFile(const std::string& path, const DigestionConfig& config) {
    FileAnalysisResult res;
    res.path = path;
    
    std::ifstream file(path, std::ios::binary);
    if (!file) return res;

    // Check size
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (size > config.maxFileSizeBytes) {
        res.isBinary = true; // Treat as binary/oversized
        return res;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for binary content (null bytes)
    if (content.find('\0') != std::string::npos) {
        res.isBinary = true;
        return res;
    }

    // Crude Tokenization (Whitespace)
    std::stringstream ss(content);
    std::string word;
    while (ss >> word) {
        res.tokenCount++;
    }

    if (config.extractMetadata) {
        // Find TODOs
        std::regex todo_regex("TODO:?.*", std::regex::icase);
        auto words_begin = std::sregex_iterator(content.begin(), content.end(), todo_regex);
        auto words_end = std::sregex_iterator();
        
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            res.todos.push_back(match.str());
        }

        // Simple function heuristic (return type + name + parens)
        // Not perfect but better than simulation
        std::regex func_regex(R"((\w+)\s+(\w+)\s*\([^)]*\)\s*\{)");
        words_begin = std::sregex_iterator(content.begin(), content.end(), func_regex);
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            if (match.size() > 2)
                res.functions.push_back(match[2].str());
        }
    }

    return res;
}
