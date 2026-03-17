#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

class RawrXDTokenizer {
    // Internal BPE structures
    struct TrieNode;
    
    std::unordered_map<std::string, int> vocab;
    std::unordered_map<int, std::string> reverse_vocab;
    // std::vector<std::pair<std::string, std::string>> merges; // Simplified BPE doesn't strictly need merges if we have vocab with scores
    
public:
    // Load from generic vocab file (or implicit logic)
    bool Load(const std::string& vocabPath);
    
    // Encode text to tokens
    std::vector<uint32_t> Encode(const std::string& text);
    
    // Decode tokens to text
    std::string Decode(const std::vector<uint32_t>& tokens);
};

        }
        return true; 
    }

    std::vector<uint32_t> Encode(const std::string& text, bool bos, bool eos) {
        std::vector<uint32_t> res;
        if (bos) res.push_back(1);
        for(char c : text) {
            res.push_back((uint8_t)c + 3);
        }
        if (eos) res.push_back(2);
        return res;
    }

    std::string Decode(const std::vector<uint32_t>& tokens) {
        std::string res;
        for(auto t : tokens) {
            if(t < 3) continue;
            res += (char)(t - 3);
        }
        return res;
    }
    
    const uint32_t EOS_ID = 2;
};

