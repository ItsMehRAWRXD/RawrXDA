// ============================================================================
// layer_latency_profiler.h — Per-Layer Latency Profiler + Adaptive Batch Sizing
// ============================================================================
// Instruments each transformer layer forward pass with RDTSC/QPC timing.
// Collects per-layer latency distributions, classifies compute vs memory
// bottlenecks, and feeds back into the batch scheduler to dynamically
// adjust batch size for optimal throughput.
//
// Architecture:
//   LayerTiming       — one forward-pass timing sample for a layer
//   LayerProfile      — accumulated stats for a layer (mean, p50, p99, etc.)
//   ModelProfile      — all layer profiles for a loaded model
//   AdaptiveBatchHint — recommended batch size from profiling data
//   LayerLatencyProfiler — the main profiler class
//
// Feedback loop:
//   1. Profile each layer during first N inferences (warm-up)
//   2. Classify each layer as memory-bound or compute-bound
//   3. Identify the bottleneck layer
//   4. Recommend batch size = floor(GPU_bandwidth / bottleneck_layer_bytes)
//   5. Periodically re-sample to detect degradation
//
// Thread safety: per-thread timing buffers, merged on demand.
// Hot-path: RDTSC read is 1-2 cycles overhead per layer.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace rawrxd {

// ---------------------------------------------------------------------------
// RDTSC helper — sub-nanosecond timing
// ---------------------------------------------------------------------------
inline uint64_t rdtscNow() {
#ifdef _MSC_VER
    return __rdtsc();
#else
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#endif
}

// ---------------------------------------------------------------------------
// LayerTiming — one timing sample
// ---------------------------------------------------------------------------
struct LayerTiming {
    uint32_t layerIndex;
    uint64_t startTSC;        // RDTSC at layer entry
    uint64_t endTSC;          // RDTSC at layer exit
    uint32_t batchSize;       // batch size when this was recorded
    uint32_t seqLen;          // sequence length at this point
    double   elapsedUs;       // computed from QPC or TSC-to-us conversion
};

// ---------------------------------------------------------------------------
// BottleneckType — compute vs memory classification
// ---------------------------------------------------------------------------
enum class BottleneckType : uint8_t {
    Unknown      = 0,
    ComputeBound = 1,   // FLOPs-limited (large matmuls)
    MemoryBound  = 2,   // Bandwidth-limited (KV cache reads)
    Balanced     = 3    // Neither dominant
};

// ---------------------------------------------------------------------------
// LayerProfile — accumulated statistics for one layer
// ---------------------------------------------------------------------------
struct LayerProfile {
    uint32_t       layerIndex;
    std::string    layerName;    // e.g. "attn.0", "mlp.0", "norm.0"

    // Latency stats (microseconds)
    double         meanUs    = 0;
    double         medianUs  = 0;
    double         p95Us     = 0;
    double         p99Us     = 0;
    double         minUs     = 1e9;
    double         maxUs     = 0;
    double         stddevUs  = 0;
    uint64_t       sampleCount = 0;

    // Classification
    BottleneckType bottleneck = BottleneckType::Unknown;
    double         computeIntensity = 0; // FLOPs / byte transferred (estimate)

    // Estimated resource usage
    double         estimatedFLOPs = 0;
    double         estimatedBytesRead = 0;
    double         estimatedBytesWritten = 0;
};

// ---------------------------------------------------------------------------
// ModelProfile — all layer profiles for a model
// ---------------------------------------------------------------------------
struct ModelProfile {
    std::string              modelName;
    uint64_t                 modelHash;
    uint32_t                 numLayers;
    std::vector<LayerProfile> layers;

    // Global stats
    double                   totalForwardUs = 0; // mean total forward pass
    uint32_t                 bottleneckLayer = 0; // index of slowest layer
    BottleneckType           dominantBottleneck = BottleneckType::Unknown;
    uint64_t                 profileSamples = 0;
    
    // Timestamps
    uint64_t                 createdEpoch = 0;
    uint64_t                 lastUpdatedEpoch = 0;
};

