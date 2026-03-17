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
    
    // SiLU (Swish): x * sigmoid(x)
    float x = input_buf.data[idx];
    float sigmoid = 1.0 / (1.0 + exp(-x));
    float output_val = x * sigmoid;
    
    output_buf.data[idx] = output_val;
}
