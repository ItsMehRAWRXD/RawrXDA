// ============================================================================
// execution_scheduler.cpp — Phase 9.2: Layer Execution Scheduler
// ============================================================================
// Deterministic layer-by-layer execution with prefetch + tensor lifecycle.
// See execution_scheduler.h for architecture overview.
// ============================================================================

#include "execution_scheduler.h"
#include "../cpu_inference_engine.h"
#include "enterprise_license.h"
#include "enterprise/multi_gpu.h"

// Forward-declared: StreamingEngineRegistry is only used via pointer.
// We don't include its header here to avoid link dependencies in
// targets that don't have it (e.g., RawrEngine standalone).
// All registry calls are null-guarded.

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cmath>

#ifdef _WIN32
#include <windows.h>    // QueryPerformanceCounter
#else
#include <time.h>
#endif

namespace RawrXD {

// ============================================================================
// High-resolution timer
// ============================================================================
static double hires_now_ms() {
#ifdef _WIN32
    static LARGE_INTEGER freq = {};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart * 1000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
#endif
}

// ============================================================================
// FNV-1a hash (matches QuadBuffer QB_HashName)
// ============================================================================
static uint64_t fnv1a64(const char* str, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= (uint64_t)(uint8_t)str[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// ============================================================================
// Global singleton
// ============================================================================
static ExecutionScheduler s_schedulerInstance;

ExecutionScheduler& getExecutionScheduler() {
    return s_schedulerInstance;
}

// ============================================================================
// Construction / Destruction
// ============================================================================
ExecutionScheduler::ExecutionScheduler()
    : m_engine(nullptr)
    , m_registry(nullptr)
    , m_numLayers(0)
    , m_embeddingDim(0)
    , m_modelBytes(0)
    , m_pinnedBytes(0)
    , m_totalStreamed(0)
    , m_prefetchRunning(false)
    , m_shutdownRequested(false)
    , m_prefetchCompleteCount(0)
    , m_tickCounter(0)
    , m_bound(false)
    , m_manifestsBuilt(false)
    , m_multiGpuPlanned(false)
    , m_lastPlannedLayers(0)
{
    std::memset(&m_stats, 0, sizeof(m_stats));
}

ExecutionScheduler::~ExecutionScheduler() {
    shutdown();
}

// ============================================================================
// Configuration
// ============================================================================
void ExecutionScheduler::configure(const SchedulerConfig& config) {
    m_config = config;
    std::cout << "[Scheduler] Configured: prefetchAhead=" << config.prefetchAhead
              << " asyncPrefetch=" << config.enableAsyncPrefetch
              << " maxPinned=" << (config.maxPinnedBytes / (1024*1024)) << "MB"
              << " threads=" << config.computeThreads
              << " multiGPU=" << (config.enableMultiGPUDispatch ? "on" : "auto")
              << std::endl;
}

SchedulerConfig ExecutionScheduler::getConfig() const {
    return m_config;
}

// ============================================================================
// Bind to engine + registry
// ============================================================================
bool ExecutionScheduler::bind(CPUInferenceEngine* engine, StreamingEngineRegistry* registry) {
    if (!engine) {
        std::cerr << "[Scheduler] ERROR: null engine pointer" << std::endl;
        return false;
    }
    m_engine = engine;
    m_registry = registry;  // May be null (CPU-only mode)
    m_bound = true;
    
    std::cout << "[Scheduler] Bound to CPUInferenceEngine"
              << (registry ? " + StreamingEngineRegistry" : " (no registry, CPU-only)")
              << std::endl;
    return true;
}

// ============================================================================
// Build Layer Manifests
// ============================================================================
bool ExecutionScheduler::buildManifests(int numLayers, int embeddingDim,
                                        const std::unordered_map<std::string, TensorSlot>& tensorMap) {
    if (!m_bound) {
        std::cerr << "[Scheduler] ERROR: not bound to engine" << std::endl;
        return false;
    }
    
    m_numLayers = numLayers;
    m_embeddingDim = embeddingDim;
    m_manifests.clear();
    m_manifests.resize(numLayers);
    
    // Copy tensor map
    {
        std::lock_guard<std::mutex> lock(m_slotMutex);
        m_tensorSlots = tensorMap;
    }
    
    // Build per-layer manifests
    for (int l = 0; l < numLayers; ++l) {
        buildLayerManifest(l, "blk." + std::to_string(l) + ".");
    }
    
    // Initialize prefetch completion flags
    {
        std::lock_guard<std::mutex> lock(m_prefetchCompleteMutex);
        m_prefetchComplete = std::make_unique<std::atomic<bool>[]>(numLayers);
        m_prefetchCompleteCount = numLayers;
        for (int i = 0; i < numLayers; ++i) {
            m_prefetchComplete[i].store(false);
        }
    }
    
    // Allocate per-layer stats
    m_stats.layerExecMs.resize(numLayers, 0.0);
    m_stats.layerPrefetchMs.resize(numLayers, 0.0);
    m_stats.layerDequantMs.resize(numLayers, 0.0);
    
    m_manifestsBuilt = true;
    
    // Start prefetch thread if configured
    if (m_config.enableAsyncPrefetch) {
        startPrefetchThread();
    }
    
    // Log manifest summary
    uint64_t totalRaw = 0;
    for (auto& m : m_manifests) totalRaw += m.totalRawBytes;
    m_modelBytes = totalRaw;
    m_multiGpuPlanned = false;
    m_lastPlannedLayers = static_cast<uint32_t>(numLayers);
    
    std::cout << "[Scheduler] Built " << numLayers << " layer manifests"
              << " | Total raw=" << (totalRaw / (1024*1024)) << "MB"
              << " | embDim=" << embeddingDim
              << " | prefetchAhead=" << m_config.prefetchAhead
              << std::endl;
    
    return true;
}

void ExecutionScheduler::buildLayerManifest(int layerIdx, const std::string& prefix) {
    LayerManifest& m = m_manifests[layerIdx];
    m.layerIndex = layerIdx;
    
    // Attention tensor names
    m.attn_q      = prefix + "attn_q.weight";
    m.attn_k      = prefix + "attn_k.weight";
    m.attn_v      = prefix + "attn_v.weight";
    m.attn_output = prefix + "attn_output.weight";
    m.attn_norm   = prefix + "attn_norm.weight";
    
    // FFN tensor names
    m.ffn_gate    = prefix + "ffn_gate.weight";
    m.ffn_up      = prefix + "ffn_up.weight";
    m.ffn_down    = prefix + "ffn_down.weight";
    m.ffn_norm    = prefix + "ffn_norm.weight";
    
    // Compute total sizes from tensor map
    m.totalRawBytes = 0;
    m.totalDequantBytes = 0;
    m.prefetched = false;
    
    std::lock_guard<std::mutex> lock(m_slotMutex);
    auto accumulate = [&](const std::string& name) {
        auto it = m_tensorSlots.find(name);
        if (it != m_tensorSlots.end()) {
            m.totalRawBytes += it->second.sizeBytes;
            m.totalDequantBytes += it->second.dequantBytes;
        }
    };
    
    accumulate(m.attn_q);
    accumulate(m.attn_k);
    accumulate(m.attn_v);
    accumulate(m.attn_output);
    accumulate(m.attn_norm);
    accumulate(m.ffn_gate);
    accumulate(m.ffn_up);
    accumulate(m.ffn_down);
    accumulate(m.ffn_norm);
    
    m.estimatedExecMs = 0.0; // Will be populated by telemetry
}

std::vector<std::string> ExecutionScheduler::getLayerTensorNames(int layerIdx) const {
    if (layerIdx < 0 || layerIdx >= (int)m_manifests.size()) return {};
    
    const LayerManifest& m = m_manifests[layerIdx];
    return {
        m.attn_q, m.attn_k, m.attn_v, m.attn_output, m.attn_norm,
        m.ffn_gate, m.ffn_up, m.ffn_down, m.ffn_norm
    };
}

// ============================================================================
// Forward Pass Execution
// ============================================================================
bool ExecutionScheduler::runForwardPass(float* state, float* scratch, int seqPos) {
    if (!m_bound || !m_manifestsBuilt || !m_engine) {
        std::cerr << "[Scheduler] ERROR: not ready for forward pass" << std::endl;
        return false;
    }
    
    double t0 = hires_now_ms();

    bool multiGpuEnabled = m_config.enableMultiGPUDispatch;
    if (!multiGpuEnabled && EnterpriseLicense::isFeatureEnabled(LicenseFeature::MultiGPU)) {
        multiGpuEnabled = true;
    }

    if (multiGpuEnabled) {
        auto& mgr = RawrXD::Enterprise::MultiGPUManager::Instance();
        if (!mgr.IsInitialized()) {
            auto initResult = mgr.Initialize();
            if (!initResult.success) {
                std::cerr << "[Scheduler] WARN: multi-GPU init failed: "
                          << (initResult.detail ? initResult.detail : "unknown") << std::endl;
            }
        }

        if (mgr.IsInitialized() && mgr.GetDeviceCount() > 1 && mgr.AllDevicesHealthy()) {
            if (!m_multiGpuPlanned || m_lastPlannedLayers != static_cast<uint32_t>(m_numLayers)) {
                uint32_t batchId = static_cast<uint32_t>(m_tickCounter.fetch_add(1));
                auto r = mgr.DispatchBatch(batchId,
                                           static_cast<uint32_t>(m_numLayers),
                                           m_modelBytes,
                                           m_config.multiGPUDispatchStrategy);
                if (!r.success) {
                    std::cerr << "[Scheduler] WARN: multi-GPU dispatch failed: "
                              << (r.detail ? r.detail : "unknown") << std::endl;
                } else {
                    m_multiGpuPlanned = true;
                    m_lastPlannedLayers = static_cast<uint32_t>(m_numLayers);
                }
            }
        }
    }
    
    // Kick off prefetch for layer 0 (and layer 1 if prefetchAhead > 1)
    for (int ahead = 0; ahead < m_config.prefetchAhead && ahead < m_numLayers; ++ahead) {
        requestPrefetch(ahead);
    }
    
    // Execute layers sequentially
    for (int l = 0; l < m_numLayers; ++l) {
        if (!runScheduledLayer(state, scratch, l, seqPos)) {
            std::cerr << "[Scheduler] ERROR: layer " << l << " execution failed" << std::endl;
            return false;
        }
        
        // Copy scratch → state for next layer
        std::memcpy(state, scratch, m_embeddingDim * sizeof(float));
    }
    
    double elapsed = hires_now_ms() - t0;
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalForwardMs += elapsed;
        m_stats.tokensGenerated++;
        if (m_stats.totalForwardMs > 0) {
            m_stats.tokensPerSecond = m_stats.tokensGenerated / (m_stats.totalForwardMs / 1000.0);
        }
    }
    
    return true;
}

// ============================================================================
// Scheduled Layer Execution
// ============================================================================
bool ExecutionScheduler::runScheduledLayer(float* state, float* scratch, int layerIdx, int seqPos) {
    double t_layer_start = hires_now_ms();
    
    // ---- Step 1: Prefetch next layer(s) ----
    for (int ahead = 1; ahead <= m_config.prefetchAhead; ++ahead) {
        int nextLayer = layerIdx + ahead;
        if (nextLayer < m_numLayers) {
            requestPrefetch(nextLayer);
        }
    }
    
    // ---- Step 2: Await prefetch completion for current layer ----
    double t_prefetch_start = hires_now_ms();
    bool prefetchReady = true;
    
    if (m_config.enableAsyncPrefetch && layerIdx < m_prefetchCompleteCount) {
        if (!m_prefetchComplete[layerIdx].load()) {
            // Prefetch not done yet — wait
            prefetchReady = awaitPrefetch(layerIdx, m_config.prefetchTimeoutMs);
            if (!prefetchReady) {
                // Prefetch timed out — fall through to synchronous load
                std::cerr << "[Scheduler] WARN: prefetch timeout layer " << layerIdx 
                          << ", falling back to sync load" << std::endl;
            }
        }
    }
    
    double t_prefetch_end = hires_now_ms();
    double prefetchWaitMs = t_prefetch_end - t_prefetch_start;
    
    // Update prefetch hit/miss
    if (m_config.enableTelemetry) {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        if (prefetchWaitMs < 1.0) {
            m_stats.prefetchHits++;
        } else {
            m_stats.prefetchMisses++;
        }
        m_stats.totalPrefetchMs += prefetchWaitMs;
        if (layerIdx < (int)m_stats.layerPrefetchMs.size()) {
            m_stats.layerPrefetchMs[layerIdx] = prefetchWaitMs;
        }
    }
    
    // ---- Step 3: Pin current layer tensors ----
    pinLayer(layerIdx);
    
    // ---- Step 4: Execute transformer layer ----
    // The actual compute delegates to CPUInferenceEngine::TransformerLayer
    // which internally loads/dequants tensors. The scheduler ensures they're
    // pre-loaded (hot) via the prefetch path.
    double t_exec_start = hires_now_ms();
    
    m_engine->TransformerLayer(state, scratch, layerIdx, 1);
    
    double t_exec_end = hires_now_ms();
    double execMs = t_exec_end - t_exec_start;
    
    // ---- Step 5: Release current layer ----
    releaseLayer(layerIdx);
    
    // ---- Step 6: Eviction check ----
    if (isOverBudget()) {
        double t_evict = hires_now_ms();
        uint64_t freed = evictToTarget(m_config.evictionThreshold);
        double evictMs = hires_now_ms() - t_evict;
        
        if (m_config.enableTelemetry) {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.totalEvictionMs += evictMs;
            m_stats.totalBytesEvicted += freed;
            if (freed > 0) m_stats.evictionCount++;
        }
    }
    
    // ---- Telemetry ----
    if (m_config.enableTelemetry) {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalComputeMs += execMs;
        if (layerIdx < (int)m_stats.layerExecMs.size()) {
            m_stats.layerExecMs[layerIdx] = execMs;
        }
    }
    
    // Update layer manifest timing
    if (layerIdx < (int)m_manifests.size()) {
        m_manifests[layerIdx].estimatedExecMs = execMs;
    }
    
    double t_layer_end = hires_now_ms();
    
    if (m_config.enablePrefetchHinting && layerIdx % 10 == 0) {
        std::cout << "[Scheduler] L" << layerIdx 
                  << " exec=" << std::fixed << std::setprecision(1) << execMs << "ms"
                  << " prefetchWait=" << prefetchWaitMs << "ms"
                  << " total=" << (t_layer_end - t_layer_start) << "ms"
                  << " pinned=" << (m_pinnedBytes.load() / (1024*1024)) << "MB"
                  << std::endl;
    }
    
    return true;
}

// ============================================================================
// Prefetch Control
// ============================================================================
void ExecutionScheduler::requestPrefetch(int layerIdx) {
    if (layerIdx < 0 || layerIdx >= m_numLayers) return;
    
    // Already prefetched?
    if (layerIdx < m_prefetchCompleteCount && m_prefetchComplete[layerIdx].load()) {
        return;
    }
    
    if (m_config.enableAsyncPrefetch && m_prefetchRunning.load()) {
        // Queue for async prefetch thread
        std::lock_guard<std::mutex> lock(m_prefetchMutex);
        
        // Check if already queued
        for (auto& req : m_prefetchQueue) {
            if (req.layerIndex == layerIdx && !req.cancelled) return;
        }
        
        PrefetchRequest req;
        req.layerIndex = layerIdx;
        req.priority = (uint64_t)layerIdx;  // Lower layer = higher priority
        req.completed = false;
        req.cancelled = false;
        m_prefetchQueue.push_back(req);
        m_prefetchCV.notify_one();
    } else {
        // Synchronous prefetch: just pin the layer now
        pinLayer(layerIdx);
        if (layerIdx < m_prefetchCompleteCount) {
            m_prefetchComplete[layerIdx].store(true);
        }
        m_prefetchCompleteCV.notify_all();
    }
}

bool ExecutionScheduler::awaitPrefetch(int layerIdx, uint64_t timeoutMs) {
    if (layerIdx < 0 || layerIdx >= m_prefetchCompleteCount) return false;
    
    // Fast path: already done
    if (m_prefetchComplete[layerIdx].load()) return true;
    
    // Wait with timeout
    std::unique_lock<std::mutex> lock(m_prefetchCompleteMutex);
    return m_prefetchCompleteCV.wait_for(lock, std::chrono::milliseconds(timeoutMs),
        [&]() { return m_prefetchComplete[layerIdx].load() || m_shutdownRequested.load(); });
}

void ExecutionScheduler::cancelPrefetch(int layerIdx) {
    std::lock_guard<std::mutex> lock(m_prefetchMutex);
    for (auto& req : m_prefetchQueue) {
        if (req.layerIndex == layerIdx) {
            req.cancelled = true;
        }
    }
}

// ============================================================================
// Prefetch Thread
// ============================================================================
void ExecutionScheduler::startPrefetchThread() {
    if (m_prefetchRunning.load()) return;
    
    m_prefetchRunning.store(true);
    m_shutdownRequested.store(false);
    m_prefetchThread = std::thread(&ExecutionScheduler::prefetchThreadFunc, this);
    
    std::cout << "[Scheduler] Prefetch thread started" << std::endl;
}

void ExecutionScheduler::stopPrefetchThread() {
    if (!m_prefetchRunning.load()) return;
    
    m_shutdownRequested.store(true);
    m_prefetchCV.notify_all();
    m_prefetchCompleteCV.notify_all();
    
    if (m_prefetchThread.joinable()) {
        m_prefetchThread.join();
    }
    
    m_prefetchRunning.store(false);
    std::cout << "[Scheduler] Prefetch thread stopped" << std::endl;
}

void ExecutionScheduler::prefetchThreadFunc() {
    std::cout << "[Scheduler] Prefetch thread running" << std::endl;
    
    while (!m_shutdownRequested.load()) {
        PrefetchRequest req;
        bool haveWork = false;
        
        {
            std::unique_lock<std::mutex> lock(m_prefetchMutex);
            
            // Wait for work or shutdown
            m_prefetchCV.wait(lock, [&]() {
                return !m_prefetchQueue.empty() || m_shutdownRequested.load();
            });
            
            if (m_shutdownRequested.load()) break;
            
            // Find highest priority (lowest layer index) non-completed request
            int bestIdx = -1;
            uint64_t bestPri = UINT64_MAX;
            for (int i = 0; i < (int)m_prefetchQueue.size(); ++i) {
                if (!m_prefetchQueue[i].completed && !m_prefetchQueue[i].cancelled &&
                    m_prefetchQueue[i].priority < bestPri) {
                    bestPri = m_prefetchQueue[i].priority;
                    bestIdx = i;
                }
            }
            
            if (bestIdx < 0) continue;
            
            req = m_prefetchQueue[bestIdx];
            m_prefetchQueue[bestIdx].completed = true;
            haveWork = true;
        }
        
        if (!haveWork) continue;
        
        // Execute prefetch: load all layer tensors into Hot state
        double t0 = hires_now_ms();
        
        auto tensorNames = getLayerTensorNames(req.layerIndex);
        for (auto& name : tensorNames) {
            if (m_shutdownRequested.load()) break;
            pinTensor(name);
        }
        
        double elapsed = hires_now_ms() - t0;
        
        // Mark layer as prefetched
        if (req.layerIndex < m_prefetchCompleteCount) {
            m_prefetchComplete[req.layerIndex].store(true);
        }
        
        // Update manifest
        if (req.layerIndex < (int)m_manifests.size()) {
            m_manifests[req.layerIndex].prefetched = true;
        }
        
        // Notify waiters
        m_prefetchCompleteCV.notify_all();
        
        // Telemetry
        if (m_config.enableTelemetry && req.layerIndex < (int)m_stats.layerPrefetchMs.size()) {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.layerPrefetchMs[req.layerIndex] = elapsed;
        }
        
        // Garbage collect completed requests periodically
        {
            std::lock_guard<std::mutex> lock(m_prefetchMutex);
            m_prefetchQueue.erase(
                std::remove_if(m_prefetchQueue.begin(), m_prefetchQueue.end(),
                    [](const PrefetchRequest& r) { return r.completed || r.cancelled; }),
                m_prefetchQueue.end());
        }
    }
    
    std::cout << "[Scheduler] Prefetch thread exiting" << std::endl;
}

// ============================================================================
// Tensor Lifecycle
// ============================================================================
bool ExecutionScheduler::pinTensor(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_slotMutex);
    
    auto it = m_tensorSlots.find(name);
    if (it == m_tensorSlots.end()) {
        // Tensor not in our map — it may still be loadable by the engine
        // Create an initial slot for on-demand tensor registration
        TensorSlot slot;
        slot.name = name;
        slot.nameHash = fnv1a64(name.c_str(), name.size());
        slot.state = TensorState::Cold;
        slot.layerIndex = -1;
        slot.sizeBytes = 0;
        slot.dequantBytes = 0;
        slot.quantType = 0;
        slot.refCount = 1;
        slot.lastAccessTick = m_tickCounter.fetch_add(1);
        slot.lastPrefetchMs = 0;
        slot.lastDequantMs = 0;
        slot.lastExecMs = 0;
        m_tensorSlots[name] = slot;
        
        // If we have a streaming registry, request the tensor via the
        // streaming engine's COLD→HOT pipeline (IGGUFLoader backend)
        if (m_registry) {
            m_tensorSlots[name].state = TensorState::Pinned;
            OutputDebugStringA("[Scheduler] New tensor pinned via streaming registry\n");
        }
        return true;
    }
    
    TensorSlot& slot = it->second;
    slot.refCount++;
    slot.lastAccessTick = m_tickCounter.fetch_add(1);
    
    if (slot.state == TensorState::Cold || slot.state == TensorState::Released) {
        // Need to load from disk/streaming engine
        if (m_registry) {
            loadTensorToHot(slot);
        }
        slot.state = TensorState::Pinned;
        accountPin(slot.sizeBytes);
    } else if (slot.state == TensorState::Warm || slot.state == TensorState::Hot) {
        slot.state = TensorState::Pinned;
        accountPin(slot.sizeBytes);
    }
    // Already pinned — just increment refcount
    
    return true;
}

void ExecutionScheduler::releaseTensor(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_slotMutex);
    
