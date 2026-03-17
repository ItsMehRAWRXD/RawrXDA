#include "bpe_tokenizer.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <numeric>
#include <unordered_map>

// Convert a single byte to its Unicode representation
std::string BPETokenizer::bytes_to_unicode(unsigned char b) {
    // GPT-2 byte encoder mapping
    static const char* byte_table[] = {
        "Ā", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?",
        "@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
        "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_",
        "`", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
        "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "{", "|", "}", "~", "Ā",
        // Extended ASCII (128-255)
        "Ā", "ā", "Ă", "ă", "Ą", "ą", "Ć", "ć", "Č", "č", "Ď", "ď", "Đ", "đ", "Ē", "ē",
        "Ĕ", "ĕ", "Ė", "ė", "Ę", "ę", "Ě", "ě", "Ĝ", "ĝ", "Ğ", "ğ", "Ġ", "ġ", "Ģ", "ģ",
        "Ĥ", "ĥ", "Ħ", "ħ", "Ĩ", "ĩ", "Ī", "ī", "Ĭ", "ĭ", "Į", "į", "İ", "ı", "Ĵ", "ĵ",
        "Ķ", "ķ", "ĸ", "ĸ", "Ĺ", "ĺ", "Ļ", "ļ", "Ľ", "ľ", "Ŀ", "ŀ", "Ł", "ł", "Ń", "ń",
        "Ņ", "ņ", "Ň", "ň", "Ŋ", "ŋ", "Ō", "ō", "Ŏ", "ŏ", "Ő", "ő", "Œ", "œ", "Ŕ", "ŕ",
        "Ŗ", "ŗ", "Ř", "ř", "Ś", "ś", "Ŝ", "ŝ", "Ş", "ş", "Š", "š", "Ţ", "ţ", "Ť", "ť",
        "Ŧ", "ŧ", "Ũ", "ũ", "Ū", "ū", "Ŭ", "ŭ", "Ů", "ů", "Ű", "ű", "Ų", "ų", "Ŵ", "ŵ",
        "Ŷ", "ŷ", "Ÿ", "ÿ", "Ź", "ź", "Ż", "ż", "Ž", "ž", "ſ", "ƀ", "Ɓ", "Ƃ", "ƃ", "Ƅ"
    };
    
    if (b < 256) {
        return byte_table[b];
    }
    // Fallback for out-of-range
    return std::string(1, (char)b);
}

// Load vocabulary from file
bool BPETokenizer::load(const std::string& vocab_file, const std::string& merges_file) {
    // Load vocabulary
    std::ifstream vf(vocab_file);
    if (!vf.is_open()) {
        std::cerr << "Failed to open vocab file: " << vocab_file << std::endl;
        return false;
    }
    
    std::string line;
    int token_id = 0;
    
    while (std::getline(vf, line)) {
        if (line.empty()) continue;
        
        // Handle JSON-encoded tokens
        if (line[0] == '"' && line[line.length() - 1] == '"') {
            line = line.substr(1, line.length() - 2);
        }
        
        encoder[line] = token_id;
        decoder[token_id] = line;
        token_id++;
    }
    vf.close();
    
    // Load merges with priority ranking
    std::ifstream mf(merges_file);
    if (!mf.is_open()) {
        std::cerr << "Failed to open merges file: " << merges_file << std::endl;
        return false;
    }
    
    int rank = 0;
    while (std::getline(mf, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        size_t space_pos = line.find(' ');
        if (space_pos == std::string::npos) continue;
        
        std::string first = line.substr(0, space_pos);
        std::string second = line.substr(space_pos + 1);
        
        std::string pair_key = first + " " + second;
        bpe_ranks[pair_key] = rank;
        rank++;
    }
    mf.close();
    
    std::cout << "✅ BPE Tokenizer loaded: " << encoder.size() << " vocab tokens, " 
              << bpe_ranks.size() << " BPE merges" << std::endl;
    
    return true;
}

// Tokenize text to token IDs
std::vector<int> BPETokenizer::encode(const std::string& text) {
    std::vector<int> result;
    
    // Process text in unicode words (split on whitespace and punctuation)
    std::vector<std::string> words;
    std::string current_word;
    
    for (char c : text) {
        if (c == ' ' || c == '\n' || c == '\t') {
            if (!current_word.empty()) {
                words.push_back(current_word);
                current_word.clear();
            }
        } else {
            current_word += c;
        }
    }
    if (!current_word.empty()) {
        words.push_back(current_word);
    }
    
    // Process each word
    for (const auto& word : words) {
        // Convert word to byte-level tokens
        std::vector<std::string> tokens;
        for (unsigned char c : word) {
            tokens.push_back(bytes_to_unicode(c));
        }
        
        // Add end-of-word marker
        if (!tokens.empty()) {
            tokens.back() = tokens.back() + "</w>";
        }
        
        // Apply BPE merges
        tokens = apply_bpe(tokens);
        
        // Convert to token IDs
        for (const auto& token : tokens) {
            auto it = encoder.find(token);
            if (it != encoder.end()) {
                result.push_back(it->second);
            } else {
                // Unknown token - use UNK token ID (usually 0)
                result.push_back(0);
            }
        }
    }
    
    return result;
}

// Apply BPE merges to byte tokens
std::vector<std::string> BPETokenizer::apply_bpe(std::vector<std::string>& tokens) {
    if (tokens.size() <= 1) return tokens;
    
    // Repeatedly merge the best pair
    while (tokens.size() > 1) {
        // Find all adjacent pairs and their ranks
        int best_rank = INT_MAX;
        int best_idx = -1;
        
        for (int i = 0; i < (int)tokens.size() - 1; i++) {
            std::string pair = tokens[i] + " " + tokens[i + 1];
            
            auto it = bpe_ranks.find(pair);
            if (it != bpe_ranks.end() && it->second < best_rank) {
                best_rank = it->second;
                best_idx = i;
            }
        }
        
        // No more valid merges
        if (best_idx == -1) break;
        
        // Merge the best pair
        tokens[best_idx] = tokens[best_idx] + tokens[best_idx + 1];
        tokens.erase(tokens.begin() + best_idx + 1);
    }
    
    return tokens;
}

std::string BPETokenizer::bytes_to_string(const std::string& bytes) {
    std::string out;
    out.reserve(bytes.size());
    for (unsigned char c : bytes) {
        out += bytes_to_unicode(c);
    }
    return out;
}

// Decode token IDs back to text
std::string BPETokenizer::decode(const std::vector<int>& token_ids) {
    std::string text;
    
    for (int token_id : token_ids) {
        auto it = decoder.find(token_id);
        if (it != decoder.end()) {
            text += it->second;
        }
    }
    
    // Remove </w> markers and restore spaces
    size_t pos = 0;
    while ((pos = text.find("</w>", pos)) != std::string::npos) {
        text.replace(pos, 4, " ");
        pos += 1;
    }
    
    // Handle Unicode escapes (simplified)
    // In production, would properly decode multi-byte UTF-8
    
    return text;
}

// Tokenize and return token strings (for debugging)
std::vector<std::string> BPETokenizer::tokenize(const std::string& text) {
    auto token_ids = encode(text);
    std::vector<std::string> token_strs;
    
    for (int id : token_ids) {
        auto it = decoder.find(id);
        if (it != decoder.end()) {
            token_strs.push_back(it->second);
        }
    }
    
    return token_strs;
}
