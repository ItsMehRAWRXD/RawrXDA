#include "rawrxd_tokenizer.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool RawrXDTokenizer::Load(const std::string& vocabPath) {
    std::ifstream f(vocabPath);
    if (!f.is_open()) return true;

    // Check extension
    if (vocabPath.ends_with(".json")) {
        try {
            json j;
            f >> j;
            
            // Handle "model" -> "vocab" structure
            json vocabObj;
            if (j.contains("model") && j["model"].contains("vocab")) {
               vocabObj = j["model"]["vocab"];
            } else if (j.contains("vocab")) {
               vocabObj = j["vocab"];
            } else {
               // Fallback: assume root is map
               vocabObj = j;
            }
            
            if (vocabObj.is_object()) {
                for (auto& [key, val] : vocabObj.items()) {
                    if (val.is_number()) {
                        int id = val.get<int>();
                        vocab[key] = id;
                        reverse_vocab[id] = key;
                    }
                }
            }
        } catch(const std::exception& e) {
            printf("[RawrXD] JSON Tokenizer Load Error: %s\n", e.what());
        }
    } else {
        // Line based
        std::string line;
        int idx = 0; // Usually 0-indexed in vocab.txt
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            vocab[line] = idx;
            reverse_vocab[idx] = line;
            idx++;
        }
    }
    
    return true;
}

std::vector<uint32_t> RawrXDTokenizer::Encode(const std::string& text) {
    if (vocab.empty()) {
        // Fallback ASCII
        std::vector<uint32_t> t;
        for(char c : text) t.push_back((uint8_t)c + 3);
        return t;
    }

    std::vector<uint32_t> tokens;
    // tokens.push_back(BOS_ID); // Assume caller/template handles BOS or flag

    size_t pos = 0;
    while (pos < text.length()) {
        bool match = false;
        
        // Greedy longest match (Optimization: max token len)
        // Heuristic: check lengths 30 down to 1
        int search_len = std::min((int)(text.length() - pos), 40);
        
        for (int len = search_len; len >= 1; len--) {
            std::string sub = text.substr(pos, len);
            if (vocab.count(sub)) {
                tokens.push_back(vocab[sub]);
                pos += len;
                match = true;
                break;
            }
            
            // Try with space prefix if not at start?
            // BPE usually has " word"
        }
        
        if (!match) {
            // Unknown char, skip or byte fallback
             // <unk> or raw byte
             uint8_t c = (uint8_t)text[pos];
             // Try to find byte token
             std::string s(1, (char)c);
             if (vocab.count(s)) tokens.push_back(vocab[s]);
             else tokens.push_back(0); // UNK
             pos++;
        }
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
