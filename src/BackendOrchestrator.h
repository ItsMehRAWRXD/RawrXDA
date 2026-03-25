// BackendOrchestrator.h — Implements the never-instantiated BackendOrchestrator
// Enhancement 1:  Dynamic backend switching (CPU → Vulkan → HIP → DML → Titan)
// Enhancement 2:  Model sharding across multiple Vulkan devices
// Enhancement 3:  Distributed KV-cache across CPU + GPU memory
// Enhancement 4:  Speculative decoding integration (draft + verify)
// Enhancement 5:  Model hot-swapping without process restart
// Enhancement 6:  Multi-tenant request handling (per-user KV isolation)
// Enhancement 7:  Priority-based inference queue (VIP / normal / batch)
// Enhancement 8:  Semantic cache: reuse completions for similar prompts
// Enhancement 9:  Incremental context compression (sink + rolling window)
// Enhancement 10: Adaptive quantization based on realtime memory pressure
// Enhancement 11: Plugin kernel architecture (GGML / oneAPI / BLAS swap)
// Enhancement 12: Backend failover + automatic health checks
// Enhancement 13: Request batching / micro-batching for throughput
// Enhancement 14: Prometheus metrics integration via InferenceProfiler
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <functional>
#include <optional>
#include <queue>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <array>

namespace RawrXD {

// Forward declarations
class InferenceProfiler;

// ─── Backend identifier ───────────────────────────────────────────────────────
enum class BackendKind : uint8_t {
    CPU    = 0,
    Vulkan = 1,
    HIP    = 2,
    DML    = 3,   // DirectML
    Titan  = 4,   // RawrXD_Titan plugin
    Count
};

inline const char* BackendName(BackendKind k) {
    static constexpr const char* names[] = { "CPU","Vulkan","HIP","DML","Titan" };
    return names[static_cast<int>(k)];
}

// ─── Backend health ───────────────────────────────────────────────────────────
struct BackendHealth {
    BackendKind kind;
    bool        available      = false;
    bool        healthy        = true;     // set false after errors
    int         error_count    = 0;
    double      avg_latency_ms = 0.0;
    float       gpu_util_pct   = 0.f;
    float       vram_used_mb   = 0.f;
    float       vram_total_mb  = 0.f;
    std::chrono::steady_clock::time_point last_check;
};

// ─── Request priority ─────────────────────────────────────────────────────────
enum class RequestPriority : uint8_t { VIP = 0, Normal = 1, Batch = 2 };

// ─── Inference request ────────────────────────────────────────────────────────
struct InferRequest {
    uint64_t                  id;
    std::string               prompt;
    std::string               tenant_id;         // multi-tenant isolation key
    RequestPriority           priority = RequestPriority::Normal;
    int                       max_tokens = 512;
    int                       seed       = 42;
    std::optional<BackendKind> preferred_backend;
    std::function<void(const std::string& token)> stream_cb;
    std::function<void(const std::string& completion, const std::string& metadata)> complete_cb;
    std::chrono::steady_clock::time_point enqueue_time;
};

// ─── Semantic cache entry ─────────────────────────────────────────────────────
struct SemanticCacheEntry {
    std::string prompt_hash;
    std::string completion;
    uint64_t    hit_count = 0;
    std::chrono::steady_clock::time_point last_accessed;
};

// ─── Kernel plugin interface ──────────────────────────────────────────────────
struct KernelPlugin {
    std::string name;
    // Called to run a matrix multiply: C = A * B
    std::function<void(const float* A, const float* B, float* C,
                       int M, int K, int N)> matmul_f32;
    // Called to run softmax in-place over [rows x cols]
    std::function<void(float* data, int rows, int cols)> softmax_f32;
    bool loaded = false;
};

// ─── Model shard descriptor ───────────────────────────────────────────────────
struct ModelShard {
    int           device_index;    // Vulkan physical device index
    int           layer_start;
    int           layer_end;       // inclusive
    size_t        vram_bytes;
    std::vector<std::string> shard_files; // GGUF files assigned to this device
    size_t        assigned_file_bytes = 0;
    bool          loaded = false;
};

// ─── Context compression config ───────────────────────────────────────────────
struct ContextCompressionConfig {
    int    sink_tokens      = 4;    // number of 'attention sink' tokens kept verbatim
    int    rolling_window   = 512;  // tokens retained in full
    int    summary_budget   = 128;  // tokens budget for soft compression summary
    bool   enabled          = true;
};

// ─── Speculative decoding config ─────────────────────────────────────────────
struct SpecDecodingConfig {
    std::string draft_model_path;   // small fast model for draft tokens
    int         draft_beam         = 4;   // tokens to generate speculatively
    bool        enabled            = false;
};

// ─── BackendOrchestrator ─────────────────────────────────────────────────────
class BackendOrchestrator {
public:
    static BackendOrchestrator& Instance();

