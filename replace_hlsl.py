import re

with open(r"d:\rawrxd\src\llm_compute.hlsl", "r", encoding="utf-8") as f:
    text = f.read()

# Add a declaration for RoPE CosSin buffer and CSRoPE_Fused 
rope_code = '''
// ── CSRoPE_Fused ──────────────────────────────────────────────────────────────────
// Fused RoPE application for Q and K buffers simultaneously.
// g_inout : Q buffer (RWStructuredBuffer)
// g_out   : K buffer (RWStructuredBuffer, using as UAV)
// g_vec   : RoPE Cos/Sin tables (float2 pairs encoded as floats? Actually we can just use StructuredBuffer<float2>)
// Wait, g_vec is float. Let's use StructuredBuffer<float> and read 2 at a time.
//   cbPosition  = current sequence pos
//   cbCols      = head_dim (must be even)
//   cbDim       = num_heads

StructuredBuffer<float2> g_rope_cossin : register(t3); // float2 table

[numthreads(64, 1, 1)]
void CSRoPE_Fused(uint3 id : SV_DispatchThreadID) {
    // id.x is the index of the pair to process:
    // pair_idx = 0 .. (num_heads * head_dim / 2) - 1
    
    const uint total_pairs = cbDim * (cbCols / 2);
    if (id.x >= total_pairs) return;
    
    const uint head_idx = id.x / (cbCols / 2);
    const uint pair_within_head = id.x % (cbCols / 2);
    
    // Original indices in the 1D float array
    const uint idx1 = head_idx * cbCols + pair_within_head;
    const uint idx2 = idx1 + (cbCols / 2);
    
    // RoPE cos/sin table index:
    // Assuming table is laid out as [seq_pos][head_dim/2]
    const uint rope_idx = cbPosition * (cbCols / 2u) + pair_within_head;
    
    float2 cos_sin = g_rope_cossin[rope_idx];
    float cos_val = cos_sin.x;
    float sin_val = cos_sin.y;
    
    // Apply to Q (g_inout)
    float q1 = g_inout[idx1];
    float q2 = g_inout[idx2];
    g_inout[idx1] = q1 * cos_val - q2 * sin_val;
    g_inout[idx2] = q1 * sin_val + q2 * cos_val;
    
    // Apply to K (g_out)
    // Wait, K might have different number of heads (GQA)? 
    // Assuming MHA for now where Q and K have same shape (cbDim heads).
    float k1 = g_out[idx1];
    float k2 = g_out[idx2];
    g_out[idx1] = k1 * cos_val - k2 * sin_val;
    g_out[idx2] = k1 * sin_val + k2 * cos_val;
}
'''
if "CSRoPE_Fused" not in text:
    text += "\n" + rope_code

with open(r"d:\rawrxd\src\llm_compute.hlsl", "w", encoding="utf-8") as f:
    f.write(text)
print("Added CSRoPE_Fused!")
