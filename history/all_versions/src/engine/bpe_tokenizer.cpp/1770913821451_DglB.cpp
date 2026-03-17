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
    // GPT-2 byte encoder — maps each byte value to a printable Unicode character
    // Identical logic to OpenAI's bytes_to_unicode() in encoder.py
    static const auto table = []() {
        std::array<uint32_t, 256> t{};
        // Printable byte ranges that map to themselves
        std::vector<int> printable;
        for (int i = 33; i <= 126; ++i) printable.push_back(i);   // '!' to '~'
        for (int i = 161; i <= 172; ++i) printable.push_back(i);  // '¡' to '¬'
        for (int i = 174; i <= 255; ++i) printable.push_back(i);  // '®' to 'ÿ'

        std::set<int> pset(printable.begin(), printable.end());
        for (int p : printable) t[p] = static_cast<uint32_t>(p);

        // Non-printable bytes (0-32, 127-160, 173) map to U+0100 onwards
        uint32_t n = 256;
        for (int i = 0; i < 256; ++i) {
            if (pset.find(i) == pset.end()) {
                t[i] = n++;
            }
        }
        return t;
    }();

    // Encode the mapped codepoint as UTF-8
    uint32_t cp = table[b];
    std::string result;
    if (cp < 0x80) {
        result += static_cast<char>(cp);
    } else if (cp < 0x800) {
        result += static_cast<char>(0xC0 | (cp >> 6));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        result += static_cast<char>(0xE0 | (cp >> 12));
        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return result;
}

std::string BPETokenizer::bytes_to_string(const std::string& encoded) {
    // Reverse GPT-2 byte encoding: decode Unicode codepoints back to raw byte values
    static const auto reverse_table = []() {
        std::unordered_map<uint32_t, unsigned char> rt;
        std::vector<int> printable;
        for (int i = 33; i <= 126; ++i) printable.push_back(i);
        for (int i = 161; i <= 172; ++i) printable.push_back(i);
        for (int i = 174; i <= 255; ++i) printable.push_back(i);

        std::set<int> pset(printable.begin(), printable.end());
        for (int p : printable) rt[static_cast<uint32_t>(p)] = static_cast<unsigned char>(p);
        uint32_t n = 256;
        for (int i = 0; i < 256; ++i) {
            if (pset.find(i) == pset.end()) {
                rt[n++] = static_cast<unsigned char>(i);
            }
        }
        return rt;
    }();

    std::string result;
    size_t i = 0;
    while (i < encoded.size()) {
        uint32_t cp = 0;
        unsigned char c = static_cast<unsigned char>(encoded[i]);
        size_t seqLen = 1;

        // Decode UTF-8 sequence to Unicode codepoint
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < encoded.size()) {
            cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(encoded[i + 1]) & 0x3F);
            seqLen = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < encoded.size()) {
            cp = ((c & 0x0F) << 12) | ((static_cast<unsigned char>(encoded[i + 1]) & 0x3F) << 6)
                 | (static_cast<unsigned char>(encoded[i + 2]) & 0x3F);
            seqLen = 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < encoded.size()) {
            cp = ((c & 0x07) << 18) | ((static_cast<unsigned char>(encoded[i + 1]) & 0x3F) << 12)
                 | ((static_cast<unsigned char>(encoded[i + 2]) & 0x3F) << 6)
                 | (static_cast<unsigned char>(encoded[i + 3]) & 0x3F);
            seqLen = 4;
        }

        auto it = reverse_table.find(cp);
        if (it != reverse_table.end()) {
            result += static_cast<char>(it->second);
        } else {
            // Not in GPT-2 byte map — preserve original UTF-8 sequence
            result.append(encoded, i, seqLen);
        }
        i += seqLen;
    }
    return result;
}
