#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SovTok_Init(void);

// RCX=input, RDX=len, R8=output token buffer, R9=index base (pass 0 for built-in table)
uint64_t SovTok_TokenizeBPE_AVX512(const uint8_t* input, uint64_t len, uint32_t* outTokens, const void* bpeIndexBase);

// RCX=input, RDX=len, R8=output token buffer, R9=trie root (pass 0 for built-in trie)
uint64_t SovTok_TokenizeTrie(const uint8_t* input, uint64_t len, uint32_t* outTokens, const void* trieRoot);

// RCX=token stream, RDX=count, R8=glyph output, R9=token table (pass 0 for built-in map)
uint64_t SovTok_RenderTokensToGlyphs(const uint32_t* tokens, uint64_t count, uint32_t* outGlyphs, const uint32_t* tokenTable);

// Returns 1 on success, 0 when ring is full.
uint32_t SovTok_SPSC_EmitToken(uint32_t token);

// Returns token in EAX and status in EDX (1=token valid, 0=empty).
uint32_t SovTok_SPSC_ConsumeToken(void);

#ifdef __cplusplus
}
#endif
