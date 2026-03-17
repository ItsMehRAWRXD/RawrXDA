// native_tokenizer.cpp — BPE tokenizer with tokenizer.json parser
// No external JSON or tokenizer library dependencies.
// Parses HuggingFace tokenizer.json format:
//   model.vocab  : {"<token>": id, ...}
//   model.merges : ["a b", "c d", ...]  (lower index = higher priority)
//   added_tokens : [{id, content, special}, ...]
//
// Encoding pipeline:
//   UTF-8 text → pre-tokenize (bytes/words) → BPE merge → token ID array

#include "native_tokenizer.h"
#include <cstdlib>
#include <cstring>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>

// ── Max sizes (static allocation to avoid heap fragmentation) ─────────────────
#define NT_MAX_VOCAB        65536
#define NT_MAX_MERGES       65536
#define NT_MAX_TOKEN_LEN    256
#define NT_MAX_TOKENS_OUT   8192
#define NT_HASH_BUCKETS     131101   // prime

// ── Internal structures ────────────────────────────────────────────────────────

typedef struct {
    char     text[NT_MAX_TOKEN_LEN];
    uint32_t id;
} VocabEntry;

typedef struct {
    char     left[NT_MAX_TOKEN_LEN];
    char     right[NT_MAX_TOKEN_LEN];
    uint32_t priority;    // lower = higher priority (index in merges list)
} MergeRule;

typedef struct HashNode {
    char     key[NT_MAX_TOKEN_LEN];
    uint32_t value;
    struct HashNode* next;
} HashNode;

struct NativeTokenizer {
    VocabEntry  vocab[NT_MAX_VOCAB];
    uint32_t    vocabCount;

    MergeRule   merges[NT_MAX_MERGES];
    uint32_t    mergeCount;

    // Open-addressing string hash table: token text → token id
    HashNode*   hashBuckets[NT_HASH_BUCKETS];
    HashNode    hashPool[NT_MAX_VOCAB];
    uint32_t    hashPoolUsed;

    // Merge lookup: "left right" → priority
    HashNode*   mergeBuckets[NT_HASH_BUCKETS];
    HashNode    mergePool[NT_MAX_MERGES];
    uint32_t    mergePoolUsed;

    uint32_t    bosId;        // beginning-of-sequence token id
    uint32_t    eosId;        // end-of-sequence token id
    uint32_t    unkId;        // unknown token id
    bool        addBos;
    bool        addEos;
    bool        byteLevel;    // true if tokenizer uses Ġ-style byte-level BPE
};

// ── FNV-1a hash ────────────────────────────────────────────────────────────────
static uint32_t fnv1a(const char* s) {
    uint32_t h = 0x811C9DC5u;
    while (*s) { h ^= (uint8_t)*s++; h *= 0x01000193u; }
    return h;
}

static void ht_insert(HashNode** buckets, HashNode* pool, uint32_t* poolUsed,
                      const char* key, uint32_t value, uint32_t numBuckets) {
    if (*poolUsed >= NT_MAX_VOCAB && buckets == nullptr) return;
    const uint32_t slot = fnv1a(key) % numBuckets;
    HashNode* node = &pool[(*poolUsed)++];
    strncpy(node->key, key, NT_MAX_TOKEN_LEN - 1);
    node->key[NT_MAX_TOKEN_LEN - 1] = '\0';
    node->value = value;
    node->next  = buckets[slot];
    buckets[slot] = node;
}

static bool ht_lookup(HashNode** buckets, const char* key,
                      uint32_t numBuckets, uint32_t* outValue) {
    const uint32_t slot = fnv1a(key) % numBuckets;
    for (HashNode* n = buckets[slot]; n; n = n->next) {
        if (strcmp(n->key, key) == 0) { *outValue = n->value; return true; }
    }
    return false;
}