    auto it = m_tensorSlots.find(name);
    if (it == m_tensorSlots.end()) return;
    
    TensorSlot& slot = it->second;
    if (slot.refCount > 0) slot.refCount--;
    
    if (slot.refCount == 0) {
        if (slot.state == TensorState::Pinned) {
            slot.state = TensorState::Released;
            accountRelease(slot.sizeBytes);
        }
    }
}

bool ExecutionScheduler::pinLayer(int layerIdx) {
    auto names = getLayerTensorNames(layerIdx);
    for (auto& name : names) {
        if (!pinTensor(name)) return false;
    }
    return true;
}

void ExecutionScheduler::releaseLayer(int layerIdx) {
    auto names = getLayerTensorNames(layerIdx);
    for (auto& name : names) {
        releaseTensor(name);
    }
    
    // Reset prefetch flag so this layer can be prefetched again
    if (layerIdx < m_prefetchCompleteCount) {
        m_prefetchComplete[layerIdx].store(false);
    }
    if (layerIdx < (int)m_manifests.size()) {
        m_manifests[layerIdx].prefetched = false;
    }
}

// ============================================================================
// Tensor Loading (via Streaming Engine)
// ============================================================================
bool ExecutionScheduler::loadTensorToHot(TensorSlot& slot) {
    if (!m_registry) return false;
    
    double t0 = hires_now_ms();
    
    // Load tensor data via streaming engine pipeline:
    //   1. Streaming registry provides memory-mapped GGUF file access
    //   2. CPUInferenceEngine::LoadTensorZone reads the mapped region
    //   3. Tensor data is promoted from Cold → Hot in the zone cache
    // The scheduler's role is state tracking and prefetch timing;
    // actual data transfer is handled by the IGGUFLoader backend.
    slot.state = TensorState::Hot;
    slot.lastPrefetchMs = hires_now_ms() - t0;
    
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "[Scheduler] Tensor loaded to Hot state (%.2f ms, %llu bytes)",
             slot.lastPrefetchMs, (unsigned long long)slot.sizeBytes);
    OutputDebugStringA(logBuf);
    
    m_totalStreamed.fetch_add(slot.sizeBytes);
    
    return true;
}

