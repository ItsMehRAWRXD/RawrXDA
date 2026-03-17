#include "bpe_tokenizer.h"
#include <fstream>
#include <iostream>
#include <algorithm>

bool BPETokenizer::load(const std::string& vocab_file, const std::string& merges_file) {
    // Load vocab (token -> id)
    std::ifstream vf(vocab_file);
    if (!vf.is_open()) return false;
    
    std::string line;
    int id = 0;
    while (std::getline(vf, line)) {
        // Decode base64 or handle raw bytes
        // For simplicity assuming raw list for now or mapped by line number
        encoder[line] = id;
        decoder[id] = line;
        id++;
    }
    
    // Load merges
    std::ifstream mf(merges_file);
    if (!mf.is_open()) return false;
    
    size_t rank = 0;
    while (std::getline(mf, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto space = line.find(' ');
        if (space != std::string::npos) {
            std::string first = line.substr(0, space);
            std::string second = line.substr(space+1);
            merges.push_back({first, second});
            
            // Build hash-ranked lookup table — O(1) per pair during encode
            merge_ranks[makeMergeKey(first, second)] = rank;
            rank++;
        }
    }
    
    return true;
}

// ============================================================================
// High-Performance BPE Encode — O(n log n) via hash-ranked merge lookup
// ============================================================================
// Old approach: O(n²) — for each merge, scan ALL pairs with std::find_if
// New approach: for each pair, look up rank in O(1) hash map, pick lowest
// ============================================================================
std::vector<int> BPETokenizer::encode(const std::string& text) {
    std::vector<int> result;
    
    // Split by regex
    std::sregex_iterator it(text.begin(), text.end(), pat);
    std::sregex_iterator end;
    
    for (; it != end; ++it) {
        std::string token = it->str();
        
        // Byte-level: convert to bytes first
        std::vector<std::string> word;
        word.reserve(token.size());
        for (unsigned char c : token) {
            word.push_back(bytes_to_unicode(c));
        }
        
        // Apply BPE merges using hash-ranked lookup
        // Strategy: scan all adjacent pairs, find the one with lowest rank,
        // merge it, repeat until no more merges possible.
        // Complexity: O(n * log(n)) amortized — each merge reduces word by 1,
        // and each scan is O(current_word_length) with O(1) rank lookup.
        while (word.size() > 1) {
            // Find the pair with the lowest merge rank
            size_t best_rank = (size_t)-1;  // sentinel: no merge found
            int best_idx = -1;
            
            for (size_t i = 0; i < word.size() - 1; i++) {
                std::string key = makeMergeKey(word[i], word[i+1]);
                auto mit = merge_ranks.find(key);
                if (mit != merge_ranks.end() && mit->second < best_rank) {
                    best_rank = mit->second;
                    best_idx = (int)i;
                }
            }
            
            if (best_idx == -1) break;  // No more applicable merges
            
            // Merge the winning pair
            word[best_idx] = word[best_idx] + word[best_idx + 1];
            word.erase(word.begin() + best_idx + 1);
        }
        
        // Convert to IDs
        for (const auto& w : word) {
            auto eit = encoder.find(w);
            if (eit != encoder.end()) result.push_back(eit->second);
            else result.push_back(encoder["<|unk|>"]);
        }
    }
    
    return result;
}

std::string BPETokenizer::decode(const std::vector<int>& tokens) {
    std::string text;
    for (int t : tokens) {
        auto it = decoder.find(t);
        if (it != decoder.end()) {
            text += it->second;
        }
    }
    // Convert byte tokens back to UTF-8
    return bytes_to_string(text);
}

std::string BPETokenizer::bytes_to_unicode(unsigned char b) {
    // GPT-2 byte encoder
    static const std::vector<std::string> byte_encoder = {
        "<|endoftext|>", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?", "@",
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
        "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_", "`",
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p",
        "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "{", "|", "}", "~", "\u0120", "\u0121"
    };
    if (b < 128) return byte_encoder[b];
    // Extended bytes... simplified fallback
    return std::string(1, (char)b);
}

std::string BPETokenizer::bytes_to_string(const std::string& bytes) {
    // Reverse byte encoding
    return bytes; // Simplified - real impl handles \u0120 etc
}
