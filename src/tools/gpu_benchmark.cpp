
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
    } else {
        RunRoPEParityTest(seq_len);
    }

    return 0;
}

