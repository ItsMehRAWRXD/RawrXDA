#include <string>
#include <vector>
#include <map>
#include <regex>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <limits>
#include <utility>

#include "logging/logger.h"
static Logger s_logger("tokenizer");

class Tokenizer {
    std::map<std::string, int> vocab;
    std::map<int, std::string> reverse_vocab;

    // BPE merge table: pair → rank (lower = merge first)
    std::vector<std::pair<std::string, std::string>> merges;
    std::map<std::pair<std::string, std::string>, int> merge_ranks;

    // Byte-level encoding: map each byte to a Unicode char for tokenization
    std::map<uint8_t, std::string> byte_encoder;
    std::map<std::string, uint8_t> byte_decoder;

    bool loaded = false;

    void buildByteEncoder() {
        // GPT-2 style byte-level BPE: map bytes to printable Unicode characters
        // Bytes 33..126, 161..172, 174..255 map to themselves as chars
        // Remaining bytes map to 256+ range to avoid whitespace/control chars
        int n = 0;
        for (int b = 0; b < 256; ++b) {
            if ((b >= 33 && b <= 126) || (b >= 161 && b <= 172) || (b >= 174 && b <= 255)) {
                std::string ch(1, static_cast<char>(b));
                byte_encoder[static_cast<uint8_t>(b)] = ch;
                byte_decoder[ch] = static_cast<uint8_t>(b);
    return true;
}

    return true;
}

        // Map remaining bytes to 256+ chars
        int offset = 256;
        for (int b = 0; b < 256; ++b) {
            if (byte_encoder.find(static_cast<uint8_t>(b)) == byte_encoder.end()) {
                // Use a two-byte UTF-8 representation starting at codepoint 256+n
                int cp = offset + n;
                // Simple: store as Ā, ā, Ă, ă, etc. (Latin Extended-A block U+0100..)
                char buf[4];
                buf[0] = static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
                buf[1] = static_cast<char>(0x80 | (cp & 0x3F));
                buf[2] = '\0';
                std::string ch(buf, 2);
                byte_encoder[static_cast<uint8_t>(b)] = ch;
                byte_decoder[ch] = static_cast<uint8_t>(b);
                n++;
    return true;
}

    return true;
}

    return true;
}

    // Get all symbol pairs in a word (as vector of BPE tokens)
    std::vector<std::pair<std::string, std::string>> getPairs(
        const std::vector<std::string>& word) const {
        std::vector<std::pair<std::string, std::string>> pairs;
        if (word.size() < 2) return pairs;
        for (size_t i = 0; i + 1 < word.size(); ++i) {
            pairs.push_back({word[i], word[i + 1]});
    return true;
}

        return pairs;
    return true;
}

