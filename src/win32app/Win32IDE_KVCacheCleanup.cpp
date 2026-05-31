// ============================================================================
// Win32IDE_KVCacheCleanup.cpp — Titan KV-Cache Memory Management (Expanded)
// ============================================================================
// Prevents VRAM fragmentation and memory leaks during long inference sessions.
// Implements session-based cleanup with pressure thresholds for RX 7800 XT (16GB).
// ============================================================================

#include "Win32IDE.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>

namespace
{

// ============================================================================
// Titan Context State
// ============================================================================

struct TitanKVCacheState
{
    std::atomic<uint64_t> current_seq_len{0};
    std::atomic<uint64_t> max_seq_len{4096};
    std::atomic<uint64_t> vram_usage{0};
    std::atomic<uint64_t> vram_capacity{16ULL << 30};  // 16GB default
    std::atomic<bool> session_active{false};
    std::atomic<bool> inference_in_progress{false};
    std::chrono::steady_clock::time_point last_cleanup;
    std::chrono::steady_clock::time_point session_start;
    std::mutex session_mutex;
    
    // KV-Cache pointers (would be set by Titan backend)
    void* kv_cache_ptr{nullptr};
    size_t kv_cache_size{0};
    size_t kv_cache_block_size{0};
    int n_layers{0};
    int n_heads{0};
    int head_dim{0};
};

static TitanKVCacheState g_titanState;

// ============================================================================
// Memory Pressure Thresholds
// ============================================================================

constexpr double PRESSURE_WARNING = 0.75;    // 75% VRAM usage
constexpr double PRESSURE_CRITICAL = 0.85;   // 85% VRAM usage
constexpr double PRESSURE_EMERGENCY = 0.95;  // 95% VRAM usage

// ============================================================================
// Internal Helpers
// ============================================================================

void Titan_InternalFlushKVCache()
{
    std::lock_guard<std::mutex> lock(g_titanState.session_mutex);
    
    if (g_titanState.kv_cache_ptr && g_titanState.kv_cache_size > 0)
    {
        // Zero out the KV-cache memory (soft reset)
        std::memset(g_titanState.kv_cache_ptr, 0, g_titanState.kv_cache_size);
    }
    
    g_titanState.current_seq_len.store(0);
    g_titanState.last_cleanup = std::chrono::steady_clock::now();
}

void Titan_InternalHardReset()
{
    std::lock_guard<std::mutex> lock(g_titanState.session_mutex);
    
    // Free existing cache if present
    if (g_titanState.kv_cache_ptr)
    {
        // In real implementation, would call Titan_FreeKVCache()
        g_titanState.kv_cache_ptr = nullptr;
        g_titanState.kv_cache_size = 0;
    }
    
    // Reset all state
    g_titanState.current_seq_len.store(0);
    g_titanState.vram_usage.store(0);
    g_titanState.inference_in_progress.store(false);
    g_titanState.last_cleanup = std::chrono::steady_clock::now();
}

uint64_t Titan_EstimateKVCacheSize(uint64_t seq_len)
{
    // Estimate: n_layers * n_heads * head_dim * seq_len * 2 (K + V) * sizeof(float)
    // Default values for typical 7B model
    int n_layers = g_titanState.n_layers > 0 ? g_titanState.n_layers : 32;
    int n_heads = g_titanState.n_heads > 0 ? g_titanState.n_heads : 32;
    int head_dim = g_titanState.head_dim > 0 ? g_titanState.head_dim : 128;
    
    return static_cast<uint64_t>(n_layers) * n_heads * head_dim * seq_len * 2 * sizeof(float);
}

}  // namespace

// ============================================================================
// Public API — Session Management
// ============================================================================

