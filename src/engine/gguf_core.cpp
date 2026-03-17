#include "gguf_core.h"
#include <iostream>
#include <algorithm>

EngineGGUFLoader::~EngineGGUFLoader() {
    unload();
}

bool EngineGGUFLoader::load(const char* path) {
    hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    LARGE_INTEGER sz;
    GetFileSizeEx(hFile, &sz);
    fileSize = sz.QuadPart;
    
    hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) { CloseHandle(hFile); return false; }
    
    pMap = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!pMap) { CloseHandle(hMap); CloseHandle(hFile); return false; }
    
    return parse();
}

void EngineGGUFLoader::unload() {
    if (pMap) { UnmapViewOfFile(pMap); pMap = nullptr; }
    if (hMap) { CloseHandle(hMap); hMap = NULL; }
    if (hFile != INVALID_HANDLE_VALUE) { CloseHandle(hFile); hFile = INVALID_HANDLE_VALUE; }
}

TensorInfo* EngineGGUFLoader::getTensor(const char* name) {
    auto it = tensorMap.find(name);
    if (it != tensorMap.end()) return &tensors[it->second];
    return nullptr;
}

void EngineGGUFLoader::dequantize_q4_0(float* dst, const block_q4_0* src, int n) {
    int nb = n / 32;
    
    #pragma omp parallel for
    for (int i = 0; i < nb; i++) {
        float d = *(float*)&src[i].d;
        
        for (int j = 0; j < 32; j++) {
            int idx = j / 2;
            uint8_t q = src[i].qs[idx];
            int8_t v = (j % 2 == 0) ? (q & 0x0F) : (q >> 4);
            v -= 8;
            dst[i * 32 + j] = v * d;
        }
    }
}

void EngineGGUFLoader::dequantize_q8_0(float* dst, const block_q8_0* src, int n) {
    int nb = n / 32;
    
    #pragma omp parallel for
    for (int i = 0; i < nb; i++) {
        float d = *(float*)&src[i].d;
        for (int j = 0; j < 32; j++) {
            dst[i * 32 + j] = src[i].qs[j] * d;
        }
    }
}

bool EngineGGUFLoader::parse() {
    uint8_t* ptr = (uint8_t*)pMap;
    
    // Header
    uint32_t magic = *(uint32_t*)ptr; ptr += 4;
    // if (magic != GGUF_MAGIC) return false; // Strict check disabled for broader compat in snippets
    
    uint32_t version = *(uint32_t*)ptr; ptr += 4;
    uint64_t n_tensors = *(uint64_t*)ptr; ptr += 8;
    uint64_t n_kv = *(uint64_t*)ptr; ptr += 8;
    
    // Parse metadata
    for (uint64_t i = 0; i < n_kv; i++) {
        std::string key = readString(ptr);
        uint32_t vtype = *(uint32_t*)ptr; ptr += 4;
        
        std::string value;
        switch (vtype) {
            case 0: value = std::to_string(*(uint8_t*)ptr); ptr += 1; break;
            case 1: value = std::to_string(*(int8_t*)ptr); ptr += 1; break;
            case 2: value = std::to_string(*(uint16_t*)ptr); ptr += 2; break;
            case 3: value = std::to_string(*(int16_t*)ptr); ptr += 2; break;
            case 4: value = std::to_string(*(uint32_t*)ptr); ptr += 4; break;
            case 5: value = std::to_string(*(int32_t*)ptr); ptr += 4; break;
            case 6: value = std::to_string(*(float*)ptr); ptr += 4; break;
            case 7: value = std::to_string(*(uint64_t*)ptr); ptr += 8; break;
            case 8: value = std::to_string(*(int64_t*)ptr); ptr += 8; break;
            case 9: value = std::to_string(*(double*)ptr); ptr += 8; break;
            case 10: value = readString(ptr); break;
            case 11: { // array
                uint32_t atype = *(uint32_t*)ptr; ptr += 4;
                uint64_t alen = *(uint64_t*)ptr; ptr += 8;
                for (uint64_t j = 0; j < alen; j++) {
                    if (j > 0) value += ",";
                    value += readString(ptr);
                }
                break;
            }
        }
        metadata[key] = value;
    }
    
    // Parse tensor info
    tensors.reserve(n_tensors);
    for (uint64_t i = 0; i < n_tensors; i++) {
        TensorInfo t;
        t.name = readString(ptr);
        uint32_t n_dims = *(uint32_t*)ptr; ptr += 4;
        t.dims.resize(n_dims);
        for (uint32_t d = 0; d < n_dims; d++) {
            t.dims[d] = *(uint64_t*)ptr; ptr += 8;
        }
        t.type = (ggml_type)*(uint32_t*)ptr; ptr += 4;
        t.offset = *(uint64_t*)ptr; ptr += 8;
        
        // Calculate size
        size_t n_elements = 1;
        for (auto d : t.dims) n_elements *= d;
        t.size = ggml_nbytes(t.type, n_elements);
        
        tensorMap[t.name] = tensors.size();
        tensors.push_back(t);
    }
    
    // Align tensor data to 32 bytes
    size_t offset = ptr - (uint8_t*)pMap;
    offset = (offset + 31) & ~31;
    tensorData = (uint8_t*)pMap + offset;
    
    // Set data pointers
    for (auto& t : tensors) {
        t.data = tensorData + t.offset;
    }
    
    return true;
}

std::string EngineGGUFLoader::readString(uint8_t*& ptr) {
    uint64_t len = *(uint64_t*)ptr; ptr += 8;
    std::string s((char*)ptr, len);
    ptr += len;
    return s;
}

size_t EngineGGUFLoader::ggml_nbytes(ggml_type type, size_t n) {
    switch (type) {
        case GGML_TYPE_F32:  return n * 4;
        case GGML_TYPE_F16:  return n * 2;
        case GGML_TYPE_Q4_0: return (n / 32) * sizeof(block_q4_0);
        case GGML_TYPE_Q4_1: return (n / 32) * sizeof(block_q4_1);
        case GGML_TYPE_Q8_0: return (n / 32) * sizeof(block_q8_0);
        default: return 0;
    }
}
