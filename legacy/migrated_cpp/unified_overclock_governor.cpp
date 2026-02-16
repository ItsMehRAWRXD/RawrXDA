// ============================================================================
// RawrXD Unified Overclock/Underclock Governor v2 — Implementation
// Zero-dependency hardware control: CPU, GPU, Memory, Storage
// Auto-tune with PID + adaptive strategies
// Bridges: OverclockGovernor, OverclockVendor, ThermalGovernor
// ============================================================================
#include "unified_overclock_governor.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <powerbase.h>
#include <powrprof.h>
#pragma comment(lib, "PowrProf.lib")
#endif

namespace RawrXD {

extern "C" {
    __int64 OverclockGov_Initialize(void* appState);
    __int64 OverclockGov_Shutdown();
    __int64 OverclockGov_IsRunning();
    __int64 OverclockGov_ApplyOffset(uint32_t domain, int32_t offsetMhz);
    __int64 OverclockGov_ApplyCpuOffset(int32_t offsetMhz);
    __int64 OverclockGov_ApplyGpuOffset(int32_t offsetMhz);
    __int64 OverclockGov_ApplyMemoryOffset(int32_t offsetMhz);
    __int64 OverclockGov_ApplyStorageOffset(int32_t offsetMhz);
    __int64 OverclockGov_ReadTemperature(uint32_t domain);
    __int64 OverclockGov_ReadFrequency(uint32_t domain);
    __int64 OverclockGov_ReadPowerDraw(uint32_t domain);
    __int64 OverclockGov_ReadUtilization(uint32_t domain);
    __int64 OverclockGov_EmergencyThrottleAll();
    __int64 OverclockGov_ResetAllToBaseline();
}

static inline float scaledTenthsToFloat(__int64 value) {
    return static_cast<float>(value) / 10.0f;
}

// ============================================================================
// String Tables
// ============================================================================
static const char* s_DomainNames[] = { "CPU", "GPU", "Memory", "Storage" };

const char* HardwareDomainToString(HardwareDomain d) {
    auto idx = static_cast<uint32_t>(d);
    if (idx >= static_cast<uint32_t>(HardwareDomain::COUNT)) return "Unknown";
    return s_DomainNames[idx];
}

// ============================================================================
// Default profiles per domain
// ============================================================================
static ClockProfile DefaultProfile(HardwareDomain domain) {
    ClockProfile p{};
    p.domain = domain;
    p.direction = ClockDirection::Stock;
    p.offsetMhz = 0;
    p.autoTuneEnabled = false;
    p.autoTuneStrategy = AutoTuneStrategy::Disabled;
    p.hysteresisC = 3.0f;

    switch (domain) {
    case HardwareDomain::CPU:
        p.minFreqMhz = 800;
        p.maxFreqMhz = 6500;
        p.targetTempC = 85.0f;
        p.criticalTempC = 100.0f;
        break;
    case HardwareDomain::GPU:
        p.minFreqMhz = 300;
        p.maxFreqMhz = 3000;
        p.targetTempC = 83.0f;
        p.criticalTempC = 95.0f;
        break;
    case HardwareDomain::Memory:
        p.minFreqMhz = 1600;
        p.maxFreqMhz = 8000;
        p.targetTempC = 70.0f;
        p.criticalTempC = 85.0f;
        break;
    case HardwareDomain::Storage:
        p.minFreqMhz = 0;    // N/A - uses queue depth/power state
        p.maxFreqMhz = 0;
        p.targetTempC = 65.0f;
        p.criticalTempC = 75.0f;
        break;
    default:
        break;
    }
    return p;
}

// ============================================================================
// Singleton
// ============================================================================
UnifiedOverclockGovernor& UnifiedOverclockGovernor::Instance() {
    static UnifiedOverclockGovernor inst;
    return inst;
}

UnifiedOverclockGovernor::UnifiedOverclockGovernor() {
    for (uint32_t i = 0; i < static_cast<uint32_t>(HardwareDomain::COUNT); ++i) {
        auto domain = static_cast<HardwareDomain>(i);
        profiles_[i] = DefaultProfile(domain);
        std::memset(&pidStates_[i], 0, sizeof(PIDState));
        pidStates_[i].kp = 0.5f;
        pidStates_[i].ki = 0.05f;
        pidStates_[i].kd = 0.1f;
        std::memset(&telemetry_[i], 0, sizeof(DomainTelemetry));
        telemetry_[i].domain = domain;
        baselines_[i] = 0.0f;
        historyHead_[i] = 0;
    }
}

UnifiedOverclockGovernor::~UnifiedOverclockGovernor() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
ClockResult UnifiedOverclockGovernor::initialize(AppState* state) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_.load()) return ClockResult::ok("Already running");

    if (!OverclockGov_Initialize(state)) {
        return ClockResult::error("MASM governor initialization failed");
    }

    appState_ = state;

    // Read baseline frequencies
    for (uint32_t i = 0; i < static_cast<uint32_t>(HardwareDomain::COUNT); ++i) {
        auto domain = static_cast<HardwareDomain>(i);
        baselines_[i] = readFrequency(domain);
        telemetry_[i].baselineFreqMhz = baselines_[i];
    }

    running_.store(true);
    emergency_.store(false);

    // Launch control loop thread
    controlThread_ = std::thread([this]() { controlLoop(); });

    return ClockResult::ok("Unified Overclock Governor v2 initialized");
}

