#pragma once
#include <string>
#include <vector>
#include <map>

class Tokenizer {
public:
    void load(const std::string& path);
    std::vector<int> encode(const std::string& text);
    std::string decode(const std::vector<int>& tokens);
};