// ── Minimal JSON string scanner (no allocations) ──────────────────────────────
// Returns pointer just past the closing quote, or NULL on error.
// Dest is filled with the unescaped string, truncated to destMax-1.
static const char* json_read_string(const char* p, char* dest, size_t destMax) {
    if (*p != '"') return NULL;
    ++p;
    size_t out = 0;
    while (*p && out + 1 < destMax) {
        if (*p == '\\') {
            ++p;
            switch (*p) {
                case '"':  dest[out++] = '"';  break;
                case '\\': dest[out++] = '\\'; break;
                case '/':  dest[out++] = '/';  break;
                case 'n':  dest[out++] = '\n'; break;
                case 'r':  dest[out++] = '\r'; break;
                case 't':  dest[out++] = '\t'; break;
                case 'u': {
                    // 4-hex-digit unicode escape → UTF-8 (BMP only)
                    uint32_t cp = 0;
                    for (int i = 0; i < 4 && p[1]; ++i) {
                        ++p;
                        const char c = *p;
                        const uint32_t d = (c >= '0' && c <= '9') ? (uint32_t)(c - '0')
                                         : (c >= 'a' && c <= 'f') ? (uint32_t)(c - 'a' + 10)
                                         : (c >= 'A' && c <= 'F') ? (uint32_t)(c - 'A' + 10)
                                         : 0u;
                        cp = (cp << 4) | d;
                    }
                    if (cp < 0x80u) {
                        if (out + 1 < destMax) dest[out++] = (char)cp;
                    } else if (cp < 0x800u) {
                        if (out + 2 < destMax) {
                            dest[out++] = (char)(0xC0 | (cp >> 6));
                            dest[out++] = (char)(0x80 | (cp & 0x3F));
                        }
                    } else {
                        if (out + 3 < destMax) {
                            dest[out++] = (char)(0xE0 | (cp >> 12));
                            dest[out++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                            dest[out++] = (char)(0x80 | (cp & 0x3F));
                        }
                    }
                    break;
                }
                default: dest[out++] = *p; break;
            }
        } else if (*p == '"') {
            dest[out] = '\0';
            return p + 1;
        } else {
            dest[out++] = *p;
        }
        ++p;
    }
    return NULL;
}

// Skip whitespace
static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    return p;
}

// Read a positive integer (returns pointer past last digit)
static const char* read_uint(const char* p, uint32_t* out) {
    *out = 0;
    if (*p < '0' || *p > '9') return p;
    while (*p >= '0' && *p <= '9') {
        *out = *out * 10u + (uint32_t)(*p - '0');
        ++p;
    }
    return p;
}

// ── JSON model.vocab parser ────────────────────────────────────────────────────
// Scans forward for "vocab": { ... } and parses each "token": id pair.
static void parse_vocab(NativeTokenizer* tok, const char* json, size_t len) {
    // Find "vocab"
    const char* end = json + len;
    const char* p = json;
    tok->vocabCount = 0;

    while (p < end) {
        p = (const char*)memchr(p, '"', (size_t)(end - p));
        if (!p) break;
        char key[32];
        const char* after = json_read_string(p, key, sizeof(key));
        if (!after) { ++p; continue; }
        p = skip_ws(after);
        if (*p != ':') { continue; }
        p = skip_ws(p + 1);
        if (strcmp(key, "vocab") == 0 && *p == '{') {
            ++p; // enter vocab object
            while (p < end && *p != '}') {
                p = skip_ws(p);
                if (*p == '}') break;
                if (*p != '"') { ++p; continue; }
                char token[NT_MAX_TOKEN_LEN];
                const char* ap = json_read_string(p, token, sizeof(token));
                if (!ap) { ++p; continue; }
                p = skip_ws(ap);
                if (*p != ':') { continue; }
                p = skip_ws(p + 1);
                uint32_t id = 0;
                p = read_uint(p, &id);
                if (tok->vocabCount < NT_MAX_VOCAB) {
                    strncpy(tok->vocab[tok->vocabCount].text, token, NT_MAX_TOKEN_LEN - 1);
                    tok->vocab[tok->vocabCount].id = id;
                    ht_insert(tok->hashBuckets, tok->hashPool, &tok->hashPoolUsed,
                               token, id, NT_HASH_BUCKETS);
                    ++tok->vocabCount;
                }
                p = skip_ws(p);
                if (*p == ',') ++p;
            }
            break;
        }
    }
}

// ── JSON model.merges parser ───────────────────────────────────────────────────
// Scans for "merges": [ ... ] and parses each "a b" string.
static void parse_merges(NativeTokenizer* tok, const char* json, size_t len) {
    const char* end = json + len;
    const char* p = json;
    tok->mergeCount = 0;

    while (p < end) {
        p = (const char*)memchr(p, '"', (size_t)(end - p));
        if (!p) break;
        char key[32];
        const char* after = json_read_string(p, key, sizeof(key));
        if (!after) { ++p; continue; }
        p = skip_ws(after);
        if (*p != ':') continue;
        p = skip_ws(p + 1);
        if (strcmp(key, "merges") == 0 && *p == '[') {
            ++p;
            uint32_t priority = 0;
            while (p < end && *p != ']') {
                p = skip_ws(p);
                if (*p == ']') break;
                if (*p != '"') { ++p; continue; }
                char merge[NT_MAX_TOKEN_LEN * 2];
                const char* ap = json_read_string(p, merge, sizeof(merge));
                if (!ap) { ++p; continue; }
                p = skip_ws(ap);

                // Split "a b" into left and right on the last space
                char* sp = strrchr(merge, ' ');
                if (sp && tok->mergeCount < NT_MAX_MERGES) {
                    *sp = '\0';
                    const char* leftStr  = merge;
                    const char* rightStr = sp + 1;
                    strncpy(tok->merges[tok->mergeCount].left,  leftStr,  NT_MAX_TOKEN_LEN - 1);
                    strncpy(tok->merges[tok->mergeCount].right, rightStr, NT_MAX_TOKEN_LEN - 1);
                    tok->merges[tok->mergeCount].priority = priority;

                    // Insert into merge hash: key = "left\x1Fright"
                    char mkey[NT_MAX_TOKEN_LEN * 2 + 2];
                    snprintf(mkey, sizeof(mkey), "%s\x1F%s", leftStr, rightStr);
                    ht_insert(tok->mergeBuckets, tok->mergePool, &tok->mergePoolUsed,
                               mkey, priority, NT_HASH_BUCKETS);
                    ++tok->mergeCount;
                }
                ++priority;
                if (*p == ',') ++p;
            }
            break;
        }
    }
}

