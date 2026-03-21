
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "gguf_d3d12_bridge.h"
#include "RawrXD_Interfaces.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace RawrXD;

struct float2 { float x, y; };

std::vector<float2> GenerateRoPETables(uint32_t seq_len, uint32_t head_dim, float theta = 10000.0f) {
    std::vector<float2> tables;
    tables.reserve(seq_len * (head_dim / 2));

    for (uint32_t pos = 0; pos < seq_len; pos++) {
        for (uint32_t i = 0; i < head_dim / 2; i++) {
            float freq = 1.0f / powf(theta, (2.0f * i) / head_dim);
            float angle = pos * freq;

            float2 cs;
            cs.x = cosf(angle);
            cs.y = sinf(angle);
            tables.push_back(cs);
        }
    }
    return tables;
}

void ApplyRoPE_CPU(float* q, float* k, const std::vector<float2>& tables,
                   uint32_t seq_len, uint32_t head_dim, uint32_t num_heads) {
    for (uint32_t pos = 0; pos < seq_len; pos++) {
        for (uint32_t h = 0; h < num_heads; h++) {
            uint32_t head_offset = (pos * num_heads + h) * head_dim;

            for (uint32_t i = 0; i < head_dim / 2; i++) {
                uint32_t idx0 = head_offset + i;
                uint32_t idx1 = head_offset + i + head_dim / 2;
                float2 cs = tables[pos * (head_dim / 2) + i];

                float q0 = q[idx0], q1 = q[idx1];
                q[idx0] = q0 * cs.x - q1 * cs.y;
                q[idx1] = q0 * cs.y + q1 * cs.x;

                float k0 = k[idx0], k1 = k[idx1];
                k[idx0] = k0 * cs.x - k1 * cs.y;
                k[idx1] = k0 * cs.y + k1 * cs.x;
            }
        }
    }
}

void RunRoPEParityTest(int seq_len)
{
    const int head_dim = 128;
    const int n_heads = 32;

    std::vector<float> q(seq_len * head_dim * n_heads);
    std::vector<float> k(seq_len * head_dim * n_heads);
    auto cossin = GenerateRoPETables(seq_len, head_dim);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);

    for (auto& v : q) v = dist(rng);
    for (auto& v : k) v = dist(rng);

    std::vector<float> q_cpu = q;
    std::vector<float> k_cpu = k;

    // reference CPU
    ApplyRoPE_CPU(q_cpu.data(), k_cpu.data(), cossin, seq_len, head_dim, n_heads);

    // D3D12 Setup
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
        std::cerr << "Failed to create D3D12 Device" << std::endl;
        return;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> cmdQueue;
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue)))) {
        std::cerr << "Failed to create command queue" << std::endl;
        return;
    }

    GGUFD3D12Bridge bridge;
    if (!bridge.Initialize(device.Get(), cmdQueue.Get()) || !bridge.LoadShadersFromDirectory("d:/rawrxd/build/bin")) {
        std::cerr << "Failed to init bridge" << std::endl;
        return;
    }

    // Upload tensors
    Microsoft::WRL::ComPtr<ID3D12Resource> q_gpu, k_gpu, cossin_gpu;
    size_t q_size = q.size() * sizeof(float);
    size_t k_size = k.size() * sizeof(float);
    size_t cossin_size = cossin.size() * sizeof(float2);

    if (!bridge.UploadTensor(q.data(), q_size, (GGMLType)0, q_gpu)) { printf("Upload Tensor failed\n"); return; }
    if (!bridge.UploadTensor(k.data(), k_size, (GGMLType)0, k_gpu)) { printf("Upload Tensor failed\n"); return; }
    if (!bridge.UploadTensor(cossin.data(), cossin_size, (GGMLType)0, cossin_gpu)) { printf("Upload Tensor failed\n"); return; }

    // Start benchmark
    auto start = std::chrono::high_resolution_clock::now();

    // GPU fused dispatch
    if (!bridge.DispatchRoPEFused(q_gpu.Get(), k_gpu.Get(), cossin_gpu.Get(), seq_len, head_dim, n_heads)) {
        std::cerr << "Failed DispatchRoPEFused" << std::endl;
        return;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "GPU RoPE " << seq_len << " dispatch took "
              << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " us" << std::endl;

    // Readback
    std::vector<float> q_gpu_result(q.size());
    if (!bridge.ReadbackBuffer(q_gpu.Get(), q_gpu_result.data(), q_size)) {
        std::cerr << "Failed ReadbackBuffer" << std::endl;
        return;
    }

    float max_err = 0.0f;
    int err_idx = -1;
    for (size_t i = 0; i < q.size(); i++)
    {
        float diff = std::fabs(q_gpu_result[i] - q_cpu[i]);
        if (diff > max_err) {
            max_err = diff;
            err_idx = i;
        }
    }

    printf("[SeqLen %d] RoPE parity max error: %.8f", seq_len, max_err);
    if (max_err > 0.0015f) {
        printf(" (Parity FAIL at idx %d!)\n", err_idx);
        printf("GPU: %f, CPU: %f\n", q_gpu_result[err_idx], q_cpu[err_idx]);
    } else {
        printf(" (Parity PASS)\n");
    }
}

