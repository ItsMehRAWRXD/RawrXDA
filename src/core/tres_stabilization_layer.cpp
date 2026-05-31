// ============================================================================
// tres_stabilization_layer.cpp — TRES: Third-Order Stabilization Implementation
// ============================================================================
// Self-stabilizing control system with adaptive feedback.
// Detects drift (TPS variance, latency spikes) and adjusts budgets dynamically.
// ============================================================================

#include "tres_stabilization_layer.hpp"
#include <cmath>
#include <cstring>
#include <chrono>
#include <algorithm>

namespace RawrXD {
namespace TRES {

// ============================================================================
// T3 Layer: Observability + Correction
// ============================================================================
TRESStabilizationLayer::TRESStabilizationLayer() = default;

TRESStabilizationLayer::~TRESStabilizationLayer() {
    shutdown();
}

bool TRESStabilizationLayer::initialize(TelemetryCallback telemetry_cb,
                                         BudgetCallback budget_cb,
                                         AutopatchCallback autopatch_cb) {
    if (initialized_) {
        shutdown();
    }

    telemetry_cb_ = telemetry_cb;
    budget_cb_ = budget_cb;
    autopatch_cb_ = autopatch_cb;

    // Reset state
    tps_index_ = 0;
    tps_window_full_ = false;
    tps_history_.fill(0.0);

    drift_detected_.store(false);
    drift_severity_.store(0.0);
    correction_count_.store(0);
    autopatch_count_.store(0);
    total_observations_.store(0);

    initialized_ = true;
    return true;
}

void TRESStabilizationLayer::shutdown() {
    stopCorrectionLoop();

    telemetry_cb_ = nullptr;
    budget_cb_ = nullptr;
    autopatch_cb_ = nullptr;

    initialized_ = false;
}

void TRESStabilizationLayer::startCorrectionLoop(uint32_t interval_ms) {
    if (!initialized_ || running_.load()) return;

    interval_ms_ = interval_ms;
    running_.store(true);
    shutdown_requested_.store(false);

    correction_thread_ = std::make_unique<std::thread>(
        &TRESStabilizationLayer::correctionLoopFunc, this);
}

void TRESStabilizationLayer::stopCorrectionLoop() {
    if (!running_.load()) return;

    shutdown_requested_.store(true);
    running_.store(false);

    if (correction_thread_ && correction_thread_->joinable()) {
        correction_thread_->join();
    }
    correction_thread_.reset();
}

void TRESStabilizationLayer::correctionLoopFunc() {
    while (!shutdown_requested_.load()) {
        // Get telemetry
        SystemTelemetry telemetry = {};
        if (telemetry_cb_) {
            telemetry = telemetry_cb_();
        }

        // Update TPS history
        tps_history_[tps_index_] = telemetry.tps_current;
        tps_index_ = (tps_index_ + 1) % TPS_WINDOW_SIZE;
        if (tps_index_ == 0) {
            tps_window_full_ = true;
        }

        // Detect drift
        bool drift = detectDrift(telemetry);
        drift_detected_.store(drift);

        if (drift) {
            double severity = computeDriftSeverity(telemetry);
            drift_severity_.store(severity);

            // Compute and apply adjustment
            if (budget_cb_) {
                BudgetAdjustment adjustment = computeAdjustment(telemetry);
                budget_cb_(adjustment);
                last_adjustment_ = adjustment;
                correction_count_.fetch_add(1);
            }

            // Evaluate autopatch
            if (autopatch_cb_) {
                AutopatchSignal signal = evaluateAutopatch(telemetry);
                if (signal.trigger) {
                    autopatch_cb_(signal);
                    autopatch_count_.fetch_add(1);
                }
            }
        }

        total_observations_.fetch_add(1);

        // Sleep until next observation
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
    }
}

bool TRESStabilizationLayer::detectDrift(const SystemTelemetry& telemetry) {
    // Check TPS variance
    if (telemetry.tps_variance > thresholds_.tps_variance_max) {
        return true;
    }

    // Check KV pressure
    if (telemetry.kv_pressure > thresholds_.kv_pressure_max) {
        return true;
    }

    // Check GPU queue depth
    if (telemetry.gpu_queue_depth > thresholds_.gpu_queue_depth_max) {
        return true;
    }

    // Check latency spike
    if (telemetry.p99_latency_ms > thresholds_.latency_spike_ms) {
        return true;
    }

    // Check budget overrun
    if (telemetry.budget_utilization > 1.0 + thresholds_.budget_overrun_pct / 100.0) {
        return true;
    }

    return false;
}

double TRESStabilizationLayer::computeDriftSeverity(const SystemTelemetry& telemetry) {
    double severity = 0.0;

    // TPS variance contribution
    double tps_severity = telemetry.tps_variance / thresholds_.tps_variance_max;
    severity = std::max(severity, tps_severity);

    // KV pressure contribution
    double kv_severity = telemetry.kv_pressure / thresholds_.kv_pressure_max;
    severity = std::max(severity, kv_severity);

    // GPU queue contribution
    double gpu_severity = telemetry.gpu_queue_depth / thresholds_.gpu_queue_depth_max;
    severity = std::max(severity, gpu_severity);

    // Latency contribution
    double latency_severity = telemetry.p99_latency_ms / thresholds_.latency_spike_ms;
    severity = std::max(severity, latency_severity);

    return std::min(severity, 1.0);  // Cap at 1.0
}

BudgetAdjustment TRESStabilizationLayer::computeAdjustment(const SystemTelemetry& telemetry) {
    BudgetAdjustment adjustment = {};

    // Adjust based on KV pressure
    if (telemetry.kv_pressure > thresholds_.kv_pressure_max) {
        // Reduce decode budget to allow more time for KV management
        adjustment.decode_delta_ns = -500'000;  // -500us
        adjustment.throttle_factor = 0.9;
    }

    // Adjust based on GPU queue depth
    if (telemetry.gpu_queue_depth > thresholds_.gpu_queue_depth_max) {
        // Increase budgets to reduce dispatch pressure
        adjustment.prefill_delta_ns = 2'000'000;  // +2ms
        adjustment.decode_delta_ns = 1'000'000;    // +1ms
        adjustment.throttle_factor = 0.85;
    }

    // Adjust based on latency spike
    if (telemetry.p99_latency_ms > thresholds_.latency_spike_ms) {
        // Emergency: reduce budgets and throttle
        adjustment.prefill_delta_ns = -1'000'000;  // -1ms
        adjustment.decode_delta_ns = -500'000;      // -500us
        adjustment.tail_delta_ns = -200'000;        // -200us
        adjustment.throttle_factor = 0.75;
        adjustment.emergency_spill = true;
    }

    // Adjust based on budget utilization
    if (telemetry.budget_utilization > 1.0) {
        // Overrun: tighten budgets
        double overrun = telemetry.budget_utilization - 1.0;
        adjustment.prefill_delta_ns = static_cast<int32_t>(-1'000'000 * overrun);
        adjustment.decode_delta_ns = static_cast<int32_t>(-500'000 * overrun);
    } else if (telemetry.budget_utilization < 0.8) {
        // Underutilization: can increase budgets slightly
        adjustment.prefill_delta_ns = 500'000;  // +500us
        adjustment.decode_delta_ns = 250'000;    // +250us
    }

    return adjustment;
}

AutopatchSignal TRESStabilizationLayer::evaluateAutopatch(const SystemTelemetry& telemetry) {
    AutopatchSignal signal = {};

    // KV pressure autopatch
    if (telemetry.kv_pressure > 0.95) {
        signal.trigger = true;
        signal.signal_type = 1;
        signal.reason = "KV cache pressure critical";
        signal.severity = telemetry.kv_pressure;
        return signal;
    }

    // GPU overrun autopatch
    if (telemetry.gpu_queue_depth > 64) {
        signal.trigger = true;
        signal.signal_type = 2;
        signal.reason = "GPU queue depth critical";
        signal.severity = telemetry.gpu_queue_depth / 100.0;
        return signal;
    }

    // Latency spike autopatch
    if (telemetry.p99_latency_ms > thresholds_.latency_spike_ms * 2) {
        signal.trigger = true;
        signal.signal_type = 3;
        signal.reason = "Severe latency spike detected";
        signal.severity = std::min(telemetry.p99_latency_ms / 100.0, 1.0);
        return signal;
    }

    return signal;
}

void TRESStabilizationLayer::injectTelemetry(const SystemTelemetry& telemetry) {
    // Manual injection for testing
    if (telemetry_cb_) {
        // Override temporarily
        auto saved_cb = telemetry_cb_;
        telemetry_cb_ = [&telemetry]() { return telemetry; };

        // Process once
        if (detectDrift(telemetry) && budget_cb_) {
            BudgetAdjustment adjustment = computeAdjustment(telemetry);
            budget_cb_(adjustment);
        }

        // Restore
        telemetry_cb_ = saved_cb;
    }
}

void TRESStabilizationLayer::setThresholds(const DriftThresholds& thresholds) {
    thresholds_ = thresholds;
}

// ============================================================================
// T2 Layer: Control Layer
// ============================================================================
TRESControlLayer::TRESControlLayer() = default;

TRESControlLayer::~TRESControlLayer() = default;

void TRESControlLayer::setBaseBudgets(uint64_t prefill_ns, uint64_t decode_ns, uint64_t tail_ns) {
    prefill_budget_ns_.store(prefill_ns);
    decode_budget_ns_.store(decode_ns);
    tail_budget_ns_.store(tail_ns);
}

void TRESControlLayer::adjustBudgets(const BudgetAdjustment& adjustment) {
    // Apply deltas with bounds checking
    int64_t prefill = static_cast<int64_t>(prefill_budget_ns_.load()) + adjustment.prefill_delta_ns;
    int64_t decode = static_cast<int64_t>(decode_budget_ns_.load()) + adjustment.decode_delta_ns;
    int64_t tail = static_cast<int64_t>(tail_budget_ns_.load()) + adjustment.tail_delta_ns;

    // Clamp to reasonable bounds (1ms - 100ms)
    prefill = std::clamp(prefill, int64_t(1'000'000), int64_t(100'000'000));
    decode = std::clamp(decode, int64_t(500'000), int64_t(50'000'000));
    tail = std::clamp(tail, int64_t(200'000), int64_t(20'000'000));

    prefill_budget_ns_.store(static_cast<uint64_t>(prefill));
    decode_budget_ns_.store(static_cast<uint64_t>(decode));
    tail_budget_ns_.store(static_cast<uint64_t>(tail));

    // Apply throttle
    throttle_factor_.store(adjustment.throttle_factor);
}

void TRESControlLayer::setPhasePriority(uint32_t phase_id, uint32_t priority) {
    // Implementation would store phase priorities
    // Simplified for now
}

uint32_t TRESControlLayer::getPhasePriority(uint32_t phase_id) const {
    // Default priorities
    switch (phase_id) {
        case 0: return 10;  // PREFILL
        case 1: return 5;   // FIRST_TOKEN
        case 2: return 3;   // STEADY_DECODE
        case 3: return 2;   // TAIL
        default: return 1;
    }
}

void TRESControlLayer::setThrottleFactor(double factor) {
    throttle_factor_.store(std::clamp(factor, 0.1, 2.0));
}

// ============================================================================
// TRES System: Complete Integration
// ============================================================================
TRESSystem::TRESSystem() = default;

TRESSystem::~TRESSystem() {
    shutdown();
}

bool TRESSystem::initialize(TRESStabilizationLayer::TelemetryCallback telemetry_cb,
                             TRESStabilizationLayer::AutopatchCallback autopatch_cb) {
    stabilization_ = std::make_unique<TRESStabilizationLayer>();
    control_ = std::make_unique<TRESControlLayer>();

    // Wire T3 -> T2 via budget callback
    auto budget_cb = [this](const BudgetAdjustment& adj) {
        onBudgetAdjustment(adj);
    };

    return stabilization_->initialize(telemetry_cb, budget_cb, autopatch_cb);
}

void TRESSystem::shutdown() {
    if (stabilization_) {
        stabilization_->shutdown();
    }
    stabilization_.reset();
    control_.reset();
}

void TRESSystem::start(uint32_t interval_ms) {
    if (stabilization_) {
        stabilization_->startCorrectionLoop(interval_ms);
    }
}

void TRESSystem::stop() {
    if (stabilization_) {
        stabilization_->stopCorrectionLoop();
    }
}

bool TRESSystem::isRunning() const {
    return stabilization_ && stabilization_->isRunning();
}

SystemTelemetry TRESSystem::getCurrentTelemetry() const {
    if (stabilization_ && stabilization_->isRunning()) {
        // Would need to store last telemetry
        return {};
    }
    return {};
}

bool TRESSystem::isStable() const {
    return stabilization_ && !stabilization_->isDriftDetected();
}

void TRESSystem::onBudgetAdjustment(const BudgetAdjustment& adjustment) {
    if (control_) {
        control_->adjustBudgets(adjustment);
    }
}

// ============================================================================
// C API Implementation
// ============================================================================
extern "C" {

struct TRESUserData {
    void* user_data;
    RawrXD_TelemetryCallback telemetry_cb;
    RawrXD_AutopatchCallback autopatch_cb;
};

RawrXD_TRESSystem rawrxd_tres_create(
    RawrXD_TelemetryCallback telemetry_cb,
    RawrXD_AutopatchCallback autopatch_cb,
    void* user_data) {

    auto* tres = new TRESSystem();

    auto t_cb = [user_data, telemetry_cb]() -> SystemTelemetry {
        if (telemetry_cb) {
            SystemTelemetry telemetry = {};
            telemetry_cb(user_data, &telemetry);
            return telemetry;
        }
        return {};
    };

    auto a_cb = [user_data, autopatch_cb](const AutopatchSignal& signal) {
        if (autopatch_cb) {
            autopatch_cb(user_data, const_cast<AutopatchSignal*>(&signal));
        }
    };

    if (!tres->initialize(t_cb, a_cb)) {
        delete tres;
        return nullptr;
    }

    return tres;
}

void rawrxd_tres_destroy(RawrXD_TRESSystem handle) {
    if (handle) {
        auto* tres = static_cast<TRESSystem*>(handle);
        tres->shutdown();
        delete tres;
    }
}

void rawrxd_tres_start(RawrXD_TRESSystem handle, uint32_t interval_ms) {
    if (handle) {
        auto* tres = static_cast<TRESSystem*>(handle);
        tres->start(interval_ms);
    }
}

void rawrxd_tres_stop(RawrXD_TRESSystem handle) {
    if (handle) {
        auto* tres = static_cast<TRESSystem*>(handle);
        tres->stop();
    }
}

int rawrxd_tres_is_stable(RawrXD_TRESSystem handle) {
    if (!handle) return 0;
    auto* tres = static_cast<TRESSystem*>(handle);
    return tres->isStable() ? 1 : 0;
}

} // extern "C"

} // namespace TRES
} // namespace RawrXD