ClockResult UnifiedOverclockGovernor::shutdown() {
    if (!running_.load()) return ClockResult::ok("Not running");

    running_.store(false);
    if (controlThread_.joinable()) controlThread_.join();

    // Reset all to baseline
    for (uint32_t i = 0; i < static_cast<uint32_t>(HardwareDomain::COUNT); ++i) {
        profiles_[i].offsetMhz = 0;
        profiles_[i].direction = ClockDirection::Stock;
    }

    OverclockGov_Shutdown();

    return ClockResult::ok("Governor shutdown");
}

bool UnifiedOverclockGovernor::isRunning() const {
    return running_.load() && (OverclockGov_IsRunning() != 0);
}

// ============================================================================
// Per-domain control
// ============================================================================
ClockResult UnifiedOverclockGovernor::setProfile(HardwareDomain domain, const ClockProfile& profile) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto idx = static_cast<uint32_t>(domain);
    if (idx >= static_cast<uint32_t>(HardwareDomain::COUNT))
        return ClockResult::error("Invalid domain");
    profiles_[idx] = profile;
    return ClockResult::ok("Profile set");
}

ClockResult UnifiedOverclockGovernor::applyOffset(HardwareDomain domain, int32_t offsetMhz) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto idx = static_cast<uint32_t>(domain);
    if (idx >= static_cast<uint32_t>(HardwareDomain::COUNT))
        return ClockResult::error("Invalid domain");

    auto& p = profiles_[idx];
    p.offsetMhz = offsetMhz;
    p.direction = offsetMhz > 0 ? ClockDirection::Overclock :
                  offsetMhz < 0 ? ClockDirection::Underclock :
                                  ClockDirection::Stock;

    // Apply immediately
    ClockResult r = ClockResult::error("Apply failed");
    switch (domain) {
    case HardwareDomain::CPU:     r = applyCpuOffset(offsetMhz); break;
    case HardwareDomain::GPU:     r = applyGpuOffset(offsetMhz); break;
    case HardwareDomain::Memory:  r = applyMemoryOffset(offsetMhz); break;
    case HardwareDomain::Storage: r = applyStorageOffset(offsetMhz); break;
    default: break;
    }

    if (r.success) {
        telemetry_[idx].appliedOffsetMhz = offsetMhz;
        telemetry_[idx].adjustmentCount++;
    }
    return r;
}

ClockResult UnifiedOverclockGovernor::resetToBaseline(HardwareDomain domain) {
    return applyOffset(domain, 0);
}

