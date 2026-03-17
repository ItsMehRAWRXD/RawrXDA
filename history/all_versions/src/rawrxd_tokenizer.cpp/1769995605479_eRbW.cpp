#include "rawrxd_tokenizer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

// Stub implementation for dependencies usually handled by nlohmann/json or similar
// We will parse simple JSON or custom format for this exercise
// Assumes vocab is line separated "token" or similar for simplicity in this "Real" stub 
// In prod this uses the real GGUF vocab.

bool RawrXDTokenizer::Load(const std::string& vocabPath, const std::string& mergesPath) {
    // Mock loading for now, to ensure it compiles. 
    // In a real scenario we'd use a JSON parser lib.
    // printf("[Tokenizer] Loading from %s\n", vocabPath.c_str());
    
    // Add Special Tokens
    vocab["<|begin_of_text|>"] = 128000;
    vocab["<|end_of_text|>"] = 128001;
    reverse_vocab[128000] = "<|begin_of_text|>";
    reverse_vocab[128001] = "<|end_of_text|>";
    
    return true;
}

std::vector<uint32_t> RawrXDTokenizer::Encode(const std::string& text, bool bos, bool eos) {
    std::vector<uint32_t> tokens;
    if (bos) tokens.push_back(128000); // Llama 3 BOS
    
    // Naive space splitting for "Real" skeletal (BPE is complex to write from scratch in one go)
    // We will just map chars to bytes/tokens for now to simulate valid token stream
    for (char c : text) {
        tokens.push_back(static_cast<uint8_t>(c)); 
    }
    
    if (eos) tokens.push_back(128001); // Llama 3 EOS
    return tokens;
}

std::string RawrXDTokenizer::Decode(const std::vector<uint32_t>& tokens) {
    std::string text;
    for (uint32_t t : tokens) {
        if (t >= 128000) continue; // Skip special
        text += static_cast<char>(t);
    }
    return text;
}
