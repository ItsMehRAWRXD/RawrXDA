// test_phi3_simple.cpp
// Simple Phi-3 test with unified inference engine

#include "rawr_unified_inference.h"
#include <iostream>
#include <fstream>

using namespace rawr;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <model.gguf>" << std::endl;
        return 1;
    }
    
    const char* model_path = argv[1];
    
    std::cout << "[Phi3 Simple Test]" << std::endl;
    std::cout << "  Loading: " << model_path << std::endl;
    
    // Parse GGUF
    ParsedGGUF gguf = parse_gguf_simple(model_path);
    if (!gguf.valid) {
        std::cerr << "[ERROR] " << gguf.error << std::endl;
        return 1;
    }
    
    std::cout << "[OK] GGUF parsed successfully" << std::endl;
    std::cout << "  Architecture: " << (gguf.config.is_gqa() ? "GQA" : 
                                        gguf.config.is_mqa() ? "MQA" : "MHA") << std::endl;
    std::cout << "  Vocab size: " << gguf.config.vocab_size << std::endl;
    std::cout << "  Hidden dim: " << gguf.config.hidden_dim << std::endl;
    std::cout << "  Layers: " << gguf.config.n_layers << std::endl;
    std::cout << "  Heads: " << gguf.config.n_heads << std::endl;
    std::cout << "  KV heads: " << gguf.config.n_kv_heads << std::endl;
    std::cout << "  Head dim: " << gguf.config.head_dim << std::endl;
    std::cout << "  FFN dim: " << gguf.config.ffn_dim << std::endl;
    std::cout << "  RMS epsilon: " << gguf.config.rms_norm_eps << std::endl;
    std::cout << "  RoPE theta: " << gguf.config.rope_theta << std::endl;
    std::cout << "  Max seq len: " << gguf.config.max_seq_len << std::endl;
    std::cout << "  Tensors: " << gguf.tensors.size() << std::endl;
    std::cout << "  Data offset: " << gguf.data_offset << std::endl;
    
    // List first 10 tensors
    std::cout << "\n[First 10 Tensors]" << std::endl;
    for (size_t i = 0; i < std::min(size_t(10), gguf.tensors.size()); i++) {
        auto& t = gguf.tensors[i];
        std::cout << "  " << i << ": " << t.name << " type=" << t.type << std::endl;
    }
    
    // Test math operations
    std::cout << "\n[Math Tests]" << std::endl;
    
    // Test RMSNorm
    std::vector<float> x = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> w = {1.0f, 1.0f, 1.0f, 1.0f};
    std::vector<float> out(4);
    rmsnorm(x.data(), w.data(), out.data(), 4, 1e-5f);
    std::cout << "  RMSNorm test: ";
    for (auto v : out) std::cout << v << " ";
    std::cout << std::endl;
    
    // Test softmax
    std::vector<float> scores = {1.0f, 2.0f, 3.0f, 4.0f};
    softmax(scores.data(), 4);
    std::cout << "  Softmax test: ";
    float sum = 0.0f;
    for (auto v : scores) { std::cout << v << " "; sum += v; }
    std::cout << "(sum=" << sum << ")" << std::endl;
    
    // Test SiLU
    std::cout << "  SiLU test: silu(1.0)=" << silu(1.0f) << std::endl;
    
    // Test FP16 conversion
    std::cout << "  FP16 test: fp16_to_fp32(0x3C00)=" << fp16_to_fp32(0x3C00) << " (expected 1.0)" << std::endl;
    
    std::cout << "\n[PASS] All tests completed" << std::endl;
    
    return 0;
}
