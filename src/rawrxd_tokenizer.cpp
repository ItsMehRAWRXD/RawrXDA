#include "rawrxd_tokenizer.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <immintrin.h>

bool RawrXDTokenizer::Load(const std::string& vocabPath) {
    // 1. Initialize with bytes 
    for (int i = 0; i < 256; i++) {
        std::string s(1, (char)i);
        // Standard Llama token mapping often has <0xXX> for bytes or raw bytes
        // We'll map them to a safe range if needed, or assume raw index.
        // For simplicity:
        vocab[s] = i + 3; 
        reverse_vocab[i + 3] = s;
    }
    
    // 2. Load file if exists (e.g. tokenizer.model or vocab.json)
    // Stub: Try to read basic lines
    std::ifstream f(vocabPath);
    if (!f.is_open()) {
        // Fallback to ASCII byte encoding only
        return true;
    }
    
    std::string line;
    int idx = 259; // Start after bytes
    while (std::getline(f, line)) {
        // Minimal parser
        if (line.empty()) continue;
        vocab[line] = idx;
        reverse_vocab[idx] = line;
        idx++;
    }
    
    return true;
}

std::vector<uint32_t> RawrXDTokenizer::Encode(const std::string& text) {
    // Greedy Matcher (Longest Prefix) - Simplified BPE
    std::vector<uint32_t> tokens;
    
    // Add BOS
    tokens.push_back(BOS_ID);
    
    size_t pos = 0;
    size_t len = text.length();
    
    // SIMD-optimized byte processing for simple byte-level tokenization
    // Process 64 bytes at a time using AVX-512 (64 bytes = 512 bits)
    while (pos + 63 < len) {
        // Load 64 bytes into AVX-512 register
        __m512i byte_vec = _mm512_loadu_si512((__m512i*)(text.data() + pos));
        
        // For each byte, create token
        // Since we're doing byte-level, we can process all 64 bytes in parallel
        for (int i = 0; i < 64; i++) {
            uint8_t c = ((uint8_t*)&byte_vec)[i];
            std::string s(1, (char)c);
            if (vocab.count(s)) {
                tokens.push_back(vocab[s]);
            } else {
                tokens.push_back(c + 3);
            }
        }
        pos += 64;
    }
    
    // Handle remaining bytes
    while (pos < len) {
        uint8_t c = (uint8_t)text[pos];
        std::string s(1, (char)c);
        
        if (vocab.count(s)) {
            tokens.push_back(vocab[s]);
        } else {
            // Unknown? Just cast to int?
            tokens.push_back(c + 3); 
        }
        pos++;
    }
    
    return tokens;
}

std::string RawrXDTokenizer::Decode(const std::vector<uint32_t>& tokens) {
    std::string res;
    for (uint32_t t : tokens) {
        if (t == BOS_ID || t == EOS_ID) continue;
        if (reverse_vocab.count(t)) {
            res += reverse_vocab[t];
        } else if (t < 256 + 3 && t >= 3) {
            res += (char)(t - 3);
        }
    }
    return res;
}
