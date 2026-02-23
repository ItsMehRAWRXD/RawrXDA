// ============================================================================
// RawrXD Quantum Beaconism Fusion Backend — Implementation
// Fuses all 10 Dual Engines into a single coherent optimization backend
// Uses quantum-inspired simulated annealing + entanglement propagation
// ============================================================================
#include "quantum_beaconism_backend.h"
#include "dual_engine_system.h"
#include "unified_overclock_governor.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <sstream>

namespace RawrXD {

// Type alias for nested FusionResult
using FusionResult = QuantumBeaconismBackend::FusionResult;

// ============================================================================
// State string table
// ============================================================================
const char* FusionStateToString(FusionState s) {
    switch (s) {
    case FusionState::Dormant:       return "Dormant";
    case FusionState::Calibrating:   return "Calibrating";
    case FusionState::Superposition: return "Superposition";
    case FusionState::Collapsing:    return "Collapsing";
    case FusionState::Entangled:     return "Entangled";
    case FusionState::Beaconing:     return "Beaconing";
    case FusionState::Faulted:       return "Faulted";
    default:                         return "Unknown";
    }
}

// ============================================================================
// Singleton
// ============================================================================
QuantumBeaconismBackend& QuantumBeaconismBackend::Instance() {
    static QuantumBeaconismBackend inst;
    return inst;
}

QuantumBeaconismBackend::QuantumBeaconismBackend()
    : rng_(static_cast<uint32_t>(
          std::chrono::steady_clock::now().time_since_epoch().count()))
{
    // Initialize all qubits in |+⟩ superposition
    for (auto& q : engineQubits_) {
        q = Qubit::superposition();
    }
    std::memset(&latestBeacon_, 0, sizeof(latestBeacon_));
}

QuantumBeaconismBackend::~QuantumBeaconismBackend() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
FusionResult QuantumBeaconismBackend::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_.load()) return FusionResult::ok("Already running");

    state_.store(FusionState::Calibrating);
    epoch_ = 0;
    history_.clear();
    entanglements_.clear();

    // Reset qubits to superposition
    for (auto& q : engineQubits_) {
        q = Qubit::superposition();
    }

    // Reset simulated annealing
    saTemperature_ = 1000.0f;

    return FusionResult::ok("Quantum Beaconism Backend initialized");
}

FusionResult QuantumBeaconismBackend::shutdown() {
    if (!running_.load() && state_.load() == FusionState::Dormant)
        return FusionResult::ok("Not running");

    running_.store(false);
    if (fusionThread_.joinable()) fusionThread_.join();

    state_.store(FusionState::Dormant);
    return FusionResult::ok("Quantum Beaconism Backend shutdown");
}

FusionState QuantumBeaconismBackend::getState() const {
    return state_.load();
}

// ============================================================================
// Fusion Control
// ============================================================================
FusionResult QuantumBeaconismBackend::beginFusion() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_.load()) return FusionResult::error("Already fusing");

    running_.store(true);
    state_.store(FusionState::Calibrating);

    fusionThread_ = std::thread([this]() { fusionLoop(); });
    return FusionResult::ok("Fusion started");
}

FusionResult QuantumBeaconismBackend::pauseFusion() {
    if (!running_.load()) return FusionResult::error("Not running");
    running_.store(false);
    if (fusionThread_.joinable()) fusionThread_.join();
    return FusionResult::ok("Fusion paused");
}

FusionResult QuantumBeaconismBackend::resumeFusion() {
    if (running_.load()) return FusionResult::error("Already running");
    if (state_.load() == FusionState::Dormant)
        return FusionResult::error("Not initialized");

    running_.store(true);
    fusionThread_ = std::thread([this]() { fusionLoop(); });
    return FusionResult::ok("Fusion resumed");
}

FusionResult QuantumBeaconismBackend::resetFusion() {
    if (running_.load()) {
        running_.store(false);
        if (fusionThread_.joinable()) fusionThread_.join();
    }
    state_.store(FusionState::Calibrating);
    saTemperature_ = 1000.0f;
    epoch_ = 0;
    entanglements_.clear();

    for (auto& q : engineQubits_) {
        q = Qubit::superposition();
    }

    return FusionResult::ok("Fusion reset to calibration");
}

