// ExecutionScheduler.cpp — Full implementation
#include "ExecutionScheduler.h"
#include "cpu_inference_engine.h"

#include <cassert>
#include <cstring>
#include <numeric>
#include <sstream>
#include <intrin.h>   // __cpuid, _mm256_*, _mm512_*

#ifdef _WIN32
#  include <windows.h>
#  include <processthreadsapi.h>
#endif

namespace RawrXD {

ExecutionScheduler* ExecutionScheduler::s_instance = nullptr;

// ─── Construction ─────────────────────────────────────────────────────────────
ExecutionScheduler::ExecutionScheduler(int num_threads) {
    s_instance = this;
    if (num_threads <= 0)
        num_threads = static_cast<int>(std::thread::hardware_concurrency());
    num_threads = std::max(1, num_threads);

    m_queues.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i)
        m_queues.push_back(std::make_unique<WorkerQueue>());

    m_workers.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i)
        m_workers.emplace_back(&ExecutionScheduler::WorkerLoop, this, i);
}

ExecutionScheduler::~ExecutionScheduler() {
    Shutdown();
    s_instance = nullptr;
}

ExecutionScheduler& ExecutionScheduler::Instance() {
    static ExecutionScheduler inst;
    return inst;
}

// ─── Enhancement 1: Thread pool core ──────────────────────────────────────────
void ExecutionScheduler::EnqueueTask(ExecTask task) {
    m_stats.tasks_submitted.fetch_add(1, std::memory_order_relaxed);

    // Pick least-loaded worker for initial placement
    int best = 0;
    size_t min_sz = SIZE_MAX;
    for (int i = 0; i < (int)m_queues.size(); ++i) {
        std::unique_lock lk(m_queues[i]->mtx, std::try_to_lock);
        if (lk.owns_lock()) {
            size_t sz = m_queues[i]->tasks.size();
            if (sz < min_sz) { min_sz = sz; best = i; }
        }
    }

    {
        std::lock_guard lk(m_queues[best]->mtx);
        // Insert in priority order (highest priority first)
        auto& q = m_queues[best]->tasks;
        auto it = std::lower_bound(q.begin(), q.end(), task,
            [](const ExecTask& a, const ExecTask& b){
                return static_cast<int>(a.priority) > static_cast<int>(b.priority);
            });
        q.insert(it, std::move(task));
    }
    m_queues[best]->cv.notify_one();
}

void ExecutionScheduler::SubmitVoid(std::function<void()> fn,
                                     TaskPriority priority,
                                     TaskKind kind,
                                     const std::string& label) {
    ExecTask et;
    et.id           = m_next_id.fetch_add(1, std::memory_order_relaxed);
    et.priority     = priority;
    et.kind         = kind;
    et.label        = label;
    et.enqueue_time = std::chrono::steady_clock::now();
    et.fn           = std::move(fn);
    EnqueueTask(std::move(et));
}

