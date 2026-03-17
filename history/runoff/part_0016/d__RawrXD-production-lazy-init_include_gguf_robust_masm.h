#pragma once
#include <cstdint>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

void* GGUF_InitContext(HANDLE hFile, uint64_t maxStringLen, uint32_t flags);
bool GGUF_ValidateHeader(void* context);
uint64_t GGUF_ParseMetadataSafe(void* context, void* callback, void* userData);
uint64_t GGUF_SkipString(void* context, const char* keyName);
bool GGUF_ShouldSkipKey(void* context, const char* keyName, uint64_t keyLen);

#ifdef __cplusplus
}
#endif