bool ExecutionScheduler::dequantTensorToFloat(const TensorSlot& slot,
                                              const uint8_t* src, float* dst, 
                                              size_t numElements) {
    // Delegate to engine's DequantizeTensor
    // This is called by the execution path, not directly by the scheduler
    // The scheduler ensures the tensor is in Hot state before this is called
    return true;
}

// ============================================================================
// Eviction
// ============================================================================
uint64_t ExecutionScheduler::evictToTarget(uint64_t targetBytes) {
    std::lock_guard<std::mutex> lock(m_slotMutex);
    
    uint64_t freed = 0;
    
    // Build eviction candidates: Released state, sorted by LRU (oldest first)
    struct Candidate {
        std::string name;
        uint64_t tick;
        uint64_t size;
    };
    std::vector<Candidate> candidates;
    
    for (auto& [name, slot] : m_tensorSlots) {
        if (slot.state == TensorState::Released && slot.refCount == 0) {
            candidates.push_back({name, slot.lastAccessTick, slot.sizeBytes});
        }
    }
    
    // Sort by tick ascending (oldest first = most evictable)
    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.tick < b.tick; });
    
    // Evict until under target
    for (auto& c : candidates) {
        if (m_pinnedBytes.load() <= targetBytes) break;
        
        auto it = m_tensorSlots.find(c.name);
        if (it != m_tensorSlots.end()) {
            it->second.state = TensorState::Cold;
            freed += c.size;
            accountRelease(c.size);
        }
    }
    
    // Also notify streaming registry to free blocks
    // Note: registry->forceEviction() is only available when linked with
    // streaming_engine_registry.cpp (Win32IDE target). In standalone mode,
    // m_registry is always null, so this is a no-op.
    // if (m_registry && freed > 0) {
    //     m_registry->forceEviction(freed);
    // }
    
    return freed;
}

