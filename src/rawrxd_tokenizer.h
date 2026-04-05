#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <cstdint>
#include <limits>
#include "utf8_validator.h"
#include "gguf_loader.h"

class RawrXDTokenizer {
    std::unordered_map<std::string, int> vocab;
    std::unordered_map<int, std::string> reverse_vocab;
    uint32_t INVALID_TOKEN_ID = std::numeric_limits<uint32_t>::max();

    bool parseOrderedVocabJson(const std::string& jsonText, std::vector<std::string>& outVocab);
    void refreshSpecialTokenIds();
    static void appendNormalizedPiece(const std::string& piece, std::string& out);
    
public:
    // Load from generic vocab file
    bool Load(const std::string& vocabPath);
    
    // Load from GGUF file
    bool LoadFromGGUF(const std::string& ggufPath);
    
    // Load from vocab vector (from GGUF)
    bool LoadFromVocab(const std::vector<std::string>& vocabStrings);
    
    // Encode text to tokens
    std::vector<uint32_t> Encode(const std::string& text);
    
    // Decode tokens to text
    std::string Decode(const std::vector<uint32_t>& tokens);
    
    // Decode with UTF-8 validation and sanitization
    std::string DecodeSafe(const std::vector<uint32_t>& tokens);
    
    // Special tokens
    uint32_t BOS_ID = std::numeric_limits<uint32_t>::max();
    uint32_t EOS_ID = std::numeric_limits<uint32_t>::max();
    uint32_t UNK_ID = std::numeric_limits<uint32_t>::max();
    uint32_t PAD_ID = std::numeric_limits<uint32_t>::max();
};


