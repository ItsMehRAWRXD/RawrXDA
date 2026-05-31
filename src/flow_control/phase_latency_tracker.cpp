// ============================================================================
// phase_latency_tracker.cpp — Predictive Phase-Aware Latency Tracker
// ============================================================================

#include "flow_control/phase_latency_tracker.hpp"
#include "flow_control/credit_governor.hpp"
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <mutex>

namespace RawrXD {
namespace FlowControl {

// ============================================================================
// Phase name mapping
// ============================================================================
const char* PhaseName(Phase p) {
    switch (p) {
        case Phase::Unknown:           return "Unknown";
        case Phase::RustFunctionDecl:  return "RustFunctionDecl";
        case Phase::RustStructDecl:    return "RustStructDecl";
        case Phase::RustEnumDecl:      return "RustEnumDecl";
        case Phase::RustTraitDecl:     return "RustTraitDecl";
        case Phase::RustImplBlock:     return "RustImplBlock";
        case Phase::RustMatchBlock:    return "RustMatchBlock";
        case Phase::RustUseStatement:  return "RustUseStatement";
        case Phase::RustLetBinding:    return "RustLetBinding";
        case Phase::RustGenericParams:  return "RustGenericParams";
        case Phase::RustWhereClause:    return "RustWhereClause";
        case Phase::Tokenize:          return "Tokenize";
        case Phase::Parse:             return "Parse";
        case Phase::TypeCheck:         return "TypeCheck";
        case Phase::Lower:             return "Lower";
        case Phase::Optimize:          return "Optimize";
        case Phase::Codegen:           return "Codegen";
        case Phase::Prefill:           return "Prefill";
        case Phase::Decode:            return "Decode";
        case Phase::Verify:            return "Verify";
        default:                       return "Invalid";
    }
}

// ============================================================================
// PhaseStats
// ============================================================================
void PhaseStats::Record(uint64_t micros) {
    totalCalls++;
    totalMicros += micros;
    if (micros > maxMicros) maxMicros = micros;
    if (micros < minMicros) minMicros = micros;
    lastMicros = micros;
    emaMicros = emaAlpha * static_cast<double>(micros) + (1.0 - emaAlpha) * emaMicros;
    lastCall = std::chrono::steady_clock::now();
}

bool PhaseStats::IsBursty() const {
    if (totalCalls < 3) return false;
    double avg = AvgMicros();
    if (avg <= 0.0) return false;
    // Burst = recent EMA deviates significantly from average
    double deviation = std::abs(emaMicros - avg) / avg;
    return deviation > 0.5;  // >50% variance = bursty
}

// ============================================================================
// PhaseLatencyTracker
// ============================================================================
PhaseLatencyTracker& PhaseLatencyTracker::instance() {
    static PhaseLatencyTracker inst;
    return inst;
}

void PhaseLatencyTracker::Record(Phase phase, uint64_t micros) {
    size_t idx = static_cast<size_t>(phase);
    if (idx >= static_cast<size_t>(Phase::Count)) return;

    std::lock_guard<std::mutex> lock(mutex_);
    stats_[idx].Record(micros);
    totalRecordedCalls_.fetch_add(1, std::memory_order_relaxed);
}

bool PhaseLatencyTracker::IsHeavyNesting() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (nestingStack_.empty()) return false;

    uint32_t heavyDepth = 0;
    for (const auto& ctx : nestingStack_) {
        size_t idx = static_cast<size_t>(ctx.phase);
        if (idx < static_cast<size_t>(Phase::Count) && stats_[idx].IsHeavy()) {
            heavyDepth++;
        }
    }
    return heavyDepth >= 3;  // 3+ nested heavy phases = heavy nesting
}

NestingContext PhaseLatencyTracker::DeepestNesting() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (nestingStack_.empty()) return {};
    return nestingStack_.back();
}

void PhaseLatencyTracker::FlushToGovernor(CreditGovernor* governor) {
    if (!governor) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // Calculate aggregate load factor from recent phase latencies
    double totalWeight = 0.0;
    double weightedLoad = 0.0;

    for (size_t i = 0; i < static_cast<size_t>(Phase::Count); ++i) {
        const auto& s = stats_[i];
        if (s.totalCalls == 0) continue;

        double load = s.emaMicros / 1000.0;  // Normalize to ms
        double weight = static_cast<double>(s.totalCalls);

        // Heavy phases get amplified weight
        if (s.IsHeavy()) weight *= 2.0;
        if (s.IsBursty()) weight *= 1.5;

        weightedLoad += load * weight;
        totalWeight += weight;
    }

    if (totalWeight <= 0.0) return;

    double avgLoad = weightedLoad / totalWeight;

    // Map load to credit adjustment
    // Base: 1.0ms → normal, >5ms → heavy, >20ms → critical
    GovernorTelemetry telemetry;
    telemetry.timestampMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count()
    );

    // If heavy nesting detected, boost throughput target temporarily
    if (IsHeavyNesting()) {
        telemetry.throughputElemPerSec = 8.0e9;  // Lower target = more credits
    } else if (avgLoad > 20.0) {
        telemetry.throughputElemPerSec = 6.0e9;   // Critical load
    } else if (avgLoad > 5.0) {
        telemetry.throughputElemPerSec = 10.0e9;  // Heavy load
    } else {
        telemetry.throughputElemPerSec = 12.0e9;  // Normal
    }

    governor->RecordTelemetry(telemetry);
}

const PhaseStats* PhaseLatencyTracker::GetStats(Phase phase) const {
    size_t idx = static_cast<size_t>(phase);
    if (idx >= static_cast<size_t>(Phase::Count)) return nullptr;
    std::lock_guard<std::mutex> lock(mutex_);
    return &stats_[idx];
}