// ── BPE core ───────────────────────────────────────────────────────────────────
// word: array of token strings (in-place merge)
// Returns final count of tokens.
static uint32_t bpe_merge_word(NativeTokenizer* tok,
                               char tokens[][NT_MAX_TOKEN_LEN], uint32_t count) {
    bool changed = true;
    while (changed && count > 1) {
        changed = false;
        // Find best (lowest priority) merge across all adjacent pairs
        uint32_t bestPri  = UINT32_MAX;
        uint32_t bestIdx  = UINT32_MAX;

        for (uint32_t i = 0; i + 1 < count; ++i) {
            char mkey[NT_MAX_TOKEN_LEN * 2 + 2];
            snprintf(mkey, sizeof(mkey), "%s\x1F%s", tokens[i], tokens[i + 1]);
            uint32_t pri;
            if (ht_lookup(tok->mergeBuckets, mkey, NT_HASH_BUCKETS, &pri)) {
                if (pri < bestPri) { bestPri = pri; bestIdx = i; }
            }
        }

        if (bestIdx == UINT32_MAX) break;  // no merge possible

        // Apply merge: tokens[bestIdx] + tokens[bestIdx+1] → merged
        strncat(tokens[bestIdx], tokens[bestIdx + 1], NT_MAX_TOKEN_LEN - strlen(tokens[bestIdx]) - 1);

        // Shift remaining tokens left
        for (uint32_t i = bestIdx + 1; i + 1 < count; ++i) {
            memcpy(tokens[i], tokens[i + 1], NT_MAX_TOKEN_LEN);
        }
        --count;
        changed = true;
    }
    return count;
}

// ── UTF-8 pre-tokenizer ────────────────────────────────────────────────────────
// Splits input into "words" (sequences of alphanumerics OR single punctuation).
// For byte-level BPE each byte maps to its Ġ-encoded form; we map back.
// Returns number of words found.
static uint32_t pretokenize(const char* text, char words[][NT_MAX_TOKEN_LEN],
                             uint32_t maxWords, bool byteLevel) {
    uint32_t wCount  = 0;
    const uint8_t* p = (const uint8_t*)text;
    while (*p && wCount < maxWords) {
        // Skip spaces — for byte-level BPE prepend Ġ (U+0120) = "\xC4\xA0"
        bool isWordStart = true;
        if (*p == ' ') {
            if (byteLevel) {
                ++p;
                isWordStart = true;
            } else {
                ++p;
                continue;
            }
        }

        // Collect a word: alphanumeric run or single non-space character
        size_t wLen = 0;
        char*  wDst = words[wCount];
        if (byteLevel && isWordStart && *p && *p != ' ') {
            // prepend Ġ = U+0120 (UTF-8: 0xC4 0xA0)
            if (wLen + 2 < NT_MAX_TOKEN_LEN) {
                wDst[wLen++] = (char)0xC4;
                wDst[wLen++] = (char)0xA0;
            }
        }

        while (*p && *p != ' ' && wLen + 1 < NT_MAX_TOKEN_LEN) {
            wDst[wLen++] = (char)*p;
            ++p;
        }
        wDst[wLen] = '\0';
        if (wLen > 0) ++wCount;
    }
    return wCount;
}

// ── Public API ─────────────────────────────────────────────────────────────────

NativeTokenizer* NativeTokenizer_Create(void) {
    NativeTokenizer* tok = (NativeTokenizer*)calloc(1, sizeof(NativeTokenizer));
    if (!tok) return NULL;
    tok->unkId   = 0;
    tok->bosId   = 1;
    tok->eosId   = 2;
    tok->addBos  = true;
    tok->addEos  = true;
    tok->byteLevel = false;
    return tok;
}

void NativeTokenizer_Destroy(NativeTokenizer* tok) {
    free(tok);
}

