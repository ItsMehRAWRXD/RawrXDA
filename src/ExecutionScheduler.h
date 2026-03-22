// ExecutionScheduler.h — Parallel execution scheduler for RawrXD inference
// Implements the ExecutionScheduler declared as friend in CPUInferenceEngine.
// Enhancement 1:  Generic thread pool (work-stealing, lock-free queues)
// Enhancement 2:  Multi-head attention head-level parallelism
// Enhancement 3:  Pipeline parallelism across transformer layers
// Enhancement 4:  Work-stealing scheduler for load balancing
// Enhancement 5:  SIMD-vectorized operation dispatch
// Enhancement 6:  Asynchronous token generation pipeline
// Enhancement 7:  Lock-free SPSC queue for token streaming
// Enhancement 8:  Parallel tokenization and dequantization
// Enhancement 9:  Cache-aware computation ordering (Morton curve)
// Enhancement 10: NUMA-aware thread affinity
// Enhancement 11: Fiber/coroutine-based cooperative multitasking
// Enhancement 12: Batch request aggregation
// Enhancement 13: Priority-based work queue
// Enhancement 14: Execution telemetry per task type
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <algorithm>
#include <future>
#include <optional>
#include <chrono>

namespace RawrXD {

// ─── Task priority ────────────────────────────────────────────────────────────
enum class TaskPriority : uint8_t {
    IDLE       = 0,
    BACKGROUND = 1,
    NORMAL     = 5,
    HIGH       = 8,
    CRITICAL   = 10
};

// ─── Task kind (for telemetry) ────────────────────────────────────────────────
enum class TaskKind : uint8_t {
    MatMul,
    Attention,
    FeedForward,
    Tokenize,
    Dequantize,
    Prefetch,
    Batch,
    Custom
};

// ─── Execution task ───────────────────────────────────────────────────────────
struct ExecTask {
    uint64_t              id       = 0;
    TaskPriority          priority = TaskPriority::NORMAL;
    TaskKind              kind     = TaskKind::Custom;
    std::string           label;
    std::function<void()> fn;
    std::chrono::steady_clock::time_point enqueue_time{};
};

// ─── Execution stats ──────────────────────────────────────────────────────────
struct ExecStats {
    std::atomic<uint64_t> tasks_submitted{0};
    std::atomic<uint64_t> tasks_completed{0};
    std::atomic<uint64_t> tasks_stolen{0};       // work-stealing events
    std::atomic<uint64_t> total_wait_us{0};       // sum of queue wait times
    std::atomic<uint64_t> total_exec_us{0};       // sum of execution times
};

// ─── Layer execution plan ─────────────────────────────────────────────────────
struct LayerExecPlan {
    int    layer_idx;
    int    seq_len;
    std::vector<int> head_indices;   // heads to compute
    bool   use_vulkan = false;
    bool   use_cpu    = true;
};

// ─── Batch request ────────────────────────────────────────────────────────────
struct BatchRequest {
    uint64_t                           request_id;
    std::vector<int32_t>               tokens;
    int                                max_tokens;
    int                                priority;
    std::function<void(const std::string&)> token_callback;
    std::function<void()>              complete_callback;
};

// ─── Lock-free SPSC ring buffer for token streaming ───────────────────────────
template<typename T, size_t CAP>
class SPSCRing {
    T                     buf_[CAP];
    std::atomic<size_t>   head_{0};
    std::atomic<size_t>   tail_{0};
public:
    bool push(T val) {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t next = (h + 1) % CAP;
        if (next == tail_.load(std::memory_order_acquire)) return false; // full
        buf_[h] = std::move(val);
        head_.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T& out) {
        size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) return false; // empty
        out = std::move(buf_[t]);
        tail_.store((t + 1) % CAP, std::memory_order_release);
        return true;
    }
    bool empty() const {
        return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
    }
};

// ─── ExecutionScheduler ───────────────────────────────────────────────────────
class CPUInferenceEngine; // forward

class ExecutionScheduler {
public:
    // ---- Construction ----
    explicit ExecutionScheduler(int num_threads = 0 /* 0 = hardware_concurrency */);
    ~ExecutionScheduler();

    static ExecutionScheduler& Instance();

    // ---- Enhancement 1: Thread pool ----
    // Submit a callable, returns a future
    template<typename F>
    auto Submit(F&& fn,
                TaskPriority priority = TaskPriority::NORMAL,
                TaskKind kind = TaskKind::Custom,
                const std::string& label = "") -> std::future<decltype(fn())> {
        using R = decltype(fn());
        auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(fn));
        std::future<R> fut = task->get_future();

        ExecTask et;
        et.id           = m_next_id.fetch_add(1, std::memory_order_relaxed);
        et.priority     = priority;
        et.kind         = kind;
        et.label        = label;
        et.enqueue_time = std::chrono::steady_clock::now();
        et.fn           = [task](){ (*task)(); };