void CPU_GEMM(const float* A, const float* B, float* C, uint32_t M, uint32_t K, uint32_t N) {
    for (uint32_t i = 0; i < M; ++i) {
        for (uint32_t j = 0; j < N; ++j) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < K; ++k) {
                sum += A[i * K + k] * B[k * N + j];
            }
            C[i * N + j] = sum;
        }
    }
}

void RunGEMMParityTest(uint32_t M, uint32_t K, uint32_t N)
{
    std::cout << "==========================================\n";
    std::cout << "GEMM Parity Test (M=" << M << ", K=" << K << ", N=" << N << ")\n";

    // Initialize D3D12
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
    D3D12_COMMAND_QUEUE_DESC qDesc{};
    qDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&queue));

    GGUFD3D12Bridge bridge;
    bridge.Initialize(device.Get(), queue.Get());
    bridge.LoadShadersFromDirectory("build/bin");

    std::vector<float> A(M * K);
    std::vector<float> B(K * N);
    std::vector<float> C_cpu(M * N, 0.0f);
    std::vector<float> C_gpu(M * N, 0.0f);

    std::mt19937 gen(42);
    auto dist = std::uniform_real_distribution<float>(-1.0f, 1.0f);
    for (auto& x : A) x = dist(gen);
    for (auto& x : B) x = dist(gen);

    std::cout << "Starting CPU GEMM...\n";
    auto start_cpu = std::chrono::high_resolution_clock::now();
    CPU_GEMM(A.data(), B.data(), C_cpu.data(), M, K, N);
    auto end_cpu = std::chrono::high_resolution_clock::now();
    std::cout << "CPU GEMM done.\n";

    Microsoft::WRL::ComPtr<ID3D12Resource> bufA, bufB, bufC;
    std::cout << "Uploading Tensors...\n";
    if (!bridge.UploadTensor(A.data(), A.size() * sizeof(float), GGMLType::F32, bufA)) { std::cout << "Upload A failed\n"; return; }
    if (!bridge.UploadTensor(B.data(), B.size() * sizeof(float), GGMLType::F32, bufB)) { std::cout << "Upload B failed\n"; return; }
    if (!bridge.UploadTensor(C_gpu.data(), C_gpu.size() * sizeof(float), GGMLType::F32, bufC)) { std::cout << "Upload C failed\n"; return; }

    std::cout << "Dispatching GEMM...\n";
    auto start_gpu = std::chrono::high_resolution_clock::now();
    if (!bridge.DispatchGEMM(bufA.Get(), bufB.Get(), bufC.Get(), M, K, N)) {
        std::cout << "DispatchGEMM failed!\n";
        return;
    }
    auto end_gpu = std::chrono::high_resolution_clock::now();
    std::cout << "GPU GEMM done.\n";

    // Readback using bridge
    if (!bridge.ReadbackBuffer(bufC.Get(), C_gpu.data(), C_gpu.size() * sizeof(float))) {
        std::cout << "Readback failed!\n";
        return;
    }

    float max_err = 0.0f;
    for (uint32_t i = 0; i < C_cpu.size(); ++i) {
        float err = std::abs(C_cpu[i] - C_gpu[i]);
        if (err > max_err) max_err = err;
    }

    std::cout << "CPU MatMul Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end_cpu - start_cpu).count() << " us\n";
    std::cout << "GPU MatMul Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end_gpu - start_gpu).count() << " us\n";
    std::cout << "Max Error Margin: " << max_err << "\n";

    if (max_err < 0.001f) {
        std::cout << "[PASS] GEMM output matches exactly.\n";
    } else {
        std::cout << "[FAIL] Error exceeds margin!\n";
    }
}


extern "C" void attention_baseline(float* q, float* k, float* v,
                                   int batch_size, int seq_len,
                                   int head_size, int num_heads,
                                   float* output) {
    if (!q || !k || !v || !output || batch_size <= 0 || seq_len <= 0 ||
        head_size <= 0 || num_heads <= 0) {
        return;
    }

    const float scale = 1.0f / std::sqrt(static_cast<float>(head_size));
    const int tokensPerBatch = seq_len * num_heads * head_size;

    for (int b = 0; b < batch_size; ++b) {
        const int batchOffset = b * tokensPerBatch;
        for (int h = 0; h < num_heads; ++h) {
            for (int i = 0; i < seq_len; ++i) {
                float maxLogit = -3.402823e38f;
                for (int j = 0; j < seq_len; ++j) {
                    float dot = 0.0f;
                    for (int d = 0; d < head_size; ++d) {
                        const int qIdx = batchOffset + ((i * num_heads + h) * head_size + d);
                        const int kIdx = batchOffset + ((j * num_heads + h) * head_size + d);
                        dot += q[qIdx] * k[kIdx];
                    }
                    const float logit = dot * scale;
                    if (logit > maxLogit) maxLogit = logit;
                }

                float sumExp = 0.0f;
                for (int d = 0; d < head_size; ++d) {
                    const int outIdx = batchOffset + ((i * num_heads + h) * head_size + d);
                    output[outIdx] = 0.0f;
                }

                for (int j = 0; j < seq_len; ++j) {
                    float dot = 0.0f;
                    for (int d = 0; d < head_size; ++d) {
                        const int qIdx = batchOffset + ((i * num_heads + h) * head_size + d);
                        const int kIdx = batchOffset + ((j * num_heads + h) * head_size + d);
                        dot += q[qIdx] * k[kIdx];
                    }
                    const float weight = std::exp(dot * scale - maxLogit);
                    sumExp += weight;
                    for (int d = 0; d < head_size; ++d) {
                        const int vIdx = batchOffset + ((j * num_heads + h) * head_size + d);
                        const int outIdx = batchOffset + ((i * num_heads + h) * head_size + d);
                        output[outIdx] += weight * v[vIdx];
                    }
                }

                if (sumExp > 0.0f) {
                    const float inv = 1.0f / sumExp;
                    for (int d = 0; d < head_size; ++d) {
                        const int outIdx = batchOffset + ((i * num_heads + h) * head_size + d);
                        output[outIdx] *= inv;
                    }
                }
            }
        }
    }
}