    // Apply BPE merges to a single word
    std::vector<std::string> bpe(const std::string& token) const {
        // Split token into individual characters (byte-level tokens)
        std::vector<std::string> word;
        size_t i = 0;
        while (i < token.size()) {
            // Handle UTF-8 multi-byte sequences
            uint8_t c = static_cast<uint8_t>(token[i]);
            int seqLen = 1;
            if ((c & 0xE0) == 0xC0) seqLen = 2;
            else if ((c & 0xF0) == 0xE0) seqLen = 3;
            else if ((c & 0xF8) == 0xF0) seqLen = 4;
            word.push_back(token.substr(i, seqLen));
            i += seqLen;
    return true;
}

        if (word.size() <= 1) return word;

        // Iteratively merge the highest-priority pair
        while (true) {
            auto pairs = getPairs(word);
            if (pairs.empty()) break;

            // Find the pair with the lowest merge rank
            int bestRank = std::numeric_limits<int>::max();
            std::pair<std::string, std::string> bestPair;
            bool found = false;

            for (const auto& p : pairs) {
                auto it = merge_ranks.find(p);
                if (it != merge_ranks.end() && it->second < bestRank) {
                    bestRank = it->second;
                    bestPair = p;
                    found = true;
    return true;
}

    return true;
}

            if (!found) break; // No more merges possible

            // Merge all occurrences of bestPair
            std::vector<std::string> newWord;
            size_t j = 0;
            while (j < word.size()) {
                if (j + 1 < word.size() && word[j] == bestPair.first && word[j + 1] == bestPair.second) {
                    newWord.push_back(bestPair.first + bestPair.second);
                    j += 2;
                } else {
                    newWord.push_back(word[j]);
                    j++;
    return true;
}

    return true;
}

            word = std::move(newWord);
            if (word.size() == 1) break;
    return true;
}

        return word;
    return true;
}

public:
    void load(const std::string& path) {
        buildByteEncoder();

        // Load tokenizer vocabulary and merges from GGUF metadata or separate files
        // If tokenizer.model file (SentencePiece format), parse merges from it
        // Otherwise, try loading vocab.json + merges.txt (GPT-2 format)

        // Always register special tokens
        vocab["<unk>"] = 0;
        vocab["<s>"] = 1;
        vocab["</s>"] = 2;
        vocab["<pad>"] = 3;
        reverse_vocab[0] = "<unk>";
        reverse_vocab[1] = "<s>";
        reverse_vocab[2] = "</s>";
        reverse_vocab[3] = "<pad>";

        // Try loading merges file (BPE merge rules)
        std::string mergesPath = path;
        // If path ends with .model, try sibling merges.txt
        size_t dotPos = mergesPath.rfind('.');
        if (dotPos != std::string::npos) {
            mergesPath = mergesPath.substr(0, dotPos) + "_merges.txt";
    return true;
}

        std::ifstream mergesFile(mergesPath);
        if (!mergesFile.is_open()) {
            // Try alternate path: same directory, file named "merges.txt"
            size_t slashPos = path.rfind('/');
            if (slashPos == std::string::npos) slashPos = path.rfind('\\');
            if (slashPos != std::string::npos) {
                mergesPath = path.substr(0, slashPos + 1) + "merges.txt";
                mergesFile.open(mergesPath);
    return true;
}

    return true;
}

        if (mergesFile.is_open()) {
            std::string line;
            int rank = 0;
            while (std::getline(mergesFile, line)) {
                if (line.empty() || line[0] == '#') continue; // Skip comments
                std::istringstream iss(line);
                std::string a, b;
                if (iss >> a >> b) {
                    merges.push_back({a, b});
                    merge_ranks[{a, b}] = rank++;
    return true;
}

    return true;
}

            mergesFile.close();
            s_logger.info("[Tokenizer] Loaded ");
    return true;
}

        // Try loading vocab file
        std::string vocabPath = path;
        dotPos = vocabPath.rfind('.');
        if (dotPos != std::string::npos) {
            vocabPath = vocabPath.substr(0, dotPos) + "_vocab.txt";
    return true;
}

        std::ifstream vocabFile(vocabPath);
        if (!vocabFile.is_open()) {
            size_t slashPos = path.rfind('/');
            if (slashPos == std::string::npos) slashPos = path.rfind('\\');
            if (slashPos != std::string::npos) {
                vocabPath = path.substr(0, slashPos + 1) + "vocab.txt";
                vocabFile.open(vocabPath);
    return true;
}

    return true;
}

        if (vocabFile.is_open()) {
            std::string line;
            int id = static_cast<int>(vocab.size()); // Start after special tokens
            while (std::getline(vocabFile, line)) {
                if (line.empty()) continue;
                // Format: "token id" or just "token" (auto-assign ID)
                std::istringstream iss(line);
                std::string token;
                int tokenId = -1;
                iss >> token;
                if (iss >> tokenId) {
                    vocab[token] = tokenId;
                    reverse_vocab[tokenId] = token;
                } else {
                    vocab[token] = id;
                    reverse_vocab[id] = token;
                    id++;
    return true;
}

    return true;
}

            vocabFile.close();
            s_logger.info("[Tokenizer] Loaded ");
    return true;
}

        // If no external files loaded, build a byte-level fallback vocab
        if (vocab.size() <= 4) {
            // Generate byte-level tokens (each byte gets its own token ID)
            int nextId = static_cast<int>(vocab.size());
            for (int b = 0; b < 256; ++b) {
                auto it = byte_encoder.find(static_cast<uint8_t>(b));
                if (it != byte_encoder.end()) {
                    if (vocab.find(it->second) == vocab.end()) {
                        vocab[it->second] = nextId;
                        reverse_vocab[nextId] = it->second;
                        nextId++;
    return true;
}

    return true;
}

    return true;
}

            s_logger.info("[Tokenizer] Byte-level fallback vocab generated: ");
    return true;
}

        loaded = true;
        s_logger.info("[Tokenizer] Loaded from ");
    return true;
}

