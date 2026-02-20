// ============================================================================
// transcendence_coordinator.cpp — Master Coordinator (Phases E → Ω)
// ============================================================================
//
// Unifies all Transcendence Architecture subsystems into a coherent
// autonomous development entity. Manages lifecycle, health monitoring,
// cross-phase event routing, and emergency stop.
//
// Pattern: PatchResult error model | No exceptions | Mutex-guarded
// ============================================================================

#include "transcendence_coordinator.hpp"
#include <cstring>

#ifdef _WIN32
extern "C" unsigned __int64 __rdtsc();
#pragma intrinsic(__rdtsc)
#define RDTSC() __rdtsc()
#else
#include <x86intrin.h>
#define RDTSC() __rdtsc()
#endif

namespace rawrxd {

// ============================================================================
//  Singleton
// ============================================================================
TranscendenceCoordinator& TranscendenceCoordinator::instance() {
    static TranscendenceCoordinator s_inst;
    return s_inst;
}

// ============================================================================
//  Helper — phase index (0-5)
// ============================================================================
static int phaseIndex(TranscendencePhase p) {
    int idx = static_cast<int>(p) - 1;
    if (idx < 0 || idx > 5) return -1;
    return idx;
}

// ============================================================================
//  Initialize All Phases (E → Ω)
// ============================================================================
PatchResult TranscendenceCoordinator::initializeAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_emergencyStopped)
        return PatchResult::error("Emergency stop active — reset required", -99);

    m_initTimestamp = RDTSC();
    std::memset(&m_stats, 0, sizeof(m_stats));

    // Initialize in order: E → F → G → H → I → Ω
    struct PhaseInit {
        TranscendencePhase phase;
        PatchResult (TranscendenceCoordinator::*initFunc)(TranscendencePhase);
    };

    // Phase E: SelfHost
    {
        int idx = phaseIndex(TranscendencePhase::SelfHost);
        PatchResult r = SelfHostEngine::instance().initialize();
        m_phases[idx].phase = TranscendencePhase::SelfHost;
        m_phases[idx].active = r.success;
        m_phases[idx].initTimestamp = RDTSC();
        m_phases[idx].lastHeartbeat = m_phases[idx].initTimestamp;
        m_phases[idx].errorCount = r.success ? 0 : 1;
        m_phases[idx].healthScore = r.success ? 1.0f : 0.0f;
        m_stats.phaseInits++;
    }

    // Phase F: HWSynth
    {
        int idx = phaseIndex(TranscendencePhase::HWSynth);
        PatchResult r = HardwareSynthesizer::instance().initialize();
        m_phases[idx].phase = TranscendencePhase::HWSynth;
        m_phases[idx].active = r.success;
        m_phases[idx].initTimestamp = RDTSC();
        m_phases[idx].lastHeartbeat = m_phases[idx].initTimestamp;
        m_phases[idx].errorCount = r.success ? 0 : 1;
        m_phases[idx].healthScore = r.success ? 1.0f : 0.0f;
        m_stats.phaseInits++;
    }

    // Phase G: MeshBrain
    {
        int idx = phaseIndex(TranscendencePhase::MeshBrain);
        PatchResult r = MeshBrain::instance().initialize();
        m_phases[idx].phase = TranscendencePhase::MeshBrain;
        m_phases[idx].active = r.success;
        m_phases[idx].initTimestamp = RDTSC();
        m_phases[idx].lastHeartbeat = m_phases[idx].initTimestamp;
        m_phases[idx].errorCount = r.success ? 0 : 1;
        m_phases[idx].healthScore = r.success ? 1.0f : 0.0f;
        m_stats.phaseInits++;
    }

    // Phase H: Speciator
    {
        int idx = phaseIndex(TranscendencePhase::Speciator);
        PatchResult r = SpeciatorEngine::instance().initialize();
        m_phases[idx].phase = TranscendencePhase::Speciator;
        m_phases[idx].active = r.success;
        m_phases[idx].initTimestamp = RDTSC();
        m_phases[idx].lastHeartbeat = m_phases[idx].initTimestamp;
        m_phases[idx].errorCount = r.success ? 0 : 1;
        m_phases[idx].healthScore = r.success ? 1.0f : 0.0f;
        m_stats.phaseInits++;
    }

    // Phase I: NeuralBridge
    {
        int idx = phaseIndex(TranscendencePhase::Neural);
        PatchResult r = NeuralBridge::instance().initialize();
        m_phases[idx].phase = TranscendencePhase::Neural;
        m_phases[idx].active = r.success;
        m_phases[idx].initTimestamp = RDTSC();
        m_phases[idx].lastHeartbeat = m_phases[idx].initTimestamp;
        m_phases[idx].errorCount = r.success ? 0 : 1;
        m_phases[idx].healthScore = r.success ? 1.0f : 0.0f;
        m_stats.phaseInits++;
    }

    // Phase Ω: OmegaOrchestrator
    {
        int idx = phaseIndex(TranscendencePhase::Omega);
        PatchResult r = OmegaOrchestrator::instance().initialize();
        m_phases[idx].phase = TranscendencePhase::Omega;
        m_phases[idx].active = r.success;
        m_phases[idx].initTimestamp = RDTSC();
        m_phases[idx].lastHeartbeat = m_phases[idx].initTimestamp;
        m_phases[idx].errorCount = r.success ? 0 : 1;
        m_phases[idx].healthScore = r.success ? 1.0f : 0.0f;
        m_stats.phaseInits++;
    }

    // Check if any phase is active
    m_anyActive = false;
    for (int i = 0; i < 6; i++) {
        if (m_phases[i].active) {
            m_anyActive = true;
            break;
        }
    }

    if (!m_anyActive)
        return PatchResult::error("No phases initialized successfully", -1);

    // Count active
    int activeCount = 0;
    for (int i = 0; i < 6; i++) {
        if (m_phases[i].active) activeCount++;
    }

    if (activeCount == 6)
        return PatchResult::ok("All 6 Transcendence phases active — Omega Point reached");

    return PatchResult::ok("Transcendence partially initialized");
}