ClockResult UnifiedOverclockGovernor::resetAllToBaseline() {
    for (uint32_t i = 0; i < static_cast<uint32_t>(HardwareDomain::COUNT); ++i) {
        auto r = resetToBaseline(static_cast<HardwareDomain>(i));
        if (!r.success) return r;
    }
    return ClockResult::ok("All domains reset to baseline");
}

// ============================================================================
// Auto-tune toggle
// ============================================================================
ClockResult UnifiedOverclockGovernor::enableAutoTune(HardwareDomain domain, AutoTuneStrategy strategy) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto idx = static_cast<uint32_t>(domain);
    if (idx >= static_cast<uint32_t>(HardwareDomain::COUNT))
        return ClockResult::error("Invalid domain");
    profiles_[idx].autoTuneEnabled = true;
    profiles_[idx].autoTuneStrategy = strategy;
    telemetry_[idx].autoTuneActive = true;
    telemetry_[idx].activeStrategy = strategy;
    return ClockResult::ok("Auto-tune enabled");
}

ClockResult UnifiedOverclockGovernor::disableAutoTune(HardwareDomain domain) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto idx = static_cast<uint32_t>(domain);
    if (idx >= static_cast<uint32_t>(HardwareDomain::COUNT))
        return ClockResult::error("Invalid domain");
    profiles_[idx].autoTuneEnabled = false;
    profiles_[idx].autoTuneStrategy = AutoTuneStrategy::Disabled;
    telemetry_[idx].autoTuneActive = false;
    return ClockResult::ok("Auto-tune disabled");
}

bool UnifiedOverclockGovernor::isAutoTuneEnabled(HardwareDomain domain) const {
    auto idx = static_cast<uint32_t>(domain);
    if (idx >= static_cast<uint32_t>(HardwareDomain::COUNT)) return false;
    return profiles_[idx].autoTuneEnabled;
}

AutoTuneStrategy UnifiedOverclockGovernor::getAutoTuneStrategy(HardwareDomain domain) const {
    auto idx = static_cast<uint32_t>(domain);
    if (idx >= static_cast<uint32_t>(HardwareDomain::COUNT)) return AutoTuneStrategy::Disabled;
    return profiles_[idx].autoTuneStrategy;
}

ClockResult UnifiedOverclockGovernor::enableAutoTuneAll(AutoTuneStrategy strategy) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(HardwareDomain::COUNT); ++i) {
        auto r = enableAutoTune(static_cast<HardwareDomain>(i), strategy);
        if (!r.success) return r;
    }
    return ClockResult::ok("Auto-tune enabled on all domains");
}

ClockResult UnifiedOverclockGovernor::disableAutoTuneAll() {
    for (uint32_t i = 0; i < static_cast<uint32_t>(HardwareDomain::COUNT); ++i) {
        auto r = disableAutoTune(static_cast<HardwareDomain>(i));
        if (!r.success) return r;
    }
    return ClockResult::ok("Auto-tune disabled on all domains");
}

// ============================================================================
// Direction shortcuts
// ============================================================================
ClockResult UnifiedOverclockGovernor::overclock(HardwareDomain domain, int32_t stepMhz) {
    int32_t newOffset = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto idx = static_cast<uint32_t>(domain);
        if (idx >= static_cast<uint32_t>(HardwareDomain::COUNT)) {
            return ClockResult::error("Invalid domain");
        }

        newOffset = profiles_[idx].offsetMhz + std::abs(stepMhz);
        newOffset = std::min(newOffset,
            profiles_[idx].maxFreqMhz - static_cast<int32_t>(baselines_[idx]));
    }

    return applyOffset(domain, newOffset);
}

ClockResult UnifiedOverclockGovernor::underclock(HardwareDomain domain, int32_t stepMhz) {
    int32_t newOffset = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto idx = static_cast<uint32_t>(domain);
        if (idx >= static_cast<uint32_t>(HardwareDomain::COUNT)) {
            return ClockResult::error("Invalid domain");
        }

        newOffset = profiles_[idx].offsetMhz - std::abs(stepMhz);
        newOffset = std::max(newOffset,
            profiles_[idx].minFreqMhz - static_cast<int32_t>(baselines_[idx]));
    }

    return applyOffset(domain, newOffset);
}

