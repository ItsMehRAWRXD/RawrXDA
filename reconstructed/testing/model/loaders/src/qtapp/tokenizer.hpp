// tokenizer.hpp — Pure C++17 tokenizer placeholder
// Converted from Qt (QString) to std::string
// Provides basic tokenize/detokenize interface for model inference

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <cstdint>

class Tokenizer {
public:
    Tokenizer() = default;

    // Simple whitespace + punctuation tokenizer
    // In production, this would use BPE/SentencePiece/etc.
    std::vector<int32_t> tokenize(const std::string& text) const {
        std::vector<int32_t> tokens;
        if (text.empty()) return tokens;

        // Simple byte-level tokenization for placeholder
        // Each unique word gets an ID
        std::string current;
        for (size_t i = 0; i < text.size(); i++) {
            char c = text[i];
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
                if (!current.empty()) {
                    tokens.push_back(wordToId(current));
                    current.clear();
                }
            } else if (isPunctuation(c)) {
                if (!current.empty()) {
                    tokens.push_back(wordToId(current));
                    current.clear();
                }
                tokens.push_back(wordToId(std::string(1, c)));
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            tokens.push_back(wordToId(current));
        }
        return tokens;
    }

    std::string detokenize(const std::vector<int32_t>& tokens) const {
        std::string result;
        for (size_t i = 0; i < tokens.size(); i++) {
            std::string word = idToWord(tokens[i]);
            if (i > 0 && !isPunctuation(word[0])) result += " ";
            result += word;
        }
        return result;
    }

    // Vocabulary management
    void addToken(const std::string& word, int32_t id) {
        m_wordToId[word] = id;
        m_idToWord[id] = word;
    }

    size_t vocabSize() const { return m_wordToId.size(); }

    bool hasToken(const std::string& word) const {
        return m_wordToId.find(word) != m_wordToId.end();
    }

    int32_t getTokenId(const std::string& word) const {
        auto it = m_wordToId.find(word);
        return (it != m_wordToId.end()) ? it->second : -1;
    }

    // Special tokens
    int32_t bosToken() const { return 1; } // Beginning of sequence
    int32_t eosToken() const { return 2; } // End of sequence
    int32_t padToken() const { return 0; } // Padding
    int32_t unkToken() const { return 3; } // Unknown

private:
    int32_t wordToId(const std::string& word) const {
        auto it = m_wordToId.find(word);
        if (it != m_wordToId.end()) return it->second;

        // Auto-assign ID (hash-based for consistency)
        uint32_t hash = 5381;
        for (char c : word) hash = ((hash << 5) + hash) + static_cast<uint8_t>(c);
        int32_t id = static_cast<int32_t>(hash % 100000) + 100; // Avoid special token IDs

        // Cache it (const_cast for mutable cache pattern)
        const_cast<Tokenizer*>(this)->m_wordToId[word] = id;
        const_cast<Tokenizer*>(this)->m_idToWord[id] = word;
        return id;
    }

    std::string idToWord(int32_t id) const {
        auto it = m_idToWord.find(id);
        return (it != m_idToWord.end()) ? it->second : "<unk>";
    }

    static bool isPunctuation(char c) {
        return c == '.' || c == ',' || c == '!' || c == '?' || c == ';' ||
               c == ':' || c == '"' || c == '\'' || c == '(' || c == ')' ||
               c == '[' || c == ']' || c == '{' || c == '}';
    }

    mutable std::unordered_map<std::string, int32_t> m_wordToId;
    mutable std::unordered_map<int32_t, std::string> m_idToWord;
};
