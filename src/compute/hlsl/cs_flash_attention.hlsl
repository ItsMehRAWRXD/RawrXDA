// ==============================================================================
// RawrXD Compute - Flash Attention Kernel (Forward Pass)
// Module: cs_flash_attention.hlsl
// Version: v14.7.0-ATTN
// Architecture: Direct3D 12 Compute Shader (SM 6.0)
// ==============================================================================

#define THREAD_GROUP_SIZE 256
#define BLOCK_SIZE_M 64
#define BLOCK_SIZE_N 64

cbuffer AttentionParams : register(b0)
{
    uint seq_len;
    uint head_dim;
    uint num_heads;
    uint batch_size;
    float scale;
};

// Q, K, V are implicitly structured as [batch_size, num_heads, seq_len, head_dim]
StructuredBuffer<float> Q : register(t0);
StructuredBuffer<float> K : register(t1);
StructuredBuffer<float> V : register(t2);

// Output shape: [batch_size, num_heads, seq_len, head_dim]
RWStructuredBuffer<float> O : register(u0);

groupshared float s_Q[BLOCK_SIZE_M * 128]; // Max head_dim 128
groupshared float s_K[BLOCK_SIZE_N * 128];
groupshared float s_V[BLOCK_SIZE_N * 128];
groupshared float s_S[BLOCK_SIZE_M * BLOCK_SIZE_N];

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 btid : SV_GroupID)
{
    uint batch_idx = btid.z;
    uint head_idx = btid.y;
    uint block_start_m = btid.x * BLOCK_SIZE_M;
    
    uint tid = gtid.x;
    
    // Calculate global offsets
    uint head_offset = (batch_idx * num_heads + head_idx) * seq_len * head_dim;
    
    // Per-thread state for softmax
    float m_i = -1e20f;
    float l_i = 0.0f;
    
    float acc_O[128]; // Local accumulator for output
    for(uint d = 0; d < head_dim; ++d) {
        acc_O[d] = 0.0f;
    }
    
    // Iterate over sequence length in chunks of BLOCK_SIZE_N
    for (uint block_start_n = 0; block_start_n < seq_len; block_start_n += BLOCK_SIZE_N) 
    {
        // -------------------------------------------------------------
        // 1. Cooperative Load: Q, K, V -> GroupShared
        // -------------------------------------------------------------
        uint end_m = min(block_start_m + BLOCK_SIZE_M, seq_len);
        uint end_n = min(block_start_n + BLOCK_SIZE_N, seq_len);
        
        // Load Q mapping threads to BLOCK_SIZE_M
        if (tid < BLOCK_SIZE_M * head_dim) {
            uint r = tid / head_dim;
            uint c = tid % head_dim;
            if (block_start_m + r < seq_len) {
                s_Q[r * head_dim + c] = Q[head_offset + (block_start_m + r) * head_dim + c];
            } else {
                s_Q[r * head_dim + c] = 0.0f;
            }
        }
        
        // Load K, V 
        if (tid < BLOCK_SIZE_N * head_dim) {
            uint r = tid / head_dim;
            uint c = tid % head_dim;
            if (block_start_n + r < seq_len) {
                s_K[r * head_dim + c] = K[head_offset + (block_start_n + r) * head_dim + c];
                s_V[r * head_dim + c] = V[head_offset + (block_start_n + r) * head_dim + c];
            } else {
                s_K[r * head_dim + c] = 0.0f;
                s_V[r * head_dim + c] = 0.0f;
            }
        }
        
        GroupMemoryBarrierWithGroupSync();
        
        // -------------------------------------------------------------
        // 2. Compute Q * K^T (Scores)
        // -------------------------------------------------------------
        if (tid < BLOCK_SIZE_M * BLOCK_SIZE_N) {
            uint r = tid / BLOCK_SIZE_N;
            uint c = tid % BLOCK_SIZE_N;
            
            float score = 0.0f;
            for(uint d = 0; d < head_dim; ++d) {
                score += s_Q[r * head_dim + d] * s_K[c * head_dim + d];
            }
            
            // Causal mask (optional based on application)
            if (block_start_m + r < block_start_n + c) {
                score = -1e20f;
            }
            
            s_S[r * BLOCK_SIZE_N + c] = score * scale;
        }
        
        GroupMemoryBarrierWithGroupSync();
        
        // -------------------------------------------------------------
        // 3. Online Softmax & Scale Output
        // -------------------------------------------------------------
        if (tid < BLOCK_SIZE_M) {
            uint r = tid;
            
            // Compute max
            float m_ij = -1e20f;
            for (uint c = 0; c < BLOCK_SIZE_N; ++c) {
                if (block_start_n + c < seq_len) {
                    m_ij = max(m_ij, s_S[r * BLOCK_SIZE_N + c]);
                }
            }
            
            float m_new = max(m_i, m_ij);
            
            // Compute sum of exponentials
            float l_ij = 0.0f;
            for (uint c = 0; c < BLOCK_SIZE_N; ++c) {
                if (block_start_n + c < seq_len) {
                    float p = exp(s_S[r * BLOCK_SIZE_N + c] - m_new);
                    s_S[r * BLOCK_SIZE_N + c] = p; // Store exp temporarily
                    l_ij += p;
                } else {
                    s_S[r * BLOCK_SIZE_N + c] = 0.0f;
                }
            }
            
            float l_new = exp(m_i - m_new) * l_i + l_ij;
            
            // Scale existing O
            float o_scale = exp(m_i - m_new);
            for (uint d = 0; d < head_dim; ++d) {
                acc_O[d] = acc_O[d] * o_scale;
                
                // Add new V contributions
                float v_sum = 0.0f;
                for (uint c = 0; c < BLOCK_SIZE_N; ++c) {
                    v_sum += s_S[r * BLOCK_SIZE_N + c] * s_V[c * head_dim + d];
                }
                acc_O[d] += v_sum;
            }
            
            m_i = m_new;
            l_i = l_new;
        }
        
        GroupMemoryBarrierWithGroupSync();
    }
    
    // -------------------------------------------------------------
    // 4. Final Write to Global Memory
    // -------------------------------------------------------------
    if (tid < BLOCK_SIZE_M) {
        uint r = tid;
        if (block_start_m + r < seq_len) {
            for (uint d = 0; d < head_dim; ++d) {
                O[head_offset + (block_start_m + r) * head_dim + d] = acc_O[d] / l_i;
            }
        }
    }
}
