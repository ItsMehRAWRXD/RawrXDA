// Simple compute shader for testing Vulkan integration
#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0) buffer InputBuffer {
    float data[];
} input_buffer;

layout(binding = 1) buffer OutputBuffer {
    float data[];
} output_buffer;

void main() {
    uint index = gl_GlobalInvocationID.x;
    if (index < input_buffer.data.length()) {
        output_buffer.data[index] = input_buffer.data[index] * 2.0f;
    }
}