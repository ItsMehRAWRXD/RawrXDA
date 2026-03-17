#version 450

layout(local_size_x = 32) in;

layout(set = 0, binding = 0) readonly buffer Input {
    float data[];
} input_buf;

layout(set = 0, binding = 1) writeonly buffer Output {
    float data[];
} output_buf;

uniform uint size;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    
    if (idx >= size) return;
    
    // Softmax: e^x / sum(e^x)
    float val = input_buf.data[idx];
    float exp_val = exp(val);
    
    float sum_exp = 0.0;
    for (uint i = 0; i < size; ++i) {
        sum_exp += exp(input_buf.data[i]);
    }
    
    output_buf.data[idx] = exp_val / sum_exp;
}