    // --- Lifecycle ---
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_initialized.load(); }

    // ---- Enhancement 1: Dynamic backend selection ----
    BackendKind SelectBestBackend(size_t model_bytes, int seq_len) const;
    void        ForceBackend(BackendKind k);
    BackendKind GetActiveBackend() const;
    bool        SwitchBackend(BackendKind target);   // hot-switch

    // ---- Enhancement 2: Model sharding ----
    bool ShardModel(const std::string& model_path, const std::vector<int>& device_indices);
    std::vector<ModelShard> GetShards() const;
    void ClearShards();

    // ---- Enhancement 3: Distributed KV-cache ----
    void   SetKVCacheGPUBudgetMB(float mb);
    void   SetKVCacheCPUBudgetMB(float mb);
    float  GetKVCacheGPUUsedMB() const;
    float  GetKVCacheCPUUsedMB() const;
    void   EvictKVCacheForTenant(const std::string& tenant_id);

    // ---- Enhancement 4: Speculative decoding ----
    bool   EnableSpecDecoding(SpecDecodingConfig cfg);
    void   DisableSpecDecoding();
    double GetSpecDecodingAcceptRate() const;

    // ---- Enhancement 5: Model hot-swapping ----
    bool   LoadModel(const std::string& path, const std::string& tag);
    bool   SwapToModel(const std::string& tag);
    bool   UnloadModel(const std::string& tag);
    std::vector<std::string> GetLoadedModelTags() const;

    // ---- Enhancement 6: Multi-tenant isolation ----
    void   CreateTenant(const std::string& tenant_id, int max_ctx = 4096);
    void   RemoveTenant(const std::string& tenant_id);
    void   SetTenantKVQuota(const std::string& tenant_id, float mb);
    std::vector<std::string> GetActiveTenants() const;

    // ---- Enhancement 7: Priority inference queue ----
    uint64_t Enqueue(InferRequest req);
    void     Cancel(uint64_t request_id);
    int      GetQueueDepth(RequestPriority p = RequestPriority::Normal) const;
    double   GetQueuedWaitTimeMs(uint64_t request_id) const;

    // ---- Enhancement 8: Semantic cache ----
    void   EnableSemanticCache(size_t max_entries = 1024);
    void   DisableSemanticCache();
    bool   LookupCache(const std::string& prompt, std::string& out_completion) const;
    void   InsertCache(const std::string& prompt, const std::string& completion);
    void   ClearCache();
    size_t GetCacheSize() const;
    double GetCacheHitRate() const;

    // ---- Enhancement 9: Incremental context compression ----
    void        SetContextCompression(ContextCompressionConfig cfg);
    std::string CompressContext(const std::string& raw_context) const;
    int         EstimateCompressedTokenCount(int raw_tokens) const;

    // ---- Enhancement 10: Adaptive quantization ----
    void SetAdaptiveQuantization(bool enabled);
    int  GetCurrentQuantBits() const;
    bool ForceQuantLevel(int bits);  // 4, 5, 6, 8

    // ---- Enhancement 11: Kernel plugins ----
    bool RegisterKernelPlugin(KernelPlugin plugin);
    bool ActivateKernelPlugin(const std::string& name);
    std::string GetActiveKernelPlugin() const;

