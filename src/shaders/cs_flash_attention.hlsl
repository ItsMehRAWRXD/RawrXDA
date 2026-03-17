// Flash Attention v2 Component - D3D12 Compute Shader
// Block dimensions: 32x32, Head Dimension: 64 to fit in 32KB cs_5_1 shared memory limit
#define Br 32
#define Bc 32
#define d_dim 64

ByteAddressBuffer BufferQ : register(t0); // [num_heads, N, d]
ByteAddressBuffer BufferK : register(t1); // [num_heads, N, d]
ByteAddressBuffer BufferV : register(t2); // [num_heads, N, d]
RWByteAddressBuffer BufferO : register(u0); // [num_heads, N, d]

cbuffer FAConstants : register(b0)
{
    uint N;           // Sequence Length
    uint d;           // Head dimension (assumed 64)
    uint num_heads;   // Number of Heads
    float scale;      // Usually 1.0f / sqrt(d)
};

// 8KB per block, 3x blocks = 24KB < LDS Limit (32KB)
groupshared float Q_shared[Br][d_dim];
groupshared float K_shared[Bc][d_dim];
groupshared float V_shared[Bc][d_dim];

[numthreads(Br, 1, 1)]
void main(uint3 gid : SV_GroupID, uint3 tid : SV_DispatchThreadID, uint3 lid : SV_GroupThreadID)
{
    uint block_idx = gid.x;
    uint head_idx = gid.y;
    
    // Each thread in the group processes one query row within this Br block
    uint thread_row = lid.x; 
    uint global_row = block_idx * Br + thread_row;
    
    uint head_offset = head_idx * N * d;
    
    // Load Q block into shared memory
    if (global_row < N) {
        for (uint i = 0; i < d_dim; i++) {
            Q_shared[thread_row][i] = asfloat(BufferQ.Load((head_offset + global_row * d + i) * 4));
        }
    } else {
        for (uint i = 0; i < d_dim; i++) {
            Q_shared[thread_row][i] = 0.0f;
        }
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    // Running statistics for Flash Attention
    float m = -1e30f;
    float l = 0.0f;
    float acc[d_dim];
    
    [unroll]
    for (uint a_idx = 0; a_idx < d_dim; a_idx++) {
        acc[a_idx] = 0.0f;
    }
    
    uint num_kv_blocks = (N + Bc - 1) / Bc;
    
    for (uint kv_b = 0; kv_b < num_kv_blocks; kv_b++) {
        // Threads collaboratively load K and V blocks
        uint kv_global_row = kv_b * Bc + thread_row;
        if (kv_global_row < N) {
            for (uint i = 0; i < d_dim; i++) {
                K_shared[thread_row][i] = asfloat(BufferK.Load((head_offset + kv_global_row * d + i) * 4));
                V_shared[thread_row][i] = asfloat(BufferV.Load((head_offset + kv_global_row * d + i) * 4));
            }
        } else {
            for (uint i = 0; i < d_dim; i++) {
                K_shared[thread_row][i] = 0.0f;
                V_shared[thread_row][i] = 0.0f;
            }
        }
        
        GroupMemoryBarrierWithGroupSync();
        
        // Execute MatMul + Online Softmax + Value accumulation
        if (global_row < N) {
            float S[Bc];
            
            // 1. Compute S = QK^T tile
            for (uint s_idx = 0; s_idx < Bc; s_idx++) {
                float sum = 0.0f;
                if (kv_b * Bc + s_idx < N) {
                    for (uint k = 0; k < d_dim; k++) {
                        sum += Q_shared[thread_row][k] * K_shared[s_idx][k];
                    }
                    sum *= scale;
                } else {
                    sum = -1e30f; // Padding masking
                }
                S[s_idx] = sum;
            }
            
            // 2. Online softmax update
            float m_new = m;
            for (uint m_idx = 0; m_idx < Bc; m_idx++) {
                m_new = max(m_new, S[m_idx]);
            }
            
            float exp_diff = exp(m - m_new);
            float l_new = l * exp_diff;
            
            float P[Bc];
            for (uint p_idx = 0; p_idx < Bc; p_idx++) {
                float p_val = exp(S[p_idx] - m_new);
                if (kv_b * Bc + p_idx >= N) p_val = 0.0f; // Force mask pad
                P[p_idx] = p_val;
                l_new += p_val;
            }
            
            // 3. Update V Accumulator
            for (uint k = 0; k < d_dim; k++) {
                acc[k] *= exp_diff;
                for (uint v_j = 0; v_j < Bc; v_j++) {
                    acc[k] += P[v_j] * V_shared[v_j][k];
                }
            }
            
            m = m_new;
            l = l_new;
        }
        
        GroupMemoryBarrierWithGroupSync();
    }
    
    // Store final normalized outputs
    if (global_row < N) {
        for (uint k = 0; k < d_dim; k++) {
            float out_val = acc[k] / l;
            BufferO.Store((head_offset + global_row * d + k) * 4, asuint(out_val));
        }
    }
}
