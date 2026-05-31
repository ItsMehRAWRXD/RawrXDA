// ============================================================================
// tres_stabilization_layer.hpp — TRES: Third-Order Stabilization System
// ============================================================================
// 3-Layer Control System:
//   T1: Execution Layer (EFK) — runs packets, no decisions, deterministic
//   T2: Control Layer (Scheduler Brain) — assigns budgets, prioritizes phases
//   T3: Observability + Correction Layer — detects drift, adjusts budgets
//
// This makes the system self-stabilizing under load spikes.
// ============================================================================

#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <thread>
#include <functional>
#include <array>

namespace RawrXD {
namespace TRES {

// Forward declarations
struct SystemTelemetry;
struct PhaseBudget;

// ============================================================================
// T3: Observability + Correction Layer
// ============================================================================

// Drift detection thresholds
struct DriftThresholds {
    double tps_variance_max = 0.15;      // 15% variance triggers correction
    double kv_pressure_max = 0.85;       // 85% KV cache utilization
    double gpu_queue_depth_max = 32;     // Max queued dispatches
    double latency_spike_ms = 50.0;      // Latency spike threshold
    double budget_overrun_pct = 10.0;    // 10% budget overrun triggers spill
};

// System telemetry snapshot
struct SystemTelemetry {
    // TPS metrics
    double tps_current = 0.0;
    double tps_rolling_avg = 0.0;
    double tps_variance = 0.0;
    uint64_t tokens_generated = 0;

    // KV cache metrics
    double kv_pressure = 0.0;            // 0.0 - 1.0
    uint64_t kv_bytes_used = 0;
    uint64_t kv_bytes_total = 0;
    double kv_quantization_ratio = 0.0;  // FP8 vs FP32

    // GPU metrics
    double gpu_queue_depth = 0.0;
    double gpu_utilization = 0.0;
    uint64_t gpu_bytes_uploaded = 0;
    uint64_t gpu_bytes_downloaded = 0;

    // Latency metrics
    double avg_prefill_ms = 0.0;
    double avg_decode_ms = 0.0;
    double p99_latency_ms = 0.0;

    // Phase timing
    uint64_t phase_budget_ns = 0;
    uint64_t phase_used_ns = 0;
    double budget_utilization = 0.0;

    // Timestamp
    uint64_t timestamp_ns = 0;
};

// Adaptive budget adjustment
struct BudgetAdjustment {
    int32_t prefill_delta_ns = 0;
    int32_t decode_delta_ns = 0;
    int32_t tail_delta_ns = 0;
    double throttle_factor = 1.0;
    bool emergency_spill = false;
};

// Autopatch trigger signal
struct AutopatchSignal {
    bool trigger = false;
    uint32_t signal_type = 0;  // 0=none, 1=kv_pressure, 2=gpu_overrun, 3=latency_spike
    const char* reason = nullptr;
    double severity = 0.0;     // 0.0 - 1.0
};

// ============================================================================
// T3 Layer: Observability + Correction
// ============================================================================
class TRESStabilizationLayer {
public:
    using TelemetryCallback = std::function<SystemTelemetry()>;
    using BudgetCallback = std::function<void(const BudgetAdjustment&)>;
    using AutopatchCallback = std::function<void(const AutopatchSignal&)>;

    TRESStabilizationLayer();
    ~TRESStabilizationLayer();

    // No copy/move
    TRESStabilizationLayer(const TRESStabilizationLayer&) = delete;
    TRESStabilizationLayer& operator=(const TRESStabilizationLayer&) = delete;

    // Initialize with callbacks
    bool initialize(TelemetryCallback telemetry_cb,
                    BudgetCallback budget_cb,
                    AutopatchCallback autopatch_cb = nullptr);

    // Shutdown
    void shutdown();

    // Start/stop the correction loop
    void startCorrectionLoop(uint32_t interval_ms = 50);
    void stopCorrectionLoop();
    bool isRunning() const { return running_.load(); }

    // Manual telemetry injection (for testing)
    void injectTelemetry(const SystemTelemetry& telemetry);

    // Get current drift status
    bool isDriftDetected() const { return drift_detected_.load(); }
    double getDriftSeverity() const { return drift_severity_.load(); }

    // Get last adjustment
    BudgetAdjustment getLastAdjustment() const { return last_adjustment_; }

    // Statistics
    uint64_t getCorrectionCount() const { return correction_count_.load(); }
    uint64_t getAutopatchCount() const { return autopatch_count_.load(); }

    // Configuration
    void setThresholds(const DriftThresholds& thresholds);
    DriftThresholds getThresholds() const { return thresholds_; }

private:
    // Correction loop
    void correctionLoopFunc();