    // ---- Enhancement 12: Backend failover + health checks ----
    BackendHealth GetBackendHealth(BackendKind k) const;
    void          StartHealthCheckThread(std::chrono::seconds interval = std::chrono::seconds(30));
    void          StopHealthCheckThread();
    void          SetFailoverOrder(std::vector<BackendKind> order);

    // ---- Enhancement 13: Micro-batching ----
    void  SetMicroBatchWindow(std::chrono::milliseconds window);
    void  SetMaxBatchSize(int n);
    int   GetCurrentBatchSize() const;
    void  FlushBatch();

    // ---- Enhancement 14: Prometheus metrics ----
    void   EnableMetricsExport(const std::string& output_path, int interval_s = 60);
    void   DisableMetricsExport();
    void   DumpMetricsNow() const;

private:
    BackendOrchestrator();
    ~BackendOrchestrator();

    std::atomic<bool>        m_initialized{false};
    mutable std::shared_mutex m_backend_mtx;
    BackendKind               m_active_backend = BackendKind::Vulkan;
    BackendKind               m_forced_backend  = BackendKind::Count; // Count = auto
    std::vector<BackendKind>  m_failover_order;

    // Health
    mutable std::mutex m_health_mtx;
    BackendHealth      m_health[static_cast<int>(BackendKind::Count)];
    std::thread        m_health_thread;
    std::atomic<bool>  m_health_running{false};
    std::condition_variable m_health_cv;
    std::mutex              m_health_cv_mtx;
    void HealthCheckLoop(std::chrono::seconds interval);
    void RefreshBackendHealth(BackendKind k);

    // Shards
    mutable std::mutex    m_shard_mtx;
    std::vector<ModelShard> m_shards;

    // KV cache budget
    std::atomic<float> m_kv_gpu_budget_mb{8192.f};
    std::atomic<float> m_kv_cpu_budget_mb{16384.f};
    std::atomic<float> m_kv_gpu_used_mb{0.f};
    std::atomic<float> m_kv_cpu_used_mb{0.f};

    // Spec decoding
    SpecDecodingConfig  m_spec_cfg;
    std::atomic<double> m_spec_accept_rate{0.0};

    // Model hot-swap registry
    mutable std::mutex m_model_mtx;
    std::unordered_map<std::string, std::string> m_model_paths;  // tag → path
    std::string m_active_model_tag;

    // Tenants
    struct TenantInfo { int max_ctx; float kv_quota_mb; };
    mutable std::mutex m_tenant_mtx;
    std::unordered_map<std::string, TenantInfo> m_tenants;

    // Request queue (3 priority buckets)
    mutable std::mutex                          m_queue_mtx;
    std::condition_variable                     m_queue_cv;
    std::array<std::deque<InferRequest>, 3>     m_queues;  // indexed by priority
    std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> m_enqueue_times;
    std::atomic<uint64_t>                       m_next_req_id{1};
    std::thread                                 m_dispatch_thread;
    std::atomic<bool>                           m_dispatch_running{false};
    void DispatchLoop();
    bool RunInference(const InferRequest& req);

    // Semantic cache
    mutable std::shared_mutex m_cache_mtx;
    std::unordered_map<std::string, SemanticCacheEntry> m_cache;
    size_t                m_cache_max = 0;
    bool                  m_cache_enabled = false;
    mutable std::atomic<uint64_t> m_cache_hits{0};
    mutable std::atomic<uint64_t> m_cache_lookups{0};
    std::string HashPrompt(const std::string& prompt) const;

    // Context compression
    ContextCompressionConfig m_ctx_comp;

    // Adaptive quantization
    bool             m_adaptive_quant = false;
    std::atomic<int> m_quant_bits{4};

    // Kernel plugins
    mutable std::mutex            m_plugin_mtx;
    std::vector<KernelPlugin>     m_plugins;
    std::string                   m_active_plugin;

    // Micro-batch
    std::chrono::milliseconds m_batch_window{5};
    int                       m_max_batch_size = 8;
    std::atomic<int>          m_cur_batch_size{0};

    // Metrics export
    std::string    m_metrics_path;
    std::thread    m_metrics_thread;
    std::atomic<bool> m_metrics_running{false};
    void MetricsExportLoop(int interval_s);
};

} // namespace RawrXD
