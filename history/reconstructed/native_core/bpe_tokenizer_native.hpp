// native_core/bpe_tokenizer_native.hpp
// Native BPE Tokenizer — C++ wrapper around MASM kernels
// Zero external deps — pure C++20 + Win32
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

extern "C" {
    uint64_t BPE_HashString(const void* str, uint64_t len);
    uint64_t BPE_UTF8ToCodepoint(const void* utf8, uint32_t* codepoint);
    uint64_t BPE_CountUTF8Chars(const void* utf8, uint64_t byteLen);
    uint32_t BPE_FindLongestMatch(const void* vocab, uint64_t count, uint64_t hash);
}

namespace RawrXD::Native {

// Vocabulary entry — matches MASM layout: [hash:8][tokenId:4][length:4]
struct VocabEntry {
    uint64_t hash;
    uint32_t tokenId;
    uint32_t length;    // byte length of the token string

    bool operator<(const VocabEntry& o) const { return hash < o.hash; }
};

class BPETokenizer {
    std::vector<VocabEntry>     sortedVocab_;
    std::vector<std::string>    tokenStrings_;   // tokenId -> string
    std::unordered_map<std::string, uint32_t> strToId_;
    uint32_t unkTokenId_ = 0;
    uint32_t bosTokenId_ = 1;
    uint32_t eosTokenId_ = 2;

public:
    // Load vocabulary from a simple text file: one token per line
    bool LoadVocab(const wchar_t* path) {
        HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        LARGE_INTEGER fileSize;
        GetFileSizeEx(hFile, &fileSize);

        std::string content(static_cast<size_t>(fileSize.QuadPart), '\0');
        DWORD bytesRead = 0;
        ReadFile(hFile, content.data(), static_cast<DWORD>(content.size()), &bytesRead, nullptr);
        CloseHandle(hFile);
        content.resize(bytesRead);

        // Parse line by line
        uint32_t tokenId = 0;
        size_t pos = 0;
        while (pos < content.size()) {
            size_t end = content.find('\n', pos);
            if (end == std::string::npos) end = content.size();

            std::string token = content.substr(pos, end - pos);
            // Strip \r
            if (!token.empty() && token.back() == '\r') token.pop_back();

            if (!token.empty()) {
                uint64_t hash = BPE_HashString(token.data(), token.size());
                sortedVocab_.push_back({hash, tokenId, static_cast<uint32_t>(token.size())});
                tokenStrings_.push_back(token);
                strToId_[token] = tokenId;
                tokenId++;
            }
            pos = end + 1;
        }

        // Sort for binary search
        std::sort(sortedVocab_.begin(), sortedVocab_.end());
        return !sortedVocab_.empty();
    }

    // Add a single token to vocabulary
    uint32_t AddToken(const std::string& token) {
        uint32_t id = static_cast<uint32_t>(tokenStrings_.size());
        uint64_t hash = BPE_HashString(token.data(), token.size());
        sortedVocab_.push_back({hash, id, static_cast<uint32_t>(token.size())});
        tokenStrings_.push_back(token);
        strToId_[token] = id;
        // Re-sort
        std::sort(sortedVocab_.begin(), sortedVocab_.end());
        return id;
    }

    // Tokenize UTF-8 text using greedy longest-match
    std::vector<uint32_t> Encode(const std::string& text) const {
        std::vector<uint32_t> tokens;
        size_t pos = 0;

        while (pos < text.size()) {
            uint32_t bestId = unkTokenId_;
            size_t   bestLen = 1;

            // Try decreasing lengths for greedy longest match
            size_t maxLen = std::min(text.size() - pos, size_t(64));
            for (size_t len = maxLen; len >= 1; --len) {
                uint64_t hash = BPE_HashString(text.data() + pos, len);
                uint32_t id = BPE_FindLongestMatch(
                    sortedVocab_.data(),
                    sortedVocab_.size(),
                    hash
                );
                if (id != 0xFFFFFFFF) {
                    bestId  = id;
                    bestLen = len;
                    break;
                }
            }

            tokens.push_back(bestId);
            pos += bestLen;
        }

        return tokens;
    }

    // Decode token IDs back to string
    std::string Decode(const std::vector<uint32_t>& tokens) const {
        std::string result;
        for (uint32_t id : tokens) {
            if (id < tokenStrings_.size()) {
                result += tokenStrings_[id];
            } else {
                result += "<unk>";
            }
        }
        return result;
    }

    // Count UTF-8 codepoints using MASM kernel
    uint64_t CountCodepoints(const std::string& text) const {
        return BPE_CountUTF8Chars(text.data(), text.size());
    }

    size_t VocabSize() const { return sortedVocab_.size(); }

    void SetSpecialTokens(uint32_t unk, uint32_t bos, uint32_t eos) {
        unkTokenId_ = unk;
        bosTokenId_ = bos;
        eosTokenId_ = eos;
    }
};

} // namespace RawrXD::Native
