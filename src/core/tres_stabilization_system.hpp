// ============================================================================
// tres_stabilization_system.hpp — TRES: Three-Layer Execution Stabilization
// ============================================================================
// T3 Layer: Adaptive stabilization + drift correction
// Makes the system self-stabilizing under load spikes.
//
// TRES ARCHITECTURE:
//   T1: Execution Layer (EFK) — lock-free packet execution
//   T2: Control Layer — budget assignment + phase planning
//   T3: Observability + Correction Layer — adaptive stabilization
// ============================================================================

#pragma once

#include "execution_fabric_kernel.hpp"
#include <math>
#include <deque>
#include <algorithm>

namespace RawrXD {
namespace TRES {

// ============================================================================
// T3: OBSERVABILITY LAYER — Drift Detection + Correction
// ============================================================================

// Drift detection thresholds
struct DriftThresholds {
    double tps_variance_threshold = 0.15;      // CV > 15% = unstable
    double kv_pressure_threshold = 0.85;       // > 85% = critical
    double gpu_queue_depth_threshold = 32.0;     // > 32ms = backed up
    double phase_overrun_threshold = 0.05;       // > 5% overruns = tight
    double latency_p99_threshold = 100.0;      // > 100ms P99 = degraded
    
    // Adaptive multipliers
    double budget_reduce_factor = 0.75;          // Reduce by 25%
    double budget_increase_factor = 1.25;        // Increase by 25%
    double emergency_factor = 0.50;              // Emergency: reduce by 50%
};

// Historical window for trend analysis
class MetricsWindow {
public:
    static constexpr size_t kDefaultWindowSize = 100;
    
    explicit MetricsWindow(size_t size = kDefaultWindowSize);
    
    void addSample(double value);
    double getMean() const;
    double getVariance() const;
    double getStdDev() const;
    double getCoefficientOfVariation() const; // stddev / mean
    double getP99() const;
    double getTrend() const; // Positive = increasing, negative = decreasing
    
    bool isFull() const { return samples_.size() >= window_size_; }
    size_t size() const { return samples_.size(); }
    void clear();
    
private:
    std::deque<double> samples_;
    size_t window_size_;
    mutable std::mutex mutex_;
};

// Drift types
enum class DriftType {
    NONE = 0,
    TPS_VARIANCE_HIGH,           // TPS fluctuating
    KV_PRESSURE_CRITICAL,        // KV cache near capacity
    GPU_QUEUE_BACKUP,            // GPU work piling up
    PHASE_OVERRUN_SPIKE,         // Budget overruns increasing
    LATENCY_DEGRADATION,         // P99 latency climbing
    COMPOUND_INSTABILITY         // Multiple factors
};

struct DriftReport {
    DriftType type;
    double severity;             // 0.0-1.0
    uint64_t timestamp_ns;
    std::string description;
    
    bool isCritical() const { return severity > 0.8; }
    bool requiresEmergency() const { return severity > 0.95; }
};

// ============================================================================
// T3: Adaptive Budget Controller
// ============================================================================

class AdaptiveBudgetController {
public:
    AdaptiveBudgetController();
    
    // Update with new telemetry
    void update(const EFK::SystemTelemetry& telemetry);
    
    // Get adjusted budget for phase
    uint64_t getAdjustedBudget(uint64_t phase_id, uint64_t base_budget) const;
    
    // Get current drift report
    DriftReport getCurrentDrift() const;
    
    // Configuration
    void setThresholds(const DriftThresholds& thresholds);
    void setBudgetRange(uint64_t phase_id, uint64_t min_ns, uint64_t max_ns);
    
    // Manual override
    void emergencyReduceAllBudgets();
    void restoreNormalOperation();
    
private:
    DriftThresholds thresholds_;
    
    // Per-phase budget ranges
    struct BudgetRange {
        uint64_t min_ns;
        uint64_t max_ns;
        uint64_t current_ns;
    };
    std::unordered_map<uint64_t, BudgetRange> budget_ranges_;
    mutable std::mutex budget_mutex_;
    
    // Historical metrics
    MetricsWindow tps_window_;
    MetricsWindow kv_pressure_window_;
    MetricsWindow gpu_queue_window_;
    MetricsWindow phase_overrun_window_;
    
    // Current drift state
    DriftReport current_drift_;
    mutable std::mutex drift_mutex_;
    
