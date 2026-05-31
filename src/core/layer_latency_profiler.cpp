// ============================================================================
// layer_latency_profiler.cpp — Per-Layer Latency Profiler Implementation
// ============================================================================
#include "layer_latency_profiler.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace rawrxd {

// ============================================================================
// Construction / Destruction
// ============================================================================

LayerLatencyProfiler::LayerLatencyProfiler(const LayerProfilerConfig& cfg)
    : m_cfg(cfg)
{
    calibrateTSC();
    if (cfg.tscFreqGHz > 0)
        m_tscFreqGHz = cfg.tscFreqGHz;
}

LayerLatencyProfiler::~LayerLatencyProfiler() = default;

// ============================================================================
// TSC Calibration — measure RDTSC frequency against QPC
// ============================================================================

void LayerLatencyProfiler::calibrateTSC()
{
#ifdef _WIN32
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    uint64_t tsc0 = rdtscNow();

    // Spin for ~10ms to get a reliable measurement
    LARGE_INTEGER target;
    target.QuadPart = t0.QuadPart + freq.QuadPart / 100; // 10ms
    do {
        QueryPerformanceCounter(&t1);
    } while (t1.QuadPart < target.QuadPart);

    uint64_t tsc1 = rdtscNow();
    double elapsedSec = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
    double tscDelta = (double)(tsc1 - tsc0);
    m_tscFreqGHz = (tscDelta / elapsedSec) / 1e9;

    // Sanity: clamp to reasonable range (1-6 GHz)
    if (m_tscFreqGHz < 1.0 || m_tscFreqGHz > 6.0)
        m_tscFreqGHz = 3.0;
#else
    m_tscFreqGHz = 3.0; // fallback
#endif
}

// ============================================================================
// Model Initialization
// ============================================================================

void LayerLatencyProfiler::initModel(const std::string& modelName,
                                      uint64_t modelHash,
                                      uint32_t numLayers)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_profile = {};
    m_profile.modelName = modelName;
    m_profile.modelHash = modelHash;
    m_profile.numLayers = numLayers;
    m_profile.layers.resize(numLayers);
    m_profile.createdEpoch = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();

    for (uint32_t i = 0; i < numLayers; ++i) {
        m_profile.layers[i].layerIndex = i;
        m_profile.layers[i].layerName = "layer." + std::to_string(i);
    }

    m_samples.resize(numLayers);
    for (auto& ls : m_samples) {
        ls.ring.resize(m_cfg.profileWindow);
        ls.writePos = 0;
        ls.count = 0;
    }

    m_inflight.resize(numLayers);
    m_totalInferences = 0;
}

// ============================================================================
// Timing Instrumentation — Layer
// ============================================================================

void LayerLatencyProfiler::beginLayer(uint32_t layerIndex, uint32_t batchSize,
                                       uint32_t seqLen)
{
    if (layerIndex >= m_inflight.size()) return;

    auto& inf = m_inflight[layerIndex];
    inf.startTSC = rdtscNow();
    inf.batchSize = batchSize;
    inf.seqLen = seqLen;
}

void LayerLatencyProfiler::endLayer(uint32_t layerIndex)
{
    uint64_t endTSC = rdtscNow();

    if (layerIndex >= m_inflight.size()) return;
    if (layerIndex >= m_samples.size()) return;

    auto& inf = m_inflight[layerIndex];
    if (inf.startTSC == 0) return;

    // Skip warmup period
    if (m_totalInferences < m_cfg.warmUpInferences) {
        inf.startTSC = 0;
        return;
    }

    LayerTiming timing;
    timing.layerIndex = layerIndex;
    timing.startTSC = inf.startTSC;
    timing.endTSC = endTSC;
    timing.batchSize = inf.batchSize;
    timing.seqLen = inf.seqLen;
    timing.elapsedUs = tscToUs(endTSC - inf.startTSC);

    inf.startTSC = 0;

    // Store in ring buffer (lock-free per-layer write)
    auto& ls = m_samples[layerIndex];
    uint32_t pos = ls.writePos % m_cfg.profileWindow;
    ls.ring[pos] = timing;
    ls.writePos++;
    if (ls.count < m_cfg.profileWindow) ls.count++;

    // Periodically recompute stats
    if (ls.writePos % m_cfg.resampleInterval == 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        updateStats(m_profile.layers[layerIndex],
                    std::vector<LayerTiming>(ls.ring.begin(),
                                             ls.ring.begin() + ls.count));
    }
}