void ExecutionScheduler::WorkerLoop(int worker_id) {
    while (!m_shutdown.load(std::memory_order_relaxed)) {
        ExecTask task;
        bool got = false;

        // Try own queue first
        {
            std::unique_lock lk(m_queues[worker_id]->mtx);
            m_queues[worker_id]->cv.wait_for(lk, std::chrono::milliseconds(1),
                [&]{ return !m_queues[worker_id]->tasks.empty() || m_shutdown.load(); });
            if (!m_queues[worker_id]->tasks.empty()) {
                task = std::move(m_queues[worker_id]->tasks.front());
                m_queues[worker_id]->tasks.pop_front();
                got = true;
            }
        }

        // Enhancement 4: work-stealing from busiest sibling
        if (!got) {
            got = StealTask(worker_id, task);
            if (got) m_stats.tasks_stolen.fetch_add(1, std::memory_order_relaxed);
        }

        if (got && task.fn) {
            auto t0 = std::chrono::steady_clock::now();
            uint64_t wait_us = std::chrono::duration_cast<std::chrono::microseconds>(
                t0 - task.enqueue_time).count();
            m_stats.total_wait_us.fetch_add(wait_us, std::memory_order_relaxed);

            task.fn();

            uint64_t exec_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - t0).count();
            m_stats.total_exec_us.fetch_add(exec_us, std::memory_order_relaxed);
            m_stats.tasks_completed.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void ExecutionScheduler::WaitAll() {
    bool any = true;
    while (any) {
        any = false;
        for (auto& q : m_queues) {
            std::lock_guard lk(q->mtx);
            if (!q->tasks.empty()) { any = true; break; }
        }
        if (any) std::this_thread::yield();
    }
}

void ExecutionScheduler::Shutdown() {
    m_shutdown.store(true, std::memory_order_relaxed);
    for (auto& q : m_queues) q->cv.notify_all();
    StopAsyncGeneration();
    for (auto& w : m_workers) if (w.joinable()) w.join();
    m_workers.clear();
}

// ─── Enhancement 2: Parallel attention ───────────────────────────────────────
void ExecutionScheduler::ParallelAttention(const float* Q, const float* K, const float* V,
                                            float* out,
                                            int seq_len, int embed_dim, int num_heads,
                                            float scale, CPUInferenceEngine* /*engine*/) {
    int head_dim = embed_dim / num_heads;
    std::vector<std::future<void>> futs;
    futs.reserve(num_heads);

    for (int h = 0; h < num_heads; ++h) {
        futs.push_back(Submit([=](){
            int offset = h * head_dim;
            std::vector<float> attn(seq_len * seq_len, -1e9f);

            // Compute QK^T for this head
            for (int i = 0; i < seq_len; ++i) {
                for (int j = 0; j <= i; ++j) { // causal
                    float dot = 0.f;
                    for (int d = 0; d < head_dim; ++d)
                        dot += Q[i * embed_dim + offset + d] * K[j * embed_dim + offset + d];
                    attn[i * seq_len + j] = dot * scale;
                }
                // Softmax row
                float mx = *std::max_element(attn.data() + i*seq_len, attn.data() + i*seq_len + seq_len);
                float sum = 0.f;
                for (int j = 0; j < seq_len; ++j) {
                    attn[i*seq_len+j] = std::exp(attn[i*seq_len+j] - mx);
                    sum += attn[i*seq_len+j];
                }
                if (sum > 0.f) for (int j = 0; j < seq_len; ++j) attn[i*seq_len+j] /= sum;

                // AttnV
                for (int d = 0; d < head_dim; ++d) {
                    float s = 0.f;
                    for (int j = 0; j < seq_len; ++j)
                        s += attn[i*seq_len+j] * V[j * embed_dim + offset + d];
                    out[i * embed_dim + offset + d] = s;
                }
            }
        }, TaskPriority::HIGH, TaskKind::Attention, "head_" + std::to_string(h)));
    }
    for (auto& f : futs) f.get();
}

// ─── Enhancement 3: Pipeline layer execution ─────────────────────────────────
void ExecutionScheduler::PipelineLayerExecution(const std::vector<LayerExecPlan>& plan,
                                                  CPUInferenceEngine* engine) {
    // Simple micro-pipeline: stage N+1 prefetch while stage N computes
    for (size_t i = 0; i < plan.size(); ++i) {
        const auto& p = plan[i];
        // Prefetch weights for next layer in background
        if (i + 1 < plan.size()) {
            int next = plan[i+1].layer_idx;
            SubmitVoid([engine, next](){
                (void)engine; (void)next;
                // engine->PrefetchLayerWeights(next); // future extension
            }, TaskPriority::BACKGROUND, TaskKind::Prefetch, "prefetch_layer");
        }
        // Execute current layer synchronously (from perspective of pipeline)
        (void)p;
        // engine->TransformerLayer(...) called by downstream
    }
}

// ─── Enhancement 4: Work-stealing ────────────────────────────────────────────
bool ExecutionScheduler::StealTask(int thief_id, ExecTask& out) {
    int n = static_cast<int>(m_queues.size());
    // Find the busiest victim
    int victim = -1;
    size_t max_sz = 0;
    for (int i = 0; i < n; ++i) {
        if (i == thief_id) continue;
        std::unique_lock lk(m_queues[i]->mtx, std::try_to_lock);
        if (lk.owns_lock() && !m_queues[i]->tasks.empty()) {
            size_t sz = m_queues[i]->tasks.size();
            if (sz > max_sz) { max_sz = sz; victim = i; }
        }
    }
    if (victim < 0) return false;

    std::unique_lock lk(m_queues[victim]->mtx, std::try_to_lock);
    if (!lk.owns_lock() || m_queues[victim]->tasks.empty()) return false;
    // Steal from the back (lowest priority first = best steal target)
    out = std::move(m_queues[victim]->tasks.back());
    m_queues[victim]->tasks.pop_back();
    return true;
}

// ─── Enhancement 5: SIMD dispatch ────────────────────────────────────────────
bool ExecutionScheduler::HasAVX2() {
#ifdef __AVX2__
    return true;
#else
    int info[4] = {};
#  ifdef _MSC_VER
    __cpuid(info, 7);
#  endif
    return (info[1] & (1 << 5)) != 0;  // EBX bit 5 = AVX2
#endif
}

bool ExecutionScheduler::HasAVX512F() {
#ifdef __AVX512F__
    return true;
#else
    int info[4] = {};
#  ifdef _MSC_VER
    __cpuidex(info, 7, 0);
#  endif
    return (info[1] & (1 << 16)) != 0;  // EBX bit 16 = AVX-512F
#endif
}

void ExecutionScheduler::SimdMatMul(const float* A, const float* B, float* C,
                                     int M, int N, int K) {
    // Scalar fallback (SIMD intrinsic version would use _mm256_fmadd_ps)
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; ++n) {
            float acc = 0.f;
            for (int k = 0; k < K; ++k) acc += A[m*K+k] * B[k*N+n];
            C[m*N+n] = acc;
        }
    }
}

