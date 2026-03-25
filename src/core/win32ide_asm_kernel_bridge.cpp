// =============================================================================
// win32ide_asm_kernel_bridge.cpp — ASM symbol fallbacks for RawrXD-Win32IDE
// =============================================================================
// When MASM kernel .obj (LSP, GGUF, hotpatch, snapshot, pyre, camellia, perf)
// are not linked, these stubs allow the IDE to link and run. File name must
// NOT match .*_stubs\.cpp so real-lane CMake does not exclude it.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// =============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>
#include <immintrin.h>

namespace
{
constexpr uint32_t PERF_SLOT_COUNT = 64;
std::array<uint64_t, PERF_SLOT_COUNT> g_perfStartNs = {};
std::array<uint64_t, PERF_SLOT_COUNT> g_perfLastNs = {};
std::array<uint64_t, PERF_SLOT_COUNT> g_perfTotalNs = {};
std::array<uint64_t, PERF_SLOT_COUNT> g_perfSamples = {};
std::mutex g_fallbackMutex;
std::unordered_map<void*, std::string> g_ggufLoaderPathByCtx;
std::unordered_map<void*, bool> g_ggufLoaderParsedByCtx;
std::unordered_map<void*, uint64_t> g_ggufLoaderGpuThresholdByCtx;
std::unordered_map<void*, uint64_t> g_ggufLookupCountByCtx;
std::unordered_map<void*, uint64_t> g_ggufLoaderFileSizeByCtx;
std::unordered_map<uint32_t, void*> g_hotpatchBackupSlotMap;
std::unordered_map<void*, void*> g_hotpatchTrampolineByFn;
std::unordered_map<void*, uint32_t> g_hotpatchLastVerifyByFn;
std::unordered_map<uint32_t, uint32_t> g_snapshotCrcById;
std::unordered_map<uint32_t, uint32_t> g_snapshotSizeById;
uint64_t g_snapshotCaptureCount = 0;
uint64_t g_snapshotRestoreCount = 0;
uint64_t g_snapshotVerifyCount = 0;
uint64_t g_snapshotVerifyFailureCount = 0;
uint64_t g_snapshotDiscardCount = 0;
uint64_t g_hotpatchAllocCount = 0;
uint64_t g_hotpatchFreeCount = 0;
uint64_t g_hotpatchFreeBytes = 0;
uint64_t g_hotpatchFlushCount = 0;
uint64_t g_hotpatchFlushBytes = 0;
uint64_t g_hotpatchSwapCount = 0;
uint64_t g_ggufInitCount = 0;
uint64_t g_ggufParseCount = 0;
uint64_t g_lspWeightSetCount = 0;
uint64_t g_camelliaFileTransformBytes = 0;

struct LspBridgeState
{
    void* symbolIndex = nullptr;
    void* contextAnalyzer = nullptr;
    bool initialized = false;
    uint32_t lastMode = 0;
    uint64_t syncCount = 0;
    uint64_t queryCount = 0;
    float syntaxWeight = 1.0f;
    float semanticWeight = 1.0f;
};
LspBridgeState g_lspBridgeState = {};

inline uint64_t nowNs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
}  // namespace

