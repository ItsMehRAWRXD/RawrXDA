<<<<<<< HEAD
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <cstdint>
#include <cstdint>

class RawrXDTokenizer {
    std::unordered_map<std::string, int> vocab;
    std::unordered_map<int, std::string> reverse_vocab;
    
public:
    // Load from generic vocab file
    bool Load(const std::string& vocabPath);
    
    // Encode text to tokens
    std::vector<uint32_t> Encode(const std::string& text);
    
    // Decode tokens to text
    std::string Decode(const std::vector<uint32_t>& tokens);
    
    // Special tokens
    uint32_t BOS_ID = 1;
    uint32_t EOS_ID = 2;
};

=======
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <cstdint>
#include <cstdint>

class RawrXDTokenizer {
    std::unordered_map<std::string, int> vocab;
    std::unordered_map<int, std::string> reverse_vocab;
    
public:
    // Load from generic vocab file
    bool Load(const std::string& vocabPath);
    
    // Encode text to tokens
    std::vector<uint32_t> Encode(const std::string& text);
    
    // Decode tokens to text
    std::string Decode(const std::vector<uint32_t>& tokens);
    
    // Special tokens
    uint32_t BOS_ID = 1;
    uint32_t EOS_ID = 2;
};

>>>>>>> origin/main
