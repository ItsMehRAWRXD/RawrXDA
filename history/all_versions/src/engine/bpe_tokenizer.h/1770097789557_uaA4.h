#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <regex>

class BPETokenizer {
public:
    std::unordered_map<std::string, int> encoder;
    std::unordered_map<int, std::string> decoder;
    std::vector<std::pair<std::string, std::string>> merges;
    
    // GPT-2/LLaMA regex pattern
    std::regex pat{ R"('s|'t|'re|'ve|'m|'ll|'d| ?\p{L}+| ?\p{N}+| ?[^\s\p{L}\p{N}]+|\s+(?!\S)|\s+)" };
    
    bool load(const std::string& vocab_file, const std::string& merges_file);
    std::vector<int> encode(const std::string& text);
    std::string decode(const std::vector<int>& tokens);

private:
    std::string bytes_to_unicode(unsigned char b);
    std::string bytes_to_string(const std::string& bytes);
};
