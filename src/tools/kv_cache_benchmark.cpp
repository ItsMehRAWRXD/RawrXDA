// kv_cache_benchmark.cpp — Phase E: GPU-Resident KV Cache Benchmark
// Tests GPU-only KV cache writes + single-head attention with zero CPU↔GPU copy.
//
// Phase 1: Per-op dispatch (each KV write + attention is separate fence wait)
// Phase 2: Fused dispatch (all heads KV writes + attention in one command list)
// Phase 3: Full autoregressive generation loop (all heads fused per token)
//
// Build: RawrXD-KVBenchmark target in CMakeLists.txt
// Run:   RawrXD-KVBenchmark.exe from D:\rawrxd

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <vector>

#include "gguf_d3d12_bridge.h"

using Microsoft::WRL::ComPtr;

// ── Model dimensions (7B-class transformer) ────────────────────────────────
static constexpr uint32_t DIM       = 4096;
static constexpr uint32_t N_HEADS   = 32;
static constexpr uint32_t HEAD_DIM  = DIM / N_HEADS; // 128
static constexpr uint32_t MAX_SEQ   = 512;            // KV cache depth
static constexpr uint32_t GEN_TOKENS = 128;           // tokens to generate

// ── Random fill ────────────────────────────────────────────────────────────
static void fillRandom(float* data, size_t n, float range = 0.5f) {
    for (size_t i = 0; i < n; ++i)
        data[i] = ((float)rand() / RAND_MAX - 0.5f) * 2.0f * range;
}