        EnqueueTask(std::move(et));
        return fut;
    }

    void SubmitVoid(std::function<void()> fn,
                    TaskPriority priority = TaskPriority::NORMAL,
                    TaskKind kind = TaskKind::Custom,
                    const std::string& label = "");

    void WaitAll();   // Drain all pending tasks
    void Shutdown();

    // ---- Enhancement 2: Multi-head attention parallelism ----
    // Splits attention computation across heads, executes in parallel
    void ParallelAttention(const float* Q, const float* K, const float* V,
                           float* out,
                           int seq_len, int embed_dim, int num_heads,
                           float scale, CPUInferenceEngine* engine = nullptr);

    // ---- Enhancement 3: Pipeline parallelism ----
    // Executes a sequence of transformer layers as a pipeline across threads
    void PipelineLayerExecution(const std::vector<LayerExecPlan>& plan,
                                 CPUInferenceEngine* engine);

    // ---- Enhancement 4: Work-stealing scheduling ----
    // Steal task from the busiest worker queue
    bool StealTask(int thief_id, ExecTask& out);

    // ---- Enhancement 5: SIMD dispatch ----
    // Vectorized matmul dispatch using AVX2/AVX-512 based on CPUID
    static void SimdMatMul(const float* A, const float* B, float* C,
                            int M, int N, int K);
    static void SimdSoftmax(float* data, int size);
    static void SimdRMSNorm(float* data, size_t dim, float eps = 1e-5f);
    static bool HasAVX2();
    static bool HasAVX512F();

    // ---- Enhancement 6: Async token generation pipeline ----
    // Starts a background generation pipeline; tokens arrive via callback
    void StartAsyncGeneration(const std::vector<int32_t>& prompt_tokens,
                               int max_tokens,
                               std::function<void(int32_t)> token_cb,
                               std::function<void()> done_cb,
                               CPUInferenceEngine* engine);
    void StopAsyncGeneration();

    // ---- Enhancement 7: Lock-free token stream (SPSC) ----
    using TokenRing = SPSCRing<int32_t, 4096>;
    TokenRing& GetTokenRing() { return m_token_ring; }

    // ---- Enhancement 8: Parallel tokenization ----
    // Split text into chunks, tokenize in parallel, merge results
    std::vector<int32_t> ParallelTokenize(const std::string& text,
                                           CPUInferenceEngine* engine,
                                           size_t chunk_size = 512);

    // ---- Enhancement 8b: Parallel dequantization ----
    // Dequantize all blocks in a tensor in parallel
    void ParallelDequantize(const uint8_t* src, float* dst,
                             size_t num_elements, int quant_type);

    // ---- Enhancement 9: Cache-aware compute ordering ----
    // Returns processing order for attention blocks minimizing cache misses
    std::vector<int> CacheAwareBlockOrder(int num_blocks, int block_size,
                                           int cache_lines = 8);

    // ---- Enhancement 10: NUMA thread affinity ----
    void SetThreadAffinity(int thread_idx, int cpu_core);
    void SetNUMAPolicy(int numa_node);

    // ---- Enhancement 11: Cooperative fiber execution ----
    // Yield current fiber and schedule another (cooperative multitasking)
    void YieldFiber();
    void SpawnFiber(std::function<void()> fn, const std::string& name = "");

    // ---- Enhancement 12: Batch request aggregation ----
    void EnqueueBatchRequest(BatchRequest req);
    void ProcessBatch(CPUInferenceEngine* engine);
    int  GetPendingBatchSize() const;

    // ---- Enhancement 13: Priority queue ----
    // Adjust priority of queued task by id
    bool ReprioritizeTask(uint64_t task_id, TaskPriority new_priority);

    // ---- Enhancement 14: Telemetry ----
    const ExecStats& GetStats() const { return m_stats; }
    void ResetStats();
    double GetAverageWaitMs() const;
    double GetAverageExecMs() const;
    double GetThroughputTasksPerSec() const;

    // ---- Thread count ----
    int ThreadCount() const { return static_cast<int>(m_workers.size()); }

private:
    void EnqueueTask(ExecTask task);
    void WorkerLoop(int worker_id);

    // Per-worker queues (work-stealing)
    struct WorkerQueue {
        std::mutex                          mtx;
        std::deque<ExecTask>                tasks; // priority-ordered
        std::condition_variable             cv;
    };
    std::vector<std::unique_ptr<WorkerQueue>> m_queues;
    std::vector<std::thread>                  m_workers;
    std::atomic<bool>                         m_shutdown{false};
    std::atomic<uint64_t>                     m_next_id{1};

    ExecStats                                 m_stats;

    // Batch request queue
    mutable std::mutex    m_batch_mtx;
    std::vector<BatchRequest> m_batch_queue;

    // Token ring for async generation
    TokenRing   m_token_ring;
    std::atomic<bool> m_gen_active{false};
    std::thread m_gen_thread;

    static ExecutionScheduler* s_instance;
};

} // namespace RawrXD
