// InferenceProfiler.h — Deep per-layer inference profiling for RawrXD
// Enhancement 1:  Per-layer timing with microsecond resolution
// Enhancement 2:  Memory bandwidth measurement per operation
// Enhancement 3:  GPU utilization sampling
// Enhancement 4:  Token generation throughput (t/s) with rolling window
// Enhancement 5:  Latency histogram: P50/P95/P99 per operation type
// Enhancement 6:  Performance regression detection (baseline comparison)
// Enhancement 7:  Automatic bottleneck identification
// Enhancement 8:  Performance prediction model (learned linear model)
// Enhancement 9:  Adaptive optimization triggers (auto-tune)
// Enhancement 10: Energy / power monitoring (RAPL on Intel/AMD)
// Enhancement 11: Thermal throttle detection
// Enhancement 12: Memory fragmentation rate tracking
// Enhancement 13: CPU cache miss rate proxy (via RDPMC or perf_event)
// Enhancement 14: Prometheus-compatible metrics export
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <optional>
#include <memory>
#include <algorithm>
#include <cmath>

namespace RawrXD {

// ─── Sample types ─────────────────────────────────────────────────────────────
struct LayerTimingSample {
    int                                   layer_idx;
    std::string                           op_name;      // "attention" / "ffn" / "norm"
    double                                duration_us;
    double                                mem_bytes_read;
    double                                mem_bytes_write;
    std::chrono::steady_clock::time_point timestamp;
};

struct ThroughputSample {
    std::chrono::steady_clock::time_point ts;
    int                                   tokens_generated;
};

struct EnergyReading {
    double cpu_joules  = 0.0;
    double dram_joules = 0.0;
    double gpu_watts   = 0.0;
};

struct ThermalReading {
    float cpu_celsius   = 0.f;
    float gpu_celsius   = 0.f;
    bool  throttling    = false;
};

struct CacheMissEstimate {
    double l1_miss_rate  = 0.0;   // estimated from timing variance
    double l2_miss_rate  = 0.0;
    double llc_miss_rate = 0.0;
};

// ─── Histogram (fixed-bucket, lock-free) ──────────────────────────────────────
struct Histogram {
    static constexpr int BUCKETS = 64;
    std::atomic<uint64_t> counts[BUCKETS]{};
    std::atomic<double>   sum{0.0};
    std::atomic<uint64_t> total{0};
    double                bucket_max_us;  // upper bound of last bucket

    explicit Histogram(double max_us = 100000.0) : bucket_max_us(max_us) {}

    void Record(double value_us) {
        int b = static_cast<int>(value_us / bucket_max_us * BUCKETS);
        b = std::clamp(b, 0, BUCKETS - 1);
        counts[b].fetch_add(1, std::memory_order_relaxed);
        sum.fetch_add(value_us, std::memory_order_relaxed);
        total.fetch_add(1, std::memory_order_relaxed);
    }

    double Percentile(double p) const {
        uint64_t n = total.load(std::memory_order_relaxed);
        if (!n) return 0.0;
        uint64_t target = static_cast<uint64_t>(p / 100.0 * n);
        uint64_t cum    = 0;
        for (int b = 0; b < BUCKETS; ++b) {
            cum += counts[b].load(std::memory_order_relaxed);
            if (cum >= target)
                return (b + 1.0) * bucket_max_us / BUCKETS;
        }
        return bucket_max_us;
    }

    double P50() const { return Percentile(50.0); }
    double P95() const { return Percentile(95.0); }
    double P99() const { return Percentile(99.0); }
    double Mean() const {
        uint64_t n = total.load();
        return n ? sum.load() / n : 0.0;
    }
};

// ─── Bottleneck descriptor ────────────────────────────────────────────────────
struct Bottleneck {
    std::string op_name;
    int         layer_idx    = -1;
    double      avg_us       = 0.0;
    double      pct_of_total = 0.0;  // 0..1
    std::string suggestion;
};

// ─── Optimizer trigger ────────────────────────────────────────────────────────
struct OptimizationTrigger {
    std::string condition;    // "p99_attention > 50ms"
    std::function<void()> action;
    bool        fired        = false;
};

// ─── Performance regression baseline ─────────────────────────────────────────
struct PerfBaseline {
    std::string              label;
    std::map<std::string, double> op_avg_us; // op_name → average µs
};

// ─── InferenceProfiler ───────────────────────────────────────────────────────
class InferenceProfiler {
public:
    static InferenceProfiler& Instance();

    // ---- Enhancement 1: Per-layer timing ----
    // Begin/end a named operation (reentrant via op-key)
    void BeginOp(const std::string& op, int layer = -1);
    void EndOp(const std::string& op, int layer = -1,
               double mem_read = 0.0, double mem_write = 0.0);