// ============================================================================
// Telemetry
// ============================================================================
DomainTelemetry UnifiedOverclockGovernor::getDomainTelemetry(HardwareDomain domain) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto idx = static_cast<uint32_t>(domain);
    if (idx >= static_cast<uint32_t>(HardwareDomain::COUNT)) {
        DomainTelemetry empty{};
        return empty;
    }
    return telemetry_[idx];
}

SystemClockTelemetry UnifiedOverclockGovernor::getSystemTelemetry() const {
    std::lock_guard<std::mutex> lock(mutex_);
    SystemClockTelemetry sys{};
    sys.totalPowerDrawWatts = 0.0f;
    sys.totalThermalEnvelopeC = 0.0f;
    sys.anyFaulted = false;
    sys.anyAutoTuning = false;
    sys.totalAdjustments = 0;

    for (uint32_t i = 0; i < static_cast<uint32_t>(HardwareDomain::COUNT); ++i) {
        sys.domains[i] = telemetry_[i];
        sys.totalPowerDrawWatts += telemetry_[i].powerDrawWatts;
        sys.totalThermalEnvelopeC = std::max(sys.totalThermalEnvelopeC, telemetry_[i].currentTempC);
        if (telemetry_[i].faultCount > 0) sys.anyFaulted = true;
        if (telemetry_[i].autoTuneActive) sys.anyAutoTuning = true;
        sys.totalAdjustments += telemetry_[i].adjustmentCount;
    }
    sys.timestamp = std::chrono::steady_clock::now();
    return sys;
}

// ============================================================================
// Safety
// ============================================================================
ClockResult UnifiedOverclockGovernor::emergencyThrottleAll() {
    emergency_.store(true);
    OverclockGov_EmergencyThrottleAll();
    return ClockResult::ok("EMERGENCY: All domains throttled to minimum");
}

bool UnifiedOverclockGovernor::isEmergencyActive() const {
    return emergency_.load();
}

ClockResult UnifiedOverclockGovernor::clearEmergency() {
    emergency_.store(false);
    return resetAllToBaseline();
}

// ============================================================================
// Session persistence (manual JSON serialization)
// ============================================================================
ClockResult UnifiedOverclockGovernor::saveSession(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream f(path);
    if (!f.is_open()) return ClockResult::error("Cannot open file for writing");

    f << "{\n";
    f << "  \"version\": 2,\n";
    f << "  \"profiles\": [\n";
    for (uint32_t i = 0; i < static_cast<uint32_t>(HardwareDomain::COUNT); ++i) {
        const auto& p = profiles_[i];
        f << "    {\n";
        f << "      \"domain\": " << i << ",\n";
        f << "      \"direction\": " << static_cast<int>(p.direction) << ",\n";
        f << "      \"offsetMhz\": " << p.offsetMhz << ",\n";
        f << "      \"minFreqMhz\": " << p.minFreqMhz << ",\n";
        f << "      \"maxFreqMhz\": " << p.maxFreqMhz << ",\n";
        f << "      \"targetTempC\": " << p.targetTempC << ",\n";
        f << "      \"criticalTempC\": " << p.criticalTempC << ",\n";
        f << "      \"hysteresisC\": " << p.hysteresisC << ",\n";
        f << "      \"autoTuneEnabled\": " << (p.autoTuneEnabled ? "true" : "false") << ",\n";
        f << "      \"autoTuneStrategy\": " << static_cast<int>(p.autoTuneStrategy) << "\n";
        f << "    }" << (i < 3 ? "," : "") << "\n";
    }
    f << "  ],\n";

    // Save PID states
    f << "  \"pidStates\": [\n";
    for (uint32_t i = 0; i < static_cast<uint32_t>(HardwareDomain::COUNT); ++i) {
        const auto& pid = pidStates_[i];
        f << "    {\n";
        f << "      \"kp\": " << pid.kp << ",\n";
        f << "      \"ki\": " << pid.ki << ",\n";
        f << "      \"kd\": " << pid.kd << ",\n";
        f << "      \"integral\": " << pid.integral << ",\n";
        f << "      \"totalFaults\": " << pid.totalFaults << "\n";
        f << "    }" << (i < 3 ? "," : "") << "\n";
    }
    f << "  ]\n";
    f << "}\n";

    return ClockResult::ok("Session saved");
}