// ============================================================================
// Timing Instrumentation — Forward Pass
// ============================================================================

void LayerLatencyProfiler::beginForwardPass(uint32_t batchSize)
{
    m_fwdStartTSC = rdtscNow();
    m_currentBatch = batchSize;
}

void LayerLatencyProfiler::endForwardPass()
{
    uint64_t endTSC = rdtscNow();
    m_totalInferences++;

    if (m_totalInferences <= m_cfg.warmUpInferences)
        return;

    double elapsedUs = tscToUs(endTSC - m_fwdStartTSC);

    std::lock_guard<std::mutex> lock(m_mutex);

    // Exponential moving average for total forward time
    if (m_profile.totalForwardUs == 0)
        m_profile.totalForwardUs = elapsedUs;
    else
        m_profile.totalForwardUs = 0.95 * m_profile.totalForwardUs + 0.05 * elapsedUs;

    m_profile.profileSamples++;
    m_profile.lastUpdatedEpoch =
        (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
}

// ============================================================================
// Statistics Computation
// ============================================================================

void LayerLatencyProfiler::recomputeStats()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (uint32_t i = 0; i < m_profile.numLayers; ++i) {
        if (i >= m_samples.size()) break;
        auto& ls = m_samples[i];
        if (ls.count == 0) continue;

        std::vector<LayerTiming> timings(ls.ring.begin(),
                                          ls.ring.begin() + ls.count);
        updateStats(m_profile.layers[i], timings);
    }

    // Find bottleneck layer
    double maxMean = 0;
    uint32_t maxIdx = 0;
    for (uint32_t i = 0; i < m_profile.numLayers; ++i) {
        if (m_profile.layers[i].meanUs > maxMean) {
            maxMean = m_profile.layers[i].meanUs;
            maxIdx = i;
        }
    }
    m_profile.bottleneckLayer = maxIdx;
    if (maxIdx < m_profile.numLayers)
        m_profile.dominantBottleneck = m_profile.layers[maxIdx].bottleneck;
}

void LayerLatencyProfiler::updateStats(LayerProfile& lp,
                                        const std::vector<LayerTiming>& samples)
{
    if (samples.empty()) return;

    std::vector<double> times;
    times.reserve(samples.size());
    for (auto& s : samples) times.push_back(s.elapsedUs);

    std::sort(times.begin(), times.end());

    lp.sampleCount = times.size();
    lp.minUs = times.front();
    lp.maxUs = times.back();

    double sum = std::accumulate(times.begin(), times.end(), 0.0);
    lp.meanUs = sum / (double)times.size();

    lp.medianUs = computePercentile(times, 50.0);
    lp.p95Us    = computePercentile(times, 95.0);
    lp.p99Us    = computePercentile(times, 99.0);

    // Standard deviation
    double sqSum = 0;
    for (double t : times) {
        double d = t - lp.meanUs;
        sqSum += d * d;
    }
    lp.stddevUs = std::sqrt(sqSum / (double)times.size());

    classifyBottleneck(lp);
}