// ---------------------------------------------------------------------------
// AdaptiveBatchHint — recommendation from the profiler
// ---------------------------------------------------------------------------
struct AdaptiveBatchHint {
    uint32_t recommendedBatchSize;
    uint32_t minBatchSize;
    uint32_t maxBatchSize;
    double   expectedLatencyUs;      // predicted latency at recommended size
    double   expectedThroughputTPS;  // tokens per second
    double   confidencePercent;      // how confident we are (0-100)
    std::string reason;              // human-readable explanation
};

// ---------------------------------------------------------------------------
// ProfilerConfig
// ---------------------------------------------------------------------------
struct LayerProfilerConfig {
    uint32_t warmUpInferences  = 10;     // skip first N for JIT warmup
    uint32_t profileWindow     = 100;    // rolling window of samples
    uint32_t resampleInterval  = 500;    // re-profile every N inferences
    double   tscFreqGHz        = 0;      // 0 = auto-detect
    double   gpuBandwidthGBs   = 500.0;  // estimated GPU memory bandwidth
    double   gpuTFLOPS         = 20.0;   // estimated GPU compute
    bool     enableAdaptive    = true;
    bool     persistProfiles   = false;  // save to disk
    std::string profileDir;              // where to save profiles
};

// ---------------------------------------------------------------------------
// LayerLatencyProfiler — the profiler engine
// ---------------------------------------------------------------------------
class LayerLatencyProfiler {
public:
    explicit LayerLatencyProfiler(const LayerProfilerConfig& cfg = {});
    ~LayerLatencyProfiler();

    // ── Timing Instrumentation (called from inference loop) ─────────────

    // Call at the start of a layer forward pass
    void beginLayer(uint32_t layerIndex, uint32_t batchSize, uint32_t seqLen);

    // Call at the end of a layer forward pass
    void endLayer(uint32_t layerIndex);

    // Call at the start/end of a full forward pass
    void beginForwardPass(uint32_t batchSize);
    void endForwardPass();

    // ── Profile Management ──────────────────────────────────────────────

    // Initialize profiling for a model (call once at model load)
    void initModel(const std::string& modelName, uint64_t modelHash,
                   uint32_t numLayers);

    // Get the current model profile (may be incomplete during warmup)
    const ModelProfile& profile() const { return m_profile; }

    // Force recompute of statistics from raw samples
    void recomputeStats();

    // ── Adaptive Batch Sizing ───────────────────────────────────────────

    // Get recommended batch size based on current profile data
    AdaptiveBatchHint recommendBatchSize() const;

    // Override: set layer FLOPs/bandwidth estimates (for better accuracy)
    void setLayerEstimates(uint32_t layerIndex,
                          double estimatedFLOPs,
                          double estimatedBytesRead,
                          double estimatedBytesWritten);

    // ── Persistence ─────────────────────────────────────────────────────

    // Save profile to disk (JSON)
    bool saveProfile(const std::string& path) const;

    // Load profile from disk
    bool loadProfile(const std::string& path);

    // ── TSC Calibration ─────────────────────────────────────────────────

    double tscFreqGHz() const { return m_tscFreqGHz; }
    double tscToUs(uint64_t cycles) const {
        return (double)cycles / (m_tscFreqGHz * 1000.0);
    }

private:
    void calibrateTSC();
    void classifyBottleneck(LayerProfile& lp) const;
    void updateStats(LayerProfile& lp, const std::vector<LayerTiming>& samples);
    double computePercentile(std::vector<double>& sorted, double pct) const;

    LayerProfilerConfig         m_cfg;
    ModelProfile                m_profile;
    double                      m_tscFreqGHz = 3.0; // default, calibrated at init

    // Per-layer raw timing samples (ring buffer)
    struct LayerSamples {
        std::vector<LayerTiming> ring;
        uint32_t                 writePos = 0;
        uint32_t                 count    = 0;
    };
    std::vector<LayerSamples>   m_samples;

    // In-flight timing state
    struct InFlight {
        uint64_t startTSC = 0;
        uint32_t batchSize = 0;
        uint32_t seqLen = 0;
    };
    std::vector<InFlight>       m_inflight;

    // Forward pass tracking
    uint64_t                    m_fwdStartTSC = 0;
    uint32_t                    m_currentBatch = 0;
    uint64_t                    m_totalInferences = 0;

    mutable std::mutex          m_mutex;
};

} // namespace rawrxd
