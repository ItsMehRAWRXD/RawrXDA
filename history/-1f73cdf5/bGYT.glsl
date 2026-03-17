#version 450

layout(local_size_x = 32) in;

layout(set = 0, binding = 0) readonly buffer Input {
    float data[];
} input_buf;

layout(set = 0, binding = 1) writeonly buffer Output {
    float data[];
} output_buf;

uniform uint size;
uniform float epsilon;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    
    if (idx >= size) return;
    
    // RMSNorm: y = x / RMS(x) * scale
    // RMS(x) = sqrt(mean(x^2) + eps)
    
    float sum_sq = 0.0;
    for (uint i = 0; i < size; ++i) {
        float val = input_buf.data[i];
        sum_sq += val * val;
    }
    
    float rms = sqrt(sum_sq / float(size) + epsilon);
    float output_val = input_buf.data[idx] / rms;
    
    output_buf.data[idx] = output_val;
}
