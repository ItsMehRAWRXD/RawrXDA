#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

class RawrXDTokenizer {
    struct TrieNode {
        std::map<char, TrieNode*> children;
        int tokenId = -1;
    };
    
    std::unordered_map<std::string, int> vocab;
    std::unordered_map<int, std::string> reverse_vocab;
    std::vector<std::pair<std::string, std::string>> merges;
    
public:
    bool Load(const std::string& vocabPath, const std::string& mergesPath) {
        // Mock/Stub Load for Unified Build
        // In real impl, read JSON. For now, basic bytes.
        vocab.clear();
        reverse_vocab.clear();
        
        // Basic ASCII
        for(int i=0; i<256; i++) {
            std::string s(1, (char)i);
            vocab[s] = i + 3; // Shift for special tokens
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

