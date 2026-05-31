#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>

/**
 * @file Win32IDE_VocabResolver.cpp
 * @brief Batch 4 (26/118): Vocabulary & Token Resolver.
 * Maps token IDs to strings for the 5 Pillars (Hermes/Nous).
 */

namespace RawrXD::Models::Vocab {

namespace {

constexpr int kByteTokenBase = 3;
constexpr int kSystemToken = 50001;
constexpr int kUserToken = 50002;
constexpr int kAssistantToken = 50003;
constexpr int kNewline2Token = 50004;

struct LexiconEntry {
    int token;
    const char* text;
};

constexpr LexiconEntry kLexicon[] = {
    {kSystemToken, "<|system|>"},
    {kUserToken, "<|user|>"},
    {kAssistantToken, "<|assistant|>"},
    {kNewline2Token, "\n\n"},
};

uint32_t Fnv1a32(const char* s) {
    uint32_t h = 2166136261u;
    while (*s) {
        h ^= static_cast<uint8_t>(*s++);
        h *= 16777619u;
    }
    return h;
}

} // namespace

// Resolves: Vocab_TokenToText
extern "C" const char* Vocab_TokenToText(int token_id) {
    if (token_id >= kByteTokenBase && token_id < (kByteTokenBase + 256)) {
        static char singleByte[2];
        singleByte[0] = static_cast<char>(token_id - kByteTokenBase);
        singleByte[1] = 0;
        return singleByte;
    }

    for (const LexiconEntry& e : kLexicon) {
        if (e.token == token_id) {
            return e.text;
        }
    }

    static char fallback[32];
    std::snprintf(fallback, sizeof(fallback), "<tok:%d>", token_id);
    return fallback;
}

// Resolves: Vocab_TextToToken
extern "C" int Vocab_TextToToken(const char* text) {
    if (!text || !text[0]) {
        return 2;
    }

    for (const LexiconEntry& e : kLexicon) {
        if (std::strcmp(text, e.text) == 0) {
            return e.token;
        }
    }

    if (text[1] == 0) {
        return kByteTokenBase + static_cast<uint8_t>(text[0]);
    }

    const uint32_t h = Fnv1a32(text);
    return 60000 + static_cast<int>(h & 4095u);
}

} // namespace RawrXD::Models::Vocab
