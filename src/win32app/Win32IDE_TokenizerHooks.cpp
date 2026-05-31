#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <string>
#include <cstdint>

/**
 * @file Win32IDE_TokenizerHooks.cpp
 * @brief Batch 4 (28/118): Tokenizer Lifecycle Hooks.
 * Manages the transition between raw bytes and model-specific tokens.
 */

namespace RawrXD::Models::Tokenizer {

namespace {

constexpr int kByteTokenBase = 3;
constexpr int kSystemToken = 50001;
constexpr int kUserToken = 50002;
constexpr int kAssistantToken = 50003;
constexpr int kNewline2Token = 50004;
constexpr int kMaxTemplateBytes = 32768;
constexpr int kTrieStates = 128;
constexpr int kTrieAlphabet = 128;

struct TrieBuildEntry {
    const char* text;
    int token;
};

struct TokenizerState {
    uint32_t mode;
    uint32_t trieBuilt;
};

static TokenizerState g_state = {};
static char g_templateBuffer[kMaxTemplateBytes] = {};
static uint16_t g_trieNext[kTrieStates][kTrieAlphabet] = {};
static int g_trieAccept[kTrieStates] = {};
static uint16_t g_trieStateCount = 1;

constexpr TrieBuildEntry kTrieLexicon[] = {
    {"<|system|>", kSystemToken},
    {"<|user|>", kUserToken},
    {"<|assistant|>", kAssistantToken},
    {"\n\n", kNewline2Token},
};

void BuildTokenizerTrie() {
    if (g_state.trieBuilt) {
        return;
    }

    g_trieStateCount = 1;
    for (const TrieBuildEntry& entry : kTrieLexicon) {
        uint16_t state = 0;
        const char* p = entry.text;
        while (*p) {
            const uint8_t b = static_cast<uint8_t>(*p);
            if (b >= kTrieAlphabet) {
                state = 0;
                break;
            }
            uint16_t next = g_trieNext[state][b];
            if (!next) {
                if (g_trieStateCount >= kTrieStates) {
                    state = 0;
                    break;
                }
                next = g_trieStateCount++;
                g_trieNext[state][b] = next;
            }
            state = next;
            ++p;
        }
        if (state != 0) {
            g_trieAccept[state] = entry.token;
        }
    }

    g_state.trieBuilt = 1;
}

int EncodeAVX512Style(const char* text, int text_len, int* out_tokens, int max_tokens) {
    if (!text || !out_tokens || text_len <= 0 || max_tokens <= 0) {
        return 0;
    }

    int produced = 0;
    int i = 0;
    while (i + 64 <= text_len && produced + 64 <= max_tokens) {
        for (int j = 0; j < 64; ++j) {
            out_tokens[produced + j] = kByteTokenBase + static_cast<uint8_t>(text[i + j]);
        }
        produced += 64;
        i += 64;
    }

    while (i < text_len && produced < max_tokens) {
        out_tokens[produced++] = kByteTokenBase + static_cast<uint8_t>(text[i++]);
    }
    return produced;
}

int EncodeTrieLinear(const char* text, int text_len, int* out_tokens, int max_tokens) {
    if (!text || !out_tokens || text_len <= 0 || max_tokens <= 0) {
        return 0;
    }

    BuildTokenizerTrie();

    int produced = 0;
    int i = 0;
    while (i < text_len && produced < max_tokens) {
        uint16_t state = 0;
        int bestToken = -1;
        int bestLen = 0;

        for (int step = 0; step < 16 && (i + step) < text_len; ++step) {
            const uint8_t b = static_cast<uint8_t>(text[i + step]);
            if (b >= kTrieAlphabet) {
                break;
            }
            state = g_trieNext[state][b];
            if (!state) {
                break;
            }
            if (g_trieAccept[state] != 0) {
                bestToken = g_trieAccept[state];
                bestLen = step + 1;
            }
        }

        if (bestToken != -1) {
            out_tokens[produced++] = bestToken;
            i += bestLen;
            continue;
        }

        out_tokens[produced++] = kByteTokenBase + static_cast<uint8_t>(text[i]);
        ++i;
    }

    return produced;
}

} // namespace

extern "C" bool Ring_PushToken(void* buffer, int token_id);

// Resolves: Tokenizer_Initialize
extern "C" void* Tokenizer_Initialize(const char* type) {
    const char* resolvedType = type ? type : "default";
    LOG_INFO("[Tokenizer] Initializing: " + std::string(resolvedType));

    g_state.mode = 0;
    if (_stricmp(resolvedType, "trie") == 0) {
        g_state.mode = 1;
    } else if (_stricmp(resolvedType, "avx512") == 0) {
        g_state.mode = 2;
    }
    BuildTokenizerTrie();
    return &g_state;
}

// Resolves: Tokenizer_ApplyTemplate
extern "C" const char* Tokenizer_ApplyTemplate(const char* prompt, const char* system_msg) {
    const char* userPrompt = prompt ? prompt : "";
    const char* systemMsg = system_msg ? system_msg : "";

    int offset = 0;
    const char* prefixA = "<|system|>\n";
    const char* prefixB = "\n\n<|user|>\n";
    const char* prefixC = "\n\n<|assistant|>\n";

    auto copyBounded = [&](const char* src) {
        while (*src && offset < (kMaxTemplateBytes - 1)) {
            g_templateBuffer[offset++] = *src++;
        }
    };

    copyBounded(prefixA);
    copyBounded(systemMsg);
    copyBounded(prefixB);
    copyBounded(userPrompt);
    copyBounded(prefixC);
    g_templateBuffer[offset] = 0;
    return g_templateBuffer;
}

extern "C" int Tokenizer_EncodeFast(void* handle, const char* text, int text_len, int* out_tokens, int max_tokens) {
    (void)handle;
    return EncodeAVX512Style(text, text_len, out_tokens, max_tokens);
}

extern "C" int Tokenizer_EncodeTrie(void* handle, const char* text, int text_len, int* out_tokens, int max_tokens) {
    (void)handle;
    return EncodeTrieLinear(text, text_len, out_tokens, max_tokens);
}

extern "C" int Tokenizer_EncodeAuto(void* handle, const char* text, int text_len, int* out_tokens, int max_tokens) {
    const TokenizerState* state = static_cast<const TokenizerState*>(handle);
    if (state && state->mode == 1) {
        return EncodeTrieLinear(text, text_len, out_tokens, max_tokens);
    }
    return EncodeAVX512Style(text, text_len, out_tokens, max_tokens);
}

extern "C" int Tokenizer_StreamToRing(void* handle, const char* text, int text_len, void* ring_buffer, int max_tokens) {
    int localTokens[1024];
    if (max_tokens <= 0 || max_tokens > 1024) {
        max_tokens = 1024;
    }

    const int n = Tokenizer_EncodeAuto(handle, text, text_len, localTokens, max_tokens);
    int pushed = 0;
    for (int i = 0; i < n; ++i) {
        if (!Ring_PushToken(ring_buffer, localTokens[i])) {
            break;
        }
        ++pushed;
    }
    return pushed;
}

} // namespace RawrXD::Models::Tokenizer