// ============================================================================
//  Initialize Single Phase
// ============================================================================
PatchResult TranscendenceCoordinator::initializePhase(TranscendencePhase phase) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_emergencyStopped)
        return PatchResult::error("Emergency stop active", -99);

    int idx = phaseIndex(phase);
    if (idx < 0) return PatchResult::error("Invalid phase", -1);

    PatchResult r = PatchResult::error("Unknown phase", -2);

    switch (phase) {
        case TranscendencePhase::SelfHost:  r = SelfHostEngine::instance().initialize();       break;
        case TranscendencePhase::HWSynth:   r = HardwareSynthesizer::instance().initialize();  break;
        case TranscendencePhase::MeshBrain: r = MeshBrain::instance().initialize();             break;
        case TranscendencePhase::Speciator: r = SpeciatorEngine::instance().initialize();       break;
        case TranscendencePhase::Neural:    r = NeuralBridge::instance().initialize();           break;
        case TranscendencePhase::Omega:     r = OmegaOrchestrator::instance().initialize();     break;
        default: break;
    }

    m_phases[idx].phase = phase;
    m_phases[idx].active = r.success;
    m_phases[idx].initTimestamp = RDTSC();
    m_phases[idx].lastHeartbeat = m_phases[idx].initTimestamp;
    m_phases[idx].errorCount = r.success ? 0 : 1;
    m_phases[idx].healthScore = r.success ? 1.0f : 0.0f;
    m_stats.phaseInits++;

    if (r.success) m_anyActive = true;
    return r;
}

