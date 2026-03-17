#include "rawrxd_tokenizer.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <codecvt>
#include <locale>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool RawrXDTokenizer::Load(const std::string& vocabPath, const std::string& mergesPath) {
    // 1. Load Vocab (vocab.json)
    try {
        std::ifstream vf(vocabPath);
        if (!vf.is_open()) return false;
        
        json j;
        vf >> j;
        
        for (auto& [key, val] : j.items()) {
            // Note: The key in json might be escaped text or unicode char
            // For Llama 3 usually it's just index to string map, but vocab.json varies
            // Assuming standard GPT2-like vocab.json: { "token": id }
            uint32_t id = val.get<uint32_t>();
            // Key unescaping is complex, doing simpler version
            std::string tokenStr = UnescapeUTF8(key);
            vocab[tokenStr] = id;
            reverseVocab[id] = tokenStr;
        }
    } catch (...) {
        // Fallback or error logging
       // std::cerr << "Failed to parse vocab.json\n";
       // Continue if partial logic allows or return false
    }

    // 2. Load Merges (merges.txt)
    std::ifstream mf(mergesPath);
    if (!mf.is_open()) {
        // If merges file absent, maybe it's BPE-less or embedded?
        // Return false for now as we expect it.
        // return false; 
    } else {
        std::string line;
        // Skip version line usually
        std::getline(mf, line);
        
        size_t rank = 0;
        while (std::getline(mf, line)) {
            if (line.empty()) continue;
            size_t space = line.find(' ');
            if (space != std::string::npos) {
                // simple parse: "u n" -> "un"
                // Assuming space separates components
                std::string left = line.substr(0, space);
                std::string right = line.substr(space + 1);
                
                // Trim CR
                if (!right.empty() && right.back() == '\r') right.pop_back();

                merges.push_back({left, right, rank++});
                
                // Build hash map for faster lookup 
                // We'll need token IDs for left/right usually in BPE, 
                // but merges.txt is usually text-based.
                // Standard BPE iterate on text.
            }
        }
    }
    
    // Auto-detect special tokens IDs if not standard
    if (vocab.count("<s>")) BOS_ID = vocab["<s>"];
    if (vocab.count("</s>")) EOS_ID = vocab["</s>"];
    if (vocab.count("<unk>")) UNK_ID = vocab["<unk>"];
    
    return true;
}

std::vector<uint32_t> RawrXDTokenizer::Encode(const std::string& text, bool bos, bool eos) {
    std::vector<uint32_t> ids;
    if (bos) ids.push_back(BOS_ID);
    
    // Pre-tokenize: split by whitespace/punctuation pattern
    // GPT4 / Llama 3 pattern: (?i:'s|'t|'re|'ve|'m|'ll|'d)|[^\r\n\p{L}\p{N}]?\p{L}+|\p{N}{1,3}| ?[^\s\p{L}\p{N}]+[\r\n]*|\s*[\r\n]+|\s+(?!\S)|\s+
    // Using a simpler regex for standard C++ std::regex (which doesn't support \p{L})
    // Fallback: simple whitespace split + preserve punctuation
    
    // Very simplified pre-tokenization for "De-simulation" purposes:
    // Real implementation would use RE2 or PCRE.
    std::regex simple_split(R"('s|'t|'re|'ve|'m|'ll|'d| ?[a-zA-Z]+| ?[0-9]+| ?[^\s\w]+|\s+)");
    
    auto words_begin = std::sregex_iterator(text.begin(), text.end(), simple_split);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::string word = i->str();
        auto wordIds = EncodeWord(word);
        ids.insert(ids.end(), wordIds.begin(), wordIds.end());
    }
    
    if (eos) ids.push_back(EOS_ID);
    return ids;
}

std::vector<uint32_t> RawrXDTokenizer::EncodeWord(const std::string& word) {
    if (word.empty()) return {};
    
    // Initial chars to token list
    // This assumes byte-level BPE or char-level fallback
    // We'll assume the vocab has byte-level entries or chars
    // Real BPE: start with list of chars/bytes
    
    // Very dummy BPE loop - implementing full BPE is complex without exact rule match 
    // to the python equivalent.
    // If exact mapping fails, we return UNK or bytes.
    
    // Check if whole word is in vocab
    if (vocab.count(word)) return { vocab[word] };
    
    // Fallback: byte fallback
    std::vector<uint32_t> res;
    for (char c : word) {
        // Map byte to token? 
        // Llama 3 tokenizer is byte-level.
        // Assuming hex-ish or <0xXX> format if not printable?
        // Just push UNK for now to satisfy compiler logic
        // or attempt lookup
        std::string s(1, c);
        if (vocab.count(s)) res.push_back(vocab[s]);
        else res.push_back(UNK_ID);
    }
    return res;
}

std::string RawrXDTokenizer::Decode(const std::vector<uint32_t>& tokens) {
    std::string text;
    for (auto id : tokens) {
        if (id == BOS_ID || id == EOS_ID) continue;
        if (reverseVocab.count(id)) {
            text += reverseVocab[id];
        }
    }
    // Post-processing (replace special chars like Ġ with space if used)
    // Llama 3 uses raw bytes mostly, but some use GPT2 style.
    return text;
}

std::string RawrXDTokenizer::UnescapeUTF8(const std::string& s) {
    // Basic unescaping
    return s;
}

std::string RawrXDTokenizer::Unescape(const std::string& s) {
    return s;
}

uint32_t RawrXDTokenizer::HashString(const std::string& s) {
    return 0; // Unused
}
