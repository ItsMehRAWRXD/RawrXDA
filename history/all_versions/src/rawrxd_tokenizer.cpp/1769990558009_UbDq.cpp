#include "rawrxd_tokenizer.h"
#include <fstream>
#include <sstream>
#include <algorithm>

bool RawrXDTokenizer::Load(const std::string& vocabPath, const std::string& mergesPath) {
    // Load vocabulary (token -> ID)
    std::ifstream vf(vocabPath);
    std::string line;
    while (std::getline(vf, line)) {
        size_t spacePos = line.find(' ');
        if (spacePos == std::string::npos) continue;
        
        std::string token = Unescape(line.substr(0, spacePos));
        uint32_t id = std::stoul(line.substr(spacePos + 1));
        
        vocab[token] = id;
        reverseVocab[id] = token;
    }
    
    // Load merge rules
    std::ifstream mf(mergesPath);
    size_t rank = 0;
    while (std::getline(mf, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        size_t spacePos = line.find(' ');
        if (spacePos == std::string::npos) continue;
        
        MergeRule rule;
        rule.left = Unescape(line.substr(0, spacePos));
        rule.right = Unescape(line.substr(spacePos + 1));
        rule.rank = rank++;
        merges.push_back(rule);
        
        // Precompute hash for fast lookup during encoding
        uint64_t key = ((uint64_t)HashString(rule.left) << 32) | HashString(rule.right);
        mergeRanks[key] = rule.rank;
    }
    
    printf("[RawrXD] Tokenizer loaded: %zu vocab, %zu merges\n", 
           vocab.size(), merges.size());
    return true;
}

std::vector<uint32_t> RawrXDTokenizer::Encode(const std::string& text, bool bos, 
                              bool eos) {
    std::vector<uint32_t> result;
    if (bos) result.push_back(BOS_ID);
    
    // Pre-tokenization: split on whitespace and punctuation
    std::vector<std::string> words = PreTokenize(text);
    
    for (const auto& word : words) {
        // Byte-level BPE encoding of each word
        auto tokens = EncodeWord(word);
        result.insert(result.end(), tokens.begin(), tokens.end());
    }
    
    if (eos) result.push_back(EOS_ID);
    return result;
}

std::string RawrXDTokenizer::Decode(const std::vector<uint32_t>& tokens) {
    std::string result;
    for (uint32_t id : tokens) {
        if (id == BOS_ID || id == EOS_ID || id == PAD_ID) continue;
        
        auto it = reverseVocab.find(id);
        if (it != reverseVocab.end()) {
            result += it->second;
        } else {
            result += UNK_STR;
        }
    }
    
    // Post-processing: replace special unicode sequences
    result = UnescapeUTF8(result);
    return result;
}

std::vector<std::string> RawrXDTokenizer::PreTokenize(const std::string& text) {
    std::vector<std::string> words;
    std::string current;
    
    for (size_t i = 0; i < text.size();) {
        unsigned char c = text[i];
        
        // UTF-8 handling
        size_t charLen = 1;
        if (c >= 0xF0) charLen = 4;
        else if (c >= 0xE0) charLen = 3;
        else if (c >= 0xC0) charLen = 2;
        
        std::string ch = text.substr(i, charLen);
        
        // Check if whitespace or punctuation
        if (ch == " " || ch == "\t" || ch == "\n" || ch == "\r") {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
            words.push_back(ch); // Keep whitespace as separate token
        } else {
            current += ch;
        }
        
        i += charLen;
    }
    
    if (!current.empty()) words.push_back(current);
    return words;
}

std::vector<uint32_t> RawrXDTokenizer::EncodeWord(const std::string& word) {
    // Start with bytes as individual tokens
    std::vector<std::string> pieces;
    for (size_t i = 0; i < word.size();) {
        unsigned char c = word[i];
        size_t len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
        pieces.push_back(word.substr(i, len));
        i += len;
    }
    
    // Apply BPE merges greedily
    while (pieces.size() > 1) {
        // Find the highest-rank (lowest number) merge pair
        size_t bestRank = SIZE_MAX;
        size_t bestIdx = SIZE_MAX;
        
        for (size_t i = 0; i < pieces.size() - 1; i++) {
            uint64_t key = ((uint64_t)HashString(pieces[i]) << 32) | HashString(pieces[i + 1]);
            auto it = mergeRanks.find(key);
            if (it != mergeRanks.end() && it->second < bestRank) {
                bestRank = it->second;
                bestIdx = i;
            }
        }
        
        if (bestIdx == SIZE_MAX) break; // No merge possible
        
        // Merge the pair
        pieces[bestIdx] = pieces[bestIdx] + pieces[bestIdx + 1];
        pieces.erase(pieces.begin() + bestIdx + 1);
    }
    
    // Convert to IDs
    std::vector<uint32_t> ids;
    for (const auto& p : pieces) {
        auto it = vocab.find(p);
        if (it != vocab.end()) ids.push_back(it->second);
        else ids.push_back(UNK_ID);
    }
    
    return ids;
}

uint32_t RawrXDTokenizer::HashString(const std::string& s) {
    // Simple FNV-1a hash for merge lookup
    uint32_t hash = 2166136261u;
    for (unsigned char c : s) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

std::string RawrXDTokenizer::Unescape(const std::string& s) {
    // Handle \n, \t, etc.
    std::string result;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case 'n': result += '\n'; i++; break;
                case 't': result += '\t'; i++; break;
                case 'r': result += '\r'; i++; break;
                case '\\': result += '\\'; i++; break;
                default: result += s[i];
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

std::string RawrXDTokenizer::UnescapeUTF8(const std::string& s) {
    // Convert byte sequences back to UTF-8 characters
    // This handles the byte-fallback BPE encoding
    return s; // Simplified - real impl would handle \xXX escapes
}