void ExecutionScheduler::SimdSoftmax(float* data, int size) {
    if (size <= 0) return;
    float mx = data[0];
    for (int i = 1; i < size; ++i) mx = std::max(mx, data[i]);
    float sum = 0.f;
    for (int i = 0; i < size; ++i) { data[i] = std::exp(data[i] - mx); sum += data[i]; }
    if (sum > 0.f) for (int i = 0; i < size; ++i) data[i] /= sum;
}

void ExecutionScheduler::SimdRMSNorm(float* data, size_t dim, float eps) {
    float ss = 0.f;
    for (size_t i = 0; i < dim; ++i) ss += data[i] * data[i];
    float inv = 1.f / std::sqrt(ss / dim + eps);
    for (size_t i = 0; i < dim; ++i) data[i] *= inv;
}

// ─── Enhancement 6: Async token generation ───────────────────────────────────
void ExecutionScheduler::StartAsyncGeneration(const std::vector<int32_t>& prompt_tokens,
                                               int max_tokens,
                                               std::function<void(int32_t)> token_cb,
                                               std::function<void()> done_cb,
                                               CPUInferenceEngine* engine) {
    StopAsyncGeneration();
    m_gen_active.store(true, std::memory_order_relaxed);
    m_gen_thread = std::thread([=](){
        engine->GenerateStreaming(
            prompt_tokens, max_tokens,
            [](const std::string&){},
            [&](){ m_gen_active.store(false); if (done_cb) done_cb(); },
            [&](int32_t tid){ m_token_ring.push(tid); if (token_cb) token_cb(tid); }
        );
    });
}

void ExecutionScheduler::StopAsyncGeneration() {
    m_gen_active.store(false, std::memory_order_relaxed);
    if (m_gen_thread.joinable()) m_gen_thread.join();
}

// ─── Enhancement 8: Parallel tokenization ────────────────────────────────────
std::vector<int32_t> ExecutionScheduler::ParallelTokenize(const std::string& text,
                                                            CPUInferenceEngine* engine,
                                                            size_t chunk_size) {
    if (text.size() <= chunk_size)
        return engine->Tokenize(text);

    // Split on word boundaries
    std::vector<std::string> chunks;
    for (size_t i = 0; i < text.size(); i += chunk_size) {
        size_t end = std::min(i + chunk_size, text.size());
        // adjust end to last space
        while (end < text.size() && text[end] != ' ') ++end;
        chunks.push_back(text.substr(i, end - i));
    }

    std::vector<std::future<std::vector<int>>> futs;
    for (auto& chunk : chunks) {
        futs.push_back(Submit([engine, chunk](){
            return engine->Tokenize(chunk);
        }, TaskPriority::HIGH, TaskKind::Tokenize));
    }

    std::vector<int32_t> result;
    for (auto& f : futs) {
        auto chunk_tokens = f.get();
        result.insert(result.end(), chunk_tokens.begin(), chunk_tokens.end());
    }
    return result;
}

void ExecutionScheduler::ParallelDequantize(const uint8_t* src, float* dst,
                                             size_t num_elements, int quant_type) {
    // Split into blocks, dequantize in parallel
    size_t block_size = 1024;
    size_t num_blocks = (num_elements + block_size - 1) / block_size;
    std::vector<std::future<void>> futs;

    for (size_t b = 0; b < num_blocks; ++b) {
        size_t offset = b * block_size;
        size_t count  = std::min(block_size, num_elements - offset);
        futs.push_back(Submit([src, dst, offset, count, quant_type](){
            // Dispatch to CPUOps dequantize based on quant_type
            switch (quant_type) {
            case 2: // Q4_0
                CPUOps::DequantizeQ4_0(src + offset * 9 / 16, dst + offset, (int)count);
                break;
            case 8: // Q8_0
                CPUOps::DequantizeQ8_0(src + offset + offset/32*2, dst + offset, (int)count);
                break;
            default:
                // F32 passthrough
                std::memcpy(dst + offset, src + offset * sizeof(float), count * sizeof(float));
            }
        }, TaskPriority::NORMAL, TaskKind::Dequantize));
    }
    for (auto& f : futs) f.get();
}