void RunFlashAttentionParityTest(int seq_len) {
    const int head_dim = 64;
    const int n_heads = 32;
    int size = seq_len * head_dim * n_heads;

    std::vector<float> Q_cpu(size);
    std::vector<float> K_cpu(size);
    std::vector<float> V_cpu(size);
    std::vector<float> Out_cpu(size);
    std::vector<float> Out_gpu(size);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    for (int i = 0; i < size; ++i) {
        Q_cpu[i] = dist(rng);
        K_cpu[i] = dist(rng);
        V_cpu[i] = dist(rng);
    }

    auto start_cpu = std::chrono::high_resolution_clock::now();
    attention_baseline(Q_cpu.data(), K_cpu.data(), V_cpu.data(), 1, seq_len, head_dim, n_heads, Out_cpu.data());
    auto end_cpu = std::chrono::high_resolution_clock::now();
    std::cout << "CPU Flash Attention done.\n";

    Microsoft::WRL::ComPtr<ID3D12Device> device;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) return;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> cmdQueue;
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue)))) return;

    GGUFD3D12Bridge bridge;
    bridge.Initialize(device.Get(), cmdQueue.Get());

    Microsoft::WRL::ComPtr<ID3D12Resource> bufQ, bufK, bufV, bufOut;
    bridge.UploadTensor(Q_cpu.data(), static_cast<uint64_t>(size * sizeof(float)),
                        GGMLType::F32, bufQ);
    bridge.UploadTensor(K_cpu.data(), static_cast<uint64_t>(size * sizeof(float)),
                        GGMLType::F32, bufK);
    bridge.UploadTensor(V_cpu.data(), static_cast<uint64_t>(size * sizeof(float)),
                        GGMLType::F32, bufV);
    bridge.AllocateBuffer(static_cast<uint64_t>(size * sizeof(float)), bufOut);

    auto start_gpu = std::chrono::high_resolution_clock::now();
    if (!bridge.DispatchFlashAttention(bufQ.Get(), bufK.Get(), bufV.Get(), bufOut.Get(), seq_len, head_dim, n_heads)) {
        std::cout << "GPU Flash Attention Dispatch failed!\n";
        return;
    }
    auto end_gpu = std::chrono::high_resolution_clock::now();
    std::cout << "GPU Flash Attention done.\n";

    if (!bridge.ReadbackBuffer(bufOut.Get(), Out_gpu.data(), Out_gpu.size() * sizeof(float))) {
        std::cout << "Readback failed!\n";
        return;
    }

    float max_err = 0.0f;
    for (uint32_t i = 0; i < Out_cpu.size(); ++i) {
        float err = std::abs(Out_cpu[i] - Out_gpu[i]);
        if (err > max_err) max_err = err;
    }

    std::cout << "CPU Flash Attention Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end_cpu - start_cpu).count() << " us\n";
    std::cout << "GPU Flash Attention Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end_gpu - start_gpu).count() << " us\n";
    std::cout << "Max Error Margin: " << max_err << "\n";

    if (max_err < 0.01f) {
        std::cout << "[PASS] Flash Attention output matches closely.\n";
    } else {
        std::cout << "[FAIL] Error exceeds margin!\n";
    }
}
int main(int argc, char** argv)
{
    std::cout << "Starting benchmark..." << std::endl << std::flush;

    int seq_len = 128;

    if (argc > 1)
        seq_len = atoi(argv[1]);

    if (seq_len == 0) {
        RunRoPEParityTest(32);
        RunRoPEParityTest(128);
        RunRoPEParityTest(512);
        RunGEMMParityTest(128, 128, 128);
        RunGEMMParityTest(256, 128, 512);
        RunFlashAttentionParityTest(128);
    } else {
        RunRoPEParityTest(seq_len);
        RunGEMMParityTest(128, 128, 128);
        RunGEMMParityTest(256, 128, 512);
        RunFlashAttentionParityTest(128);
    }

    return 0;
}