ClockResult UnifiedOverclockGovernor::loadSession(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream f(path);
    if (!f.is_open()) return ClockResult::error("Cannot open session file");

    // Simple JSON value extraction (manual parser — no deps)
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // Parse each profile block by searching for "domain":N patterns
    auto extractInt = [&](const std::string& key, size_t startPos) -> int {
        std::string pat = "\"" + key + "\": ";
        auto pos = content.find(pat, startPos);
        if (pos == std::string::npos) return 0;
        pos += pat.size();
        return std::atoi(content.c_str() + pos);
    };

    auto extractFloat = [&](const std::string& key, size_t startPos) -> float {
        std::string pat = "\"" + key + "\": ";
        auto pos = content.find(pat, startPos);
        if (pos == std::string::npos) return 0.0f;
        pos += pat.size();
        return static_cast<float>(std::atof(content.c_str() + pos));
    };

    auto extractBool = [&](const std::string& key, size_t startPos) -> bool {
        std::string pat = "\"" + key + "\": ";
        auto pos = content.find(pat, startPos);
        if (pos == std::string::npos) return false;
        pos += pat.size();
        return content.substr(pos, 4) == "true";
    };

    // Find profiles array
    size_t profilesPos = content.find("\"profiles\"");
    if (profilesPos != std::string::npos) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(HardwareDomain::COUNT); ++i) {
            std::string domainPat = "\"domain\": " + std::to_string(i);
            auto blockPos = content.find(domainPat, profilesPos);
            if (blockPos == std::string::npos) continue;

            auto& p = profiles_[i];
            p.direction = static_cast<ClockDirection>(extractInt("direction", blockPos));
            p.offsetMhz = extractInt("offsetMhz", blockPos);
            p.minFreqMhz = extractInt("minFreqMhz", blockPos);
            p.maxFreqMhz = extractInt("maxFreqMhz", blockPos);
            p.targetTempC = extractFloat("targetTempC", blockPos);
            p.criticalTempC = extractFloat("criticalTempC", blockPos);
            p.hysteresisC = extractFloat("hysteresisC", blockPos);
            p.autoTuneEnabled = extractBool("autoTuneEnabled", blockPos);
            p.autoTuneStrategy = static_cast<AutoTuneStrategy>(extractInt("autoTuneStrategy", blockPos));
        }
    }

    return ClockResult::ok("Session loaded");
}

