// ============================================================================
// hotpatch_wiring.cpp — Phase 12: Hotpatcher ↔ Governor Integration
// ============================================================================
// Implementation of three-layer hotpatch coordination system.
// ============================================================================

#include "hotpatch_wiring.h"
#include "execution_governor.h"
#include <chrono>
#include <algorithm>
#include <iostream>
#include <cstring>

// ============================================================================
// HotpatchCoordinator Implementation
// ============================================================================

static HotpatchCoordinator* g_hotpatchCoordinator = nullptr;
static std::atomic<bool> g_coordinatorInitialized{false};

HotpatchCoordinator& HotpatchCoordinator::instance() {
    if (!g_hotpatchCoordinator) {
        g_hotpatchCoordinator = new HotpatchCoordinator();
    }
    return *g_hotpatchCoordinator;
}

HotpatchCoordinator::HotpatchCoordinator()
    : m_governor(nullptr),
      m_phase(HotpatchPhase::Idle),
      m_phaseEnteredAtMs(0),
      m_shutdown(false),
      m_version(0)
{
}

HotpatchCoordinator::~HotpatchCoordinator() {
    shutdown();
}

bool HotpatchCoordinator::initialize(ExecutionGovernor* governor) {
    if (!governor) {
        return false;
    }
    m_governor = governor;
    m_phase.store(HotpatchPhase::Idle, std::memory_order_release);
    m_shutdown.store(false, std::memory_order_release);
    g_coordinatorInitialized.store(true, std::memory_order_release);
    return true;
}

void HotpatchCoordinator::shutdown() {
    m_shutdown.store(true, std::memory_order_release);
    m_phase.store(HotpatchPhase::Idle, std::memory_order_release);
}

bool HotpatchCoordinator::requestPatch(const HotpatchDescriptor& descriptor) {
    if (!m_governor || m_shutdown.load(std::memory_order_acquire)) {
        return false;
    }

    // Only accept patches when idle or after last one completes
    HotpatchPhase current = m_phase.load(std::memory_order_acquire);
    if (current != HotpatchPhase::Idle && current != HotpatchPhase::Validated) {
        return false;  // Already patching
    }

    m_currentPatch = descriptor;
    m_governor->setActivePatchId(descriptor.patchId);
    m_checkpointedTasks.clear();
    m_phase.store(HotpatchPhase::RequestPending, std::memory_order_release);
    m_phaseEnteredAtMs.store(std::chrono::steady_clock::now().time_since_epoch().count() / 1000000, std::memory_order_release);

    m_version.fetch_add(1, std::memory_order_release);
    return true;
}

HotpatchPhase HotpatchCoordinator::getPhase() const {
    return m_phase.load(std::memory_order_acquire);
}

std::string HotpatchCoordinator::getCurrentPatchId() const {
    if (getPhase() != HotpatchPhase::Idle) {
        return m_currentPatch.patchId;
    }
    return "";
}

bool HotpatchCoordinator::waitForPhase(HotpatchPhase targetPhase, uint64_t timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (m_phase.load(std::memory_order_acquire) == targetPhase) {
            return true;
        }
        Sleep(10);  // Poll every 10ms
    }
    return m_phase.load(std::memory_order_acquire) == targetPhase;
}

HotpatchResult HotpatchCoordinator::getLastResult() const {
    return m_lastResult;
}

bool HotpatchCoordinator::abortPatch() {
    HotpatchPhase current = m_phase.load(std::memory_order_acquire);
    if (current == HotpatchPhase::Idle) {
        return true;  // Already idle
    }

    // Transition to rolled state
    transitionTo(HotpatchPhase::Rolled);
    m_lastResult = HotpatchResult::error("patch aborted by request");
    m_phase.store(HotpatchPhase::Idle, std::memory_order_release);
    
    // Notify Governor to resume
    if (m_governor) {
        m_governor->onHotpatchStateChange(static_cast<uint8_t>(HotpatchPhase::Idle));
    }

    PatchSafetyGate::instance().markPatchComplete();
    return true;
}

