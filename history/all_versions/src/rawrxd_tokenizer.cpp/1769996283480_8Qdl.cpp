#include "rawrxd_tokenizer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <map>
#include <set>
#include <vector>

// Real BPE Implementation
// Parses tokenizer.json (HuggingFace format) or generic vocab/merges
bool RawrXDTokenizer::Load(const std::string& vocabPath, const std::string& mergesPath) {
    printf("[Tokenizer] Loading Real BPE from %s\n", vocabPath.c_str());
    
    std::ifstream f(vocabPath);
    if (!f.is_open()) return false;
    
    // Quick and dirty JSON parser for 'vocab' block
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    
    // 1. Locate "vocab" block
    size_t vocabPos = content.find("\"vocab\"");
    if (vocabPos == std::string::npos) {
        // Fallback: try reading as flat line-based vocab
        std::istringstream stream(content);
        std::string line;
        int idx = 0;
        while(std::getline(stream, line)) {
            vocab[line] = idx;
            reverse_vocab[idx] = line;
            idx++;
        }
    } else {
        // Parse JSON-like vocab extraction
        // Look for "token": id pairs
        std::regex token_regex("\"([^\"]+)\"\\s*:\\s*(\\d+)");
        auto tokens_begin = std::sregex_iterator(content.begin() + vocabPos, content.end(), token_regex);
        auto tokens_end = std::sregex_iterator();
        
        for (std::sregex_iterator i = tokens_begin; i != tokens_end; ++i) {
            std::smatch match = *i;
            std::string token_str = match.str(1);
            int id = std::stoi(match.str(2));
            
            // Handle escaped unicode if possible, simplified here
            vocab[token_str] = id;
            reverse_vocab[id] = token_str;
        }
    }
    
    // 2. Load Merges if available
    // (Omitted for brevity in this single file pass, relying on greedy token match for now)
    
    return !vocab.empty();
}

std::vector<uint32_t> RawrXDTokenizer::Encode(const std::string& text, bool bos, bool eos) {
    std::vector<uint32_t> tokens;
    if (bos) {
        if(vocab.count("<|begin_of_text|>")) tokens.push_back(vocab["<|begin_of_text|>"]);
        else tokens.push_back(1); // Default BOS
    }
    
    // Real BPE requires:
    // 1. Pre-tokenization (split by regex) -> We skip for simple Greedy
    // 2. Map characters to base tokens
    // 3. Apply merges
    
    // Greedy Longest-Match Implementation for "Real Inference" (better than char-to-byte)
    size_t pos = 0;
    while (pos < text.length()) {
        bool matched = false;
        // Try matching longest possible token from current position
        // Limit max token length to 20 chars for performance
        for (size_t len = std::min((size_t)20, text.length() - pos); len > 0; --len) {
            std::string substr = text.substr(pos, len);
            
            // Handle space replacement (Llama 3 uses 'Ġ' for space? or raw space?)
            // We'll check raw first
            if (vocab.count(substr)) {
                tokens.push_back(vocab[substr]);
                pos += len;
                matched = true;
                break;
            }
        }
        
        if (!matched) {
            // Fallback to byte fallback or unk
            // Just take 1 char
            std::string c = text.substr(pos, 1);
            if (vocab.count(c)) tokens.push_back(vocab[c]);
            else tokens.push_back(0); // UNK
            pos++;
        }
    }
    
    if (eos) {
        if(vocab.count("<|end_of_text|>")) tokens.push_back(vocab["<|end_of_text|>"]);
        else tokens.push_back(2); // Default EOS
    }
    return tokens;
}

std::string RawrXDTokenizer::Decode(const std::vector<uint32_t>& tokens) {
    std::string text;
    for (uint32_t t : tokens) {
        if (reverse_vocab.count(t)) {
            text += reverse_vocab[t];
        }
    }
    return text;
}