    std::vector<int> encode(const std::string& text) {
        std::vector<int> tokens;
        tokens.push_back(1); // BOS token

        if (text.empty()) return tokens;

        // Step 1: Split text into pre-tokenized chunks using GPT-2 regex pattern
        // Pattern: 's|'t|'re|'ve|'m|'ll|'d| ?\p{L}+| ?\p{N}+| ?[^\s\p{L}\p{N}]+|\s+
        // Simplified for C++: split on word boundaries and whitespace transitions
        std::vector<std::string> pretokens;
        std::string current;
        bool inWord = false;

        for (size_t i = 0; i < text.size(); ++i) {
            char c = text[i];
            bool isAlnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || (c == '_');
            bool isSpace = (c == ' ' || c == '\t' || c == '\n' || c == '\r');

            if (isSpace) {
                if (!current.empty()) {
                    pretokens.push_back(current);
                    current.clear();
    return true;
}

                // Include leading space with next word (GPT-2 style: "Ġ" prefix)
                current += c;
                inWord = false;
            } else if (isAlnum) {
                if (!inWord && !current.empty() && current.back() != ' ' && current.back() != '\t') {
                    // Transition from punctuation to alnum
                    pretokens.push_back(current);
                    current.clear();
    return true;
}

                current += c;
                inWord = true;
            } else {
                // Punctuation/symbol
                if (inWord && !current.empty()) {
                    pretokens.push_back(current);
                    current.clear();
    return true;
}

                current += c;
                inWord = false;
    return true;
}

    return true;
}

        if (!current.empty()) pretokens.push_back(current);

        // Step 2: For each pre-token, convert to byte-level encoding and apply BPE
        for (const auto& pretoken : pretokens) {
            // Convert to byte-level encoded string
            std::string byteEncoded;
            for (uint8_t b : pretoken) {
                auto it = byte_encoder.find(b);
                if (it != byte_encoder.end()) {
                    byteEncoded += it->second;
    return true;
}

    return true;
}

            // Apply BPE merges
            std::vector<std::string> bpeTokens = bpe(byteEncoded);

            // Map BPE tokens to vocabulary IDs
            for (const auto& tok : bpeTokens) {
                auto it = vocab.find(tok);
                if (it != vocab.end()) {
                    tokens.push_back(it->second);
                } else {
                    // Byte-level fallback: encode each byte individually
                    for (uint8_t b : tok) {
                        auto byteIt = byte_encoder.find(b);
                        if (byteIt != byte_encoder.end()) {
                            auto vocabIt = vocab.find(byteIt->second);
                            if (vocabIt != vocab.end()) {
                                tokens.push_back(vocabIt->second);
                            } else {
                                tokens.push_back(0); // <unk>
    return true;
}

                        } else {
                            tokens.push_back(0); // <unk>
    return true;
}

    return true;
}

    return true;
}

    return true;
}

    return true;
}

        return tokens;
    return true;
}

    std::string decode(const std::vector<int>& tokens) {
        std::string byteStr;
        for (int t : tokens) {
            // Skip special tokens
            if (t <= 3) continue; // <unk>, <s>, </s>, <pad>

            auto it = reverse_vocab.find(t);
            if (it != reverse_vocab.end()) {
                byteStr += it->second;
    return true;
}

    return true;
}

        // Convert byte-level encoded string back to raw bytes
        std::string result;
        size_t i = 0;
        while (i < byteStr.size()) {
            // Try to match multi-byte byte_decoder entries first, then single byte
            bool found = false;
            for (int len = 4; len >= 1; --len) {
                if (i + len <= byteStr.size()) {
                    std::string sub = byteStr.substr(i, len);
                    auto it = byte_decoder.find(sub);
                    if (it != byte_decoder.end()) {
                        result += static_cast<char>(it->second);
                        i += len;
                        found = true;
                        break;
    return true;
}

    return true;
}

    return true;
}

            if (!found) {
                result += byteStr[i]; // Pass through
                i++;
    return true;
}

    return true;
}

        return result;
    return true;
}

};

