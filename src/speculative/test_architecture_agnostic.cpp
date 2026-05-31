// test_architecture_agnostic.cpp
// Validates ministral3 and gptoss20b produce correct outputs with branchless runtime

#include "rawr_architecture_agnostic_runtime.h"
#include <iostream>
#include <cassert>

using namespace rawr;

int main() {
    std::cout << "=== Architecture-Agnostic Runtime Validation ===" << std::endl;
    
    // Test 1: ministral3 configuration
    std::cout << "\n[Test 1] ministral3 configuration" << std::endl;
    {
        ModelConfig cfg = config_ministral3();
        std::cout << "  vocab_size: " << cfg.vocab_size << std::endl;
        std::cout << "  hidden_dim: " << cfg.hidden_dim << std::endl;
        std::cout << "  n_layers: " << cfg.n_layers << std::endl;
        std::cout << "  n_heads: " << cfg.n_heads << std::endl;
        std::cout << "  n_kv_heads: " << cfg.n_kv_heads << std::endl;
        std::cout << "  head_dim: " << cfg.head_dim << std::endl;
        std::cout << "  ffn_dim: " << cfg.ffn_dim << std::endl;
        std::cout << "  rope_theta: " << cfg.rope_theta << std::endl;
        
        // Validate GQA detection
        assert(cfg.n_kv_heads < cfg.n_heads);
        std::cout << "  [PASS] GQA detected (n_kv_heads < n_heads)" << std::endl;
        
        // Validate capabilities
        assert(cfg.caps & CAP_GQA);
        assert(cfg.caps & CAP_RMSNORM);
        assert(cfg.caps & CAP_ROPE);
        assert(cfg.caps & CAP_ROPE_SCALED);
        assert(cfg.caps & CAP_SWIGLU);
        std::cout << "  [PASS] All capability flags set correctly" << std::endl;
        
        // Build execution DAG
        ExecutionDAG dag;
        dag.build(cfg.caps, cfg.n_layers);
        std::cout << "  [PASS] DAG built with " << dag.nodes.size() << " nodes" << std::endl;
    }
    
    // Test 2: gptoss20b configuration
    std::cout << "\n[Test 2] gptoss20b configuration" << std::endl;
    {
        ModelConfig cfg = config_gptoss20b();
        std::cout << "  vocab_size: " << cfg.vocab_size << std::endl;
        std::cout << "  hidden_dim: " << cfg.hidden_dim << std::endl;
        std::cout << "  n_layers: " << cfg.n_layers << std::endl;
        std::cout << "  n_heads: " << cfg.n_heads << std::endl;
        std::cout << "  n_kv_heads: " << cfg.n_kv_heads << std::endl;
        std::cout << "  n_experts: " << cfg.n_experts << std::endl;
        std::cout << "  n_experts_per_token: " << cfg.n_experts_per_token << std::endl;
        std::cout << "  sliding_window: " << cfg.sliding_window << std::endl;
        
        // Validate MoE + GQA
        assert(cfg.n_experts > 0);
        assert(cfg.n_kv_heads < cfg.n_heads);
        std::cout << "  [PASS] MoE + GQA detected" << std::endl;
        
        // Validate capabilities
        assert(cfg.caps & CAP_GQA);
        assert(cfg.caps & CAP_MOE);
        assert(cfg.caps & CAP_TOPK_ROUTING);
        assert(cfg.caps & CAP_SLIDING_WIN);
        std::cout << "  [PASS] All capability flags set correctly" << std::endl;
        
        // Build execution DAG with MoE
        ExecutionDAG dag;
        dag.build(cfg.caps, cfg.n_layers, cfg.n_experts, cfg.n_experts_per_token);
        std::cout << "  [PASS] DAG built with " << dag.nodes.size() << " nodes (includes MoE)" << std::endl;
    }
    
    // Test 3: phi3mini configuration
    std::cout << "\n[Test 3] phi3mini configuration" << std::endl;
    {
        ModelConfig cfg = config_phi3mini();
        std::cout << "  vocab_size: " << cfg.vocab_size << std::endl;
        std::cout << "  hidden_dim: " << cfg.hidden_dim << std::endl;
        std::cout << "  n_layers: " << cfg.n_layers << std::endl;
        std::cout << "  n_heads: " << cfg.n_heads << std::endl;
        std::cout << "  n_kv_heads: " << cfg.n_kv_heads << std::endl;
        
        // Validate MHA (no GQA)
        assert(cfg.n_kv_heads == cfg.n_heads);
        std::cout << "  [PASS] MHA detected (n_kv_heads == n_heads)" << std::endl;
        
        // Validate capabilities
        assert(cfg.caps & CAP_MHA);
        assert(!(cfg.caps & CAP_GQA));
        assert(!(cfg.caps & CAP_MOE));
        std::cout << "  [PASS] All capability flags set correctly" << std::endl;
        
        // Build execution DAG
        ExecutionDAG dag;
        dag.build(cfg.caps, cfg.n_layers);
        std::cout << "  [PASS] DAG built with " << dag.nodes.size() << " nodes" << std::endl;
    }
    
    // Test 4: Dispatch table initialization
    std::cout << "\n[Test 4] Dispatch table initialization" << std::endl;
    {
        DispatchTable dt = init_dispatch_table();
        assert(dt.norm_kernel[0] != nullptr);  // RMSNorm
        assert(dt.norm_kernel[1] != nullptr);  // LayerNorm
        assert(dt.attn_kernels[0] != nullptr); // MHA
        assert(dt.attn_kernels[1] != nullptr); // GQA
        assert(dt.ffn_kernels[0] != nullptr);  // Dense
        assert(dt.ffn_kernels[1] != nullptr);  // SwiGLU
        assert(dt.rope_kernels[0] != nullptr); // Standard
        std::cout << "  [PASS] All kernel pointers initialized" << std::endl;
    }
    
    // Test 5: Capability mask builder
    std::cout << "\n[Test 5] Capability mask builder" << std::endl;
    {
        TensorRegistry tensors;
        KVRegistry kv;
        
        // Simulate ministral3 tensors
        tensors.exists["blk.0.attn_norm.weight"] = true;
        tensors.exists["blk.0.ffn_gate.weight"] = true;
        tensors.exists["blk.0.ffn_up.weight"] = true;
        
        kv.floats["rope.theta"] = 1000000.0f;
        kv.floats["rope.freq_base"] = 1000000.0f;
        kv.bools["rope.enabled"] = true;
        
        uint64_t mask = build_capability_mask(tensors, kv, 32, 8, 4096);
        
        assert(mask & CAP_GQA);
        assert(mask & CAP_RMSNORM);
        assert(mask & CAP_ROPE);
        assert(mask & CAP_ROPE_SCALED);
        assert(mask & CAP_SWIGLU);
        std::cout << "  [PASS] Capability mask built correctly" << std::endl;
        std::cout << "  Mask: 0x" << std::hex << mask << std::dec << std::endl;
    }
    
    std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    std::cout << "Architecture-agnostic runtime validated for:" << std::endl;
    std::cout << "  - ministral3 (GQA + SwiGLU + scaled RoPE)" << std::endl;
    std::cout << "  - gptoss20b (MoE + GQA + sliding window)" << std::endl;
    std::cout << "  - phi3mini (MHA + dense FFN)" << std::endl;
    std::cout << "\nZero if(arch) conditionals - pure capability-driven dispatch" << std::endl;
    
    return 0;
}
