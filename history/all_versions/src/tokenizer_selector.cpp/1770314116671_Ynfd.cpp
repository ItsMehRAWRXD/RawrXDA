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
    loadVocabIfAvailable();
}

TokenizerSelector::TokenizerConfig TokenizerSelector::getConfiguration() const {
    return m_config;
}

bool TokenizerSelector::loadTokenizer(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;
    
    // Load simple line-based vocab for .txt files
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
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    if (filePath.find(".json") != std::string::npos) {
        file << "{\n";
        file << "  \"name\": \"" << m_config.name << "\",\n";
        file << "  \"language\": " << static_cast<int>(m_config.language) << ",\n";
        file << "  \"tokenizerType\": " << static_cast<int>(m_config.tokenizerType) << ",\n";
        file << "  \"vocabSize\": " << m_config.vocabSize << ",\n";
        file << "  \"minFrequency\": " << m_config.minFrequency << ",\n";
        file << "  \"characterCoverage\": " << m_config.characterCoverage << ",\n";
        file << "  \"lowercaseTokens\": " << (m_config.lowercaseTokens ? "true" : "false") << ",\n";
        file << "  \"addSpecialTokens\": " << (m_config.addSpecialTokens ? "true" : "false") << ",\n";
        file << "  \"specialTokens\": " << m_config.specialTokensJson << ",\n";
        file << "  \"maxTokenLength\": " << m_config.maxTokenLength << ",\n";
        file << "  \"enableSubwordRegularization\": " << (m_config.enableSubwordRegularization ? "true" : "false") << ",\n";
        file << "  \"subwordRegularizationAlpha\": " << m_config.subwordRegularizationAlpha << ",\n";
        file << "  \"vocabPath\": \"" << m_config.vocabPath << "\",\n";
        file << "  \"mergesPath\": \"" << m_config.mergesPath << "\",\n";
        file << "  \"vocab\": [\n";
        for (size_t i = 0; i < m_vocab.size(); ++i) {
            file << "    \"" << m_vocab[i] << "\"";
            if (i + 1 < m_vocab.size()) file << ",";
            file << "\n";
        }
        file << "  ]\n";
        file << "}\n";
    } else {
        for (const auto& token : m_vocab) {
            file << token << "\n";
        }
    }
    return file.good();
}

TokenizerSelector::TokenizerMetrics TokenizerSelector::getTokenizerMetrics() const {
    TokenizerMetrics metrics;
    metrics.vocabularySize = static_cast<int>(m_vocab.size());
    metrics.uniqueTokens = static_cast<int>(m_vocabIndex.size());
    if (!m_vocab.empty()) {
        double total_len = 0.0;
        for (const auto& token : m_vocab) total_len += token.size();
        metrics.averageTokensPerSentence = static_cast<float>(total_len / m_vocab.size());
    }
    if (m_config.vocabSize > 0) {
        float coverage = static_cast<float>(metrics.uniqueTokens) / static_cast<float>(m_config.vocabSize);
        metrics.oovRate = std::max(0.0f, 1.0f - coverage);
    }
    metrics.encoding = "utf-8";
    return metrics;
}

std::vector<std::string> TokenizerSelector::previewTokenization(const std::string& text) const {
    std::vector<std::string> tokens;
    
    if (m_vocab.empty()) {
        // Fallback to whitespace if no vocab loaded
        std::stringstream ss(text);
        std::string segment;
        while (std::getline(ss, segment, ' ')) {
            if (!segment.empty()) {
                tokens.push_back(segment);
            }
        }
        return tokens;
    }

    // Greedy matching against m_vocab
    size_t i = 0;
    while (i < text.size()) {
        std::string best_token;
        size_t best_len = 0;
        
        size_t max_search = std::min(text.size() - i, (size_t)64);
        
        for (const auto& token : m_vocab) {
            if (token.size() > best_len && token.size() <= max_search) {
                if (text.compare(i, token.size(), token) == 0) {
                    best_token = token;
                    best_len = token.size();
                }
            }
        }
        
        if (best_len > 0) {
            tokens.push_back(best_token);
            i += best_len;
        } else {
            tokens.push_back(std::string(1, text[i]));
            i++;
        }
    }
    
    return tokens;
}

std::string TokenizerSelector::getJsonValue(const std::string& json, const std::string& key) const {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";

    size_t start = json.find_first_not_of(" \t\n\r", pos + 1);
    if (start == std::string::npos) return "";

    if (json[start] == '"') {
        size_t end = json.find('"', start + 1);
        if (end == std::string::npos) return "";
        return json.substr(start + 1, end - start - 1);
    }

    size_t end = json.find_first_of(",}\n\r\t ", start);
    if (end == std::string::npos) end = json.size();
    return json.substr(start, end - start);
}

void TokenizerSelector::loadVocabIfAvailable() {
    if (!m_config.vocabPath.empty()) {
        loadTokenizer(m_config.vocabPath);
    }
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