// ════════════════════════════════════════════════════════════════════════════
int main() {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  RawrXD Phase E — GPU-Resident KV Cache Benchmark\n");
    printf("  Model: %u dim, %u heads, %u head_dim\n", DIM, N_HEADS, HEAD_DIM);
    printf("  KV Cache: %u max seq, generating %u tokens\n", MAX_SEQ, GEN_TOKENS);
    printf("═══════════════════════════════════════════════════════════════\n\n");

    srand(42);

    // ── D3D12 Device + Queue ───────────────────────────────────────────────
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        fprintf(stderr, "FATAL: CreateDXGIFactory1 failed\n"); return 1;
    }

    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { adapter.Reset(); continue; }
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                         IID_PPV_ARGS(&device)))) {
            printf("[GPU] %ls (VRAM: %llu MB)\n\n",
                   desc.Description, desc.DedicatedVideoMemory / (1024 * 1024));
            break;
        }
        adapter.Reset();
    }
    if (!device) { fprintf(stderr, "FATAL: No D3D12 device\n"); return 1; }

    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    if (FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)))) {
        fprintf(stderr, "FATAL: CreateCommandQueue failed\n"); return 1;
    }

    // ── Initialize bridge ──────────────────────────────────────────────────
    RawrXD::GGUFD3D12Bridge bridge;
    if (!bridge.Initialize(device.Get(), queue.Get())) {
        fprintf(stderr, "FATAL: Bridge init failed\n"); return 1;
    }

    if (!bridge.CompileShadersFromHLSL(L"src/llm_compute.hlsl")) {
        fprintf(stderr, "FATAL: Shader compilation failed\n"); return 1;
    }
    printf("[INIT] All 10 shaders compiled (Phase C+D+E)\n");

    // ── Allocate GPU-resident KV cache ─────────────────────────────────────
    ComPtr<ID3D12Resource> kvCache;
    if (!bridge.AllocateKVCache(MAX_SEQ, N_HEADS, HEAD_DIM, kvCache)) {
        fprintf(stderr, "FATAL: AllocateKVCache failed\n"); return 1;
    }
    uint64_t kvSizeBytes = (uint64_t)MAX_SEQ * N_HEADS * 2 * HEAD_DIM * sizeof(float);
    printf("[INIT] KV cache allocated: %.1f MB (GPU-resident, zero-copy)\n",
           kvSizeBytes / (1024.0 * 1024.0));

    // ── Allocate per-head scratch buffers ──────────────────────────────────
    // For each head: query (HEAD_DIM), key (HEAD_DIM), value (HEAD_DIM), output (HEAD_DIM)
    ComPtr<ID3D12Resource> gpuQuery, gpuKey, gpuValue, gpuHeadOut;
    bridge.AllocateBuffer(HEAD_DIM * sizeof(float), gpuQuery);
    bridge.AllocateBuffer(HEAD_DIM * sizeof(float), gpuKey);
    bridge.AllocateBuffer(HEAD_DIM * sizeof(float), gpuValue);
    bridge.AllocateBuffer(HEAD_DIM * sizeof(float), gpuHeadOut);

    // Upload initial random data for query/key/value vectors
    std::vector<float> hostVec(HEAD_DIM);
    fillRandom(hostVec.data(), HEAD_DIM, 1.0f);

    ComPtr<ID3D12Resource> gpuQueryUpload;
    bridge.UploadTensor(hostVec.data(), HEAD_DIM * sizeof(float),
                        RawrXD::GGMLType::F32, gpuQueryUpload);

    // We also need upload buffers for K and V to simulate incoming vectors
    ComPtr<ID3D12Resource> gpuKeyUpload, gpuValUpload;
    fillRandom(hostVec.data(), HEAD_DIM, 0.5f);
    bridge.UploadTensor(hostVec.data(), HEAD_DIM * sizeof(float),
                        RawrXD::GGMLType::F32, gpuKeyUpload);
    fillRandom(hostVec.data(), HEAD_DIM, 0.5f);
    bridge.UploadTensor(hostVec.data(), HEAD_DIM * sizeof(float),
                        RawrXD::GGMLType::F32, gpuValUpload);

    printf("[INIT] Scratch buffers allocated. Ready for benchmark.\n\n");

    // ════════════════════════════════════════════════════════════════════════
    // PHASE 1: Per-op dispatch — each KV write + attention = separate fence
    // ════════════════════════════════════════════════════════════════════════
    printf("── Phase 1: Per-op KV dispatch (baseline) ─────────────────────\n");
    {
        // Test: write K+V for 1 head, then run attention, per-op per token
        auto t0 = std::chrono::high_resolution_clock::now();
        int opsOk = 0;

        for (uint32_t tok = 0; tok < GEN_TOKENS; ++tok) {
            uint32_t pos = tok; // position in cache

            // Write K vector for head 0
            if (bridge.DispatchKVCacheWrite(kvCache.Get(), gpuKeyUpload.Get(),
                                            pos, 0, false,
                                            N_HEADS, HEAD_DIM))
                opsOk++;

            // Write V vector for head 0
            if (bridge.DispatchKVCacheWrite(kvCache.Get(), gpuValUpload.Get(),
                                            pos, 0, true,
                                            N_HEADS, HEAD_DIM))
                opsOk++;

            // Run attention over cached positions [0..tok]
            if (bridge.DispatchAttentionHead(kvCache.Get(), gpuQueryUpload.Get(),
                                             gpuHeadOut.Get(),
                                             tok + 1, 0, N_HEADS, HEAD_DIM))
                opsOk++;
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double tps = GEN_TOKENS / (ms / 1000.0);
        printf("  Per-op (1 head): %d ops in %.1f ms = %.1f TPS\n",
               opsOk, ms, tps);
        printf("  Fence waits: %d (3 per token: writeK + writeV + attention)\n\n", opsOk);
    }

    // ════════════════════════════════════════════════════════════════════════
    // PHASE 2: Fused dispatch — all 3 ops per token in one command list
    // ════════════════════════════════════════════════════════════════════════
    printf("── Phase 2: Fused KV dispatch (1 head, 3 ops fused) ──────────\n");
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        int opsOk = 0;
        int batches = 0;

        for (uint32_t tok = 0; tok < GEN_TOKENS; ++tok) {
            uint32_t pos = tok;

            if (!bridge.BeginFusedDispatch()) {
                fprintf(stderr, "  [ERR] BeginFusedDispatch failed at token %u\n", tok);
                continue;
            }

            bool ok = true;
            // Write K for head 0 (fused)
            ok &= bridge.RecordKVCacheWrite(kvCache.Get(), gpuKeyUpload.Get(),
                                            pos, 0, false, N_HEADS, HEAD_DIM);
            // Write V for head 0 (fused)
            ok &= bridge.RecordKVCacheWrite(kvCache.Get(), gpuValUpload.Get(),
                                            pos, 0, true, N_HEADS, HEAD_DIM);
            // Attention over [0..tok] for head 0 (fused)
            ok &= bridge.RecordAttentionHead(kvCache.Get(), gpuQueryUpload.Get(),
                                             gpuHeadOut.Get(),
                                             tok + 1, 0, N_HEADS, HEAD_DIM);

            if (!ok) { bridge.FlushAndWait(); continue; }
            if (bridge.FlushAndWait()) {
                opsOk += 3;
                batches++;
            }
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double tps = GEN_TOKENS / (ms / 1000.0);
        printf("  Fused (1 head): %d ops in %.1f ms = %.1f TPS\n",
               opsOk, ms, tps);
        printf("  Fence waits: %d (1 per token instead of 3)\n\n", batches);
    }

    // ════════════════════════════════════════════════════════════════════════
    // PHASE 3: Full multi-head fused — all 32 heads KV writes + attention
    // ════════════════════════════════════════════════════════════════════════
    printf("── Phase 3: Full multi-head fused (all %u heads per token) ────\n", N_HEADS);
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        int opsOk = 0;
        int batches = 0;

        for (uint32_t tok = 0; tok < GEN_TOKENS; ++tok) {
            uint32_t pos = tok;

            if (!bridge.BeginFusedDispatch()) {
                fprintf(stderr, "  [ERR] BeginFusedDispatch failed at token %u\n", tok);
                continue;
            }

            bool ok = true;

            // For each head: write K, write V, then run attention
            for (uint32_t h = 0; h < N_HEADS; ++h) {
                ok &= bridge.RecordKVCacheWrite(kvCache.Get(), gpuKeyUpload.Get(),
                                                pos, h, false, N_HEADS, HEAD_DIM);
                ok &= bridge.RecordKVCacheWrite(kvCache.Get(), gpuValUpload.Get(),
                                                pos, h, true, N_HEADS, HEAD_DIM);
                ok &= bridge.RecordAttentionHead(kvCache.Get(), gpuQueryUpload.Get(),
                                                 gpuHeadOut.Get(),
                                                 tok + 1, h, N_HEADS, HEAD_DIM);
            }

            // Total per token: 32×(writeK + writeV + attention) = 96 ops, 1 fence
            if (!ok) { bridge.FlushAndWait(); continue; }
            if (bridge.FlushAndWait()) {
                opsOk += N_HEADS * 3;
                batches++;
            }
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double tps = GEN_TOKENS / (ms / 1000.0);
        uint32_t perOpFenceWouldBe = GEN_TOKENS * N_HEADS * 3;
        printf("  Full multi-head: %d ops in %.1f ms = %.1f TPS\n",
               opsOk, ms, tps);
        printf("  Fence waits: %d (vs %u if per-op) — %.0fx fence reduction\n\n",
               batches, perOpFenceWouldBe,
               (double)perOpFenceWouldBe / (batches > 0 ? batches : 1));
    }

    // ── Readback verification ──────────────────────────────────────────────
    printf("── Verification: readback head output ─────────────────────────\n");
    {
        std::vector<float> result(HEAD_DIM);
        if (bridge.ReadbackBuffer(gpuHeadOut.Get(), result.data(), HEAD_DIM * sizeof(float))) {
            float sum = 0, maxVal = 0;
            for (uint32_t i = 0; i < HEAD_DIM; ++i) {
                sum += result[i];
                maxVal = fmaxf(maxVal, fabsf(result[i]));
            }
            printf("  Head output: sum=%.4f max=%.4f mean=%.6f\n",
                   sum, maxVal, sum / HEAD_DIM);
            printf("  Status: %s\n",
                   (maxVal > 0.0f && maxVal < 1e6f) ? "VALID" : "SUSPECT");
        } else {
            printf("  Readback FAILED\n");
        }
    }

    // ── Telemetry JSON ─────────────────────────────────────────────────────
    {
        FILE* fp = fopen("gpu_kv_cache_benchmark.json", "w");
        if (fp) {
            fprintf(fp, "{\n");
            fprintf(fp, "  \"version\": \"v16.4-KV-CACHE\",\n");
            fprintf(fp, "  \"phase\": \"E\",\n");
            fprintf(fp, "  \"model_dim\": %u,\n", DIM);
            fprintf(fp, "  \"n_heads\": %u,\n", N_HEADS);
            fprintf(fp, "  \"head_dim\": %u,\n", HEAD_DIM);
            fprintf(fp, "  \"max_seq_len\": %u,\n", MAX_SEQ);
            fprintf(fp, "  \"gen_tokens\": %u,\n", GEN_TOKENS);
            fprintf(fp, "  \"kv_cache_bytes\": %llu,\n", kvSizeBytes);
            fprintf(fp, "  \"gpu_resident\": true,\n");
            fprintf(fp, "  \"zero_copy\": true,\n");
            fprintf(fp, "  \"fused_dispatch\": true,\n");
            fprintf(fp, "  \"kernels\": [\"CSKVCacheWrite\", \"CSAttentionHead\"]\n");
            fprintf(fp, "}\n");
            fclose(fp);
            printf("\n[TELEMETRY] Written to gpu_kv_cache_benchmark.json\n");
        }
    }

    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Phase E: GPU-Resident KV Cache Benchmark COMPLETE\n");
    printf("  KV Cache: %u max seq × %u heads × %u dim = %.1f MB GPU-resident\n",
           MAX_SEQ, N_HEADS, HEAD_DIM, kvSizeBytes / (1024.0 * 1024.0));
    printf("  Zero CPU↔GPU copy per token. All ops GPU-only.\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    bridge.Shutdown();
    return 0;
}
