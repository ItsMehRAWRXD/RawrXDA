// ==============================================================================
// RawrXD Compute - Flash Attention Bridge & Parity
// Module: RawrXD_FlashAttention.h
// ==============================================================================
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#endif

namespace RawrXD {
namespace Compute {

struct AttentionParams {
    unsigned int seq_len;
    unsigned int head_dim;
    unsigned int num_heads;
    unsigned int batch_size;
    float scale;
};

class FlashAttentionBridge {
public:
    // CPU Parity implementation for validation and fallback
    static void ComputeCPUParity(
        const float* Q, const float* K, const float* V, float* O, 
        const AttentionParams& params, bool use_causal_mask = true) 
    {
        const size_t batch_size = params.batch_size;
        const size_t num_heads = params.num_heads;
        const size_t seq_len = params.seq_len;
        const size_t head_dim = params.head_dim;
        const float scale = params.scale;

        for (size_t b = 0; b < batch_size; ++b) {
            for (size_t h = 0; h < num_heads; ++h) {
                size_t head_offset = (b * num_heads + h) * seq_len * head_dim;
                
                for (size_t i = 0; i < seq_len; ++i) {
                    std::vector<float> scores(seq_len, -1e20f);
                    float m_i = -1e20f;
                    
                    // Compute Q * K^T
                    for (size_t j = 0; j < seq_len; ++j) {
                        if (use_causal_mask && i < j) continue; // Causal mask
                        
                        float score = 0.0f;
                        for (size_t d = 0; d < head_dim; ++d) {
                            score += Q[head_offset + i * head_dim + d] * 
                                     K[head_offset + j * head_dim + d];
                        }
                        score *= scale;
                        scores[j] = score;
                        m_i = std::max(m_i, score);
                    }
                    
                    // Softmax
                    float l_i = 0.0f;
                    for (size_t j = 0; j < seq_len; ++j) {
                        if (use_causal_mask && i < j) continue;
                        scores[j] = std::exp(scores[j] - m_i);
                        l_i += scores[j];
                    }
                    
                    // Compute output (Scores * V)
                    for (size_t d = 0; d < head_dim; ++d) {
                        float out_val = 0.0f;
                        for (size_t j = 0; j < seq_len; ++j) {
                            if (use_causal_mask && i < j) continue;
                            out_val += scores[j] * V[head_offset + j * head_dim + d];
                        }
                        O[head_offset + i * head_dim + d] = out_val / l_i;
                    }
                }
            }
        }
    }

    // Implementation for D3D12 Dispatch
    static void DispatchHLSL(
        void* cmd_list_ptr,
        void* root_signature_ptr,
        void* pipeline_state_ptr,
        uint64_t cbv_address,
        uint64_t q_srv, uint64_t k_srv, uint64_t v_srv, uint64_t o_uav,
        const AttentionParams& params) 
    {
#ifdef _WIN32
        ID3D12GraphicsCommandList* cmdList = static_cast<ID3D12GraphicsCommandList*>(cmd_list_ptr);
        ID3D12RootSignature* rootSig = static_cast<ID3D12RootSignature*>(root_signature_ptr);
        ID3D12PipelineState* pso = static_cast<ID3D12PipelineState*>(pipeline_state_ptr);

        if(!cmdList || !rootSig || !pso) {
            std::cerr << "[-] DispatchHLSL: Invalid D3D12 Context Pointers provided." << std::endl;
            return;
        }

        cmdList->SetComputeRootSignature(rootSig);
        cmdList->SetPipelineState(pso);
        
        // Match Root Signature layout expected by CS:
        // [0] Constant Buffer (AttentionParams)
        // [1] Q (SRV t0)
        // [2] K (SRV t1)
        // [3] V (SRV t2)
        // [4] O (UAV u0)
        
        cmdList->SetComputeRootConstantBufferView(0, static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(cbv_address));
        cmdList->SetComputeRootShaderResourceView(1, static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(q_srv));
        cmdList->SetComputeRootShaderResourceView(2, static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(k_srv));
        cmdList->SetComputeRootShaderResourceView(3, static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(v_srv));
        cmdList->SetComputeRootUnorderedAccessView(4, static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(o_uav));
        
        // Default Dispatch sizing based on BLOCK_SIZE_M (64)
        unsigned int grid_x = (params.seq_len + 63) / 64; 
        unsigned int grid_y = params.num_heads;
        unsigned int grid_z = params.batch_size;
        
        cmdList->Dispatch(grid_x, grid_y, grid_z);
        std::cout << "[+] Dispached HLSL Flash Attention -> ThreadGroups: (" 
                  << grid_x << ", " << grid_y << ", " << grid_z << ")\n";
#else
        std::cerr << "[-] D3D12 Dispatch is not supported on this platform." << std::endl;
#endif
    }
};

} // namespace Compute
} // namespace RawrXD
