#pragma once
// RawrXD Pure MASM64 Model Loader API — Zero Python, Zero Qt, Zero llama.cpp
#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllimport) int GGUF_LoadModel(const char* path, size_t maxMem);
__declspec(dllimport) int GGUF_GetTensor(const char* name, void** data, size_t* size);
__declspec(dllimport) void GGUF_UnloadModel(void);
__declspec(dllimport) int Inference_StreamTokens(void* model, int* input, int nTokens, void* callback);
__declspec(dllimport) void Dequantize_Q4_0_AVX512(void* src, float* dst, size_t nBlocks);
#ifdef __cplusplus
}
#endif
