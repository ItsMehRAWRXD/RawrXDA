#include "rawrxd_tokenizer.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <immintrin.h>
#include <climits>

bool RawrXDTokenizer::Load(const std::string& vocabPath) {
    // 1. Initialize byte-level fallback tokens (indices 3..258)
    for (int i = 0; i < 256; i++) {
        std::string s(1, (char)i);
        vocab[s] = i + 3;
        reverse_vocab[i + 3] = s;
    }
    
    // 2. Load vocab file
    std::ifstream f(vocabPath);
    if (!f.is_open()) {
        printf("[RawrXD] Vocab file not found: %s — using byte-level fallback\n", vocabPath.c_str());
        return true;
    }
    
    std::string line;
    int idx = 259; // Start after byte tokens
    
    // Detect format: line-per-token (simple) or "token score" (SentencePiece-style)
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        
        // Skip comments / version headers
        if (line[0] == '#') continue;
        
        // Try SentencePiece format: "piece\tscore" or "piece score"
        size_t tabPos = line.find('\t');
        size_t spacePos = line.rfind(' ');
        
        std::string token;
        if (tabPos != std::string::npos) {
            // Tab-separated: token\tscore
            token = line.substr(0, tabPos);
        } else if (spacePos != std::string::npos && spacePos > 0) {
            // Try space-separated: token score
            std::string maybeSuffix = line.substr(spacePos + 1);
            bool isNumber = !maybeSuffix.empty();
            for (char c : maybeSuffix) {
                if (c != '-' && c != '.' && !isdigit((unsigned char)c)) {
                    isNumber = false;
                    break;
                }
            }
            if (isNumber) {
                token = line.substr(0, spacePos);
            } else {
                token = line; // Whole line is the token
            }
        } else {
            token = line;
        }
        
        if (token.empty()) continue;
        
        // Replace SentencePiece's ▁ (U+2581) with space
        // ▁ is 3 bytes: 0xE2 0x96 0x81
        std::string normalized;
        for (size_t i = 0; i < token.size(); ) {
            if (i + 2 < token.size() && 
                (unsigned char)token[i] == 0xE2 && 
                (unsigned char)token[i+1] == 0x96 && 
                (unsigned char)token[i+2] == 0x81) {
                normalized += ' ';
                i += 3;
            } else {
                normalized += token[i];
                i++;
            }
        }
        
        // Only add if not already present
        if (vocab.find(normalized) == vocab.end()) {
            vocab[normalized] = idx;
            reverse_vocab[idx] = normalized;
            idx++;
        }
    }
    
    printf("[RawrXD] Tokenizer loaded: %zu vocab entries\n", vocab.size());
    return true;
}

bool RawrXDTokenizer::LoadMerges(const std::string& mergesPath) {
    std::ifstream f(mergesPath);
    if (!f.is_open()) return false;
    
    std::string line;
    int priority = 0;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        // Format: "left right"
        size_t spacePos = line.find(' ');
        if (spacePos == std::string::npos) continue;
        
        MergePair mp;
        mp.left = line.substr(0, spacePos);
        mp.right = line.substr(spacePos + 1);
        mp.merged = mp.left + mp.right;
        mp.priority = priority;
        
        merges.push_back(mp);
        mergeLookup[mp.left + " " + mp.right] = priority;
        priority++;
    }
    
    printf("[RawrXD] Loaded %zu BPE merge rules\n", merges.size());
    return true;
}

// BPE merge algorithm: repeatedly merge the highest-priority pair
std::vector<std::string> RawrXDTokenizer::BytePairMerge(const std::vector<std::string>& pieces) const {
    if (pieces.size() <= 1 || mergeLookup.empty()) return pieces;
    
    std::vector<std::string> current = pieces;
    
    while (current.size() > 1) {
        // Find the pair with the lowest priority value (= highest merge priority)
        int bestPriority = INT_MAX;
        size_t bestIdx = SIZE_MAX;
        
        for (size_t i = 0; i + 1 < current.size(); i++) {
            std::string key = current[i] + " " + current[i + 1];
            auto it = mergeLookup.find(key);
            if (it != mergeLookup.end() && it->second < bestPriority) {
                bestPriority = it->second;
                bestIdx = i;
            }
        }
        
        if (bestIdx == SIZE_MAX) break; // No more merges possible
        
        // Apply merge at bestIdx
        std::vector<std::string> next;
        next.reserve(current.size() - 1);
        for (size_t i = 0; i < current.size(); ) {
            if (i == bestIdx) {
                next.push_back(current[i] + current[i + 1]);
                i += 2;
            } else {
                next.push_back(current[i]);
                i++;
            }
        }
        current = std::move(next);
    }
    
    return current;
}

std::vector<uint32_t> RawrXDTokenizer::Encode(const std::string& text) {
    std::vector<uint32_t> tokens;
    tokens.push_back(BOS_ID);
    
    if (text.empty()) return tokens;
    
    // Step 1: Split text into initial pieces (UTF-8 bytes as single-char strings)
    std::vector<std::string> pieces;
    pieces.reserve(text.size());
    for (size_t i = 0; i < text.size(); i++) {
        pieces.push_back(std::string(1, text[i]));
    }
    
    // Step 2: Apply BPE merges if merge rules loaded
    if (!mergeLookup.empty()) {
        pieces = BytePairMerge(pieces);
    } else {
        // Greedy longest-prefix matching (fallback when no merge rules)
        pieces.clear();
        size_t pos = 0;
        while (pos < text.size()) {
            size_t bestLen = 1;
            // Try progressively longer prefixes
            for (size_t len = (std::min)(text.size() - pos, (size_t)32); len > 1; len--) {
                std::string candidate = text.substr(pos, len);
                if (vocab.count(candidate)) {
                    bestLen = len;
                    break;
                }
            }
            pieces.push_back(text.substr(pos, bestLen));
            pos += bestLen;
        }
    }
    
    // Step 3: Convert pieces to token IDs
    for (const auto& piece : pieces) {
        auto it = vocab.find(piece);
        if (it != vocab.end()) {
            tokens.push_back((uint32_t)it->second);
        } else {
            // Fallback: encode each byte separately
            for (unsigned char c : piece) {
                tokens.push_back((uint32_t)(c + 3));
            }
        }
    }
    
    return tokens;
}

std::string RawrXDTokenizer::Decode(const std::vector<uint32_t>& tokens) {
    std::string res;
    for (uint32_t t : tokens) {
        if (t == BOS_ID || t == EOS_ID) continue;
        auto it = reverse_vocab.find(t);
        if (it != reverse_vocab.end()) {
            res += it->second;
        } else if (t >= 3 && t < 259) {
            res += (char)(t - 3);
        }
    }
    return res;
}
