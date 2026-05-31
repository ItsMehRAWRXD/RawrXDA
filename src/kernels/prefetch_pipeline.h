#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64)
#  include <immintrin.h>
#endif

namespace RawrXD::Kernels {

// PrefetchPipeline runs a background thread that issues cache prefetch
// hints for the next layer's weights while the current layer is being
// computed.  This hides DRAM latency for large models on CPU.
class PrefetchPipeline {
public:
    struct LayerInfo {
        const void* data;     // pointer to weight block
        size_t      bytes;    // total size to prefetch
    };

    explicit PrefetchPipeline(size_t n_layers)
        : m_layers(n_layers), m_stop(false), m_nextLayer(-1)
    {
        m_worker = std::thread([this] { workerLoop(); });
    }

    ~PrefetchPipeline() {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_stop = true;
        }
        m_cv.notify_all();
        if (m_worker.joinable()) m_worker.join();
    }

    void registerLayer(size_t idx, const void* data, size_t bytes) {
        m_layers[idx] = {data, bytes};
    }

    // Trigger prefetch for layer `idx` in the background.
    void prefetch(size_t idx) {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_nextLayer = (int)idx;
        }
        m_cv.notify_one();
    }

private:
    static void prefetchRange(const void* ptr, size_t bytes) {
        const char* p   = reinterpret_cast<const char*>(ptr);
        const char* end = p + bytes;
        constexpr size_t stride = 64; // typical cache line size
        for (; p < end; p += stride) {
#if defined(__x86_64__) || defined(_M_X64)
            _mm_prefetch(p, _MM_HINT_T1);
#elif defined(__aarch64__)
            __builtin_prefetch(p, 0, 1);
#endif
        }
    }

    void workerLoop() {
        while (true) {
            int target = -1;
            {
                std::unique_lock<std::mutex> lk(m_mtx);
                m_cv.wait(lk, [this] { return m_stop || m_nextLayer >= 0; });
                if (m_stop) return;
                target = m_nextLayer;
                m_nextLayer = -1;
            }
            if (target >= 0 && target < (int)m_layers.size()) {
                const auto& li = m_layers[(size_t)target];
                if (li.data && li.bytes > 0) {
                    prefetchRange(li.data, li.bytes);
                }
            }
        }
    }

    std::vector<LayerInfo>  m_layers;
    std::thread             m_worker;
    std::mutex              m_mtx;
    std::condition_variable m_cv;
    std::atomic<bool>       m_stop;
    int                     m_nextLayer;
};

// RAII guard: tells the pipeline to start prefetching the NEXT layer as
// soon as this object is constructed, so the I/O overlaps computation.
class LayerComputeGuard {
public:
    LayerComputeGuard(PrefetchPipeline& pipeline, size_t next_layer)
        : m_pipeline(pipeline)
    {
        m_pipeline.prefetch(next_layer);
    }
    ~LayerComputeGuard() = default;

private:
    PrefetchPipeline& m_pipeline;
};

} // namespace RawrXD::Kernels