    // Scoped RAII timer
    struct ScopedOp {
        InferenceProfiler* prof;
        std::string        op;
        int                layer;
        ScopedOp(InferenceProfiler* p, const std::string& op, int layer)
            : prof(p), op(op), layer(layer) { prof->BeginOp(op, layer); }
        ~ScopedOp() { prof->EndOp(op, layer); }
    };
    ScopedOp MakeScoped(const std::string& op, int layer = -1) {
        return ScopedOp(this, op, layer);
    }

    const std::vector<LayerTimingSample>& GetSamples() const { return m_samples; }
    void ClearSamples();

    // ---- Enhancement 2: Memory bandwidth per op ----
    double GetOpMemBandwidthGBps(const std::string& op) const;

    // ---- Enhancement 3: GPU utilization ----
    float  GetGPUUtilizationPct() const;
    void   SampleGPUUtilization();  // call from background thread

    // ---- Enhancement 4: Token throughput ----
    void   RecordTokenGenerated();
    double GetTokensPerSecond() const;      // rolling 5-second window
    double GetPeakTokensPerSecond() const;

    // ---- Enhancement 5: Latency histograms ----
    Histogram& GetHistogram(const std::string& op);
    double GetP50Ms(const std::string& op) const;
    double GetP95Ms(const std::string& op) const;
    double GetP99Ms(const std::string& op) const;

    // ---- Enhancement 6: Regression detection ----
    void     CaptureBaseline(const std::string& label);
    bool     IsRegressionDetected(const std::string& baseline_label,
                                   double threshold_pct = 20.0) const;
    std::vector<std::string> GetRegressedOps(const std::string& baseline_label,
                                              double threshold_pct = 20.0) const;

    // ---- Enhancement 7: Bottleneck identification ----
    std::vector<Bottleneck> IdentifyBottlenecks(int top_n = 5) const;
    void                    PrintBottlenecks(int top_n = 5) const;

    // ---- Enhancement 8: Performance prediction ----
    // Fit a simple linear predictor: time = a*seq_len + b*num_layers + c
    void   FitPredictionModel();
    double PredictLatencyMs(int seq_len, int num_layers) const;

    // ---- Enhancement 9: Adaptive optimization triggers ----
    void RegisterTrigger(OptimizationTrigger trigger);
    void EvaluateTriggers();   // call after each inference pass

    // ---- Enhancement 10: Energy monitoring ----
    EnergyReading ReadEnergy() const;
    double GetJoulesPerToken() const;

    // ---- Enhancement 11: Thermal monitoring ----
    ThermalReading ReadThermal() const;
    bool           IsThrottling() const;

    // ---- Enhancement 12: Memory fragmentation ----
    void   RecordFragmentationSample(float ratio);
    double GetAvgFragmentationRatio() const;
    float  GetPeakFragmentation() const;

    // ---- Enhancement 13: Cache miss proxy ----
    CacheMissEstimate EstimateCacheMissRates() const;

    // ---- Enhancement 14: Prometheus export ----
    bool ExportPrometheus(const std::string& output_path) const;
    std::string GetPrometheusText() const;

    // ---- Aggregate stats ----
    double GetTotalInferenceTimeMs() const;
    double GetAvgInferenceTimeMs() const;
    int    GetInferenceCount() const;
    void   Reset();

private:
    InferenceProfiler() = default;

    // Per-op timing start map
    mutable std::mutex m_start_mtx;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_starts;

    // Sample storage
    mutable std::mutex             m_sample_mtx;
    std::vector<LayerTimingSample> m_samples;

    // Histograms per op name
    mutable std::mutex m_hist_mtx;
    std::unordered_map<std::string, std::shared_ptr<Histogram>> m_histograms;

    // Token throughput tracking
    mutable std::mutex              m_tok_mtx;
    std::vector<ThroughputSample>   m_tok_samples;
    mutable std::atomic<double>     m_peak_tps{0.0};

    // GPU utilization
    std::atomic<float> m_gpu_util{0.f};

    // Baselines
    mutable std::mutex m_baseline_mtx;
    std::vector<PerfBaseline> m_baselines;

    // Prediction model
    double m_pred_a = 0.0, m_pred_b = 0.0, m_pred_c = 0.0;
    bool   m_model_fitted = false;

    // Triggers
    mutable std::mutex              m_trigger_mtx;
    std::vector<OptimizationTrigger> m_triggers;

    // Energy/Thermal state
    mutable std::atomic<double>  m_joules_total{0.0};
    mutable std::atomic<uint64_t> m_token_energy_count{0};

    // Fragmentation
    mutable std::mutex       m_frag_mtx;
    std::vector<float>       m_frag_samples;

    // Aggregate timing
    std::atomic<double>   m_total_inference_ms{0.0};
    std::atomic<int>      m_inference_count{0};

    double GetOpAvgUs(const std::string& op) const;
    double GetTotalSampledUs() const;
};

} // namespace RawrXD
