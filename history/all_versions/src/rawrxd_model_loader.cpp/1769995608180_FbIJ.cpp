#include "rawrxd_model_loader.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>

bool RawrXDModelLoader::Load(const std::wstring& path, VkDevice device, VkPhysicalDevice physDevice) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    file_size = size.QuadPart;
    
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    mapped_data = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    
    CloseHandle(hMap);
    CloseHandle(hFile);
    
    // printf("[Loader] Mapped %zu bytes\n", file_size);
    // In real implementation, parse GGUF header here
    
    return true;
}

int RawrXDModelLoader::GetMetadataInt(const std::string& key) {
    // Stub: returns mock values typical for Llama-3-8B
    if (key == "embedding_length") return 4096;
    if (key == "block_count") return 32;
    if (key == "attention.head_count") return 32;
    if (key == "attention.head_count_kv") return 8;
    if (key == "tokenizer.ggml.tokens") return 128256;
    return 0;
}

float* RawrXDModelLoader::GetTensor(const std::string& name) {
    // Stub: Return pointer to start of file (risky but compiles) + offset
    // Real impl finds tensor in map
    return (float*)mapped_data; 
}

RawrXDModelLoader::~RawrXDModelLoader() {
    if (mapped_data) UnmapViewOfFile(mapped_data);
}
