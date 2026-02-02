#include "token_generator.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace RawrXD {

TokenGenerator::TokenGenerator() {
    m_vocab["<pad>"] = 0;
    m_vocab["<s>"] = 1;
    m_vocab["</s>"] = 2;
    m_vocab["<unk>"] = 3;
    
    const char* commonTokens[] = {
        "the", "be", "to", "of", "and", "a", "in", "that", "hello", "world"
        // ... (truncated list for brevity, assumption is user provided list is loaded)
    };
    
    for (const auto& token : commonTokens) {
        m_vocab[token] = (int)m_vocab.size();
    }
    
    for (const auto& [token, id] : m_vocab) {
        m_idToToken[id] = token;
    }
}

std::expected<std::vector<int>, TokenError> TokenGenerator::encode(const std::string& text) {
    if (m_vocab.empty()) return std::unexpected(TokenError::VocabularyNotLoaded);
    std::lock_guard lock(m_mutex);
    return bpeEncode(text);
}

std::expected<std::string, TokenError> TokenGenerator::decode(const std::vector<int>& tokens) {
    if (m_vocab.empty()) return std::unexpected(TokenError::VocabularyNotLoaded);
    std::lock_guard lock(m_mutex);
    return bpeDecode(tokens);
}

std::expected<std::vector<int>, TokenError> TokenGenerator::bpeEncode(const std::string& text) {
    std::vector<int> tokens;
    std::stringstream ss(text);
    std::string word;
    while (ss >> word) {
        // Simplified BPE
        if (m_vocab.count(word)) {
            tokens.push_back(m_vocab[word]);
        } else {
             tokens.push_back(m_unkToken);
        }
    }
    return tokens;
}

std::expected<std::string, TokenError> TokenGenerator::bpeDecode(const std::vector<int>& tokens) {
    std::string text;
    for (int t : tokens) {
        if (m_idToToken.count(t)) {
            text += m_idToToken[t] + " ";
        }
    }
    return text;
}

std::expected<int, TokenError> TokenGenerator::findToken(const std::string& token) {
    if (m_vocab.count(token)) return m_vocab[token];
    return std::unexpected(TokenError::TokenNotFound);
}

std::expected<std::string, TokenError> TokenGenerator::findTokenString(int tokenId) {
    if (m_idToToken.count(tokenId)) return m_idToToken[tokenId];
    return std::unexpected(TokenError::TokenNotFound);
}

std::expected<void, TokenError> TokenGenerator::loadVocabulary(const std::string& vocabPath) {
    std::lock_guard lock(m_mutex);
    std::ifstream file(vocabPath);
    if (!file) return std::unexpected(TokenError::VocabularyNotLoaded);
    
    std::string line;
    int id = 0;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            m_vocab[line] = id;
            m_idToToken[id] = line;
            id++;
        }
    }
    return {};
}

} // namespace RawrXD
