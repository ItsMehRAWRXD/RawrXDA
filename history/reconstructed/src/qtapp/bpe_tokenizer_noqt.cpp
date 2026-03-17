/**
 * bpe_tokenizer_noqt.cpp
 * Pure C++ BPE tokenizer implementation without Qt dependencies
 */

#include "bpe_tokenizer_noqt.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>

BPETokenizer::BPETokenizer() {
    // Initialize with common special tokens
    m_vocab["<PAD>"] = 0;
    m_vocab["<BOS>"] = 1;
    m_vocab["<EOS>"] = 2;
    m_vocab["<UNK>"] = 3;
    
    m_vocabReverse[0] = "<PAD>";
    m_vocabReverse[1] = "<BOS>";
    m_vocabReverse[2] = "<EOS>";
    m_vocabReverse[3] = "<UNK>";
}

BPETokenizer::BPETokenizer(const std::string& vocabPath) : BPETokenizer() {
    loadVocab(vocabPath);
}

bool BPETokenizer::loadVocab(const std::string& vocabPath) {
    std::ifstream file(vocabPath);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    int32_t tokenId = 4;  // Start after special tokens
    
    while (std::getline(file, line)) {
        // Remove whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.empty() || line[0] == '#') continue;
        
        // Parse line format: "token<TAB>frequency" or just "token"
        std::istringstream iss(line);
        std::string token;
        int frequency = 0;
        
        if (std::getline(iss, token, '\t')) {
            iss >> frequency;
        } else {
            continue;
        }
        
        if (!token.empty()) {
            m_vocab[token] = tokenId;
            m_vocabReverse[tokenId] = token;
            tokenId++;
        }
    }
    
    return true;
}

std::vector<int32_t> BPETokenizer::tokenize(const std::string& text) {
    if (m_vocab.empty()) {
        return basicTokenize(text);
    }
    
    auto tokens = basicTokenize(text);
    return bpeMerge(tokens);
}

std::vector<int32_t> BPETokenizer::basicTokenize(const std::string& text) {
    std::vector<int32_t> tokens;
    
    // Convert to lowercase if enabled
    std::string processedText = text;
    if (m_lowercase) {
        std::transform(processedText.begin(), processedText.end(), 
                      processedText.begin(), ::tolower);
    }
    
    if (m_byteLevel) {
        // Byte-level tokenization
        for (unsigned char c : processedText) {
            std::string byteStr(1, c);
            auto it = m_vocab.find(byteStr);
            if (it != m_vocab.end()) {
                tokens.push_back(it->second);
            } else {
                // Unknown token
                tokens.push_back(3);  // UNK token
            }
        }
    } else {
        // Word-level tokenization (whitespace separated)
        std::istringstream iss(processedText);
        std::string word;
        
        while (iss >> word) {
            auto it = m_vocab.find(word);
            if (it != m_vocab.end()) {
                tokens.push_back(it->second);
            } else {
                // Fallback: tokenize character by character
                for (char c : word) {
                    std::string charStr(1, c);
                    auto charIt = m_vocab.find(charStr);
                    if (charIt != m_vocab.end()) {
                        tokens.push_back(charIt->second);
                    }
                }
            }
        }
    }
    
    return tokens;
}

std::vector<int32_t> BPETokenizer::bpeMerge(std::vector<int32_t> tokens) {
    if (tokens.empty() || m_mergeRules.empty()) {
        return tokens;
    }
    
    // Apply merge rules until no more merges possible
    bool changed = true;
    while (changed) {
        changed = false;
        std::vector<int32_t> newTokens;
        
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i < tokens.size() - 1) {
                auto pair = std::make_pair(tokens[i], tokens[i + 1]);
                auto it = m_mergeRules.find(pair);
                
                if (it != m_mergeRules.end()) {
                    // Merge found
                    newTokens.push_back(tokens[i]);
                    changed = true;
                    ++i;  // Skip next token as it's merged
                    continue;
                }
            }
            newTokens.push_back(tokens[i]);
        }
        
        tokens = newTokens;
    }
    
    return tokens;
}

std::string BPETokenizer::detokenize(const std::vector<int32_t>& tokens) {
    std::string result;
    
    for (int32_t token : tokens) {
        auto it = m_vocabReverse.find(token);
        if (it != m_vocabReverse.end()) {
            result += it->second;
        }
    }
    
    return result;
}

std::string BPETokenizer::detokenizeSpecial(const std::vector<int32_t>& tokens) {
    std::string result;
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        int32_t token = tokens[i];
        auto it = m_vocabReverse.find(token);
        
        if (it != m_vocabReverse.end()) {
            std::string tokenStr = it->second;
            
            // Handle special formatting
            if (tokenStr == "</s>") {
                result += " ";
            } else if (tokenStr == "<s>") {
                // Beginning token, skip
            } else {
                result += tokenStr;
            }
        }
    }
    
    return result;
}

std::vector<int32_t> BPETokenizer::tokenizeSpecial(const std::string& text) {
    // First tokenize normally
    auto tokens = tokenize(text);
    
    // Prepend BOS token if not present
    if (tokens.empty() || tokens[0] != 1) {
        tokens.insert(tokens.begin(), 1);
    }
    
    return tokens;
}

std::string BPETokenizer::decode(int32_t token) {
    auto it = m_vocabReverse.find(token);
    if (it != m_vocabReverse.end()) {
        return it->second;
    }
    return "<UNK>";
}

int32_t BPETokenizer::encode(const std::string& text) {
    auto it = m_vocab.find(text);
    if (it != m_vocab.end()) {
        return it->second;
    }
    return 3;  // UNK token
}

std::string BPETokenizer::getToken(int32_t id) const {
    auto it = m_vocabReverse.find(id);
    if (it != m_vocabReverse.end()) {
        return it->second;
    }
    return "<UNK>";
}

std::string BPETokenizer::bytesToString(const std::vector<uint8_t>& bytes) {
    std::string result(bytes.begin(), bytes.end());
    return result;
}

std::vector<uint8_t> BPETokenizer::stringToBytes(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}
