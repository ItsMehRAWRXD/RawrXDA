#pragma once
// LayerPrefetchEngine.h — Async DMA pipeline for VRAM layer streaming
// Runs a background thread that pre-stages upcoming transformer layers
// into GPU memory before the compute pipeline needs them.
// No Qt. No exceptions. C++20 only.

#include "VRAMHotpatchScaler.h"
#include "../RawrXD_SignalSlot.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <functional>

namespace RawrXD {

// ═══════════════════════════════════════════════════════════════════════════════
// DMARequest — A single layer transfer request for the prefetch queue
// ═══════════════════════════════════════════════════════════════════════════════
struct DMARequest {
    int      layerIdx    = -1;
    int      priority    = 0;     // Higher = more urgent (current+1 > current+3)
    uint64_t requestTick = 0;
    bool     cancelled   = false;

    // Priority queue ordering (highest priority first)
    bool operator<(const DMARequest& other) const {
        return priority < other.priority; // std::priority_queue is max-heap
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// LayerPrefetchEngine — Async background DMA with cancellation support
//
// Design rationale (reverse-engineering perspective):
//   ROCm/HIP's oversubscription manager does demand paging — it waits for
//   a page fault, then transfers. This is the WORST case: every fault stalls
//   the GPU compute queue until the page arrives over PCIe.
//
//   We do the opposite: predictive prefetch. Since transformer execution is
//   purely sequential (layer 0 → 1 → 2 → ... → N), we KNOW what's needed
//   next. We issue DMA transfers 2-3 layers ahead of the compute cursor.
//
//   The GPU computes on layer N while we DMA layers N+1, N+2 into zones.
//   By the time layer N finishes, N+1 is already resident → zero stall.
//
//   This converts random PCIe page faults into a pipelined stream.
// ═══════════════════════════════════════════════════════════════════════════════
class LayerPrefetchEngine {
public:
    Signal<int, double>         onPrefetchComplete;  // (layerIdx, dmaMs)
    Signal<int>                 onPrefetchCancelled;
    Signal<const std::string&>  onEngineEvent;

private:
    VRAMHotpatchScaler* m_scaler = nullptr;

    // DMA work queue
    std::priority_queue<DMARequest> m_requestQueue;
    std::mutex              m_queueMutex;
    std::condition_variable m_queueCV;

    // Worker thread
    std::thread       m_worker;
    std::atomic<bool> m_running{false};
    std::atomic<int>  m_computeCursor{0};  // Current layer being computed

    // Bandwidth tracking for pipeline tuning
    double m_avgDmaMs    = 5.0;   // Rolling average DMA time per layer
    double m_avgComputeMs = 3.0;  // Rolling average compute time per layer
    int    m_optimalDepth = 2;    // Computed optimal prefetch depth

    // Cancel set — layers queued but no longer needed (e.g., if we jumped)
    std::unordered_set<int> m_cancelledLayers;

public:
    LayerPrefetchEngine() = default;

    ~LayerPrefetchEngine() {
        Stop();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Start/Stop
    // ═══════════════════════════════════════════════════════════════════════════
    bool Start(VRAMHotpatchScaler* scaler)
    {
        if (!scaler) return false;
        m_scaler = scaler;
        m_running = true;

        m_worker = std::thread([this]() { WorkerLoop(); });

        onEngineEvent.emit("[Prefetch] Engine started — background DMA active");
        return true;
    }

    void Stop()
    {
        m_running = false;
        m_queueCV.notify_all();
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // AdvanceComputeCursor — Called when forward pass moves to next layer
    // This triggers prefetch planning
    // ═══════════════════════════════════════════════════════════════════════════
    void AdvanceComputeCursor(int layerIdx)
    {
        m_computeCursor = layerIdx;

        // Cancel any prefetch requests for layers we've already passed
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            // Mark layers behind cursor as cancelled
            for (int i = 0; i < layerIdx; i++) {
                m_cancelledLayers.insert(i);
            }
        }

        // Issue prefetch requests for upcoming layers
        int depth = m_optimalDepth;
        for (int d = 1; d <= depth; d++) {
            int targetLayer = layerIdx + d;
            RequestPrefetch(targetLayer, depth - d + 10); // Higher priority for closer layers
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // RequestPrefetch — Queue a layer for async DMA
    // ═══════════════════════════════════════════════════════════════════════════
    void RequestPrefetch(int layerIdx, int priority = 5)
    {
        if (!m_running) return;

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);

            // Don't re-queue cancelled or already-computed layers
            if (m_cancelledLayers.count(layerIdx)) return;

            DMARequest req;
            req.layerIdx = layerIdx;
            req.priority = priority;
            req.requestTick = GetTickCount64();

            m_requestQueue.push(req);
        }

        m_queueCV.notify_one();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // UpdateTimings — Called after each layer compute to tune pipeline
    // ═══════════════════════════════════════════════════════════════════════════
    void UpdateTimings(double computeMs, double lastDmaMs)
    {
        // Exponential moving average
        m_avgComputeMs = m_avgComputeMs * 0.8 + computeMs * 0.2;
        if (lastDmaMs > 0) {
            m_avgDmaMs = m_avgDmaMs * 0.8 + lastDmaMs * 0.2;
        }

        // Optimal depth: how many DMA transfers can overlap with one compute?
        // If DMA takes 5ms and compute takes 3ms, we need depth ≥ 2 to hide latency
        if (m_avgComputeMs > 0) {
            m_optimalDepth = (int)std::ceil(m_avgDmaMs / m_avgComputeMs) + 1;
            m_optimalDepth = std::clamp(m_optimalDepth, 1, m_scaler ? m_scaler->GetMaxZones() - 1 : 8);
        }
    }

    int GetOptimalDepth() const { return m_optimalDepth; }
    double GetAvgDmaMs() const { return m_avgDmaMs; }
    double GetAvgComputeMs() const { return m_avgComputeMs; }
    bool IsRunning() const { return m_running; }

private:
    // ═══════════════════════════════════════════════════════════════════════════
    // WorkerLoop — Background thread processing DMA requests
    // ═══════════════════════════════════════════════════════════════════════════
    void WorkerLoop()
    {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        while (m_running) {
            DMARequest req;

            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_queueCV.wait(lock, [this]() {
                    return !m_requestQueue.empty() || !m_running;
                });

                if (!m_running) break;
                if (m_requestQueue.empty()) continue;

                req = m_requestQueue.top();
                m_requestQueue.pop();

                // Skip cancelled requests
                if (m_cancelledLayers.count(req.layerIdx)) {
                    onPrefetchCancelled.emit(req.layerIdx);
                    continue;
                }
            }

            // Execute the DMA
            if (m_scaler) {
                auto t0 = std::chrono::high_resolution_clock::now();
                
                // This calls into VRAMHotpatchScaler which does the actual DMA
                void* ptr = m_scaler->EnsureLayerResident(req.layerIdx);
                
                auto t1 = std::chrono::high_resolution_clock::now();
                double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

                if (ptr) {
                    onPrefetchComplete.emit(req.layerIdx, ms);
                }
            }
        }

        onEngineEvent.emit("[Prefetch] Engine stopped");
    }
};

} // namespace RawrXD
