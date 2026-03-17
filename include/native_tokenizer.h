#pragma once
// native_tokenizer.h — BPE tokenizer interface (C ABI, MASM-callable)

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NativeTokenizer NativeTokenizer;

// Create and destroy
NativeTokenizer* NativeTokenizer_Create  (void);
void             NativeTokenizer_Destroy (NativeTokenizer* tok);

// Load from tokenizer.json bytes already in memory.
// Returns 0 on success, -1 if vocab parsing fails.
int NativeTokenizer_LoadJson(NativeTokenizer* tok,
                              const char*      jsonData,
                              size_t           jsonLen);

// BPE-encode UTF-8 text.  Writes token IDs into outIds[0..maxIds-1].
// Returns number of ids written, or -1 on error.
int NativeTokenizer_Encode(NativeTokenizer* tok,
                            const char*      text,
                            uint32_t*        outIds,
                            uint32_t         maxIds);

// Decode a single token id back to its text representation.
// Returned pointer is valid for the lifetime of tok.
const char* NativeTokenizer_Decode    (NativeTokenizer* tok, uint32_t id);

// Total vocab entries loaded.
uint32_t    NativeTokenizer_VocabSize (NativeTokenizer* tok);

#ifdef __cplusplus
}
#endif