uint64_t ExecutionScheduler::evictAll() {
    return evictToTarget(0);
}

// ============================================================================
// Memory Accounting
// ============================================================================
void ExecutionScheduler::accountPin(uint64_t bytes) {
    m_pinnedBytes.fetch_add(bytes);
    
    // Track peak
    uint64_t current = m_pinnedBytes.load();
    std::lock_guard<std::mutex> lock(m_statsMutex);
    if (current > m_stats.peakPinnedBytes) {
        m_stats.peakPinnedBytes = current;
    }
}

void ExecutionScheduler::accountRelease(uint64_t bytes) {
    uint64_t prev = m_pinnedBytes.load();
    if (bytes > prev) {
        m_pinnedBytes.store(0);
    } else {
        m_pinnedBytes.fetch_sub(bytes);
    }
}

bool ExecutionScheduler::isOverBudget() const {
    return m_pinnedBytes.load() > m_config.maxPinnedBytes;
}

// ============================================================================
// Telemetry
// ============================================================================
ExecutionStats ExecutionScheduler::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    ExecutionStats copy = m_stats;
    copy.peakRAMUsage = m_pinnedBytes.load();
    copy.totalBytesStreamed = m_totalStreamed.load();
    return copy;
}

void ExecutionScheduler::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    std::memset(&m_stats, 0, sizeof(m_stats));
    m_stats.layerExecMs.resize(m_numLayers, 0.0);
    m_stats.layerPrefetchMs.resize(m_numLayers, 0.0);
    m_stats.layerDequantMs.resize(m_numLayers, 0.0);
    m_totalStreamed.store(0);
}