    // Drift detection
    bool detectDrift(const SystemTelemetry& telemetry);
    double computeDriftSeverity(const SystemTelemetry& telemetry);

    // Budget adjustment
    BudgetAdjustment computeAdjustment(const SystemTelemetry& telemetry);

    // Autopatch decision
    AutopatchSignal evaluateAutopatch(const SystemTelemetry& telemetry);

    // Rolling window for TPS variance
    static constexpr uint32_t TPS_WINDOW_SIZE = 20;
    std::array<double, TPS_WINDOW_SIZE> tps_history_ = {};
    uint32_t tps_index_ = 0;
    bool tps_window_full_ = false;

    // Callbacks
    TelemetryCallback telemetry_cb_;
    BudgetCallback budget_cb_;
    AutopatchCallback autopatch_cb_;

    // Configuration
    DriftThresholds thresholds_;
    uint32_t interval_ms_ = 50;

    // State
    std::unique_ptr<std::thread> correction_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<bool> drift_detected_{false};
    std::atomic<double> drift_severity_{0.0};

    // Last adjustment
    BudgetAdjustment last_adjustment_;

    // Statistics
    std::atomic<uint64_t> correction_count_{0};
    std::atomic<uint64_t> autopatch_count_{0};
    std::atomic<uint64_t> total_observations_{0};

    // Initialized flag
    bool initialized_ = false;
};

// ============================================================================
// T2 Layer: Control Layer (Scheduler Brain)
// ============================================================================
class TRESControlLayer {
public:
    TRESControlLayer();
    ~TRESControlLayer();

    // Phase budget management
    void setBaseBudgets(uint64_t prefill_ns, uint64_t decode_ns, uint64_t tail_ns);
    void adjustBudgets(const BudgetAdjustment& adjustment);

    // Get current budgets
    uint64_t getPrefillBudget() const { return prefill_budget_ns_.load(); }
    uint64_t getDecodeBudget() const { return decode_budget_ns_.load(); }
    uint64_t getTailBudget() const { return tail_budget_ns_.load(); }

    // Priority management
    void setPhasePriority(uint32_t phase_id, uint32_t priority);
    uint32_t getPhasePriority(uint32_t phase_id) const;

    // Throttle control
    void setThrottleFactor(double factor);
    double getThrottleFactor() const { return throttle_factor_.load(); }

private:
    std::atomic<uint64_t> prefill_budget_ns_{10'000'000};  // 10ms default
    std::atomic<uint64_t> decode_budget_ns_{5'000'000};     // 5ms default
    std::atomic<uint64_t> tail_budget_ns_{2'000'000};      // 2ms default
    std::atomic<double> throttle_factor_{1.0};
};

// ============================================================================
// TRES Integration: Complete 3-Layer System
// ============================================================================
class TRESSystem {
public:
    TRESSystem();
    ~TRESSystem();

    // Initialize all layers
    bool initialize(TRESStabilizationLayer::TelemetryCallback telemetry_cb,
                    TRESStabilizationLayer::AutopatchCallback autopatch_cb = nullptr);

    // Shutdown
    void shutdown();

    // Access layers
    TRESStabilizationLayer* getT3Layer() { return stabilization_.get(); }
    TRESControlLayer* getT2Layer() { return control_.get(); }

    // Convenience: Start/stop correction
    void start(uint32_t interval_ms = 50);
    void stop();
    bool isRunning() const;

    // Get system status
    SystemTelemetry getCurrentTelemetry() const;
    bool isStable() const;

private:
    std::unique_ptr<TRESStabilizationLayer> stabilization_;
    std::unique_ptr<TRESControlLayer> control_;

    // Internal budget callback (wires T3 -> T2)
    void onBudgetAdjustment(const BudgetAdjustment& adjustment);
};

// ============================================================================
// C API for integration
// ============================================================================
extern "C" {
    typedef void* RawrXD_TRESSystem;

    // Callback types
    typedef void (*RawrXD_TelemetryCallback)(void* user_data, SystemTelemetry* telemetry);
    typedef void (*RawrXD_AutopatchCallback)(void* user_data, AutopatchSignal* signal);

    RawrXD_TRESSystem rawrxd_tres_create(
        RawrXD_TelemetryCallback telemetry_cb,
        RawrXD_AutopatchCallback autopatch_cb,
        void* user_data);

    void rawrxd_tres_destroy(RawrXD_TRESSystem handle);
    void rawrxd_tres_start(RawrXD_TRESSystem handle, uint32_t interval_ms);
    void rawrxd_tres_stop(RawrXD_TRESSystem handle);
    int rawrxd_tres_is_stable(RawrXD_TRESSystem handle);
}

} // namespace TRES
} // namespace RawrXD
