/*
====================================================================
 GGUF Loader Test - Memory Mapped Version
 Tests the strict GGUF loader against actual model files using mmap
====================================================================
*/

#include "gguf_loader_strict.h"
#include <windows.h>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const char* filename = (argc > 1) ? argv[1] : "d:/codestral22b.gguf";
    
    std::cerr << "[TEST] Loading: " << filename << std::endl;
    
    // Open file
    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "[ERROR] Failed to open: " << filename << " (error=" << GetLastError() << ")" << std::endl;
        return 1;
    }
    
    // Get file size
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        std::cerr << "[ERROR] Failed to get file size" << std::endl;
        CloseHandle(hFile);
        return 1;
    }
    
    std::cerr << "[TEST] File size: " << fileSize.QuadPart << " bytes (" 
              << (fileSize.QuadPart / (1024.0*1024*1024)) << " GB)" << std::endl;
    
    // Create file mapping
    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) {
        std::cerr << "[ERROR] Failed to create file mapping (error=" << GetLastError() << ")" << std::endl;
        CloseHandle(hFile);
        return 1;
    }
    
    // Map view
    const uint8_t* data = (const uint8_t*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!data) {
        std::cerr << "[ERROR] Failed to map view (error=" << GetLastError() << ")" << std::endl;
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return 1;
    }
    
    std::cerr << "[TEST] Memory mapped successfully" << std::endl;
    
    // Parse GGUF
    rawr::GGUFLoader loader;
    bool ok = loader.load(data, fileSize.QuadPart);
    
    // Cleanup
    UnmapViewOfFile(data);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    
    if (!ok) {
        std::cerr << "[ERROR] GGUF parse failed" << std::endl;
        return 1;
    }
    
    // Print summary
    std::cerr << "\n[SUMMARY]" << std::endl;
    std::cerr << "  Tensors: " << loader.tensors.size() << std::endl;
    std::cerr << "  Vocab: " << loader.n_vocab << std::endl;
    std::cerr << "  Embed: " << loader.n_embd << std::endl;
    std::cerr << "  Heads: " << loader.n_head << std::endl;
    std::cerr << "  KV Heads: " << loader.n_head_kv << std::endl;
    std::cerr << "  Layers: " << loader.n_layer << std::endl;
    std::cerr << "  Arch: " << loader.get_arch() << std::endl;
    
    // List first 10 tensors
    std::cerr << "\n[First 10 Tensors]" << std::endl;
    for (size_t i = 0; i < (loader.tensors.size() < 10 ? loader.tensors.size() : 10); i++) {
        const auto& t = loader.tensors[i];
        std::cerr << "  [" << i << "] '" << t.name << "' ";
        for (auto d : t.dims) std::cerr << d << "x";
        std::cerr << " type=" << (int)t.type << std::endl;
    }
    
    // Check for key tensors
    std::cerr << "\n[Key Tensors]" << std::endl;
    const char* keys[] = {
        "token_embd.weight",
        "output.weight",
        "output_norm.weight",
        "blk.0.attn_norm.weight",
        "blk.0.attn_q.weight",
        "blk.0.attn_k.weight",
        "blk.0.attn_v.weight",
        "blk.0.attn_output.weight",
        "blk.0.ffn_norm.weight",
        "blk.0.ffn_gate.weight",
        "blk.0.ffn_up.weight",
        "blk.0.ffn_down.weight",
    };
    
    for (const char* key : keys) {
        auto* t = loader.get_tensor(key);
        if (t) {
            std::cerr << "  [OK] '" << key << "' ";
            for (auto d : t->dims) std::cerr << d << "x";
            std::cerr << std::endl;
        } else {
            std::cerr << "  [MISSING] '" << key << "'" << std::endl;
        }
    }
    
    std::cerr << "\n[TEST] PASSED" << std::endl;
    return 0;
}
