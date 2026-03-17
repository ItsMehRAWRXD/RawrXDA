#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

class RawrXDTokenizer {
    struct MergeRule {
        std::string left, right;
        size_t rank;
        
        bool operator<(const MergeRule& other) const {
            return rank < other.rank;
        }
    };
    
    std::unordered_map<std::string, uint32_t> vocab;
    std::unordered_map<uint32_t, std::string> reverseVocab;
    std::vector<MergeRule> merges;
    std::unordered_map<uint64_t, size_t> mergeRanks; // (left<<32|right) -> rank
    
    // Special tokens
    uint32_t BOS_ID = 1, EOS_ID = 2, PAD_ID = 0, UNK_ID = 3;
    std::string BOS_STR = "<s>", EOS_STR = "</s>", PAD_STR = "<pad>", UNK_STR = "<unk>";

public:
    bool Load(const std::string& vocabPath, const std::string& mergesPath);
    
    // Encode: text -> token IDs
    std::vector<uint32_t> Encode(const std::string& text, bool bos = true, 
                                  bool eos = true);
    
    // Decode: token IDs -> text
    std::string Decode(const std::vector<uint32_t>& tokens);

private:
    std::vector<std::string> PreTokenize(const std::string& text);
    std::vector<uint32_t> EncodeWord(const std::string& word);
    uint32_t HashString(const std::string& s);
    std::string Unescape(const std::string& s);
    std::string UnescapeUTF8(const std::string& s);
};
