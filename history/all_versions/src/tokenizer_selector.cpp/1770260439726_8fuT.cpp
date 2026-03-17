#include "../include/tokenizer_selector.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>

TokenizerSelector::TokenizerSelector() {
    initialize();
}

void TokenizerSelector::initialize() {
    initializeTokenizerMap();
    m_config = TokenizerConfig();
}

void TokenizerSelector::initializeTokenizerMap() {
    m_availableTokenizers[Language::English] = {TokenizerType::BPE, TokenizerType::WordPiece};
    m_availableTokenizers[Language::Chinese] = {TokenizerType::CharacterBased, TokenizerType::WordPiece};
    m_availableTokenizers[Language::Japanese] = {TokenizerType::Janome, TokenizerType::MeCab};
    m_availableTokenizers[Language::Multilingual] = {TokenizerType::SentencePiece};
}

void TokenizerSelector::setConfiguration(const TokenizerConfig& config) {
    m_config = config;
}

TokenizerSelector::TokenizerConfig TokenizerSelector::getConfiguration() const {
    return m_config;
}

bool TokenizerSelector::loadTokenizer(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;
    
    // Simulate loading logic or implement real logic if format is known
    // For now, we'll verify it's a valid file and maybe read some headers
    
    // In a real scenario, this would parse vocab files, merges, etc.
    // Assuming simple line-based vocab for now if extension is .txt
    
    if (filePath.find(".txt") != std::string::npos) {
        std::string line;
        m_vocab.clear();
        m_vocabIndex.clear();
        int idx = 0;
        while (std::getline(file, line)) {
            line = trim(line);
            if (!line.empty()) {
                m_vocab.push_back(line);
                m_vocabIndex[line] = idx++;
            }
        }
    }
    
    return true;
}

bool TokenizerSelector::saveTokenizer(const std::string& filePath) const {
    // Stub: saving tokenizer configuration
    return true;
}

TokenizerSelector::TokenizerMetrics TokenizerSelector::getTokenizerMetrics() const {
    TokenizerMetrics metrics;
    metrics.vocabularySize = static_cast<int>(m_vocab.size());
    metrics.uniqueTokens = static_cast<int>(m_vocabIndex.size());
    metrics.averageTokensPerSentence = 0.0f; // Placeholder
    metrics.oovRate = 0.0f; // Placeholder
    metrics.encoding = "utf-8";
    return metrics;
}

std::vector<std::string> TokenizerSelector::previewTokenization(const std::string& text) const {
    std::vector<std::string> tokens;
    
    // Simple whitespace tokenization as a fallback/preview
    // In a real implementation this would use BPE/WordPiece logic
    
    std::stringstream ss(text);
    std::string segment;
    while (std::getline(ss, segment, ' ')) {
        if (!segment.empty()) {
            tokens.push_back(segment);
        }
    }
    
    return tokens;
}

std::string TokenizerSelector::getJsonValue(const std::string& json, const std::string& key) const {
    // Basic JSON parser stub
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    
    auto start = json.find_first_not_of(" \t\n\r\"", pos + 1);
    auto end = json.find_first_of(",}", start);
    
    if (start == std::string::npos) return "";
    
    std::string val = json.substr(start, end - start);
    // Remove quotes if present
    if (val.front() == '"') val.erase(0, 1);
    if (val.back() == '"') val.pop_back();
    
    return val;
}

std::string TokenizerSelector::trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> TokenizerSelector::split(const std::string& s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}
