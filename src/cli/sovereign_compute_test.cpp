// =============================================================================
// sovereign_compute_test.cpp — Real Compute Verification Harness
// Tests: GGUF header parse + AVX-512 matmul correctness
// Build: cl /std:c++20 /O2 /MT /I..\agentic /I..\asm sovereign_compute_test.cpp
//        ..\asm\gguf_parser.obj ..\asm\avx512_matmul.obj msvcrt.lib kernel32.lib
// =============================================================================

#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cmath>

// C linkage from MASM
extern "C" {
    int32_t GGUF_ParseHeader(void* mappedBase, uint64_t fileSize, void* out);
    int32_t GGUF_GetTensorInfo(void* mappedBase, uint32_t tensorIdx, void* info, void* out);
    void avx512_matmul_f32(const float* A, const float* B, float* C,
                           int64_t M, int64_t N, int64_t K);
}

// Packed structs matching MASM offsets
#pragma pack(push, 1)
struct GGUF_Info {
    uint32_t magic;
    uint32_t version;
    uint32_t tensor_count;
    uint32_t metadata_count;
    uint64_t header_size;
    uint64_t tensor_offset;
    uint64_t metadata_offset;
    uint32_t alignment;
};

struct GGUF_Tensor {
    uint64_t name_len;
    uint64_t name_ptr;
    uint32_t n_dims;
    uint64_t dims[4];
    uint32_t type;
    uint64_t offset;
    uint64_t size;
    void*    data_ptr;
};
#pragma pack(pop)

// ---------------------------------------------------------------------------
// Test 1: AVX-512 matmul correctness
// ---------------------------------------------------------------------------
static bool test_matmul() {
    constexpr int M = 8, N = 16, K = 4;
    alignas(64) float A[M * K];
    alignas(64) float B[K * N];
    alignas(64) float C[M * N];
    alignas(64) float C_ref[M * N];

    // Fill A, B with deterministic values
    for (int i = 0; i < M * K; ++i) A[i] = static_cast<float>(i + 1);
    for (int i = 0; i < K * N; ++i) B[i] = static_cast<float>(i + 1) * 0.5f;

    // Zero C
    for (int i = 0; i < M * N; ++i) C[i] = 0.0f;

    // Call AVX-512 kernel
    avx512_matmul_f32(A, B, C, M, N, K);

    // Reference scalar implementation
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; ++n) {
            float sum = 0.0f;
            for (int k = 0; k < K; ++k) {
                sum += A[m * K + k] * B[k * N + n];
            }
            C_ref[m * N + n] = sum;
        }
    }

    // Compare with tolerance
    float max_err = 0.0f;
    for (int i = 0; i < M * N; ++i) {
        float err = std::fabs(C[i] - C_ref[i]);
        if (err > max_err) max_err = err;
    }

    printf("[TEST] AVX-512 matmul max error: %e\n", max_err);
    return max_err < 1e-4f;
}

// ---------------------------------------------------------------------------
// Test 2: GGUF header parse on a synthetic minimal file
// ---------------------------------------------------------------------------
static bool test_gguf_parse() {
    // Minimal synthetic GGUF v3 header
    // magic(4) + version(4) + tensor_count(8) + metadata_count(8) = 24 bytes
    uint8_t fake[64] = {};
    fake[0] = 0x47; fake[1] = 0x47; fake[2] = 0x55; fake[3] = 0x46; // "GGUF"
    fake[4] = 3; fake[5] = 0;                                       // version 3
    fake[8] = 2;                                                   // 2 tensors
    fake[12] = 1;                                                  // 1 metadata

    GGUF_Info info = {};
    int32_t rc = GGUF_ParseHeader(fake, sizeof(fake), &info);

    printf("[TEST] GGUF parse rc=%d magic=0x%08X version=%u tensors=%u meta=%u\n",
           rc, info.magic, info.version, info.tensor_count, info.metadata_count);

    return rc == 0 && info.magic == 0x46554747 && info.version == 3
           && info.tensor_count == 2 && info.metadata_count == 1;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    printf("=== Sovereign Compute Verification ===\n");

    bool ok1 = test_matmul();
    bool ok2 = test_gguf_parse();

    printf("\n=== Results ===\n");
    printf("AVX-512 matmul: %s\n", ok1 ? "PASS" : "FAIL");
    printf("GGUF parser:    %s\n", ok2 ? "PASS" : "FAIL");

    return (ok1 && ok2) ? 0 : 1;
}