// ============================================================================
// CLI dispatch
// ============================================================================
ClockResult UnifiedOverclockGovernor::dispatchCLI(const std::string& command, const std::string& args) {
    if (command == "oc" || command == "overclock") {
        // Format: "domain offsetMhz"  e.g. "cpu 200"
        std::istringstream iss(args);
        std::string domainStr;
        int32_t offset = 100;
        iss >> domainStr >> offset;

        HardwareDomain domain = HardwareDomain::CPU;
        if (domainStr == "gpu") domain = HardwareDomain::GPU;
        else if (domainStr == "mem" || domainStr == "memory") domain = HardwareDomain::Memory;
        else if (domainStr == "storage" || domainStr == "disk") domain = HardwareDomain::Storage;

        return applyOffset(domain, offset);
    }
    if (command == "uc" || command == "underclock") {
        std::istringstream iss(args);
        std::string domainStr;
        int32_t offset = -100;
        iss >> domainStr >> offset;
        if (offset > 0) offset = -offset;

        HardwareDomain domain = HardwareDomain::CPU;
        if (domainStr == "gpu") domain = HardwareDomain::GPU;
        else if (domainStr == "mem" || domainStr == "memory") domain = HardwareDomain::Memory;
        else if (domainStr == "storage" || domainStr == "disk") domain = HardwareDomain::Storage;

        return applyOffset(domain, offset);
    }
    if (command == "autotune") {
        std::istringstream iss(args);
        std::string action, domainStr, stratStr;
        iss >> action >> domainStr >> stratStr;

        HardwareDomain domain = HardwareDomain::CPU;
        if (domainStr == "gpu") domain = HardwareDomain::GPU;
        else if (domainStr == "mem" || domainStr == "memory") domain = HardwareDomain::Memory;
        else if (domainStr == "storage" || domainStr == "disk") domain = HardwareDomain::Storage;

        if (action == "on" || action == "enable") {
            AutoTuneStrategy strat = AutoTuneStrategy::Balanced;
            if (stratStr == "conservative") strat = AutoTuneStrategy::Conservative;
            else if (stratStr == "aggressive") strat = AutoTuneStrategy::Aggressive;
            else if (stratStr == "adaptive" || stratStr == "ml") strat = AutoTuneStrategy::AdaptiveML;
            return enableAutoTune(domain, strat);
        }
        if (action == "off" || action == "disable") {
            return disableAutoTune(domain);
        }
        if (action == "all-on") return enableAutoTuneAll(AutoTuneStrategy::Balanced);
        if (action == "all-off") return disableAutoTuneAll();
    }
    if (command == "emergency") return emergencyThrottleAll();
    if (command == "reset") return resetAllToBaseline();
    if (command == "save") return saveSession(args.empty() ? "overclock_session.json" : args);
    if (command == "load") return loadSession(args.empty() ? "overclock_session.json" : args);

    return ClockResult::error("Unknown command");
}

