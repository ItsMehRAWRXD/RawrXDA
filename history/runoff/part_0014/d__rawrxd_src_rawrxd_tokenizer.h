#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <cstdint>

class RawrXDTokenizer {
    std::unordered_map<std::string, int> vocab;
    std::unordered_map<int, std::string> reverse_vocab;
    
    // BPE merge pairs: (left, right) → merged token, ordered by priority (lower = higher priority)
    struct MergePair {
        std::string left;
        std::string right;
        std::string merged;
        int priority;  // Lower = higher priority (earlier merge)
    };
    std::vector<MergePair> merges;
    
    // Merge lookup: "left right" → priority rank
    std::unordered_map<std::string, int> mergeLookup;
    
    // Internal BPE merge algorithm
    std::vector<std::string> BytePairMerge(const std::vector<std::string>& pieces) const;
    
public:
    // Load from generic vocab file (supports line-per-token or GGUF-style vocab + merges)
    bool Load(const std::string& vocabPath);
    
    // Load merge pairs from merges.txt (GPT-2/Llama style)
    bool LoadMerges(const std::string& mergesPath);
    
    // Encode text to tokens
    std::vector<uint32_t> Encode(const std::string& text);
    
    // Decode tokens to text
    std::string Decode(const std::vector<uint32_t>& tokens);
    
    // Special tokens
    uint32_t BOS_ID = 1;
    uint32_t EOS_ID = 2;
    
    // Vocab size
    size_t VocabSize() const { return vocab.size(); }
};

