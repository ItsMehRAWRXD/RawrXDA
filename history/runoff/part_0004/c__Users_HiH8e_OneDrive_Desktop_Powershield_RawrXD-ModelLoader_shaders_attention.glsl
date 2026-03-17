#version 450

layout(local_size_x = 16, local_size_y = 16) in;

layout(set = 0, binding = 0) readonly buffer Queries {
    float data[];
} queries;

layout(set = 0, binding = 1) readonly buffer Keys {
    float data[];
} keys;

layout(set = 0, binding = 2) readonly buffer Values {
    float data[];
} values;

layout(set = 0, binding = 3) writeonly buffer Output {
    float data[];
} output_buf;

uniform uint seq_len;
uniform uint head_dim;
uniform float scale;

void main() {
    uint q_pos = gl_GlobalInvocationID.y;
    uint feat = gl_GlobalInvocationID.x;
    
    if (q_pos >= seq_len || feat >= head_dim) return;
    
    // Compute attention scores: Q * K^T / sqrt(d_k)
    float score = 0.0;
    for (uint k_pos = 0; k_pos < seq_len; ++k_pos) {
        float qk = 0.0;
        for (uint d = 0; d < head_dim; ++d) {
            qk += queries.data[q_pos * head_dim + d] * keys.data[k_pos * head_dim + d];
        }
        qk *= scale;
        
        // Softmax (simplified - full implementation would need two passes)
        float attn = exp(qk);
        
        // Weighted sum of values
        score += attn * values.data[k_pos * head_dim + feat];
    }
    
    output_buf.data[q_pos * head_dim + feat] = score;
}
