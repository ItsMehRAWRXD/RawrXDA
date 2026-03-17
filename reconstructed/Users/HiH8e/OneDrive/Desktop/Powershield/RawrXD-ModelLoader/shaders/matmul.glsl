#version 450

layout(local_size_x = 16, local_size_y = 16) in;

layout(set = 0, binding = 0) readonly buffer InputA {
    float data[];
} input_a;

layout(set = 0, binding = 1) readonly buffer InputB {
    float data[];
} input_b;

layout(set = 0, binding = 2) writeonly buffer Output {
    float data[];
} output_buf;

layout(push_constant) uniform MatMulPush {
    uint M;
    uint K;
    uint N;
} pc;

shared float tile_a[16][16];
shared float tile_b[16][16];

void main() {
    const uint row = gl_GlobalInvocationID.y;
    const uint col = gl_GlobalInvocationID.x;
    const uint tiles = (pc.K + 15) / 16;
    
    float sum = 0.0;
    
    for (uint block = 0; block < tiles; ++block) {
        // Load tiles
        uint a_col = block * 16 + gl_LocalInvocationID.x;
        uint b_row = block * 16 + gl_LocalInvocationID.y;
        
        if (row < pc.M && a_col < pc.K) {
            tile_a[gl_LocalInvocationID.y][gl_LocalInvocationID.x] = input_a.data[row * pc.K + a_col];
        } else {
            tile_a[gl_LocalInvocationID.y][gl_LocalInvocationID.x] = 0.0;
        }
        
        if (b_row < pc.K && col < pc.N) {
            tile_b[gl_LocalInvocationID.y][gl_LocalInvocationID.x] = input_b.data[b_row * pc.N + col];
        } else {
            tile_b[gl_LocalInvocationID.y][gl_LocalInvocationID.x] = 0.0;
        }
        
        barrier();
        
        // Multiply and accumulate
        for (uint k = 0; k < 16; ++k) {
            sum += tile_a[gl_LocalInvocationID.y][k] * tile_b[k][gl_LocalInvocationID.x];
        }
        
        barrier();
    }
    
    if (row < pc.M && col < pc.N) {
        output_buf.data[row * pc.N + col] = sum;
    }
}
