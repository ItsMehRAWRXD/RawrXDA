#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

class RawrXDTokenizer {
    struct TrieNode {
        std::map<char, TrieNode*> children;
        int tokenId = -1;
    };
    
    std::unordered_map<std::string, int> vocab;
    std::unordered_map<int, std::string> reverse_vocab;
    std::vector<std::pair<std::string, std::string>> merges;
    
public:
    bool Load(const std::string& vocabPath, const std::string& mergesPath);
    std::vector<uint32_t> Encode(const std::string& text, bool bos, bool eos);
    std::string Decode(const std::vector<uint32_t>& tokens);
};