void HotpatchCoordinator::registerValidator(const std::string& name,
                                           std::function<bool()> validatorFn) {
    if (validatorFn) {
        m_validators.push_back({name, validatorFn});
    }
}

void HotpatchCoordinator::notifyTaskCheckpoint(uint64_t taskId) {
    // Record that this task has reached a checkpoint
    auto it = std::find(m_checkpointedTasks.begin(), m_checkpointedTasks.end(), taskId);
    if (it == m_checkpointedTasks.end()) {
        m_checkpointedTasks.push_back(taskId);
    }
}

void HotpatchCoordinator::notifyAllCheckpointed() {
    // Governor signals that all tasks have checkpointed (or wait timeout exceeded)
    HotpatchPhase current = m_phase.load(std::memory_order_acquire);
    if (current == HotpatchPhase::CheckpointWait) {
        transitionTo(HotpatchPhase::Applying);
    }
}

void HotpatchCoordinator::pollOnce() {
    if (m_shutdown.load(std::memory_order_acquire)) {
        return;
    }

    HotpatchPhase current = m_phase.load(std::memory_order_acquire);

    switch (current) {
        case HotpatchPhase::RequestPending:
            // Signal Governor to start quiescence
            if (m_governor) {
                m_governor->onHotpatchStateChange(static_cast<uint8_t>(HotpatchPhase::QuiescenceWait));
            }
            transitionTo(HotpatchPhase::QuiescenceWait);
            break;

        case HotpatchPhase::QuiescenceWait:
            // Governor pauses new submissions, then enters checkpoint wait
            if (m_governor) {
                m_governor->onHotpatchStateChange(static_cast<uint8_t>(HotpatchPhase::CheckpointWait));
            }
            transitionTo(HotpatchPhase::CheckpointWait);
            break;

        case HotpatchPhase::CheckpointWait:
            handleCheckpointWait();
            break;

        case HotpatchPhase::Applying:
            handleApplying();
            break;

        case HotpatchPhase::Applied:
            handleValidation();
            break;

        case HotpatchPhase::Validated:
            // Patch complete, resume normal operation
            if (m_governor) {
                m_governor->onHotpatchStateChange(static_cast<uint8_t>(HotpatchPhase::Idle));
            }
            m_lastResult = HotpatchResult::ok(HotpatchPhase::Validated, "patch applied and validated", 0);
            PatchSafetyGate::instance().markPatchComplete();
            transitionTo(HotpatchPhase::Idle);
            break;

        case HotpatchPhase::Idle:
        case HotpatchPhase::Error:
        case HotpatchPhase::Rolled:
        default:
            break;  // No action needed
    }
}

void HotpatchCoordinator::transitionTo(HotpatchPhase phase) {
    m_phase.store(phase, std::memory_order_release);
    m_phaseEnteredAtMs.store(
        std::chrono::steady_clock::now().time_since_epoch().count() / 1000000,
        std::memory_order_release);
}

void HotpatchCoordinator::handleCheckpointWait() {
    // Wait for all active tasks to checkpoint, or timeout
    uint64_t now = std::chrono::steady_clock::now().time_since_epoch().count() / 1000000;
    uint64_t elapsed = now - m_phaseEnteredAtMs.load(std::memory_order_acquire);

    if (elapsed >= CHECKPOINT_TIMEOUT_MS || allTasksCheckpointed()) {
        // Safe to apply patch
        PatchSafetyGate::instance().markPatchActive(m_currentPatch.patchId);
        transitionTo(HotpatchPhase::Applying);
    }
}