// ============================================================================
// Entanglement Management
// ============================================================================
FusionResult QuantumBeaconismBackend::entangleEngines(uint32_t engineA, uint32_t engineB,
                                                       float strength, bool antiCorrelated) {
    if (engineA >= 10 || engineB >= 10 || engineA == engineB)
        return FusionResult::error("Invalid engine indices");

    // ============================================================================
    // RawrXD Quantum Beaconism Fusion Backend — Deterministic MASM x64 backend
    // Replaces simulated/random behavior with concrete state convergence kernels.
    // ============================================================================
    #include "quantum_beaconism_backend.h"
    #include "dual_engine_system.h"
    #include "unified_overclock_governor.h"

    #include <algorithm>
    #include <cmath>
    #include <cstdlib>
    #include <cstring>
    #include <sstream>
    #include <thread>

    namespace RawrXD {

    namespace {

    constexpr size_t kEngineCount = 10;

    constexpr float kEngineWeights[kEngineCount] = {
        1.5f, 1.0f, 1.2f, 1.1f, 0.8f,
        0.7f, 1.3f, 1.4f, 1.2f, 0.5f
    };

    inline float clamp01(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }

    inline bool parseFloatNoExcept(const std::string& text, float& outValue) {
        const char* begin = text.c_str();
        char* end = nullptr;
        const float parsed = std::strtof(begin, &end);
        if (end == begin) return false;
        while (*end == ' ' || *end == '\t') ++end;
        if (*end != '\0') return false;
        outValue = parsed;
        return true;
    }

    #if defined(RAWR_HAS_MASM) && RAWR_HAS_MASM
    extern "C" void qb_masm_normalize_qubit(float* alpha, float* beta);
    extern "C" float qb_masm_weighted_fitness(const float* values, const float* weights, uint32_t count);
    extern "C" void qb_masm_entangle_pair(float* betaA, float* betaB, float strength, uint32_t antiCorrelated);
    extern "C" float qb_masm_abs_dot2_ptr(const float* lhs2, const float* rhs2);

    inline void normalizeQubit(Qubit& q) {
        qb_masm_normalize_qubit(&q.alpha, &q.beta);
    }

    inline float weightedFitness(const float* values, const float* weights, uint32_t count) {
        return qb_masm_weighted_fitness(values, weights, count);
    }

    inline void applyEntangledUpdate(float& betaA, float& betaB, float strength, bool antiCorrelated) {
        qb_masm_entangle_pair(&betaA, &betaB, strength, antiCorrelated ? 1u : 0u);
    }

    inline float absDot2(float a0, float a1, float b0, float b1) {
        const float lhs[2] = {a0, a1};
        const float rhs[2] = {b0, b1};
        return qb_masm_abs_dot2_ptr(lhs, rhs);
    }
    #else
    inline void normalizeQubit(Qubit& q) {
        const float norm = std::sqrt(q.alpha * q.alpha + q.beta * q.beta);
        if (norm <= 0.000001f) {
            q.alpha = 0.70710678f;
            q.beta = 0.70710678f;
            return;
        }
        q.alpha /= norm;
        q.beta /= norm;
    }

    inline float weightedFitness(const float* values, const float* weights, uint32_t count) {
        if (!values || !weights || count == 0) return 0.0f;
        float weightedSum = 0.0f;
        float weightSum = 0.0f;
        for (uint32_t i = 0; i < count; ++i) {
            weightedSum += values[i] * weights[i];
            weightSum += weights[i];
        }
        if (weightSum <= 0.000001f) return 0.0f;
        return weightedSum / weightSum;
    }

    inline void applyEntangledUpdate(float& betaA, float& betaB, float strength, bool antiCorrelated) {
        const float blend = strength * 0.1f;
        const float delta = antiCorrelated ? ((1.0f - betaB) - betaA) * blend : (betaB - betaA) * blend;
        betaA = clamp01(betaA + delta);
        betaB = clamp01(betaB - delta);
    }

    inline float absDot2(float a0, float a1, float b0, float b1) {
        return std::abs(a0 * b0 + a1 * b1);
    }
    #endif

    } // namespace

    const char* FusionStateToString(FusionState s) {
        switch (s) {
        case FusionState::Dormant:       return "Dormant";
        case FusionState::Calibrating:   return "Calibrating";
        case FusionState::Superposition: return "Superposition";
        case FusionState::Collapsing:    return "Collapsing";
        case FusionState::Entangled:     return "Entangled";
        case FusionState::Beaconing:     return "Beaconing";
        case FusionState::Faulted:       return "Faulted";
        default:                         return "Unknown";
        }
    }

    QuantumBeaconismBackend& QuantumBeaconismBackend::Instance() {
        static QuantumBeaconismBackend instance;
        return instance;
    }

    QuantumBeaconismBackend::QuantumBeaconismBackend() {
        for (auto& q : engineQubits_) {
            q = Qubit::superposition();
        }
        std::memset(&latestBeacon_, 0, sizeof(latestBeacon_));
        latestBeacon_.timestamp = std::chrono::steady_clock::now();
    }

    QuantumBeaconismBackend::~QuantumBeaconismBackend() {
        shutdown();
    }

    FusionResult QuantumBeaconismBackend::initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_.load()) return FusionResult::error("Cannot initialize while running");

        state_.store(FusionState::Calibrating);
        epoch_ = 0;
        saTemperature_ = std::max(saTemperature_, saMinTemperature_);
        history_.clear();
        entanglements_.clear();
        latestBeacon_ = Beacon{};
        latestBeacon_.timestamp = std::chrono::steady_clock::now();

        for (auto& q : engineQubits_) {
            q = Qubit::superposition();
            normalizeQubit(q);
        }

        return FusionResult::ok("Quantum Beaconism backend initialized");
    }

    FusionResult QuantumBeaconismBackend::shutdown() {
        running_.store(false);
        if (fusionThread_.joinable()) fusionThread_.join();

        state_.store(FusionState::Dormant);
        return FusionResult::ok("Quantum Beaconism backend shutdown");
    }

    FusionState QuantumBeaconismBackend::getState() const {
        return state_.load();
    }

    FusionResult QuantumBeaconismBackend::beginFusion() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_.load()) return FusionResult::error("Fusion already running");

        if (state_.load() == FusionState::Dormant) {
            state_.store(FusionState::Calibrating);
        }
        running_.store(true);
        fusionThread_ = std::thread([this]() { fusionLoop(); });
        return FusionResult::ok("Fusion started");
    }

    FusionResult QuantumBeaconismBackend::pauseFusion() {
        if (!running_.load()) return FusionResult::error("Fusion is not running");
        running_.store(false);
        if (fusionThread_.joinable()) fusionThread_.join();
        return FusionResult::ok("Fusion paused");
    }

    FusionResult QuantumBeaconismBackend::resumeFusion() {
        if (running_.load()) return FusionResult::error("Fusion already running");
        if (state_.load() == FusionState::Dormant) return FusionResult::error("Backend not initialized");

        running_.store(true);
        fusionThread_ = std::thread([this]() { fusionLoop(); });
        return FusionResult::ok("Fusion resumed");
    }

    FusionResult QuantumBeaconismBackend::resetFusion() {
        running_.store(false);
        if (fusionThread_.joinable()) fusionThread_.join();

        std::lock_guard<std::mutex> lock(mutex_);
        state_.store(FusionState::Calibrating);
        saTemperature_ = std::max(1000.0f, saMinTemperature_);
        epoch_ = 0;
        entanglements_.clear();
        history_.clear();
        for (auto& q : engineQubits_) {
            q = Qubit::superposition();
            normalizeQubit(q);
        }
        return FusionResult::ok("Fusion reset");
    }

    FusionResult QuantumBeaconismBackend::entangleEngines(uint32_t engineA, uint32_t engineB, float strength, bool antiCorrelated) {
        if (engineA >= kEngineCount || engineB >= kEngineCount || engineA == engineB) {
            return FusionResult::error("Invalid engine index");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        const float clampedStrength = clamp01(strength);

        for (auto& pair : entanglements_) {
            if ((pair.engineA == engineA && pair.engineB == engineB) ||
                (pair.engineA == engineB && pair.engineB == engineA)) {
                pair.correlationStrength = clampedStrength;
                pair.antiCorrelated = antiCorrelated;
                return FusionResult::ok("Entanglement updated");
            }
        }

        entanglements_.push_back({engineA, engineB, clampedStrength, antiCorrelated});
        return FusionResult::ok("Entanglement created");
    }

    FusionResult QuantumBeaconismBackend::disentangleEngines(uint32_t engineA, uint32_t engineB) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = std::remove_if(entanglements_.begin(), entanglements_.end(),
            [engineA, engineB](const EntangledPair& pair) {
                return (pair.engineA == engineA && pair.engineB == engineB) ||
                       (pair.engineA == engineB && pair.engineB == engineA);
            });

        if (it == entanglements_.end()) return FusionResult::error("Entanglement not found");

        entanglements_.erase(it, entanglements_.end());
        return FusionResult::ok("Entanglement removed");
    }

    FusionResult QuantumBeaconismBackend::autoEntangle() {
        std::lock_guard<std::mutex> lock(mutex_);
        entanglements_.clear();

        entanglements_.push_back({0, 7, 0.90f, false});
        entanglements_.push_back({1, 8, 0.82f, false});
        entanglements_.push_back({2, 3, 0.95f, true});
        entanglements_.push_back({4, 5, 0.74f, false});
        entanglements_.push_back({6, 9, 0.88f, false});

        return FusionResult::ok("Auto entanglement profile applied");
    }

    std::vector<EntangledPair> QuantumBeaconismBackend::getEntanglements() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entanglements_;
    }

    Beacon QuantumBeaconismBackend::getLatestBeacon() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return latestBeacon_;
    }

    FusionResult QuantumBeaconismBackend::forceBeacon() {
        broadcastBeacon();
        return FusionResult::ok("Beacon broadcast");
    }

    void QuantumBeaconismBackend::setBeaconInterval(uint32_t ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        beaconIntervalMs_ = std::max(10u, ms);
    }

    void QuantumBeaconismBackend::setInitialTemperature(float temp) {
        std::lock_guard<std::mutex> lock(mutex_);
        saTemperature_ = std::max(temp, saMinTemperature_);
    }

    void QuantumBeaconismBackend::setCoolingRate(float rate) {
        std::lock_guard<std::mutex> lock(mutex_);
        saCoolingRate_ = std::clamp(rate, 0.50f, 0.99999f);
    }

    void QuantumBeaconismBackend::setMinTemperature(float temp) {
        std::lock_guard<std::mutex> lock(mutex_);
        saMinTemperature_ = std::max(temp, 0.00001f);
        if (saTemperature_ < saMinTemperature_) {
            saTemperature_ = saMinTemperature_;
        }
    }

    float QuantumBeaconismBackend::getCurrentTemperature() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return saTemperature_;
    }

    FusionTelemetry QuantumBeaconismBackend::getTelemetry() const {
        std::lock_guard<std::mutex> lock(mutex_);

        FusionTelemetry telemetry{};
        telemetry.state = state_.load();
        telemetry.epoch = epoch_;
        telemetry.annealingTemperature = saTemperature_;
        telemetry.entangledPairs = static_cast<uint32_t>(entanglements_.size());
        telemetry.lastBeacon = latestBeacon_.timestamp;
        telemetry.systemFitness = computeFitness();

        for (size_t i = 0; i < kEngineCount; ++i) {
            telemetry.engineFitness[i] = clamp01(engineQubits_[i].beta * engineQubits_[i].beta);
        }

        telemetry.convergenceRate = 0.0f;
        if (!history_.empty()) {
            telemetry.convergenceRate = telemetry.systemFitness - history_.back().systemFitness;
        }

        telemetry.beaconsBroadcast = static_cast<uint32_t>(epoch_);
        telemetry.collapseCount = 0;
        for (const auto& h : history_) {
            if (h.state == FusionState::Collapsing) telemetry.collapseCount++;
        }

        telemetry.totalPowerSavingsWatts = 0.0f;
        telemetry.totalPerfGainPct = telemetry.systemFitness * 100.0f;
        telemetry.thermalHeadroom = 0.0f;
        if (overclockGov_) {
            const auto govTelemetry = overclockGov_->getSystemTelemetry();
            telemetry.thermalHeadroom = std::clamp(100.0f - govTelemetry.totalThermalEnvelopeC, 0.0f, 100.0f);
        }

        return telemetry;
    }

    std::vector<FusionTelemetry> QuantumBeaconismBackend::getHistory(uint32_t maxEntries) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (maxEntries == 0 || maxEntries >= history_.size()) {
            return history_;
        }
        return std::vector<FusionTelemetry>(history_.end() - maxEntries, history_.end());
    }

    FusionResult QuantumBeaconismBackend::bridgeOverclockGovernor(UnifiedOverclockGovernor* gov) {
        std::lock_guard<std::mutex> lock(mutex_);
        overclockGov_ = gov;
        return FusionResult::ok("Overclock governor bridged");
    }

    FusionResult QuantumBeaconismBackend::bridgeDualEngines(DualEngineCoordinator* coord) {
        std::lock_guard<std::mutex> lock(mutex_);
        dualEngines_ = coord;
        return FusionResult::ok("Dual engine coordinator bridged");
    }

    FusionResult QuantumBeaconismBackend::dispatchCLI(const std::string& command, const std::string& args) {
        if (command == "start" || command == "begin") return beginFusion();
        if (command == "stop" || command == "pause") return pauseFusion();
        if (command == "resume") return resumeFusion();
        if (command == "reset") return resetFusion();
        if (command == "status") return FusionResult::ok(FusionStateToString(getState()));
        if (command == "auto-entangle") return autoEntangle();
        if (command == "beacon") return forceBeacon();

        if (command == "entangle") {
            uint32_t a = 0;
            uint32_t b = 0;
            float strength = 0.8f;
            bool anti = false;

            std::istringstream iss(args);
            iss >> a >> b >> strength;
            std::string antiArg;
            if (iss >> antiArg) {
                anti = (antiArg == "anti" || antiArg == "true" || antiArg == "1");
            }
            return entangleEngines(a, b, strength, anti);
        }

        if (command == "disentangle") {
            uint32_t a = 0;
            uint32_t b = 0;
            std::istringstream iss(args);
            iss >> a >> b;
            return disentangleEngines(a, b);
        }

        if (command == "temperature") {
            float value = 1000.0f;
            if (!parseFloatNoExcept(args, value)) return FusionResult::error("Invalid temperature value");
            setInitialTemperature(value);
            return FusionResult::ok("Temperature updated");
        }

        if (command == "cooling-rate") {
            float value = 0.995f;
            if (!parseFloatNoExcept(args, value)) return FusionResult::error("Invalid cooling-rate value");
            setCoolingRate(value);
            return FusionResult::ok("Cooling rate updated");
        }

        if (command == "min-temperature") {
            float value = 0.01f;
            if (!parseFloatNoExcept(args, value)) return FusionResult::error("Invalid min-temperature value");
            setMinTemperature(value);
            return FusionResult::ok("Minimum temperature updated");
        }

        return FusionResult::error("Unknown beaconism command");
    }

    void QuantumBeaconismBackend::fusionLoop() {
        state_.store(FusionState::Calibrating);

        while (running_.load()) {
            ++epoch_;

            switch (state_.load()) {
            case FusionState::Calibrating:
                calibrate();
                if (epoch_ >= 2) state_.store(FusionState::Superposition);
                break;

            case FusionState::Superposition:
                superpositionExplore();
                if (saTemperature_ <= (saMinTemperature_ * 4.0f) || ((epoch_ & 0x1F) == 0)) {
                    state_.store(FusionState::Collapsing);
                }
                break;

            case FusionState::Collapsing:
                collapseToOptimal();
                state_.store(entanglements_.empty() ? FusionState::Beaconing : FusionState::Entangled);
                break;

            case FusionState::Entangled:
                entangledCooperate();
                state_.store(FusionState::Beaconing);
                break;

            case FusionState::Beaconing:
                broadcastBeacon();
                state_.store(entanglements_.empty() ? FusionState::Superposition : FusionState::Entangled);
                break;

            case FusionState::Faulted:
                resetFusion();
                state_.store(FusionState::Calibrating);
                break;

            case FusionState::Dormant:
            default:
                state_.store(FusionState::Calibrating);
                break;
            }

            const FusionTelemetry snapshot = getTelemetry();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (history_.size() >= MAX_HISTORY) {
                    history_.erase(history_.begin());
                }
                history_.push_back(snapshot);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(beaconIntervalMs_));
        }
    }

    void QuantumBeaconismBackend::calibrate() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (dualEngines_) {
            const auto telemetry = dualEngines_->getAllTelemetry();
            const size_t count = std::min(telemetry.size(), kEngineCount);
            for (size_t i = 0; i < count; ++i) {
                const float efficiency = clamp01(telemetry[i].efficiencyRatio);
                engineQubits_[i].beta = efficiency;
                engineQubits_[i].alpha = std::sqrt(std::max(0.0f, 1.0f - (efficiency * efficiency)));
                normalizeQubit(engineQubits_[i]);
            }
        }

        if (overclockGov_) {
            const auto govTelemetry = overclockGov_->getSystemTelemetry();
            const float headroom = std::clamp(100.0f - govTelemetry.totalThermalEnvelopeC, 0.0f, 100.0f) / 100.0f;
            for (auto& q : engineQubits_) {
                q.beta = clamp01(q.beta * (0.85f + headroom * 0.30f));
                q.alpha = std::sqrt(std::max(0.0f, 1.0f - (q.beta * q.beta)));
                normalizeQubit(q);
            }
        }
    }

    void QuantumBeaconismBackend::superpositionExplore() {
        std::lock_guard<std::mutex> lock(mutex_);

        std::array<float, kEngineCount> targets{};
        for (size_t i = 0; i < kEngineCount; ++i) {
            targets[i] = engineQubits_[i].beta;
        }

        if (dualEngines_) {
            const auto telemetry = dualEngines_->getAllTelemetry();
            const size_t count = std::min(telemetry.size(), kEngineCount);
            for (size_t i = 0; i < count; ++i) {
                targets[i] = clamp01(telemetry[i].efficiencyRatio);
            }
        }

        const float thermalScale = overclockGov_
            ? std::clamp((100.0f - overclockGov_->getSystemTelemetry().totalThermalEnvelopeC) / 100.0f, 0.25f, 1.0f)
            : 1.0f;

        const float annealScale = std::clamp(saTemperature_ / 1000.0f, 0.05f, 1.0f);
        const float step = 0.20f * thermalScale * annealScale;

        for (size_t i = 0; i < kEngineCount; ++i) {
            const float delta = (targets[i] - engineQubits_[i].beta) * step;
            engineQubits_[i].beta = clamp01(engineQubits_[i].beta + delta);
            engineQubits_[i].alpha = std::sqrt(std::max(0.0f, 1.0f - (engineQubits_[i].beta * engineQubits_[i].beta)));
            normalizeQubit(engineQubits_[i]);
        }

        for (const auto& pair : entanglements_) {
            if (pair.engineA >= kEngineCount || pair.engineB >= kEngineCount) continue;
            float& betaA = engineQubits_[pair.engineA].beta;
            float& betaB = engineQubits_[pair.engineB].beta;
            applyEntangledUpdate(betaA, betaB, pair.correlationStrength, pair.antiCorrelated);
            engineQubits_[pair.engineA].alpha = std::sqrt(std::max(0.0f, 1.0f - (betaA * betaA)));
            engineQubits_[pair.engineB].alpha = std::sqrt(std::max(0.0f, 1.0f - (betaB * betaB)));
            normalizeQubit(engineQubits_[pair.engineA]);
            normalizeQubit(engineQubits_[pair.engineB]);
        }

        coolDown();
    }

    void QuantumBeaconismBackend::collapseToOptimal() {
        std::lock_guard<std::mutex> lock(mutex_);

        float threshold = 0.55f;
        if (overclockGov_) {
            const float thermal = overclockGov_->getSystemTelemetry().totalThermalEnvelopeC;
            threshold = std::clamp(0.50f + (thermal / 400.0f), 0.45f, 0.70f);
        }

        for (size_t i = 0; i < kEngineCount; ++i) {
            engineQubits_[i] = (engineQubits_[i].beta >= threshold) ? Qubit::one() : Qubit::zero();
        }

        if (dualEngines_) {
            for (size_t i = 0; i < kEngineCount; ++i) {
                if (engineQubits_[i].beta > 0.5f) {
                    if (auto* engine = dualEngines_->getEngine(static_cast<EngineId>(i))) {
                        engine->executeFeatureA("");
                    }
                }
            }
        }

        if (overclockGov_) {
            if (engineQubits_[3].beta > 0.5f) {
                overclockGov_->overclock(HardwareDomain::CPU, 100);
                overclockGov_->overclock(HardwareDomain::GPU, 50);
            }
            if (engineQubits_[2].beta > 0.5f) {
                overclockGov_->enableAutoTuneAll(AutoTuneStrategy::Balanced);
            }
        }
    }

    void QuantumBeaconismBackend::entangledCooperate() {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& pair : entanglements_) {
            if (pair.engineA >= kEngineCount || pair.engineB >= kEngineCount) continue;

            float& betaA = engineQubits_[pair.engineA].beta;
            float& betaB = engineQubits_[pair.engineB].beta;
            applyEntangledUpdate(betaA, betaB, pair.correlationStrength, pair.antiCorrelated);
            engineQubits_[pair.engineA].alpha = std::sqrt(std::max(0.0f, 1.0f - (betaA * betaA)));
            engineQubits_[pair.engineB].alpha = std::sqrt(std::max(0.0f, 1.0f - (betaB * betaB)));
            normalizeQubit(engineQubits_[pair.engineA]);
            normalizeQubit(engineQubits_[pair.engineB]);

            if (dualEngines_ && betaA > 0.70f && betaB > 0.70f) {
                if (auto* engA = dualEngines_->getEngine(static_cast<EngineId>(pair.engineA))) engA->executeFeatureA("");
                if (auto* engB = dualEngines_->getEngine(static_cast<EngineId>(pair.engineB))) engB->executeFeatureA("");
            }
        }
    }

    void QuantumBeaconismBackend::broadcastBeacon() {
        std::lock_guard<std::mutex> lock(mutex_);

        latestBeacon_.epoch = epoch_;
        latestBeacon_.globalOptimality = computeFitness();
        latestBeacon_.temperature = saTemperature_;
        latestBeacon_.timestamp = std::chrono::steady_clock::now();

        float entropy = 0.0f;
        uint32_t activeMask = 0;
        for (size_t i = 0; i < kEngineCount; ++i) {
            const float p1 = clamp01(engineQubits_[i].beta * engineQubits_[i].beta);
            const float p0 = 1.0f - p1;
            if (p0 > 0.0001f) entropy -= p0 * std::log2(p0);
            if (p1 > 0.0001f) entropy -= p1 * std::log2(p1);
            if (p1 > 0.5f) activeMask |= (1u << static_cast<uint32_t>(i));
        }

        latestBeacon_.entropy = entropy;
        latestBeacon_.activeEngines = activeMask;
    }

    float QuantumBeaconismBackend::computeFitness() {
        std::array<float, kEngineCount> values{};
        for (size_t i = 0; i < kEngineCount; ++i) {
            values[i] = clamp01(engineQubits_[i].beta * engineQubits_[i].beta);
        }

        float fitness = weightedFitness(values.data(), kEngineWeights, static_cast<uint32_t>(kEngineCount));

        for (const auto& pair : entanglements_) {
            if (pair.engineA >= kEngineCount || pair.engineB >= kEngineCount) continue;
            const float rho = correlationCoefficient(pair.engineA, pair.engineB);
            const float bonus = pair.antiCorrelated ? (1.0f - rho) : rho;
            fitness += bonus * pair.correlationStrength * 0.1f;
        }

        return clamp01(fitness);
    }

    bool QuantumBeaconismBackend::acceptTransition(float currentFitness, float newFitness) {
        if (newFitness >= currentFitness) return true;
        const float tolerance = std::clamp(saTemperature_ / 10000.0f, 0.0f, 0.2f);
        return (currentFitness - newFitness) <= tolerance;
    }

    void QuantumBeaconismBackend::coolDown() {
        saTemperature_ *= saCoolingRate_;
        if (saTemperature_ < saMinTemperature_) saTemperature_ = saMinTemperature_;
    }

    void QuantumBeaconismBackend::propagateEntanglement(uint32_t sourceEngine, float delta) {
        if (sourceEngine >= kEngineCount) return;
        for (const auto& pair : entanglements_) {
            uint32_t target = UINT32_MAX;
            if (pair.engineA == sourceEngine) target = pair.engineB;
            if (pair.engineB == sourceEngine) target = pair.engineA;
            if (target >= kEngineCount) continue;

            const float scaledDelta = delta * pair.correlationStrength * (pair.antiCorrelated ? -0.3f : 0.3f);
            engineQubits_[target].beta = clamp01(engineQubits_[target].beta + scaledDelta);
            engineQubits_[target].alpha = std::sqrt(std::max(0.0f, 1.0f - (engineQubits_[target].beta * engineQubits_[target].beta)));
            normalizeQubit(engineQubits_[target]);
        }
    }

    float QuantumBeaconismBackend::correlationCoefficient(uint32_t engineA, uint32_t engineB) const {
        if (engineA >= kEngineCount || engineB >= kEngineCount) return 0.0f;

        const Qubit& a = engineQubits_[engineA];
        const Qubit& b = engineQubits_[engineB];
        return clamp01(absDot2(a.alpha, a.beta, b.alpha, b.beta));
    }

    } // namespace RawrXD