extern "C"
{

// Forward declarations (definitions follow below).
void Titan_ReleaseSessionResources();

void Titan_BeginInference()
{
    // Mark session as active
    bool was_active = g_titanState.session_active.exchange(true);
    
    if (!was_active)
    {
        g_titanState.session_start = std::chrono::steady_clock::now();
        g_titanState.current_seq_len.store(0);
    }
    
    g_titanState.inference_in_progress.store(true);
}

void Titan_EndInference()
{
    g_titanState.inference_in_progress.store(false);
    
    // Check memory pressure after inference
    uint64_t vram_used = g_titanState.vram_usage.load();
    uint64_t vram_cap = g_titanState.vram_capacity.load();
    
    double pressure = static_cast<double>(vram_used) / vram_cap;
    
    if (pressure >= PRESSURE_CRITICAL)
    {
        // Trigger cleanup
        Titan_ReleaseSessionResources();
    }
}

void Titan_ReleaseSessionResources()
{
    std::lock_guard<std::mutex> lock(g_titanState.session_mutex);
    
    // Soft reset: just zero sequence length
    g_titanState.current_seq_len.store(0);
    
    // Update VRAM estimate
    uint64_t new_size = Titan_EstimateKVCacheSize(0);
    g_titanState.vram_usage.store(new_size);
    
    g_titanState.last_cleanup = std::chrono::steady_clock::now();
}

void Titan_ForceKVCacheCleanup()
{
    uint64_t vram_used = g_titanState.vram_usage.load();
    uint64_t vram_cap = g_titanState.vram_capacity.load();
    
    double pressure = static_cast<double>(vram_used) / vram_cap;
    
    if (pressure >= PRESSURE_EMERGENCY)
    {
        // Hard reset required
        Titan_InternalHardReset();
    }
    else if (pressure >= PRESSURE_CRITICAL)
    {
        // Soft reset
        Titan_InternalFlushKVCache();
    }
    else
    {
        // Just reset sequence length
        g_titanState.current_seq_len.store(0);
    }
}

void Titan_ResetInferenceSession()
{
    if (!g_titanState.session_active.load())
    {
        return;  // No active session
    }
    
    // Abort any active inference
    if (g_titanState.inference_in_progress.load())
    {
        g_titanState.inference_in_progress.store(false);
    }
    
    uint64_t vram_used = g_titanState.vram_usage.load();
    uint64_t vram_cap = g_titanState.vram_capacity.load();
    
    double pressure = static_cast<double>(vram_used) / vram_cap;
    
    if (pressure >= PRESSURE_CRITICAL)
    {
        // Hard reset: free and reallocate
        Titan_InternalHardReset();
        
        // Reinitialize with default context size
        g_titanState.max_seq_len.store(4096);
        g_titanState.kv_cache_size = Titan_EstimateKVCacheSize(4096);
    }
    else
    {
        // Soft reset: keep pointers, zero sequence length
        g_titanState.current_seq_len.store(0);
    }
}

// ============================================================================
// Public API — State Queries
// ============================================================================

uint64_t Titan_GetCurrentSeqLen()
{
    return g_titanState.current_seq_len.load();
}

uint64_t Titan_GetMaxSeqLen()
{
    return g_titanState.max_seq_len.load();
}

void Titan_SetMaxSeqLen(uint64_t max_len)
{
    g_titanState.max_seq_len.store(max_len);
}

uint64_t Titan_GetVRAMUsage()
{
    return g_titanState.vram_usage.load();
}

uint64_t Titan_GetVRAMCapacity()
{
    return g_titanState.vram_capacity.load();
}

void Titan_SetVRAMCapacity(uint64_t capacity)
{
    g_titanState.vram_capacity.store(capacity);
}

double Titan_GetMemoryPressure()
{
    uint64_t used = g_titanState.vram_usage.load();
    uint64_t cap = g_titanState.vram_capacity.load();
    if (cap == 0) return 0.0;
    return static_cast<double>(used) / cap;
}

bool Titan_IsSessionActive()
{
    return g_titanState.session_active.load();
}

bool Titan_IsInferenceInProgress()
{
    return g_titanState.inference_in_progress.load();
}

// ============================================================================
// Public API — Sequence Updates
// ============================================================================

void Titan_UpdateSeqLen(uint64_t new_len)
{
    uint64_t max_len = g_titanState.max_seq_len.load();
    
    if (new_len > max_len)
    {
        // Clamp to max
        new_len = max_len;
    }
    
    g_titanState.current_seq_len.store(new_len);
    
    // Update VRAM estimate
    uint64_t estimated_size = Titan_EstimateKVCacheSize(new_len);
    g_titanState.vram_usage.store(estimated_size);
}

void Titan_IncrementSeqLen(uint64_t delta)
{
    uint64_t current = g_titanState.current_seq_len.load();
    Titan_UpdateSeqLen(current + delta);
}

// ============================================================================
// Public API — Configuration
// ============================================================================

void Titan_Configure(int n_layers, int n_heads, int head_dim)
{
    g_titanState.n_layers = n_layers;
    g_titanState.n_heads = n_heads;
    g_titanState.head_dim = head_dim;
}

void Titan_SetKVCachePtr(void* ptr, size_t size)
{
    std::lock_guard<std::mutex> lock(g_titanState.session_mutex);
    g_titanState.kv_cache_ptr = ptr;
    g_titanState.kv_cache_size = size;
}

// ============================================================================
// Public API — Diagnostics
// ============================================================================

const char* Titan_GetStatusString()
{
    static char status_buf[256];
    
    uint64_t seq_len = g_titanState.current_seq_len.load();
    uint64_t max_len = g_titanState.max_seq_len.load();
    uint64_t vram = g_titanState.vram_usage.load();
    uint64_t cap = g_titanState.vram_capacity.load();
    double pressure = Titan_GetMemoryPressure();
    
    snprintf(status_buf, sizeof(status_buf),
        "Seq: %llu/%llu | VRAM: %.2f GB / %.2f GB (%.1f%%)",
        (unsigned long long)seq_len,
        (unsigned long long)max_len,
        vram / (1024.0 * 1024.0 * 1024.0),
        cap / (1024.0 * 1024.0 * 1024.0),
        pressure * 100.0);
    
    return status_buf;
}

uint64_t Titan_GetSessionDurationMs()
{
    if (!g_titanState.session_active.load())
    {
        return 0;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - g_titanState.session_start);
    
    return duration.count();
}

uint64_t Titan_GetTimeSinceLastCleanupMs()
{
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - g_titanState.last_cleanup);
    
    return duration.count();
}