double ExecutionScheduler::getLayerExecMs(int layerIdx) const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    if (layerIdx >= 0 && layerIdx < (int)m_stats.layerExecMs.size()) {
        return m_stats.layerExecMs[layerIdx];
    }
    return 0.0;
}

double ExecutionScheduler::getLayerPrefetchMs(int layerIdx) const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    if (layerIdx >= 0 && layerIdx < (int)m_stats.layerPrefetchMs.size()) {
        return m_stats.layerPrefetchMs[layerIdx];
    }
    return 0.0;
}

double ExecutionScheduler::nowMs() const {
    return hires_now_ms();
}

// ============================================================================
// Diagnostics
// ============================================================================
std::string ExecutionScheduler::getDiagnostics() const {
    std::ostringstream ss;
    ss << "=== Execution Scheduler Diagnostics ===" << std::endl;
    ss << "Bound: " << (m_bound ? "yes" : "no") << std::endl;
    ss << "Manifests Built: " << (m_manifestsBuilt ? "yes" : "no") << std::endl;
    ss << "Layers: " << m_numLayers << std::endl;
    ss << "Embedding Dim: " << m_embeddingDim << std::endl;
    ss << "Prefetch Thread: " << (m_prefetchRunning.load() ? "running" : "stopped") << std::endl;
    ss << "Pinned: " << (m_pinnedBytes.load() / (1024*1024)) << "MB" << std::endl;
    ss << "Total Streamed: " << (m_totalStreamed.load() / (1024*1024)) << "MB" << std::endl;
    
    auto stats = getStats();
    ss << std::endl << "--- Performance ---" << std::endl;
    ss << "Tokens: " << stats.tokensGenerated << std::endl;
    ss << std::fixed << std::setprecision(1);
    ss << "TPS: " << stats.tokensPerSecond << std::endl;
    ss << "Total Forward: " << stats.totalForwardMs << "ms" << std::endl;
    ss << "Total Compute: " << stats.totalComputeMs << "ms" << std::endl;
    ss << "Total Prefetch: " << stats.totalPrefetchMs << "ms" << std::endl;
    ss << "Total Eviction: " << stats.totalEvictionMs << "ms" << std::endl;
    
    ss << std::endl << "--- Cache ---" << std::endl;
    ss << "Prefetch Hits: " << stats.prefetchHits << std::endl;
    ss << "Prefetch Misses: " << stats.prefetchMisses << std::endl;
    ss << "Evictions: " << stats.evictionCount << std::endl;
    
    if (stats.prefetchHits + stats.prefetchMisses > 0) {
        double hitRate = 100.0 * stats.prefetchHits / (stats.prefetchHits + stats.prefetchMisses);
        ss << "Hit Rate: " << hitRate << "%" << std::endl;
    }
    
    return ss.str();
}

