/*
====================================================================
 GGUF Loader Test - Standalone Validation
 Tests the strict GGUF loader against actual model files
====================================================================
*/

#include "gguf_loader_strict.h"
#include <fstream>
#include <vector>
#include <iostream>

int main(int argc, char** argv) {
    const char* filename = (argc > 1) ? argv[1] : "d:/codestral22b.gguf";
    
    std::cerr << "[TEST] Loading: " << filename << std::endl;
    
    // Read file
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "[ERROR] Failed to open: " << filename << std::endl;
        return 1;
    }
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::cerr << "[TEST] Allocating " << size << " bytes..." << std::endl;
    std::vector<uint8_t> data;
    try {
        data.resize(size);
    } catch (const std::bad_alloc& e) {
        std::cerr << "[ERROR] Failed to allocate " << size << " bytes: " << e.what() << std::endl;
        return 1;
    }
    
    std::cerr << "[TEST] Reading file..." << std::endl;
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "[ERROR] Failed to read file: " << strerror(errno) << std::endl;
        return 1;
    }
    
    std::cerr << "[TEST] File size: " << size << " bytes" << std::endl;
    
    // Parse GGUF
    rawr::GGUFLoader loader;
    if (!loader.load(data.data(), size)) {
        std::cerr << "[ERROR] GGUF parse failed" << std::endl;
        return 1;
    }
    
    // Print summary
    std::cerr << "\n[SUMMARY]" << std::endl;
    std::cerr << "  Tensors: " << loader.tensors.size() << std::endl;
    std::cerr << "  Vocab: " << loader.n_vocab << std::endl;
    std::cerr << "  Embed: " << loader.n_embd << std::endl;
    std::cerr << "  Heads: " << loader.n_head << std::endl;
    std::cerr << "  Layers: " << loader.n_layer << std::endl;
    
    // List first 10 tensors
    std::cerr << "\n[First 10 Tensors]" << std::endl;
    for (size_t i = 0; i < std::min(size_t(10), loader.tensors.size()); i++) {
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
    
    // Sample some tensor data
    std::cerr << "\n[Data Samples]" << std::endl;
    auto* embd = loader.get_tensor_f32("token_embd.weight");
    if (embd) {
        std::cerr << "  token_embd.weight[0:5]: ";
        for (int i = 0; i < 5; i++) std::cerr << embd[i] << " ";
        std::cerr << std::endl;
    }
    
    auto* norm = loader.get_tensor_f32("output_norm.weight");
    if (norm) {
        std::cerr << "  output_norm.weight[0:5]: ";
        for (int i = 0; i < 5; i++) std::cerr << norm[i] << " ";
        std::cerr << std::endl;
    }
    
    std::cerr << "\n[TEST] PASSED" << std::endl;
    return 0;
}