void PhaseLatencyTracker::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : stats_) {
        s = PhaseStats{};
    }
    nestingStack_.clear();
    totalRecordedCalls_.store(0, std::memory_order_relaxed);
}

std::string PhaseLatencyTracker::ReportJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "{";
    oss << "\"totalCalls\":" << totalRecordedCalls_.load() << ",";
    oss << "\"phases\":[";

    bool first = true;
    for (size_t i = 0; i < static_cast<size_t>(Phase::Count); ++i) {
        const auto& s = stats_[i];
        if (s.totalCalls == 0) continue;

        if (!first) oss << ",";
        first = false;

        oss << "{";
        oss << "\"name\":\"" << PhaseName(static_cast<Phase>(i)) << "\",";
        oss << "\"calls\":" << s.totalCalls << ",";
        oss << "\"avgUs\":" << std::fixed << std::setprecision(2) << s.AvgMicros() << ",";
        oss << "\"emaUs\":" << s.emaMicros << ",";
        oss << "\"maxUs\":" << s.maxMicros << ",";
        oss << "\"minUs\":" << (s.minMicros == UINT64_MAX ? 0 : s.minMicros) << ",";
        oss << "\"heavy\":" << (s.IsHeavy() ? "true" : "false") << ",";
        oss << "\"bursty\":" << (s.IsBursty() ? "true" : "false");
        oss << "}";
    }

    oss << "],";
    oss << "\"nestingDepth\":" << nestingStack_.size() << ",";
    oss << "\"heavyNesting\":" << (IsHeavyNesting() ? "true" : "false");
    oss << "}";
    return oss.str();
}

void PhaseLatencyTracker::PushPhase(Phase phase) {
    std::lock_guard<std::mutex> lock(mutex_);
    NestingContext ctx;
    ctx.phase = phase;
    ctx.depth = static_cast<uint32_t>(nestingStack_.size()) + 1;
    nestingStack_.push_back(ctx);
}

void PhaseLatencyTracker::PopPhase(uint64_t micros) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (nestingStack_.empty()) return;

    auto& ctx = nestingStack_.back();
    ctx.cumulativeMicros += micros;

    size_t idx = static_cast<size_t>(ctx.phase);
    if (idx < static_cast<size_t>(Phase::Count)) {
        stats_[idx].Record(micros);
    }

    // Mark as heavy nesting if depth > 3 with heavy phases
    uint32_t heavyDepth = 0;
    for (const auto& c : nestingStack_) {
        size_t ci = static_cast<size_t>(c.phase);
        if (ci < static_cast<size_t>(Phase::Count) && stats_[ci].IsHeavy()) {
            heavyDepth++;
        }
    }
    ctx.isHeavyNesting = (heavyDepth >= 3);

    nestingStack_.pop_back();
}

// ============================================================================
// PhaseScopeGuard
// ============================================================================
PhaseScopeGuard::PhaseScopeGuard(Phase phase)
    : phase_(phase), start_(std::chrono::steady_clock::now()), completed_(false) {
    PhaseLatencyTracker::instance().PushPhase(phase);
}

PhaseScopeGuard::~PhaseScopeGuard() {
    if (!completed_) {
        Complete();
    }
}

void PhaseScopeGuard::Complete() {
    if (completed_) return;
    auto end = std::chrono::steady_clock::now();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
    PhaseLatencyTracker::instance().PopPhase(static_cast<uint64_t>(micros));
    completed_ = true;
}

// ============================================================================
// PredictiveGovernor
// ============================================================================
PredictiveGovernor::PredictiveGovernor(CreditGovernor* baseGovernor)
    : baseGovernor_(baseGovernor), tracker_(PhaseLatencyTracker::instance()) {}

void PredictiveGovernor::OnPhaseComplete(Phase phase, uint64_t micros) {
    tracker_.Record(phase, micros);

    // Update load factor based on recent heavy phases
    const PhaseStats* stats = tracker_.GetStats(phase);
    if (stats && stats->IsHeavy()) {
        currentLoadFactor_ = std::min(currentLoadFactor_ * 1.2, 3.0);  // Cap at 3x
    } else {
        currentLoadFactor_ = std::max(currentLoadFactor_ * 0.95, 1.0);  // Decay to 1x
    }

    // Flush to base governor periodically
    if (baseGovernor_) {
        tracker_.FlushToGovernor(baseGovernor_);
    }
}

void PredictiveGovernor::EnterHeavyRegion(Phase phase) {
    inHeavyRegion_ = true;
    heavyPhase_ = phase;
    currentLoadFactor_ = std::min(currentLoadFactor_ * 1.5, 3.0);

    // Preemptive credit boost
    ApplyPredictiveBoost();
}

void PredictiveGovernor::ExitHeavyRegion(Phase phase) {
    if (heavyPhase_ == phase) {
        inHeavyRegion_ = false;
        heavyPhase_ = Phase::Unknown;
        currentLoadFactor_ = std::max(currentLoadFactor_ * 0.8, 1.0);
    }
}

void PredictiveGovernor::ApplyPredictiveBoost() {
    if (!baseGovernor_) return;

    // Calculate preemptive credit boost
    uint32_t boostCredits = static_cast<uint32_t>(50.0 * (currentLoadFactor_ - 1.0));
    if (boostCredits > 0) {
        // Temporarily override minCredits upward
        auto config = baseGovernor_->GetCurrentConfig();
        uint32_t newMin = config.minCredits + boostCredits;
        baseGovernor_->OverrideMinCredits(newMin);
    }
}

double PredictiveGovernor::PredictedLoadFactor() const {
    return currentLoadFactor_;
}

} // namespace FlowControl
} // namespace RawrXD