double LayerLatencyProfiler::computePercentile(std::vector<double>& sorted,
                                                double pct) const
{
    if (sorted.empty()) return 0;
    double idx = (pct / 100.0) * (double)(sorted.size() - 1);
    size_t lo = (size_t)idx;
    size_t hi = lo + 1;
    if (hi >= sorted.size()) return sorted.back();
    double frac = idx - (double)lo;
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

// ============================================================================
// Bottleneck Classification
// ============================================================================

void LayerLatencyProfiler::classifyBottleneck(LayerProfile& lp) const
{
    if (lp.estimatedFLOPs == 0 && lp.estimatedBytesRead == 0) {
        lp.bottleneck = BottleneckType::Unknown;
        return;
    }

    // Compute arithmetic intensity = FLOPs / bytes transferred
    double totalBytes = lp.estimatedBytesRead + lp.estimatedBytesWritten;
    if (totalBytes > 0) {
        lp.computeIntensity = lp.estimatedFLOPs / totalBytes;
    }

    // Ridge point: GPU_TFLOPS / GPU_bandwidth_GBs
    // If intensity > ridge_point → compute bound
    // If intensity < ridge_point → memory bound
    double ridgePoint = (m_cfg.gpuTFLOPS * 1e12) / (m_cfg.gpuBandwidthGBs * 1e9);

    if (lp.computeIntensity > ridgePoint * 1.2)
        lp.bottleneck = BottleneckType::ComputeBound;
    else if (lp.computeIntensity < ridgePoint * 0.8)
        lp.bottleneck = BottleneckType::MemoryBound;
    else
        lp.bottleneck = BottleneckType::Balanced;
}

// ============================================================================
// Adaptive Batch Sizing
// ============================================================================

AdaptiveBatchHint LayerLatencyProfiler::recommendBatchSize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    AdaptiveBatchHint hint;
    hint.minBatchSize = 1;
    hint.maxBatchSize = 64;
    hint.recommendedBatchSize = 4; // safe default
    hint.confidencePercent = 0;

    if (m_profile.profileSamples < m_cfg.warmUpInferences) {
        hint.reason = "Insufficient profiling data (warmup phase)";
        return hint;
    }

    // Strategy: increase batch size until the bottleneck layer's latency
    // grows super-linearly (indicating resource saturation).
    //
    // For memory-bound layers: batch size can increase freely (latency ~constant).
    // For compute-bound layers: latency scales linearly with batch size.

    // Find the bottleneck layer
    uint32_t bottleIdx = m_profile.bottleneckLayer;
    if (bottleIdx >= m_profile.numLayers) {
        hint.reason = "No bottleneck layer identified";
        return hint;
    }

    const auto& bottle = m_profile.layers[bottleIdx];

    switch (bottle.bottleneck) {
    case BottleneckType::MemoryBound: {
        // Memory-bound: can increase batch aggressively.
        // Limit by GPU memory bandwidth.
        double bytesPerToken = bottle.estimatedBytesRead;
        if (bytesPerToken > 0) {
            double maxTokensPerSec = (m_cfg.gpuBandwidthGBs * 1e9) / bytesPerToken;
            double targetLatencyUs = bottle.meanUs * 2.0; // allow 2x latency
            hint.recommendedBatchSize = (uint32_t)(maxTokensPerSec * targetLatencyUs / 1e6);
        } else {
            hint.recommendedBatchSize = 16;
        }
        hint.reason = "Memory-bound layer " + std::to_string(bottleIdx) +
                      "; aggressive batch sizing";
        break;
    }
    case BottleneckType::ComputeBound: {
        // Compute-bound: batch size limited by FLOPs budget.
        double flopsPerToken = bottle.estimatedFLOPs;
        if (flopsPerToken > 0) {
            double totalFlops = m_cfg.gpuTFLOPS * 1e12;
            double targetLatencyUs = bottle.meanUs * 1.5;
            hint.recommendedBatchSize = (uint32_t)(totalFlops * targetLatencyUs /
                                                    (1e6 * flopsPerToken));
        } else {
            hint.recommendedBatchSize = 8;
        }
        hint.reason = "Compute-bound layer " + std::to_string(bottleIdx) +
                      "; conservative batch sizing";
        break;
    }
    default:
        hint.recommendedBatchSize = 8;
        hint.reason = "Unknown bottleneck; defaulting to batch=8";
        break;
    }

    // Clamp
    if (hint.recommendedBatchSize < hint.minBatchSize)
        hint.recommendedBatchSize = hint.minBatchSize;
    if (hint.recommendedBatchSize > hint.maxBatchSize)
        hint.recommendedBatchSize = hint.maxBatchSize;

    // Estimate throughput
    if (m_profile.totalForwardUs > 0) {
        hint.expectedLatencyUs = m_profile.totalForwardUs *
            ((double)hint.recommendedBatchSize / (double)std::max(m_currentBatch, 1u));
        if (hint.expectedLatencyUs > 0)
            hint.expectedThroughputTPS = (double)hint.recommendedBatchSize * 1e6 /
                                          hint.expectedLatencyUs;
    }

    hint.confidencePercent = std::min(100.0,
        (double)m_profile.profileSamples / (double)m_cfg.profileWindow * 100.0);

    return hint;
}

