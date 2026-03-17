#pragma once
#include "common_types.h"
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>

class EngineGGUFLoader {
public:
    EngineGGUFLoader() = default;
    ~EngineGGUFLoader();

    bool load(const char* path);
    void unload();
    
    TensorInfo* getTensor(const char* name);
    
    // Dequantization helpers
    static void dequantize_q4_0(float* dst, const block_q4_0* src, int n);
    static void dequantize_q8_0(float* dst, const block_q8_0* src, int n);

    std::unordered_map<std::string, std::string> metadata;
    std::vector<TensorInfo> tensors;

private:
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMap = NULL;
    void* pMap = nullptr;
    size_t fileSize = 0;
    uint8_t* tensorData = nullptr;
    std::unordered_map<std::string, size_t> tensorMap;

    bool parse();
    std::string readString(uint8_t*& ptr);
    size_t ggml_nbytes(ggml_type type, size_t n);
};