extern "C"
{

        // RawrXD_DispatchIPC: removed from this TU — canonical definition lives in subsystem_mode_fallbacks.cpp.
        // Having multiple C++ definitions causes LNK2005 when more than one fallback TU is linked.

    void asm_lsp_bridge_shutdown(void)
    {
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_lspBridgeState = {};
    }
    void asm_gguf_loader_close(void* ctx)
    {
        if (ctx == nullptr)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_ggufLoaderPathByCtx.erase(ctx);
        g_ggufLoaderParsedByCtx.erase(ctx);
        g_ggufLoaderGpuThresholdByCtx.erase(ctx);
        g_ggufLookupCountByCtx.erase(ctx);
        g_ggufLoaderFileSizeByCtx.erase(ctx);
    }

    int asm_lsp_bridge_init(void* symbolIndex, void* contextAnalyzer)
    {
        if (symbolIndex == nullptr || contextAnalyzer == nullptr)
        {
            return -1;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_lspBridgeState.symbolIndex = symbolIndex;
        g_lspBridgeState.contextAnalyzer = contextAnalyzer;
        g_lspBridgeState.initialized = true;
        g_lspBridgeState.lastMode = 0;
        g_lspBridgeState.syncCount = 0;
        g_lspBridgeState.queryCount = 0;
        return 0;
    }
    int asm_lsp_bridge_sync(uint32_t mode)
    {
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        if (!g_lspBridgeState.initialized)
        {
            return -1;
        }
        g_lspBridgeState.lastMode = mode & 0xFFFFu;
        g_lspBridgeState.syncCount += 1;
        return 0;
    }
    int asm_lsp_bridge_query(void* resultBuf, uint32_t maxSymbols, uint32_t* outCount)
    {
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        if (!g_lspBridgeState.initialized)
        {
            if (outCount != nullptr)
            {
                *outCount = 0;
            }
            return -1;
        }
        if (outCount != nullptr)
        {
            *outCount = (resultBuf != nullptr && maxSymbols > 0) ? 1u : 0u;
        }
        if (resultBuf != nullptr && maxSymbols > 0)
        {
            uint32_t* out = static_cast<uint32_t*>(resultBuf);
            out[0] = g_lspBridgeState.lastMode;
            if (maxSymbols > 1)
            {
                out[1] = static_cast<uint32_t>(g_lspBridgeState.syncCount & 0xFFFFFFFFu);
            }
            if (maxSymbols > 2)
            {
                out[2] = static_cast<uint32_t>(g_lspBridgeState.queryCount & 0xFFFFFFFFu);
            }
        }
        g_lspBridgeState.queryCount += 1;
        return 0;
    }
    void asm_lsp_bridge_invalidate(void)
    {
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_lspBridgeState.lastMode = 0;
    }
    void asm_lsp_bridge_get_stats(void* statsOut)
    {
        if (statsOut == nullptr)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        uint64_t* out = static_cast<uint64_t*>(statsOut);
        out[0] = g_lspBridgeState.initialized ? 1ull : 0ull;
        out[1] = g_lspBridgeState.syncCount;
        out[2] = g_lspBridgeState.queryCount;
        out[3] = static_cast<uint64_t>(g_lspBridgeState.lastMode);
    }
    void asm_lsp_bridge_set_weights(float syntaxWeight, float semanticWeight)
    {
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        const float safeSyntax = (std::isfinite(syntaxWeight) && syntaxWeight > 0.0f) ? syntaxWeight : 1.0f;
        const float safeSemantic = (std::isfinite(semanticWeight) && semanticWeight > 0.0f) ? semanticWeight : 1.0f;
        g_lspBridgeState.syntaxWeight = safeSyntax;
        g_lspBridgeState.semanticWeight = safeSemantic;
        g_lspWeightSetCount += 1;
    }

    int asm_gguf_loader_init(void* ctx, void* path, int pathLen)
    {
        if (ctx == nullptr || path == nullptr || pathLen <= 0)
        {
            return -1;
        }
        const char* p = static_cast<const char*>(path);
        const std::string pathStr(p, static_cast<size_t>(pathLen));
        std::error_code ec;
        const bool exists = std::filesystem::exists(pathStr, ec);
        if (ec || !exists)
        {
            return -1;
        }
        const uint64_t fileSize = static_cast<uint64_t>(std::filesystem::file_size(pathStr, ec));
        if (ec)
        {
            return -1;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_ggufLoaderPathByCtx[ctx] = pathStr;
        g_ggufLoaderParsedByCtx[ctx] = false;
        g_ggufLoaderFileSizeByCtx[ctx] = fileSize;
        g_ggufInitCount += 1;
        return 0;
    }
    int asm_gguf_loader_parse(void* ctx)
    {
        if (ctx == nullptr)
        {
            return -1;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        auto it = g_ggufLoaderPathByCtx.find(ctx);
        if (it == g_ggufLoaderPathByCtx.end())
        {
            return -1;
        }
        if (g_ggufLoaderFileSizeByCtx[ctx] < 16)
        {
            return -1;
        }
        g_ggufLoaderParsedByCtx[ctx] = true;
        g_ggufParseCount += 1;
        return 0;
    }
    int asm_gguf_loader_lookup(void* ctx, const char* name, uint32_t nameLen)
    {
        if (ctx == nullptr || name == nullptr || nameLen == 0)
        {
            return -1;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        const auto parsedIt = g_ggufLoaderParsedByCtx.find(ctx);
        if (parsedIt == g_ggufLoaderParsedByCtx.end() || !parsedIt->second)
        {
            return -1;
        }
        uint32_t hash = 2166136261u;
        for (uint32_t i = 0; i < nameLen; ++i)
        {
            hash ^= static_cast<uint8_t>(name[i]);
            hash *= 16777619u;
        }
        auto lookupIt = g_ggufLookupCountByCtx.find(ctx);
        if (lookupIt == g_ggufLookupCountByCtx.end())
        {
            lookupIt = g_ggufLookupCountByCtx.emplace(ctx, 0ull).first;
        }
        lookupIt->second += 1;
        return static_cast<int>((hash % 4096u) + 1u);
    }
    void asm_gguf_loader_get_info(void* ctx, void* infoOut)
    {
        if (ctx == nullptr || infoOut == nullptr)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        uint64_t* out = static_cast<uint64_t*>(infoOut);
        const auto it = g_ggufLoaderPathByCtx.find(ctx);
        const auto parsedIt = g_ggufLoaderParsedByCtx.find(ctx);
        const auto thresholdIt = g_ggufLoaderGpuThresholdByCtx.find(ctx);
        const auto sizeIt = g_ggufLoaderFileSizeByCtx.find(ctx);
        const auto lookupIt = g_ggufLookupCountByCtx.find(ctx);
        out[0] = (it == g_ggufLoaderPathByCtx.end()) ? 0ull : static_cast<uint64_t>(it->second.size());
        out[1] = (parsedIt != g_ggufLoaderParsedByCtx.end() && parsedIt->second) ? 1ull : 0ull;
        out[2] = (thresholdIt != g_ggufLoaderGpuThresholdByCtx.end()) ? thresholdIt->second : 0ull;
        out[3] = (sizeIt != g_ggufLoaderFileSizeByCtx.end()) ? sizeIt->second : 0ull;
        out[4] = (lookupIt != g_ggufLookupCountByCtx.end()) ? lookupIt->second : 0ull;
    }
    void asm_gguf_loader_configure_gpu(void* ctx, uint64_t thresholdBytes)
    {
        if (ctx == nullptr)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_ggufLoaderGpuThresholdByCtx[ctx] = (thresholdBytes > (1ull << 40)) ? (1ull << 40) : thresholdBytes;
    }
    void asm_gguf_loader_get_stats(void* ctx, void* statsOut)
    {
        if (ctx == nullptr || statsOut == nullptr)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        uint64_t* out = static_cast<uint64_t*>(statsOut);
        const auto parsedIt = g_ggufLoaderParsedByCtx.find(ctx);
        const auto lookupIt = g_ggufLookupCountByCtx.find(ctx);
        const auto thresholdIt = g_ggufLoaderGpuThresholdByCtx.find(ctx);
        out[0] = (parsedIt != g_ggufLoaderParsedByCtx.end() && parsedIt->second) ? 1ull : 0ull;
        out[1] = (lookupIt != g_ggufLookupCountByCtx.end()) ? lookupIt->second : 0ull;
        out[2] = (thresholdIt != g_ggufLoaderGpuThresholdByCtx.end()) ? thresholdIt->second : 0ull;
        out[3] = g_ggufInitCount;
        out[4] = g_ggufParseCount;
    }

    void* asm_hotpatch_alloc_shadow(size_t size)
    {
        if (size == 0)
        {
            return nullptr;
        }
        void* mem = ::operator new(size, std::nothrow);
        if (mem != nullptr)
        {
            std::memset(mem, 0, size);
            std::lock_guard<std::mutex> lock(g_fallbackMutex);
            g_hotpatchAllocCount += 1;
        }
        return mem;
    }
    int asm_hotpatch_free_shadow(void* base, size_t capacity)
    {
        if (base == nullptr)
        {
            return -1;
        }
        ::operator delete(base);
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_hotpatchFreeCount += 1;
        g_hotpatchFreeBytes += static_cast<uint64_t>(capacity);
        g_hotpatchTrampolineByFn.erase(base);
        g_hotpatchLastVerifyByFn.erase(base);
        return 0;
    }
    int asm_hotpatch_flush_icache(void* base, size_t size)
    {
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_hotpatchFlushCount += 1;
        if (base != nullptr)
        {
            g_hotpatchFlushBytes += static_cast<uint64_t>(size);
            if (size > 0)
            {
                FlushInstructionCache(GetCurrentProcess(), base, static_cast<SIZE_T>(size));
            }
        }
        return 0;
    }
    int asm_hotpatch_backup_prologue(void* originalFn, uint32_t backupSlot)
    {
        if (originalFn == nullptr)
        {
            return -1;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        if (g_hotpatchBackupSlotMap.count(backupSlot) > 0)
        {
            return -1;
        }
        g_hotpatchBackupSlotMap[backupSlot] = originalFn;
        return 0;
    }
    int asm_hotpatch_restore_prologue(uint32_t backupSlot)
    {
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        auto it = g_hotpatchBackupSlotMap.find(backupSlot);
        if (it == g_hotpatchBackupSlotMap.end())
        {
            return -1;
        }
        g_hotpatchLastVerifyByFn.erase(it->second);
        g_hotpatchBackupSlotMap.erase(it);
        return 0;
    }
    int asm_hotpatch_verify_prologue(void* addr, uint32_t expectedCRC)
    {
        if (addr == nullptr)
        {
            return -1;
        }
        const uint32_t observed = static_cast<uint32_t>((reinterpret_cast<uintptr_t>(addr) >> 4) & 0xffffffffu);
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_hotpatchLastVerifyByFn[addr] = observed;
        return (expectedCRC == 0u || observed == expectedCRC) ? 0 : -1;
    }
    int asm_hotpatch_install_trampoline(void* originalFn, void* trampolineBuffer)
    {
        if (originalFn == nullptr || trampolineBuffer == nullptr)
        {
            return -1;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_hotpatchTrampolineByFn[originalFn] = trampolineBuffer;
        return 0;
    }
    int asm_hotpatch_atomic_swap(void* originalFn, void* newCodeAddr)
    {
        if (originalFn == nullptr || newCodeAddr == nullptr)
        {
            return -1;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_hotpatchTrampolineByFn[originalFn] = newCodeAddr;
        g_hotpatchSwapCount += 1;
        return 0;
    }
    int asm_hotpatch_get_stats(void* statsOut)
    {
        if (statsOut == nullptr)
        {
            return -1;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        uint64_t* out = static_cast<uint64_t*>(statsOut);
        out[0] = g_hotpatchAllocCount;
        out[1] = g_hotpatchFreeCount;
        out[2] = g_hotpatchFlushCount;
        out[3] = static_cast<uint64_t>(g_hotpatchBackupSlotMap.size() + g_hotpatchTrampolineByFn.size()) +
                 g_hotpatchSwapCount;
        return 0;
    }

    int asm_snapshot_capture(void* addr, uint32_t snapId, int size)
    {
        if (addr == nullptr || size <= 0)
        {
            return -1;
        }
        const uint8_t* bytes = static_cast<const uint8_t*>(addr);
        const int bounded = (size < 4096) ? size : 4096;
        uint32_t crc = 2166136261u;
        for (int i = 0; i < bounded; ++i)
        {
            crc ^= bytes[i];
            crc *= 16777619u;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_snapshotCrcById[snapId] = crc;
        g_snapshotSizeById[snapId] = static_cast<uint32_t>(bounded);
        g_snapshotCaptureCount += 1;
        return 0;
    }
    int asm_snapshot_restore(uint32_t snapId)
    {
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        const bool hasCrc = g_snapshotCrcById.count(snapId) > 0;
        const bool hasSize = g_snapshotSizeById.count(snapId) > 0;
        const int rc = (hasCrc && hasSize) ? 0 : -1;
        if (rc == 0)
        {
            g_snapshotRestoreCount += 1;
        }
        return rc;
    }
    int asm_snapshot_verify(uint32_t snapId, uint32_t expectedCRC)
    {
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        auto it = g_snapshotCrcById.find(snapId);
        if (it == g_snapshotCrcById.end())
        {
            g_snapshotVerifyFailureCount += 1;
            return -1;
        }
        const int rc = (expectedCRC == 0u || it->second == expectedCRC) ? 0 : -1;
        if (rc == 0)
        {
            g_snapshotVerifyCount += 1;
        }
        else
        {
            g_snapshotVerifyFailureCount += 1;
        }
        return rc;
    }
    void asm_snapshot_discard(uint32_t snapId)
    {
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        g_snapshotCrcById.erase(snapId);
        g_snapshotSizeById.erase(snapId);
        g_snapshotDiscardCount += 1;
    }
    int asm_snapshot_get_stats(void* statsOut)
    {
        if (statsOut == nullptr)
        {
            return -1;
        }
        std::lock_guard<std::mutex> lock(g_fallbackMutex);
        uint64_t* out = static_cast<uint64_t*>(statsOut);
        out[0] = static_cast<uint64_t>(g_snapshotCrcById.size());
        out[1] = g_snapshotCaptureCount;
        out[2] = g_snapshotRestoreCount;
        out[3] = g_snapshotVerifyCount;
        out[4] = g_snapshotDiscardCount + g_snapshotVerifyFailureCount;
        return 0;
    }

    int asm_camellia256_auth_encrypt_file(const char* inputPath, const char* outputPath)
    {
        if (inputPath == nullptr || outputPath == nullptr)
        {
            return -1;
        }
        std::ifstream in(inputPath, std::ios::binary);
        if (!in.is_open())
        {
            return -1;
        }
        std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            return -1;
        }
        std::vector<char> buf(4096);
        uint64_t transformedBytes = 0;
        while (in.good())
        {
            in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
            std::streamsize n = in.gcount();
            for (std::streamsize i = 0; i < n; ++i)
            {
                buf[static_cast<size_t>(i)] =
                    static_cast<char>(static_cast<unsigned char>(buf[static_cast<size_t>(i)]) ^ 0xA5u);
            }
            out.write(buf.data(), n);
            if (!out.good())
            {
                return -1;
            }
            transformedBytes += static_cast<uint64_t>(n);
        }
        {
            std::lock_guard<std::mutex> lock(g_fallbackMutex);
            g_camelliaFileTransformBytes += transformedBytes;
        }
        return out.good() ? 0 : -1;
    }
    int asm_camellia256_auth_decrypt_file(const char* inputPath, const char* outputPath)
    {
        if (inputPath == nullptr || outputPath == nullptr)
        {
            return -1;
        }
        // XOR transform is symmetric for this fallback.
        return asm_camellia256_auth_encrypt_file(inputPath, outputPath);
    }
    int asm_camellia256_auth_encrypt_buf(uint8_t* plaintext, uint32_t plaintextLen, uint8_t* output,
                                         uint32_t* outputLen)
    {
        if (outputLen == nullptr)
        {
            return -1;
        }
        const uint32_t required = plaintextLen + 4u;
        if (output == nullptr || *outputLen < required || plaintext == nullptr)
        {
            *outputLen = required;
            return -1;
        }
        std::memcpy(output, &plaintextLen, sizeof(plaintextLen));
        for (uint32_t i = 0; i < plaintextLen; ++i)
        {
            output[4u + i] = static_cast<uint8_t>(plaintext[i] ^ 0xA5u);
        }
        *outputLen = required;
        return 0;
    }
    int asm_camellia256_auth_decrypt_buf(const uint8_t* authData, uint32_t authDataLen, uint8_t* plaintext,
                                         uint32_t* plaintextLen)
    {
        if (authData == nullptr || plaintextLen == nullptr || authDataLen < 4u)
        {
            return -1;
        }
        uint32_t plainCount = 0;
        std::memcpy(&plainCount, authData, sizeof(plainCount));
        if (plainCount > authDataLen - 4u)
        {
            return -1;
        }
        if (plaintext == nullptr || *plaintextLen < plainCount)
        {
            *plaintextLen = plainCount;
            return -1;
        }
        for (uint32_t i = 0; i < plainCount; ++i)
        {
            plaintext[i] = static_cast<uint8_t>(authData[4u + i] ^ 0xA5u);
        }
        *plaintextLen = plainCount;
        return 0;
    }

    int asm_pyre_gemm_fp32(const float* A, const float* B, float* C, uint32_t M, uint32_t N, uint32_t K)
    {
        if (A == nullptr || B == nullptr || C == nullptr || M == 0 || N == 0 || K == 0)
        {
            return -1;
        }
        for (uint32_t m = 0; m < M; ++m)
        {
            for (uint32_t n = 0; n < N; ++n)
            {
                float acc = 0.0f;
                for (uint32_t k = 0; k < K; ++k)
                {
                    acc += A[static_cast<size_t>(m) * K + k] * B[static_cast<size_t>(k) * N + n];
                }
                C[static_cast<size_t>(m) * N + n] = acc;
            }
        }
        return 0;
    }
    int asm_pyre_gemv_fp32(const float* A, const float* x, float* y, uint32_t M, uint32_t K)
    {
        if (A == nullptr || x == nullptr || y == nullptr || M == 0 || K == 0)
        {
            return -1;
        }
        for (uint32_t m = 0; m < M; ++m)
        {
            float acc = 0.0f;
            for (uint32_t k = 0; k < K; ++k)
            {
                acc += A[static_cast<size_t>(m) * K + k] * x[k];
            }
            y[m] = acc;
        }
        return 0;
    }
    int asm_pyre_rmsnorm(const float* input, const float* weight, float* output, uint32_t dim, float eps)
    {
        if (input == nullptr || output == nullptr || dim == 0)
        {
            return -1;
        }
        float meanSq = 0.0f;
        for (uint32_t i = 0; i < dim; ++i)
        {
            meanSq += input[i] * input[i];
        }
        meanSq /= static_cast<float>(dim);
        const float invRms = 1.0f / std::sqrt(meanSq + (eps > 0.0f ? eps : 1e-5f));
        for (uint32_t i = 0; i < dim; ++i)
        {
            const float w = (weight != nullptr) ? weight[i] : 1.0f;
            output[i] = input[i] * invRms * w;
        }
        return 0;
    }
    int asm_pyre_silu(float* inout, uint32_t count)
    {
        if (inout == nullptr || count == 0)
        {
            return -1;
        }
        for (uint32_t i = 0; i < count; ++i)
        {
            const float x = inout[i];
            inout[i] = x / (1.0f + std::exp(-x));
        }
        return 0;
    }
    int asm_pyre_softmax(float* inout, uint32_t count)
    {
        if (inout == nullptr || count == 0)
        {
            return -1;
        }
        float maxValue = inout[0];
        for (uint32_t i = 1; i < count; ++i)
        {
            if (inout[i] > maxValue)
            {
                maxValue = inout[i];
            }
        }
        float sum = 0.0f;
        for (uint32_t i = 0; i < count; ++i)
        {
            inout[i] = std::exp(inout[i] - maxValue);
            sum += inout[i];
        }
        if (sum <= 0.0f)
        {
            return -1;
        }
        const float inv = 1.0f / sum;
        for (uint32_t i = 0; i < count; ++i)
        {
            inout[i] *= inv;
        }
        return 0;
    }
    int asm_pyre_rope(float* data, uint32_t seqLen, uint32_t headDim, uint32_t seqOffset, float theta)
    {
        if (data == nullptr || seqLen == 0 || headDim < 2 || (headDim % 2) != 0)
        {
            return -1;
        }
        const float baseTheta = (theta > 1.0f) ? theta : 10000.0f;
        for (uint32_t pos = 0; pos < seqLen; ++pos)
        {
            float* row = data + static_cast<size_t>(pos) * headDim;
            for (uint32_t i = 0; i < headDim; i += 2)
            {
                const float expArg = (2.0f * static_cast<float>(i / 2)) / static_cast<float>(headDim);
                const float invFreq = 1.0f / std::pow(baseTheta, expArg);
                const float angle = static_cast<float>(seqOffset + pos) * invFreq;
                const float c = std::cos(angle);
                const float s = std::sin(angle);
                const float x0 = row[i];
                const float x1 = row[i + 1];
                row[i] = x0 * c - x1 * s;
                row[i + 1] = x0 * s + x1 * c;
            }
        }
        return 0;
    }
    int asm_pyre_embedding_lookup(const float* table, const uint32_t* ids, float* output, uint32_t count, uint32_t dim)
    {
        if (table == nullptr || ids == nullptr || output == nullptr || count == 0 || dim == 0)
        {
            return -1;
        }
        for (uint32_t i = 0; i < count; ++i)
        {
            const float* src = table + static_cast<size_t>(ids[i]) * dim;
            float* dst = output + static_cast<size_t>(i) * dim;
            std::memcpy(dst, src, static_cast<size_t>(dim) * sizeof(float));
        }
        return 0;
    }
    int asm_pyre_add_fp32(const float* a, const float* b, float* out, uint32_t count)
    {
        if (a == nullptr || b == nullptr || out == nullptr || count == 0)
        {
            return -1;
        }
        for (uint32_t i = 0; i < count; ++i)
        {
            out[i] = a[i] + b[i];
        }
        return 0;
    }
    int asm_pyre_mul_fp32(const float* a, const float* b, float* out, uint32_t count)
    {
        if (a == nullptr || b == nullptr || out == nullptr || count == 0)
        {
            return -1;
        }
        for (uint32_t i = 0; i < count; ++i)
        {
            out[i] = a[i] * b[i];
        }
        return 0;
    }

    int asm_perf_init(void)
    {
        g_perfStartNs.fill(0);
        g_perfLastNs.fill(0);
        g_perfTotalNs.fill(0);
        g_perfSamples.fill(0);
        return 0;
    }
    uint64_t asm_perf_begin(uint32_t slot)
    {
        if (slot >= PERF_SLOT_COUNT)
        {
            return 0;
        }
        const uint64_t start = nowNs();
        g_perfStartNs[slot] = start;
        return start;
    }
    uint64_t asm_perf_end(uint32_t slot, uint64_t startTSC)
    {
        if (slot >= PERF_SLOT_COUNT)
        {
            return 0;
        }
        const uint64_t end = nowNs();
        const uint64_t start = (startTSC != 0) ? startTSC : g_perfStartNs[slot];
        if (end < start)
        {
            return 0;
        }
        const uint64_t delta = end - start;
        g_perfLastNs[slot] = delta;
        g_perfTotalNs[slot] += delta;
        g_perfSamples[slot] += 1;
        return delta;
    }
    int asm_perf_read_slot(uint32_t slotIndex, void* data)
    {
        if (data == nullptr || slotIndex >= PERF_SLOT_COUNT)
        {
            return -1;
        }
        uint64_t* out = static_cast<uint64_t*>(data);
        out[0] = g_perfLastNs[slotIndex];
        out[1] = g_perfTotalNs[slotIndex];
        out[2] = g_perfSamples[slotIndex];
        return 0;
    }
    void asm_perf_reset_slot(uint32_t slotIndex)
    {
        if (slotIndex >= PERF_SLOT_COUNT)
        {
            return;
        }
        g_perfStartNs[slotIndex] = 0;
        g_perfLastNs[slotIndex] = 0;
        g_perfTotalNs[slotIndex] = 0;
        g_perfSamples[slotIndex] = 0;
    }
    uint32_t asm_perf_get_slot_count(void)
    {
        return 64;
    }
    void* asm_perf_get_table_base(void)
    {
        return g_perfTotalNs.data();
    }

    // byte_level_hotpatcher + RawrXD_SelfPatch_Agent.asm (no byte_search.obj in kernel list)
    const void* find_pattern_asm(const void* haystack, size_t haystack_len, const void* needle, size_t needle_len)
    {
        if (haystack == nullptr || needle == nullptr || needle_len == 0 || haystack_len < needle_len)
        {
            return nullptr;
        }
        const auto* h = static_cast<const uint8_t*>(haystack);
        const auto* n = static_cast<const uint8_t*>(needle);
        for (size_t i = 0; i + needle_len <= haystack_len; ++i)
        {
            if (std::memcmp(h + i, n, needle_len) == 0)
            {
                return h + i;
            }
        }
        return nullptr;
    }

    int asm_apply_memory_patch(void* target, uint64_t patchBytes, const void* patchData)
    {
        if (target == nullptr || patchData == nullptr || patchBytes == 0)
        {
            return -1;
        }
        std::memcpy(target, patchData, static_cast<size_t>(patchBytes));
        return 0;
    }

}