// ============================================================================
//  Shutdown All (Reverse Order: Ω → E)
// ============================================================================
PatchResult TranscendenceCoordinator::shutdownAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Shutdown in reverse order
    OmegaOrchestrator::instance().shutdown();
    m_phases[phaseIndex(TranscendencePhase::Omega)].active = false;
    m_stats.phaseShutdowns++;

    NeuralBridge::instance().shutdown();
    m_phases[phaseIndex(TranscendencePhase::Neural)].active = false;
    m_stats.phaseShutdowns++;

    SpeciatorEngine::instance().shutdown();
    m_phases[phaseIndex(TranscendencePhase::Speciator)].active = false;
    m_stats.phaseShutdowns++;

    MeshBrain::instance().shutdown();
    m_phases[phaseIndex(TranscendencePhase::MeshBrain)].active = false;
    m_stats.phaseShutdowns++;

    HardwareSynthesizer::instance().shutdown();
    m_phases[phaseIndex(TranscendencePhase::HWSynth)].active = false;
    m_stats.phaseShutdowns++;

    SelfHostEngine::instance().shutdown();
    m_phases[phaseIndex(TranscendencePhase::SelfHost)].active = false;
    m_stats.phaseShutdowns++;

    m_anyActive = false;
    m_stats.uptime = RDTSC() - m_initTimestamp;

    return PatchResult::ok("All Transcendence phases shut down");
}

// ============================================================================
//  Shutdown Single Phase
// ============================================================================
PatchResult TranscendenceCoordinator::shutdownPhase(TranscendencePhase phase) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = phaseIndex(phase);
    if (idx < 0) return PatchResult::error("Invalid phase", -1);

    switch (phase) {
        case TranscendencePhase::SelfHost:  SelfHostEngine::instance().shutdown();       break;
        case TranscendencePhase::HWSynth:   HardwareSynthesizer::instance().shutdown();  break;
        case TranscendencePhase::MeshBrain: MeshBrain::instance().shutdown();             break;
        case TranscendencePhase::Speciator: SpeciatorEngine::instance().shutdown();       break;
        case TranscendencePhase::Neural:    NeuralBridge::instance().shutdown();           break;
        case TranscendencePhase::Omega:     OmegaOrchestrator::instance().shutdown();     break;
        default: break;
    }

    m_phases[idx].active = false;
    m_stats.phaseShutdowns++;

    // Recheck if any still active
    m_anyActive = false;
    for (int i = 0; i < 6; i++) {
        if (m_phases[i].active) { m_anyActive = true; break; }
    }

    return PatchResult::ok("Phase shutdown complete");
}

// ============================================================================
//  Emergency Stop
// ============================================================================
PatchResult TranscendenceCoordinator::emergencyStop() {
    // No lock — must be immediate
    m_emergencyStopped = true;
    m_stats.emergencyStops++;

    // Force shutdown all
    OmegaOrchestrator::instance().shutdown();
    NeuralBridge::instance().shutdown();
    SpeciatorEngine::instance().shutdown();
    MeshBrain::instance().shutdown();
    HardwareSynthesizer::instance().shutdown();
    SelfHostEngine::instance().shutdown();

    for (int i = 0; i < 6; i++) {
        m_phases[i].active = false;
        m_phases[i].healthScore = 0.0f;
    }
    m_anyActive = false;

    return PatchResult::ok("EMERGENCY STOP — All systems halted");
}

// ============================================================================
//  Run Autonomous Cycle
// ============================================================================
PipelineResult TranscendenceCoordinator::runAutonomousCycle(
    const char* requirement, uint32_t length) {

    std::lock_guard<std::mutex> lock(m_mutex);
    PipelineResult result{0, 0, 0, 0, 0, false};

    if (!m_anyActive || m_emergencyStopped) return result;

    // Delegate to Omega Orchestrator
    result = OmegaOrchestrator::instance().runAutonomousCycle(requirement, length);
    m_stats.autonomousCycles++;

    return result;
}