int NativeTokenizer_LoadJson(NativeTokenizer* tok, const char* jsonData, size_t jsonLen) {
    if (!tok || !jsonData || jsonLen == 0) return -1;

    // Detect byte-level BPE by presence of "\u0120" or "Ġ" in the JSON
    tok->byteLevel = (strstr(jsonData, "\\u0120") != NULL ||
                      strstr(jsonData, "\xC4\xA0")  != NULL);

    // Parse vocab and merges
    parse_vocab(tok, jsonData, jsonLen);
    parse_merges(tok, jsonData, jsonLen);

    // Detect BOS/EOS/UNK ids from special tokens (best-effort scan)
    const char* specStart = strstr(jsonData, "\"added_tokens\"");
    if (specStart) {
        const char* p = specStart;
        const char* end = jsonData + jsonLen;
        while (p < end) {
            // Scan for "special":true and capture adjacent "id" and "content"
            const char* sp = strstr(p, "\"special\": true");
            if (!sp) sp = strstr(p, "\"special\":true");
            if (!sp) break;
            // Find the enclosing object by scanning backward for "id"
            const char* idPos = sp;
            while (idPos > p && *idPos != '{') --idPos;
            // Extract "id": N
            const char* idTag = strstr(idPos, "\"id\":");
            uint32_t specId = 0;
            if (idTag && idTag < sp + 200) {
                const char* np = skip_ws(idTag + 5);
                np = read_uint(np, &specId);
            }
            // Extract "content": "..."
            const char* ctTag = strstr(idPos, "\"content\":");
            char content[32] = {};
            if (ctTag && ctTag < sp + 200) {
                const char* cp = skip_ws(ctTag + 10);
                json_read_string(cp, content, sizeof(content));
            }
            if (strcmp(content, "<s>") == 0 || strcmp(content, "<|begin_of_text|>") == 0)
                tok->bosId = specId;
            else if (strcmp(content, "</s>") == 0 || strcmp(content, "<|end_of_text|>") == 0)
                tok->eosId = specId;
            else if (strcmp(content, "<unk>") == 0)
                tok->unkId = specId;
            p = sp + 16;
        }
    }

    return (tok->vocabCount > 0) ? 0 : -1;
}

int NativeTokenizer_Encode(NativeTokenizer* tok,
                            const char* text,
                            uint32_t* outIds,
                            uint32_t  maxIds) {
    if (!tok || !text || !outIds || maxIds == 0) return -1;

    uint32_t outCount = 0;

    // Prepend BOS
    if (tok->addBos && outCount < maxIds) {
        outIds[outCount++] = tok->bosId;
    }

    // Pre-tokenize
    static char words[NT_MAX_TOKENS_OUT][NT_MAX_TOKEN_LEN];
    uint32_t wCount = pretokenize(text, words, NT_MAX_TOKENS_OUT, tok->byteLevel);

    // BPE encode each word
    for (uint32_t w = 0; w < wCount && outCount < maxIds; ++w) {
        // Split word into individual UTF-8 characters (initial BPE tokens)
        static char wtoks[NT_MAX_TOKEN_LEN][NT_MAX_TOKEN_LEN];
        uint32_t tCount = 0;

        const uint8_t* wp = (const uint8_t*)words[w];
        while (*wp && tCount < NT_MAX_TOKEN_LEN) {
            // Determine UTF-8 sequence length
            uint32_t seqLen = 1;
            if      ((*wp & 0xF0) == 0xF0) seqLen = 4;
            else if ((*wp & 0xE0) == 0xE0) seqLen = 3;
            else if ((*wp & 0xC0) == 0xC0) seqLen = 2;

            size_t i = 0;
            while (i < seqLen && wp[i]) {
                wtoks[tCount][i] = (char)wp[i];
                ++i;
            }
            wtoks[tCount][i] = '\0';
            wp += i;
            ++tCount;
        }

        // Run BPE merge
        tCount = bpe_merge_word(tok, wtoks, tCount);

        // Convert merged tokens to ids
        for (uint32_t t = 0; t < tCount && outCount < maxIds; ++t) {
            uint32_t id;
            if (ht_lookup(tok->hashBuckets, wtoks[t], NT_HASH_BUCKETS, &id)) {
                outIds[outCount++] = id;
            } else {
                outIds[outCount++] = tok->unkId;
            }
        }
    }

    // Append EOS
    if (tok->addEos && outCount < maxIds) {
        outIds[outCount++] = tok->eosId;
    }

    return (int)outCount;
}

const char* NativeTokenizer_Decode(NativeTokenizer* tok, uint32_t id) {
    if (!tok || id >= tok->vocabCount) return "";
    // Linear scan (acceptably fast for single-token decode; use reverse hash if perf-critical)
    for (uint32_t i = 0; i < tok->vocabCount; ++i) {
        if (tok->vocab[i].id == id) return tok->vocab[i].text;
    }
    return "";
}

uint32_t NativeTokenizer_VocabSize(NativeTokenizer* tok) {
    return tok ? tok->vocabCount : 0u;
}