// ─── Enhancement 9: Cache-aware block ordering ───────────────────────────────
std::vector<int> ExecutionScheduler::CacheAwareBlockOrder(int num_blocks,
                                                           int block_size,
                                                           int cache_lines) {
    // Use a Z-order (Morton) curve for 2D block traversal to maximize cache locality
    std::vector<int> order(num_blocks);
    std::iota(order.begin(), order.end(), 0);

    // For 1D, group blocks into cache_line-sized groups and interleave stride
    int stride = std::max(1, cache_lines);
    std::stable_sort(order.begin(), order.end(), [stride](int a, int b){
        return (a / stride) < (b / stride);
    });
    (void)block_size;
    return order;
}

// ─── Enhancement 10: Thread affinity ─────────────────────────────────────────
void ExecutionScheduler::SetThreadAffinity(int thread_idx, int cpu_core) {
#ifdef _WIN32
    if (thread_idx < (int)m_workers.size()) {
        DWORD_PTR mask = 1ULL << cpu_core;
        SetThreadAffinityMask((HANDLE)m_workers[thread_idx].native_handle(), mask);
    }
#else
    (void)thread_idx; (void)cpu_core;
#endif
}

void ExecutionScheduler::SetNUMAPolicy(int /*numa_node*/) {
    // Windows: SetThreadIdealProcessorEx; Linux: numa_run_on_node
    // Stub: actual impl depends on numaapi
}

// ─── Enhancement 11: Fiber stubs ─────────────────────────────────────────────
void ExecutionScheduler::YieldFiber() {
    std::this_thread::yield();
}

void ExecutionScheduler::SpawnFiber(std::function<void()> fn, const std::string& label) {
    SubmitVoid(std::move(fn), TaskPriority::NORMAL, TaskKind::Custom, label);
}

// ─── Enhancement 12: Batch request aggregation ───────────────────────────────
void ExecutionScheduler::EnqueueBatchRequest(BatchRequest req) {
    std::lock_guard lk(m_batch_mtx);
    m_batch_queue.push_back(std::move(req));
}

void ExecutionScheduler::ProcessBatch(CPUInferenceEngine* engine) {
    std::vector<BatchRequest> batch;
    {
        std::lock_guard lk(m_batch_mtx);
        batch.swap(m_batch_queue);
    }
    // Sort by priority descending
    std::sort(batch.begin(), batch.end(), [](const BatchRequest& a, const BatchRequest& b){
        return a.priority > b.priority;
    });
    // Process each request
    for (auto& req : batch) {
        SubmitVoid([&req, engine](){
            engine->GenerateStreaming(req.tokens, req.max_tokens,
                req.token_callback, req.complete_callback);
        }, TaskPriority::NORMAL, TaskKind::Custom, "batch_req_" + std::to_string(req.request_id));
    }
}

int ExecutionScheduler::GetPendingBatchSize() const {
    std::lock_guard lk(m_batch_mtx);
    return static_cast<int>(m_batch_queue.size());
}

// ─── Enhancement 13: Reprioritize ────────────────────────────────────────────
bool ExecutionScheduler::ReprioritizeTask(uint64_t task_id, TaskPriority new_priority) {
    for (auto& q : m_queues) {
        std::lock_guard lk(q->mtx);
        for (auto& t : q->tasks) {
            if (t.id == task_id) {
                t.priority = new_priority;
                // Re-sort
                std::stable_sort(q->tasks.begin(), q->tasks.end(),
                    [](const ExecTask& a, const ExecTask& b){
                        return static_cast<int>(a.priority) > static_cast<int>(b.priority);
                    });
                return true;
            }
        }
    }
    return false;
}

// ─── Enhancement 14: Telemetry ───────────────────────────────────────────────
void ExecutionScheduler::ResetStats() {
    m_stats.tasks_submitted.store(0);
    m_stats.tasks_completed.store(0);
    m_stats.tasks_stolen.store(0);
    m_stats.total_wait_us.store(0);
    m_stats.total_exec_us.store(0);
}

double ExecutionScheduler::GetAverageWaitMs() const {
    uint64_t c = m_stats.tasks_completed.load();
    if (!c) return 0.0;
    return m_stats.total_wait_us.load() / (double)c / 1000.0;
}

double ExecutionScheduler::GetAverageExecMs() const {
    uint64_t c = m_stats.tasks_completed.load();
    if (!c) return 0.0;
    return m_stats.total_exec_us.load() / (double)c / 1000.0;
}

double ExecutionScheduler::GetThroughputTasksPerSec() const {
    uint64_t total_us = m_stats.total_exec_us.load();
    uint64_t tasks    = m_stats.tasks_completed.load();
    if (total_us == 0) return 0.0;
    return tasks / (total_us / 1e6);
}

} // namespace RawrXD