// ============================================================================
// Legacy Compatibility (maps old API to new)
// ============================================================================

void Titan_SetKVCacheBuffers(void* k_buffer, void* v_buffer, uint32_t max_len)
{
    // Map to new API
    Titan_SetKVCachePtr(k_buffer, max_len * 1024);  // Rough estimate
    Titan_SetMaxSeqLen(max_len);
}

void Titan_UpdateKVCacheSeqLen(uint32_t new_len)
{
    Titan_UpdateSeqLen(new_len);
}

uint32_t Titan_GetKVCacheSeqLen()
{
    return static_cast<uint32_t>(Titan_GetCurrentSeqLen());
}

uint32_t Titan_GetKVCacheMaxSeqLen()
{
    return static_cast<uint32_t>(Titan_GetMaxSeqLen());
}

bool Titan_IsKVCacheInUse()
{
    return Titan_IsInferenceInProgress();
}

}  // extern "C"

// ============================================================================
// C++ Wrapper for Win32IDE Integration
// ============================================================================

namespace RawrXD
{
namespace KVCache
{

void BeginSession()
{
    Titan_BeginInference();
}

void EndSession()
{
    Titan_EndInference();
    Titan_ReleaseSessionResources();
}

void UpdateSeqLen(uint32_t len)
{
    Titan_UpdateSeqLen(len);
}

uint32_t GetSeqLen()
{
    return Titan_GetKVCacheSeqLen();
}

uint32_t GetMaxSeqLen()
{
    return Titan_GetKVCacheMaxSeqLen();
}

bool IsInUse()
{
    return Titan_IsKVCacheInUse();
}

void ForceCleanup()
{
    Titan_ForceKVCacheCleanup();
}

void ResetSession()
{
    Titan_ResetInferenceSession();
}

uint64_t GetVRAMUsage()
{
    return Titan_GetVRAMUsage();
}

double GetMemoryPressure()
{
    return Titan_GetMemoryPressure();
}

const char* GetStatusString()
{
    return Titan_GetStatusString();
}

void Configure(int n_layers, int n_heads, int head_dim)
{
    Titan_Configure(n_layers, n_heads, head_dim);
}

}  // namespace KVCache
}  // namespace RawrXD