void HotpatchCoordinator::handleApplying() {
    // Patch is being applied (or has been applied by external layer)
    // Brief window for streaming re-quantization, model surgery, etc.

    uint64_t now = std::chrono::steady_clock::now().time_since_epoch().count() / 1000000;
    uint64_t elapsed = now - m_phaseEnteredAtMs.load(std::memory_order_acquire);

    if (elapsed >= m_currentPatch.estimatedDurationMs) {
        // Patch should be complete
        transitionTo(HotpatchPhase::Applied);
    } else if (elapsed > APPLY_TIMEOUT_MS) {
        // Timeout: patch took too long, rollback
        m_lastResult = HotpatchResult::error("patch apply timeout");
        transitionTo(HotpatchPhase::Rolled);
        PatchSafetyGate::instance().markPatchComplete();
    }
}

void HotpatchCoordinator::handleValidation() {
    // Run registered validators
    bool allValid = true;
    for (const auto& vp : m_validators) {
        if (!vp.second()) {
            allValid = false;
            break;
        }
    }

    if (allValid) {
        transitionTo(HotpatchPhase::Validated);
    } else {
        m_lastResult = HotpatchResult::error("patch validation failed");
        transitionTo(HotpatchPhase::Rolled);
    }
}

bool HotpatchCoordinator::allTasksCheckpointed() const {
    if (!m_governor) {
        return true;
    }

    const GovernorStats stats = m_governor->getStats();
    if (stats.activeTaskCount == 0) {
        return true;
    }

    return m_governor->getCheckpointedTaskCount() >= stats.activeTaskCount;
}

bool HotpatchCoordinator::isApplyingTakingTooLong() const {
    uint64_t now = std::chrono::steady_clock::now().time_since_epoch().count() / 1000000;
    uint64_t elapsed = now - m_phaseEnteredAtMs.load(std::memory_order_acquire);
    return elapsed > APPLY_TIMEOUT_MS;
}

// ============================================================================
// PatchSafetyGate Implementation
// ============================================================================

static PatchSafetyGate* g_patchSafetyGate = nullptr;

PatchSafetyGate& PatchSafetyGate::instance() {
    if (!g_patchSafetyGate) {
        g_patchSafetyGate = new PatchSafetyGate();
    }
    return *g_patchSafetyGate;
}

PatchSafetyGate::PatchSafetyGate()
    : m_patchActive(false),
      m_instructionChecksum(0),
      m_version(0)
{
}

PatchSafetyGate::~PatchSafetyGate() {
}

bool PatchSafetyGate::initialize() {
    // In Phase 12: record initial instruction state
    // This could hash the instructions provider or check contract state
    m_patchActive.store(false, std::memory_order_release);
    m_version.fetch_add(1, std::memory_order_release);
    return true;
}

uint64_t PatchSafetyGate::recordInstructionChecksum() {
    // Record a snapshot of instruction state before critical operation
    // In a full implementation, this would hash the instructions provider
    m_version.fetch_add(1, std::memory_order_release);
    return m_version.load(std::memory_order_acquire);
}

bool PatchSafetyGate::validateInstructionChecksum(uint64_t checkpoint) {
    // Check if version has changed (indicating patch was applied)
    uint64_t current = m_version.load(std::memory_order_acquire);
    return current == checkpoint;  // True if unchanged, False if instructions drifted
}

void PatchSafetyGate::markPatchActive(const std::string& patchId) {
    m_activePatchId = patchId;
    m_patchActive.store(true, std::memory_order_release);
    m_version.fetch_add(1, std::memory_order_release);
}

void PatchSafetyGate::markPatchComplete() {
    m_patchActive.store(false, std::memory_order_release);
    m_activePatchId.clear();
    m_version.fetch_add(1, std::memory_order_release);
}

bool PatchSafetyGate::isPatchActive() const {
    return m_patchActive.load(std::memory_order_acquire);
}

std::string PatchSafetyGate::getActivePatchId() const {
    if (m_patchActive.load(std::memory_order_acquire)) {
        return m_activePatchId;
    }
    return "";
}