// ============================================================================
// Control Loop
// ============================================================================
void UnifiedOverclockGovernor::controlLoop() {
    while (running_.load()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            for (uint32_t i = 0; i < static_cast<uint32_t>(HardwareDomain::COUNT); ++i) {
                auto domain = static_cast<HardwareDomain>(i);

                // Read sensors
                telemetry_[i].currentTempC = readTemperature(domain);
                telemetry_[i].currentFreqMhz = readFrequency(domain);
                telemetry_[i].powerDrawWatts = readPowerDraw(domain);
                telemetry_[i].utilizationPct = readUtilization(domain);

                // Safety check — emergency throttle if critical
                if (!checkThermalSafety(domain)) {
                    recordFault(domain);
                    if (shouldRollback(domain)) {
                        profiles_[i].offsetMhz = 0;
                        profiles_[i].direction = ClockDirection::Stock;
                    }
                    continue;
                }

                // PID update
                pidUpdate(domain);

                // Auto-tune step if enabled
                if (profiles_[i].autoTuneEnabled) {
                    autoTuneStep(domain);
                }

                // Compute efficiency score
                if (telemetry_[i].powerDrawWatts > 0.0f) {
                    telemetry_[i].efficiencyScore =
                        telemetry_[i].utilizationPct / (telemetry_[i].powerDrawWatts + 0.001f);
                }

                // Snapshot PID state into telemetry
                telemetry_[i].pidSnapshot = pidStates_[i];
            }
        }

        // Control loop interval: 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// ============================================================================
// PID Update
// ============================================================================
void UnifiedOverclockGovernor::pidUpdate(HardwareDomain domain) {
    auto idx = static_cast<uint32_t>(domain);
    auto& pid = pidStates_[idx];
    auto& profile = profiles_[idx];

    float error = profile.targetTempC - telemetry_[idx].currentTempC;

    // Deadband: don't adjust within hysteresis
    if (std::abs(error) < profile.hysteresisC) return;

    pid.integral += error;
    // Anti-windup: clamp integral
    pid.integral = std::clamp(pid.integral, -100.0f, 100.0f);

    float derivative = error - pid.prevError;
    pid.output = pid.kp * error + pid.ki * pid.integral + pid.kd * derivative;
    pid.prevError = error;

    // Convert PID output to frequency offset adjustment
    int32_t adjustment = static_cast<int32_t>(pid.output);
    int32_t newOffset = profile.offsetMhz + adjustment;

    // Clamp to profile limits
    int32_t maxAllowedOffset = profile.maxFreqMhz - static_cast<int32_t>(baselines_[idx]);
    int32_t minAllowedOffset = profile.minFreqMhz - static_cast<int32_t>(baselines_[idx]);
    newOffset = std::clamp(newOffset, minAllowedOffset, maxAllowedOffset);

    if (newOffset != profile.offsetMhz) {
        profile.offsetMhz = newOffset;
        profile.direction = newOffset > 0 ? ClockDirection::Overclock :
                           newOffset < 0 ? ClockDirection::Underclock :
                                           ClockDirection::Stock;

        switch (domain) {
        case HardwareDomain::CPU:     applyCpuOffset(newOffset); break;
        case HardwareDomain::GPU:     applyGpuOffset(newOffset); break;
        case HardwareDomain::Memory:  applyMemoryOffset(newOffset); break;
        case HardwareDomain::Storage: applyStorageOffset(newOffset); break;
        default: break;
        }
        telemetry_[idx].appliedOffsetMhz = newOffset;
        telemetry_[idx].adjustmentCount++;
    }
}

// ============================================================================
// Auto-Tune Step
// ============================================================================
void UnifiedOverclockGovernor::autoTuneStep(HardwareDomain domain) {
    auto idx = static_cast<uint32_t>(domain);
    auto& profile = profiles_[idx];
    auto& pid = pidStates_[idx];

    // Record history
    auto& head = historyHead_[idx];
    auto& entry = autoTuneHistory_[idx][head % AUTO_TUNE_HISTORY_SIZE];
    entry.tempC = telemetry_[idx].currentTempC;
    entry.freqMhz = telemetry_[idx].currentFreqMhz;
    entry.powerW = telemetry_[idx].powerDrawWatts;
    entry.utilPct = telemetry_[idx].utilizationPct;
    entry.offsetApplied = profile.offsetMhz;
    entry.resultingEfficiency = telemetry_[idx].efficiencyScore;
    head++;

    // Strategy-dependent step size and aggressiveness
    int32_t stepSize = 0;
    float deadband = profile.hysteresisC;

    switch (profile.autoTuneStrategy) {
    case AutoTuneStrategy::Conservative:
        stepSize = 10;   // 10 MHz steps
        deadband = 5.0f; // wide deadband
        break;
    case AutoTuneStrategy::Balanced:
        stepSize = 25;   // 25 MHz steps
        deadband = 3.0f;
        break;
    case AutoTuneStrategy::Aggressive:
        stepSize = 50;   // 50 MHz steps
        deadband = 1.0f; // tight deadband
        break;
    case AutoTuneStrategy::AdaptiveML: {
        // Machine-learned PID gains from history
        if (head >= 16) {
            // Compute average efficiency gradient over last 16 samples
            float effGrad = 0.0f;
            for (size_t i = 1; i < 16; ++i) {
                size_t cur = (head - i) % AUTO_TUNE_HISTORY_SIZE;
                size_t prev = (head - i - 1) % AUTO_TUNE_HISTORY_SIZE;
                effGrad += autoTuneHistory_[idx][cur].resultingEfficiency -
                           autoTuneHistory_[idx][prev].resultingEfficiency;
            }
            effGrad /= 15.0f;

            // Adapt PID gains based on efficiency trend
            if (effGrad > 0.01f) {
                // Efficiency improving: increase aggressiveness
                pid.kp = std::min(pid.kp * 1.05f, 2.0f);
                stepSize = 35;
            } else if (effGrad < -0.01f) {
                // Efficiency declining: reduce aggressiveness
                pid.kp = std::max(pid.kp * 0.95f, 0.1f);
                stepSize = 10;
            } else {
                stepSize = 20;
            }
        } else {
            stepSize = 25; // Not enough data, use balanced
        }
        deadband = 2.0f;
        break;
    }
    default:
        return; // Auto-tune disabled
    }

    // Apply auto-tune adjustment based on thermal headroom
    float headroom = profile.targetTempC - telemetry_[idx].currentTempC;
    if (headroom > deadband && telemetry_[idx].utilizationPct > 50.0f) {
        // We have thermal headroom and load is significant: try to OC
        profile.offsetMhz += stepSize;
    } else if (headroom < -deadband) {
        // Over target temp: underclock
        profile.offsetMhz -= stepSize;
    }

    // Clamp
    int32_t maxOffset = profile.maxFreqMhz - static_cast<int32_t>(baselines_[idx]);
    int32_t minOffset = profile.minFreqMhz - static_cast<int32_t>(baselines_[idx]);
    profile.offsetMhz = std::clamp(profile.offsetMhz, minOffset, maxOffset);
}

// ============================================================================
// Platform-Specific Apply Functions
// ============================================================================
ClockResult UnifiedOverclockGovernor::applyCpuOffset(int32_t offsetMhz) {
    return OverclockGov_ApplyCpuOffset(offsetMhz)
        ? ClockResult::ok("CPU offset applied")
        : ClockResult::error("CPU offset apply failed");
}

ClockResult UnifiedOverclockGovernor::applyGpuOffset(int32_t offsetMhz) {
    return OverclockGov_ApplyGpuOffset(offsetMhz)
        ? ClockResult::ok("GPU offset applied")
        : ClockResult::error("GPU offset apply failed");
}

ClockResult UnifiedOverclockGovernor::applyMemoryOffset(int32_t offsetMhz) {
    return OverclockGov_ApplyMemoryOffset(offsetMhz)
        ? ClockResult::ok("Memory offset applied")
        : ClockResult::error("Memory offset apply failed");
}

ClockResult UnifiedOverclockGovernor::applyStorageOffset(int32_t offsetMhz) {
    return OverclockGov_ApplyStorageOffset(offsetMhz)
        ? ClockResult::ok("Storage offset applied")
        : ClockResult::error("Storage offset apply failed");
}

// ============================================================================
// Platform-Specific Read Functions
// ============================================================================
float UnifiedOverclockGovernor::readTemperature(HardwareDomain domain) {
    return scaledTenthsToFloat(OverclockGov_ReadTemperature(static_cast<uint32_t>(domain)));
}

float UnifiedOverclockGovernor::readFrequency(HardwareDomain domain) {
    return static_cast<float>(OverclockGov_ReadFrequency(static_cast<uint32_t>(domain)));
}

float UnifiedOverclockGovernor::readPowerDraw(HardwareDomain domain) {
    return scaledTenthsToFloat(OverclockGov_ReadPowerDraw(static_cast<uint32_t>(domain)));
}

float UnifiedOverclockGovernor::readUtilization(HardwareDomain domain) {
    return scaledTenthsToFloat(OverclockGov_ReadUtilization(static_cast<uint32_t>(domain)));
}

// ============================================================================
// Safety Checks
// ============================================================================
bool UnifiedOverclockGovernor::checkThermalSafety(HardwareDomain domain) {
    auto idx = static_cast<uint32_t>(domain);
    float temp = telemetry_[idx].currentTempC;
    float critical = profiles_[idx].criticalTempC;

    if (temp >= critical) {
        // CRITICAL: emergency throttle this domain
        profiles_[idx].offsetMhz = profiles_[idx].minFreqMhz - static_cast<int32_t>(baselines_[idx]);
        profiles_[idx].direction = ClockDirection::Underclock;
        return false;
    }
    return true;
}

void UnifiedOverclockGovernor::recordFault(HardwareDomain domain) {
    auto idx = static_cast<uint32_t>(domain);
    pidStates_[idx].consecutiveFaults++;
    pidStates_[idx].totalFaults++;
    pidStates_[idx].lastFaultTime = std::chrono::steady_clock::now();
    telemetry_[idx].faultCount++;
}

bool UnifiedOverclockGovernor::shouldRollback(HardwareDomain domain) const {
    auto idx = static_cast<uint32_t>(domain);
    // Rollback if 3+ consecutive faults
    return pidStates_[idx].consecutiveFaults >= 3;
}

} // namespace RawrXD