    // Analysis
    DriftType detectDrift(const EFK::SystemTelemetry& telemetry);
    double calculateSeverity(DriftType type, const EFK::SystemTelemetry& telemetry);
    void adjustBudgetsForDrift(const DriftReport& drift);
};

// ============================================================================
// T3: Correction Action Engine
// ============================================================================

enum class CorrectionAction {
    NONE = 0,
    REDUCE_DECODE_BUDGET,
    INCREASE_DECODE_BUDGET,
    REDUCE_ALL_BUDGETS,
    INCREASE_ALL_BUDGETS,
    EMERGENCY_THROTTLE,
    TRIGGER_AUTOPATCH,
    REQUEST_KV_EVICTION,
    FLUSH_GPU_QUEUE,
    NOTIFY_LOAD_SHED
};

struct CorrectionCommand {
    CorrectionAction action;
    uint64_t timestamp_ns;
    double intensity; // 0.0-1.0
    std::string reason;
};

class CorrectionEngine {
public:
    using ActionHandler = std::function<void(const CorrectionCommand&)>;
    
    CorrectionEngine();
    
    // Register handlers for actions
    void registerHandler(CorrectionAction action, ActionHandler handler);
    
    // Execute correction based on drift
    void executeCorrection(const DriftReport& drift);
    
    // Get pending corrections
    std::vector<CorrectionCommand> getPendingCorrections() const;
    void clearPendingCorrections();
    
private:
    std::unordered_map<CorrectionAction, ActionHandler> handlers_;
    std::vector<CorrectionCommand> pending_corrections_;
    mutable std::mutex correction_mutex_;
    
    CorrectionAction selectActionForDrift(const DriftReport& drift);
    double calculateIntensity(const DriftReport& drift);
};

// ============================================================================
// T3: Stabilization Loop
// ============================================================================

class StabilizationLoop {
public:
    static constexpr uint64_t kDefaultIntervalMs = 50; // 50ms default
    
    explicit StabilizationLoop(uint64_t interval_ms = kDefaultIntervalMs);
    ~StabilizationLoop();
    
    // Lifecycle
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // Components
    AdaptiveBudgetController& getBudgetController() { return budget_controller_; }
    CorrectionEngine& getCorrectionEngine() { return correction_engine_; }
    
    // Telemetry input
    void feedTelemetry(const EFK::SystemTelemetry& telemetry);
    
    // Diagnostics
    std::string getStatusString() const;
    
private:
    void loopThreadFunc();
    
    uint64_t interval_ms_;
    std::atomic<bool> running_{false};
    std::thread loop_thread_;
    
    AdaptiveBudgetController budget_controller_;
    CorrectionEngine correction_engine_;
    
    // Telemetry buffer
    std::deque<EFK::SystemTelemetry> telemetry_buffer_;
    mutable std::mutex telemetry_mutex_;
    std::condition_variable telemetry_cv_;
    
    // Stats
    std::atomic<uint64_t> iterations_{0};
    std::atomic<uint64_t> corrections_triggered_{0};
};

// ============================================================================
// TRES INTEGRATION: Full Three-Layer System
// ============================================================================

class TRESSystem {
public:
    static TRESSystem& instance();
    
    // Initialize all three layers
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_.load(); }
    
    // Layer accessors
    EFK::ExecutionFabricKernel& getT1ExecutionLayer() { return *t1_; }
    EFK::PhasePlanner& getT2ControlLayer() { return t1_->getPlanner(); }
    StabilizationLoop& getT3ObservabilityLayer() { return *t3_; }
    
    // Unified telemetry feed
    void submitTelemetry(const EFK::SystemTelemetry& telemetry);
    
    // System status
    std::string getSystemReport() const;
    bool isSystemStable() const;
    
    // Emergency controls
    void emergencyStabilize();
    void resumeNormalOperation();
    
private:
    TRESSystem() = default;
    
    std::unique_ptr<EFK::ExecutionFabricKernel> t1_;
    std::unique_ptr<StabilizationLoop> t3_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> emergency_mode_{false};
};

// ============================================================================
// TRES: Convenience Macros
// ============================================================================

#define TRES_INIT() RawrXD::TRES::TRESSystem::instance().initialize()
#define TRES_SHUTDOWN() RawrXD::TRES::TRESSystem::instance().shutdown()
#define TRES_T1() RawrXD::TRES::TRESSystem::instance().getT1ExecutionLayer()
#define TRES_T3() RawrXD::TRES::TRESSystem::instance().getT3ObservabilityLayer()
#define TRES_FEED(telemetry) RawrXD::TRES::TRESSystem::instance().submitTelemetry(telemetry)
#define TRES_STABLE() RawrXD::TRES::TRESSystem::instance().isSystemStable()

} // namespace TRES
} // namespace RawrXD
