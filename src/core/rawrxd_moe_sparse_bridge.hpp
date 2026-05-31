#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <windows.h>

namespace RawrXD::MoE {

struct MoeConfig {
    uint32_t num_experts;
    uint32_t top_k;
    uint32_t hidden_dim;
    uint32_t ffn_dim;
    uint32_t weight_dtype;
    uint64_t expert_size_bytes;
    void* weights_base;
    void* scales_base;
};

// Externs for MASM kernel
extern "C" void* SparseGather_Initialize(MoeConfig* config, void* telemetry_handle);
extern "C" int SparseGather_Execute(void* context, float* input, float* router_logits, float* output, uint32_t layer_index);
extern "C" void SparseGather_FlushCache(void* context);
extern "C" void SparseGather_GetStats(void* context, uint64_t* loaded, uint64_t* skipped);

class SparseGatherBridge {
public:
    static inline bool ShouldFailLoud() {
        // Default fail-loud to prevent silent no-op benchmark runs.
        // Set RAWRXD_SPARSEGATHER_FAIL_LOUD=0 to opt out.
        char buf[8] = {};
        const DWORD n = GetEnvironmentVariableA("RAWRXD_SPARSEGATHER_FAIL_LOUD", buf, static_cast<DWORD>(sizeof(buf)));
        if (n == 0) {
            return true;
        }
        return !(buf[0] == '0' || buf[0] == 'n' || buf[0] == 'N' || buf[0] == 'f' || buf[0] == 'F');
    }

    static inline void FailLoud(const char* reason) {
        char msg[512] = {};
        std::snprintf(msg, sizeof(msg),
                      "[SparseGatherBridge] FATAL: %s\n"
                      "This would produce invalid/no-op benchmarking results.\n"
                      "Set RAWRXD_SPARSEGATHER_FAIL_LOUD=0 to downgrade to soft-fail.\n",
                      reason ? reason : "unknown error");
        OutputDebugStringA(msg);
        std::fputs(msg, stderr);
        std::fflush(stderr);
        std::abort();
    }

    static inline bool ValidateConfig(const MoeConfig& config, const char** why = nullptr) {
        if (config.num_experts == 0) {
            if (why) *why = "num_experts is zero";
            return false;
        }
        if (config.hidden_dim == 0 || config.ffn_dim == 0) {
            if (why) *why = "hidden_dim/ffn_dim is zero";
            return false;
        }
        if (config.weights_base == nullptr) {
            if (why) *why = "weights_base is null";
            return false;
        }
        return true;
    }

    SparseGatherBridge(const MoeConfig& config) {
        m_config = config;
        const char* why = nullptr;
        if (!ValidateConfig(m_config, &why)) {
            if (ShouldFailLoud()) {
                FailLoud(why);
            }
            return;
        }
        m_context = SparseGather_Initialize(&m_config, nullptr);
        if (!m_context && ShouldFailLoud()) {
            FailLoud("SparseGather_Initialize returned null context");
        }
    }

    ~SparseGatherBridge() {
        if (m_context) {
            // Cleanup logic (VirtualFree) should be in SparseGather_Shutdown in MASM
        }
    }

    bool ExecuteLayer(float* input, float* router_logits, float* output, uint32_t layer) {
        const char* why = nullptr;
        if (!ValidateConfig(m_config, &why)) {
            if (ShouldFailLoud()) {
                FailLoud(why);
            }
            return false;
        }
        if (!m_context) {
            if (ShouldFailLoud()) {
                FailLoud("SparseGather context is null");
            }
            return false;
        }
        return SparseGather_Execute(m_context, input, router_logits, output, layer) == 1;
    }

    void Flush() {
        if (m_context) SparseGather_FlushCache(m_context);
    }

    struct Stats {
        uint64_t loaded;
        uint64_t skipped;
    };

    Stats GetStats() {
        Stats s = {0, 0};
        if (m_context) SparseGather_GetStats(m_context, &s.loaded, &s.skipped);
        return s;
    }

private:
    MoeConfig m_config;
    void* m_context = nullptr;
};

} // namespace RawrXD::MoE
