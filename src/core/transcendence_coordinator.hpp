// ============================================================================
// transcendence_coordinator.hpp — Master Coordinator (Phases E → Ω)
// ============================================================================
//
// The Transcendence Coordinator unifies all subsystems into a coherent
// autonomous development entity:
//
//   Phase E: SelfHost Engine      — Recursive self-compilation
//   Phase F: Hardware Synthesizer — FPGA/ASIC acceleration
//   Phase G: Mesh Brain           — Distributed P2P consciousness
//   Phase H: Speciator Engine     — Metamorphic evolution
//   Phase I: Neural Bridge        — Direct cortex interface
//   Phase Ω: Omega Orchestrator   — Autonomous development pipeline
//
// The coordinator provides:
//   - Unified initialization / shutdown lifecycle
//   - Cross-phase event routing and data flow
//   - System-wide health monitoring
//   - Transcendence level tracking (E → Ω progression)
//   - Emergency kill switch
//
// Pattern: singleton + PatchResult error model + mutex protection
// ============================================================================
#pragma once

#include "model_memory_hotpatch.hpp"    // PatchResult
#include "self_host_engine.hpp"
#include "hardware_synthesizer.hpp"
#include "mesh_brain.hpp"
#include "speciator_engine.hpp"
#include "neural_bridge.hpp"
#include "omega_orchestrator.hpp"
#include <cstdint>
#include <mutex>

namespace rawrxd {

// ============================================================================
//  Enums
// ============================================================================

/// Transcendence phases
enum class TranscendencePhase : uint32_t {
    None      = 0,
    SelfHost  = 1,    // Phase E
    HWSynth   = 2,    // Phase F
    MeshBrain = 3,    // Phase G
    Speciator = 4,    // Phase H
    Neural    = 5,    // Phase I
    Omega     = 6     // Phase Ω
};

/// System health status
enum class HealthLevel : uint32_t {
    Nominal     = 0,    // All systems green
    Degraded    = 1,    // Some subsystems impaired
    Critical    = 2,    // Multiple failures
    Emergency   = 3     // Kill switch active
};

// ============================================================================
//  Structures
// ============================================================================

/// Per-phase status
struct PhaseStatus {
    TranscendencePhase phase;
    bool               active;
    uint64_t           initTimestamp;
    uint64_t           lastHeartbeat;
    uint32_t           errorCount;
    float              healthScore;     // 0.0-1.0
};

/// System-wide health snapshot
struct TranscendenceHealth {
    HealthLevel         level;
    uint32_t            activePhasesCount;
    PhaseStatus         phases[6];      // E through Ω
    uint64_t            totalOperations;
    uint64_t            totalErrors;
    float               overallScore;   // 0.0-1.0
    uint64_t            uptime;         // TSC delta since init
};

/// Cross-phase event for routing
struct TranscendenceEvent {
    TranscendencePhase  source;
    TranscendencePhase  target;
    uint32_t            eventType;      // Phase-specific
    uint64_t            data;           // Payload / pointer
    uint64_t            timestamp;
};

/// Transcendence coordinator statistics
struct TranscendenceStats {
    uint64_t eventsRouted;
    uint64_t phaseInits;
    uint64_t phaseShutdowns;
    uint64_t healthChecks;
    uint64_t emergencyStops;
    uint64_t autonomousCycles;
    uint64_t crossPhaseOps;
    uint64_t uptime;
};

// ============================================================================
//  TranscendenceCoordinator — Master Singleton
// ============================================================================
class TranscendenceCoordinator {
public:
    static TranscendenceCoordinator& instance();

    /// Initialize all transcendence phases (E → Ω)
    PatchResult initializeAll();

    /// Initialize a single phase
    PatchResult initializePhase(TranscendencePhase phase);

    /// Shutdown all phases (Ω → E, reverse order)
    PatchResult shutdownAll();

    /// Shutdown a single phase
    PatchResult shutdownPhase(TranscendencePhase phase);

    /// Emergency kill switch — immediate halt of all subsystems
    PatchResult emergencyStop();

    /// Run full autonomous development cycle
    /// requirement → plan → architect → implement → verify → deploy → observe → evolve
    PipelineResult runAutonomousCycle(const char* requirement, uint32_t length);

    /// Route an event between phases
    PatchResult routeEvent(const TranscendenceEvent& event);

    /// Run system health check
    TranscendenceHealth checkHealth() const;

    /// Get current transcendence level (highest active phase)
    TranscendencePhase getCurrentLevel() const;

    /// Get per-phase status
    PhaseStatus getPhaseStatus(TranscendencePhase phase) const;

    /// Get coordinator statistics
    TranscendenceStats getStats() const;

    /// Is any phase active?
    bool isActive() const { return m_anyActive; }

    /// Diagnostics dump (all phases)
    void dumpDiagnostics() const;

    // ---- Direct subsystem accessors ----
    SelfHostEngine&       selfHost()    { return SelfHostEngine::instance(); }
    HardwareSynthesizer&  hwSynth()     { return HardwareSynthesizer::instance(); }
    MeshBrain&            meshBrain()   { return MeshBrain::instance(); }
    SpeciatorEngine&      speciator()   { return SpeciatorEngine::instance(); }
    NeuralBridge&         neural()      { return NeuralBridge::instance(); }
    OmegaOrchestrator&    omega()       { return OmegaOrchestrator::instance(); }

private:
    TranscendenceCoordinator() = default;
    ~TranscendenceCoordinator() = default;
    TranscendenceCoordinator(const TranscendenceCoordinator&) = delete;
    TranscendenceCoordinator& operator=(const TranscendenceCoordinator&) = delete;

    mutable std::mutex      m_mutex;
    bool                    m_anyActive = false;
    bool                    m_emergencyStopped = false;
    uint64_t                m_initTimestamp = 0;
    PhaseStatus             m_phases[6] = {};
    TranscendenceStats      m_stats = {};
};

} // namespace rawrxd