// ============================================================================
//  Route Event Between Phases
// ============================================================================
PatchResult TranscendenceCoordinator::routeEvent(const TranscendenceEvent& event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_anyActive) return PatchResult::error("No phases active", -1);

    int srcIdx = phaseIndex(event.source);
    int tgtIdx = phaseIndex(event.target);

    if (srcIdx < 0 || tgtIdx < 0)
        return PatchResult::error("Invalid source or target phase", -2);

    if (!m_phases[tgtIdx].active)
        return PatchResult::error("Target phase not active", -3);

    // Update heartbeats
    m_phases[srcIdx].lastHeartbeat = RDTSC();
    m_phases[tgtIdx].lastHeartbeat = RDTSC();

    m_stats.eventsRouted++;
    m_stats.crossPhaseOps++;

    return PatchResult::ok("Event routed");
}

// ============================================================================
//  Health Check
// ============================================================================
TranscendenceHealth TranscendenceCoordinator::checkHealth() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    TranscendenceHealth health{};

    health.activePhasesCount = 0;
    health.totalOperations = m_stats.eventsRouted + m_stats.autonomousCycles;
    health.totalErrors = 0;

    for (int i = 0; i < 6; i++) {
        health.phases[i] = m_phases[i];
        if (m_phases[i].active) health.activePhasesCount++;
        health.totalErrors += m_phases[i].errorCount;
    }

    // Calculate overall score
    if (health.activePhasesCount == 0) {
        health.overallScore = 0.0f;
        health.level = HealthLevel::Emergency;
    } else {
        float sum = 0.0f;
        for (int i = 0; i < 6; i++) {
            if (m_phases[i].active) sum += m_phases[i].healthScore;
        }
        health.overallScore = sum / 6.0f;

        if (health.overallScore > 0.8f)
            health.level = HealthLevel::Nominal;
        else if (health.overallScore > 0.5f)
            health.level = HealthLevel::Degraded;
        else
            health.level = HealthLevel::Critical;
    }

    if (m_emergencyStopped) {
        health.level = HealthLevel::Emergency;
    }

    health.uptime = RDTSC() - m_initTimestamp;

    return health;
}

// ============================================================================
//  Get Current Transcendence Level
// ============================================================================
TranscendencePhase TranscendenceCoordinator::getCurrentLevel() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Highest active phase
    for (int i = 5; i >= 0; i--) {
        if (m_phases[i].active) return m_phases[i].phase;
    }

    return TranscendencePhase::None;
}

// ============================================================================
//  Get Phase Status
// ============================================================================
PhaseStatus TranscendenceCoordinator::getPhaseStatus(TranscendencePhase phase) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = phaseIndex(phase);
    if (idx < 0) return PhaseStatus{TranscendencePhase::None, false, 0, 0, 0, 0.0f};

    return m_phases[idx];
}

// ============================================================================
//  Get Stats
// ============================================================================
TranscendenceStats TranscendenceCoordinator::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    TranscendenceStats st = m_stats;
    st.uptime = RDTSC() - m_initTimestamp;
    return st;
}

// ============================================================================
//  Diagnostics Dump
// ============================================================================
void TranscendenceCoordinator::dumpDiagnostics() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Delegate diagnostics to each active subsystem
    char diagBuf[4096];
    if (m_phases[phaseIndex(TranscendencePhase::SelfHost)].active)
        SelfHostEngine::instance().dumpDiagnostics(diagBuf, sizeof(diagBuf));
    if (m_phases[phaseIndex(TranscendencePhase::HWSynth)].active)
        HardwareSynthesizer::instance().dumpDiagnostics(diagBuf, sizeof(diagBuf));
    if (m_phases[phaseIndex(TranscendencePhase::MeshBrain)].active)
        MeshBrain::instance().dumpDiagnostics(diagBuf, sizeof(diagBuf));
    if (m_phases[phaseIndex(TranscendencePhase::Speciator)].active)
        SpeciatorEngine::instance().dumpDiagnostics(diagBuf, sizeof(diagBuf));
    if (m_phases[phaseIndex(TranscendencePhase::Neural)].active)
        NeuralBridge::instance().dumpDiagnostics();
    if (m_phases[phaseIndex(TranscendencePhase::Omega)].active)
        OmegaOrchestrator::instance().dumpDiagnostics();
}

} // namespace rawrxd