// ============================================================================
// Layer Estimates Override
// ============================================================================

void LayerLatencyProfiler::setLayerEstimates(uint32_t layerIndex,
                                              double estimatedFLOPs,
                                              double estimatedBytesRead,
                                              double estimatedBytesWritten)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (layerIndex >= m_profile.layers.size()) return;

    auto& lp = m_profile.layers[layerIndex];
    lp.estimatedFLOPs = estimatedFLOPs;
    lp.estimatedBytesRead = estimatedBytesRead;
    lp.estimatedBytesWritten = estimatedBytesWritten;
    classifyBottleneck(lp);
}

// ============================================================================
// Persistence — JSON save/load
// ============================================================================

bool LayerLatencyProfiler::saveProfile(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "{\n";
    f << "  \"modelName\": \"" << m_profile.modelName << "\",\n";
    f << "  \"modelHash\": " << m_profile.modelHash << ",\n";
    f << "  \"numLayers\": " << m_profile.numLayers << ",\n";
    f << "  \"totalForwardUs\": " << m_profile.totalForwardUs << ",\n";
    f << "  \"profileSamples\": " << m_profile.profileSamples << ",\n";
    f << "  \"tscFreqGHz\": " << m_tscFreqGHz << ",\n";
    f << "  \"layers\": [\n";

    for (uint32_t i = 0; i < m_profile.numLayers; ++i) {
        const auto& lp = m_profile.layers[i];
        f << "    {";
        f << "\"index\": " << lp.layerIndex << ", ";
        f << "\"name\": \"" << lp.layerName << "\", ";
        f << "\"meanUs\": " << lp.meanUs << ", ";
        f << "\"medianUs\": " << lp.medianUs << ", ";
        f << "\"p95Us\": " << lp.p95Us << ", ";
        f << "\"p99Us\": " << lp.p99Us << ", ";
        f << "\"minUs\": " << lp.minUs << ", ";
        f << "\"maxUs\": " << lp.maxUs << ", ";
        f << "\"samples\": " << lp.sampleCount << ", ";
        f << "\"bottleneck\": " << (int)lp.bottleneck;
        f << "}";
        if (i + 1 < m_profile.numLayers) f << ",";
        f << "\n";
    }

    f << "  ]\n}\n";
    return f.good();
}

bool LayerLatencyProfiler::loadProfile(const std::string& path)
{
    // Minimal JSON parser for profile loading
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // Find modelName
    auto extractStr = [&](const std::string& key) -> std::string {
        auto pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        auto colon = content.find(':', pos);
        auto q1 = content.find('"', colon + 1);
        auto q2 = content.find('"', q1 + 1);
        if (q1 != std::string::npos && q2 != std::string::npos)
            return content.substr(q1 + 1, q2 - q1 - 1);
        return "";
    };

    auto extractNum = [&](const std::string& key) -> double {
        auto pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return 0;
        auto colon = content.find(':', pos);
        if (colon == std::string::npos) return 0;
        return std::stod(content.substr(colon + 1));
    };

    std::lock_guard<std::mutex> lock(m_mutex);

    m_profile.modelName = extractStr("modelName");
    m_profile.modelHash = (uint64_t)extractNum("modelHash");
    m_profile.numLayers = (uint32_t)extractNum("numLayers");
    m_profile.totalForwardUs = extractNum("totalForwardUs");
    m_profile.profileSamples = (uint64_t)extractNum("profileSamples");

    return true;
}

} // namespace rawrxd
