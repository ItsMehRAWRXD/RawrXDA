// cs_rope_fused.hlsl
// Fused RoPE: Applies position embeddings to Q and K in single dispatch

cbuffer RoPEParams : register(b0) {
    uint g_seq_len;
    uint g_head_dim;
    uint g_num_heads;
    float g_theta;
};

StructuredBuffer<float> g_Q : register(t0);      // [seq_len, num_heads, head_dim]
StructuredBuffer<float> g_K : register(t1);      // [seq_len, num_heads, head_dim]
// t2 unused
StructuredBuffer<float2> g_CosSin : register(t3); // [seq_len, head_dim/2] (cos, sin)

RWStructuredBuffer<float> g_Q_Out : register(u0); // In-place alias of Q
RWStructuredBuffer<float> g_K_Out : register(u1); // In-place alias of K

// Rotary position embedding application
void ApplyRoPE(uint global_idx, uint pos, uint head, uint dim_idx) {
    uint head_offset = (pos * g_num_heads + head) * g_head_dim;
    uint pair_idx = dim_idx / 2;
    // uint elem_in_pair = dim_idx % 2; // unused
    
    // Index into cos/sin table: [pos, pair_idx]
    uint rope_idx = pos * (g_head_dim / 2) + pair_idx;
    float2 cs = g_CosSin[rope_idx];
    float cos_val = cs.x;
    float sin_val = cs.y;
    
    // Get the pair (x0, x1) at positions [dim_idx, dim_idx + head_dim/2]
    uint idx0 = head_offset + pair_idx;
    uint idx1 = head_offset + pair_idx + g_head_dim / 2;
    
    float x0 = g_Q[idx0];
    float x1 = g_Q[idx1];
    
    // Apply rotation: [x0, x1] * [cos, -sin; sin, cos]
    g_Q_Out[idx0] = x0 * cos_val - x1 * sin_val;
    g_Q_Out[idx1] = x0 * sin_val + x1 * cos_val;
    
    // Repeat for K
    float k0 = g_K[idx0];
    float k1 = g_K[idx1];
    g_K_Out[idx0] = k0 * cos_val - k1 * sin_val;
    g_K_Out[idx1] = k0 * sin_val + k1 * cos_val;
}

[numthreads(64, 1, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID) {
    uint global_idx = tid.x;
    uint total_elements = g_seq_len * g_num_heads * (g_head_dim / 2);
    
    if (global_idx >= total_elements) return;
    
    // Decode 3D index from flat
    uint pairs_per_seq = g_num_heads * (g_head_dim / 2);
    uint pos = global_idx / pairs_per_seq;
    uint remainder = global_idx % pairs_per_seq;
    uint head = remainder / (g_head_dim / 2);
    uint pair = remainder % (g_head_dim / 2);
    
    // Apply to both elements of the pair
    ApplyRoPE(global_idx, pos, head, pair * 2);
}