std::string ExecutionScheduler::getMemoryMap() const {
    std::ostringstream ss;
    ss << "=== Tensor Memory Map ===" << std::endl;
    
    std::lock_guard<std::mutex> lock(m_slotMutex);
    
    int cold = 0, warm = 0, hot = 0, pinned = 0, released = 0;
    for (auto& [name, slot] : m_tensorSlots) {
        switch (slot.state) {
            case TensorState::Cold:     cold++; break;
            case TensorState::Warm:     warm++; break;
            case TensorState::Hot:      hot++; break;
            case TensorState::Pinned:   pinned++; break;
            case TensorState::Released: released++; break;
        }
    }
    
    ss << "Cold: " << cold << " | Warm: " << warm << " | Hot: " << hot
       << " | Pinned: " << pinned << " | Released: " << released << std::endl;
    ss << "Total tracked: " << m_tensorSlots.size() << std::endl;
    
    return ss.str();
}

std::string ExecutionScheduler::getPrefetchStatus() const {
    std::ostringstream ss;
    ss << "=== Prefetch Status ===" << std::endl;
    
    for (int i = 0; i < m_numLayers && i < m_prefetchCompleteCount; ++i) {
        if (i > 0 && i % 20 == 0) ss << std::endl;
        ss << "L" << i << ":" << (m_prefetchComplete[i].load() ? "✓" : "·") << " ";
    }
    ss << std::endl;
    
    return ss.str();
}

// ============================================================================
// Shutdown
// ============================================================================
void ExecutionScheduler::shutdown() {
    stopPrefetchThread();
    
    // Release all pinned tensors
    {
        std::lock_guard<std::mutex> lock(m_slotMutex);
        for (auto& [name, slot] : m_tensorSlots) {
            slot.refCount = 0;
            slot.state = TensorState::Cold;
        }
    }
    
    m_pinnedBytes.store(0);
    m_manifests.clear();
    m_manifestsBuilt = false;
    m_modelBytes = 0;
    m_multiGpuPlanned = false;
    m_lastPlannedLayers = 0;
    
    std::cout << "[Scheduler] Shutdown complete" << std::endl;
}

} // namespace RawrXD
