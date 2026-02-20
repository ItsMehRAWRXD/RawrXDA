// hotpatch_control_plane.cpp — Phase 14: Advanced Hotpatch Control Plane
// Versioned patch management with dependency graphs, atomic multi-layer
// transactions, rollback chains, validation pipelines, and safety verification.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured PatchResult returns only.
// Rule: All threading via std::mutex + std::lock_guard. No recursive locks.
#include "hotpatch_control_plane.hpp"
#include <algorithm>
#include <queue>
#include <fstream>
#include <cstring>

// ============================================================================
// Singleton
// ============================================================================
HotpatchControlPlane& HotpatchControlPlane::instance() {
    static HotpatchControlPlane inst;
    return inst;
}

HotpatchControlPlane::HotpatchControlPlane() = default;
HotpatchControlPlane::~HotpatchControlPlane() = default;

// ============================================================================
// Patch Registration
// ============================================================================
PatchResult HotpatchControlPlane::registerPatch(PatchManifest& manifest) {
    std::lock_guard<std::mutex> lock(m_mutex);

    manifest.patchId     = m_nextPatchId.fetch_add(1);
    manifest.state       = PatchLifecycleState::Draft;
    manifest.created     = std::chrono::steady_clock::now();
    manifest.lastModified = manifest.created;
    manifest.validated   = false;

    m_patches[manifest.patchId] = manifest;
    m_versionHistory[manifest.patchId].push_back(manifest.version);
    m_stats.totalPatches.fetch_add(1);

    recordAudit(manifest.patchId, 0, PatchLifecycleState::Draft,
                PatchLifecycleState::Draft, "system", "Patch registered");

    return PatchResult::ok("Patch registered");
}

PatchResult HotpatchControlPlane::unregisterPatch(uint64_t patchId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) {
        return PatchResult::error("Patch not found", -1);
    }

    if (it->second.state == PatchLifecycleState::Applied) {
        return PatchResult::error("Cannot unregister an active patch — suspend or rollback first", -2);
    }

    recordAudit(patchId, 0, it->second.state, PatchLifecycleState::Archived,
                "system", "Patch unregistered");

    m_patches.erase(it);
    return PatchResult::ok("Patch unregistered");
}

PatchResult HotpatchControlPlane::updatePatchMetadata(uint64_t patchId,
    const std::string& name, const std::string& desc, const std::string& author) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return PatchResult::error("Patch not found", -1);

    if (!name.empty())   it->second.name = name;
    if (!desc.empty())   it->second.description = desc;
    if (!author.empty()) it->second.author = author;
    it->second.lastModified = std::chrono::steady_clock::now();

    return PatchResult::ok("Metadata updated");
}

// ============================================================================
// Lifecycle Management
// ============================================================================
PatchResult HotpatchControlPlane::validatePatch(uint64_t patchId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return PatchResult::error("Patch not found", -1);

    auto& manifest = it->second;
    if (manifest.state != PatchLifecycleState::Draft &&
        manifest.state != PatchLifecycleState::RolledBack) {
        return PatchResult::error("Patch must be in Draft or RolledBack state to validate", -2);
    }

    // Run validation rules
    bool allPassed = true;
    std::string report;

    for (auto& [ruleId, rule] : m_validationRules) {
        if (static_cast<uint8_t>(manifest.safetyLevel) < static_cast<uint8_t>(rule.minLevel)) {
            continue; // Rule doesn't apply to this safety level
        }

        if (rule.validate) {
            char msg[512] = {};
            bool passed = rule.validate(&manifest, msg, sizeof(msg));
            if (!passed) {
                allPassed = false;
                report += "[FAIL] " + rule.name + ": " + msg + "\n";
                m_stats.validationsFailed.fetch_add(1);
            } else {
                report += "[PASS] " + rule.name + "\n";
                m_stats.validationsPassed.fetch_add(1);
            }
        }
    }

    manifest.validated = allPassed;
    manifest.validationReport = report;

    if (allPassed) {
        PatchLifecycleState oldState = manifest.state;
        manifest.state = PatchLifecycleState::Validated;
        manifest.lastModified = std::chrono::steady_clock::now();
        recordAudit(patchId, 0, oldState, PatchLifecycleState::Validated,
                    "validation-pipeline", "Validation passed");
        return PatchResult::ok("Validation passed");
    } else {
        return PatchResult::error("Validation failed — see report", -3);
    }
}

PatchResult HotpatchControlPlane::stagePatch(uint64_t patchId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return PatchResult::error("Patch not found", -1);

    if (it->second.state != PatchLifecycleState::Validated) {
        return PatchResult::error("Patch must be Validated before staging", -2);
    }

    // Check dependencies are applied
    if (!checkDependencies(patchId)) {
        m_stats.dependencyErrors.fetch_add(1);
        return PatchResult::error("Unmet dependencies — required patches not applied", -3);
    }

    // Check conflicts
    if (!checkConflicts(patchId)) {
        m_stats.conflictsDetected.fetch_add(1);
        return PatchResult::error("Conflicting patch is currently active", -4);
    }

    PatchLifecycleState oldState = it->second.state;
    it->second.state = PatchLifecycleState::Staged;
    it->second.lastModified = std::chrono::steady_clock::now();

    recordAudit(patchId, 0, oldState, PatchLifecycleState::Staged, "system", "Patch staged");
    return PatchResult::ok("Patch staged");
}

PatchResult HotpatchControlPlane::applyPatch(uint64_t patchId,
    const std::string& actor, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return PatchResult::error("Patch not found", -1);

    if (it->second.state != PatchLifecycleState::Staged &&
        it->second.state != PatchLifecycleState::Suspended) {
        return PatchResult::error("Patch must be Staged or Suspended to apply", -2);
    }

    PatchLifecycleState oldState = it->second.state;
    it->second.state = PatchLifecycleState::Applied;
    it->second.lastModified = std::chrono::steady_clock::now();
    m_stats.activePatches.fetch_add(1);

    recordAudit(patchId, 0, oldState, PatchLifecycleState::Applied, actor, reason);

    if (m_patchStateCb) {
        m_patchStateCb(patchId, PatchLifecycleState::Applied, m_patchStateData);
    }

    return PatchResult::ok("Patch applied");
}

PatchResult HotpatchControlPlane::suspendPatch(uint64_t patchId,
    const std::string& actor, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return PatchResult::error("Patch not found", -1);

    if (it->second.state != PatchLifecycleState::Applied) {
        return PatchResult::error("Only Applied patches can be suspended", -2);
    }

    it->second.state = PatchLifecycleState::Suspended;
    it->second.lastModified = std::chrono::steady_clock::now();
    m_stats.activePatches.fetch_sub(1);

    recordAudit(patchId, 0, PatchLifecycleState::Applied,
                PatchLifecycleState::Suspended, actor, reason);

    if (m_patchStateCb) {
        m_patchStateCb(patchId, PatchLifecycleState::Suspended, m_patchStateData);
    }

    return PatchResult::ok("Patch suspended");
}

PatchResult HotpatchControlPlane::rollbackPatch(uint64_t patchId,
    const std::string& actor, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return PatchResult::error("Patch not found", -1);

    PatchLifecycleState oldState = it->second.state;
    if (oldState != PatchLifecycleState::Applied &&
        oldState != PatchLifecycleState::Suspended &&
        oldState != PatchLifecycleState::Staged) {
        return PatchResult::error("Patch not in a rollbackable state", -2);
    }

    // Check if any other applied patch depends on this one
    for (auto& [pid, patch] : m_patches) {
        if (patch.state == PatchLifecycleState::Applied) {
            for (auto depId : patch.dependencies) {
                if (depId == patchId) {
                    return PatchResult::error("Cannot rollback — other active patches depend on this", -3);
                }
            }
        }
    }

    if (oldState == PatchLifecycleState::Applied) {
        m_stats.activePatches.fetch_sub(1);
    }

    it->second.state = PatchLifecycleState::RolledBack;
    it->second.lastModified = std::chrono::steady_clock::now();

    recordAudit(patchId, 0, oldState, PatchLifecycleState::RolledBack, actor, reason);

    if (m_patchStateCb) {
        m_patchStateCb(patchId, PatchLifecycleState::RolledBack, m_patchStateData);
    }

    return PatchResult::ok("Patch rolled back");
}

PatchResult HotpatchControlPlane::deprecatePatch(uint64_t patchId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return PatchResult::error("Patch not found", -1);

    PatchLifecycleState oldState = it->second.state;
    it->second.state = PatchLifecycleState::Deprecated;
    it->second.lastModified = std::chrono::steady_clock::now();

    recordAudit(patchId, 0, oldState, PatchLifecycleState::Deprecated,
                "system", "Patch deprecated");

    return PatchResult::ok("Patch deprecated");
}

PatchResult HotpatchControlPlane::archivePatch(uint64_t patchId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return PatchResult::error("Patch not found", -1);

    if (it->second.state == PatchLifecycleState::Applied) {
        return PatchResult::error("Cannot archive an active patch", -2);
    }

    PatchLifecycleState oldState = it->second.state;
    it->second.state = PatchLifecycleState::Archived;
    it->second.lastModified = std::chrono::steady_clock::now();

    recordAudit(patchId, 0, oldState, PatchLifecycleState::Archived,
                "system", "Patch archived");

    return PatchResult::ok("Patch archived");
}

// ============================================================================
// Transaction Support
// ============================================================================
uint64_t HotpatchControlPlane::beginTransaction(const std::string& description) {
    std::lock_guard<std::mutex> lock(m_mutex);

    PatchTransaction txn;
    txn.transactionId = m_nextTxnId.fetch_add(1);
    txn.description   = description;
    txn.committed     = false;
    txn.rolledBack    = false;
    txn.startTime     = std::chrono::steady_clock::now();

    m_transactions[txn.transactionId] = txn;
    m_stats.totalTransactions.fetch_add(1);

    return txn.transactionId;
}

PatchResult HotpatchControlPlane::addToTransaction(uint64_t txnId, uint64_t patchId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto txnIt = m_transactions.find(txnId);
    if (txnIt == m_transactions.end()) return PatchResult::error("Transaction not found", -1);

    if (txnIt->second.committed || txnIt->second.rolledBack) {
        return PatchResult::error("Transaction already finalized", -2);
    }

    auto patchIt = m_patches.find(patchId);
    if (patchIt == m_patches.end()) return PatchResult::error("Patch not found", -3);

    txnIt->second.patchIds.push_back(patchId);
    return PatchResult::ok("Patch added to transaction");
}

PatchResult HotpatchControlPlane::commitTransaction(uint64_t txnId, const std::string& actor) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto txnIt = m_transactions.find(txnId);
    if (txnIt == m_transactions.end()) return PatchResult::error("Transaction not found", -1);

    auto& txn = txnIt->second;
    if (txn.committed || txn.rolledBack) {
        return PatchResult::error("Transaction already finalized", -2);
    }

    // Validate all patches in order
    for (size_t i = 0; i < txn.patchIds.size(); i++) {
        uint64_t pid = txn.patchIds[i];
        auto pIt = m_patches.find(pid);
        if (pIt == m_patches.end()) {
            // Rollback already-applied patches in this transaction
            for (size_t j = 0; j < i; j++) {
                auto rollIt = m_patches.find(txn.patchIds[j]);
                if (rollIt != m_patches.end() &&
                    rollIt->second.state == PatchLifecycleState::Applied) {
                    rollIt->second.state = PatchLifecycleState::RolledBack;
                    m_stats.activePatches.fetch_sub(1);
                }
            }
            txn.rolledBack = true;
            return PatchResult::error("Missing patch in transaction — rolled back", -3);
        }

        // Apply each patch
        PatchLifecycleState oldState = pIt->second.state;
        pIt->second.state = PatchLifecycleState::Applied;
        pIt->second.lastModified = std::chrono::steady_clock::now();
        m_stats.activePatches.fetch_add(1);

        recordAudit(pid, txnId, oldState, PatchLifecycleState::Applied,
                    actor, "Applied via transaction " + std::to_string(txnId));
    }

    txn.committed = true;
    txn.endTime = std::chrono::steady_clock::now();
    m_stats.committedTransactions.fetch_add(1);

    if (m_txnCb) {
        m_txnCb(txnId, true, m_txnData);
    }

    return PatchResult::ok("Transaction committed");
}

PatchResult HotpatchControlPlane::rollbackTransaction(uint64_t txnId, const std::string& actor) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto txnIt = m_transactions.find(txnId);
    if (txnIt == m_transactions.end()) return PatchResult::error("Transaction not found", -1);

    auto& txn = txnIt->second;
    if (txn.rolledBack) return PatchResult::error("Transaction already rolled back", -2);

    // Rollback in reverse order
    for (auto it = txn.patchIds.rbegin(); it != txn.patchIds.rend(); ++it) {
        auto pIt = m_patches.find(*it);
        if (pIt != m_patches.end() &&
            pIt->second.state == PatchLifecycleState::Applied) {
            PatchLifecycleState oldState = pIt->second.state;
            pIt->second.state = PatchLifecycleState::RolledBack;
            pIt->second.lastModified = std::chrono::steady_clock::now();
            m_stats.activePatches.fetch_sub(1);

            recordAudit(*it, txnId, oldState, PatchLifecycleState::RolledBack,
                        actor, "Rolled back via transaction " + std::to_string(txnId));
        }
    }

    txn.rolledBack = true;
    txn.endTime = std::chrono::steady_clock::now();
    m_stats.rolledBackTransactions.fetch_add(1);

    if (m_txnCb) {
        m_txnCb(txnId, false, m_txnData);
    }

    return PatchResult::ok("Transaction rolled back");
}

PatchResult HotpatchControlPlane::createSavepoint(uint64_t txnId, const std::string& label) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto txnIt = m_transactions.find(txnId);
    if (txnIt == m_transactions.end()) return PatchResult::error("Transaction not found", -1);

    PatchTransaction::Savepoint sp;
    sp.savepointId = m_nextSavepointId.fetch_add(1);
    sp.patchIndex  = txnIt->second.patchIds.size();
    sp.label       = label;

    txnIt->second.savepoints.push_back(sp);
    return PatchResult::ok("Savepoint created");
}

PatchResult HotpatchControlPlane::rollbackToSavepoint(uint64_t txnId, uint64_t savepointId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto txnIt = m_transactions.find(txnId);
    if (txnIt == m_transactions.end()) return PatchResult::error("Transaction not found", -1);

    auto& txn = txnIt->second;
    const PatchTransaction::Savepoint* sp = nullptr;
    for (auto& s : txn.savepoints) {
        if (s.savepointId == savepointId) { sp = &s; break; }
    }
    if (!sp) return PatchResult::error("Savepoint not found", -2);

    // Rollback patches after savepoint index
    for (size_t i = txn.patchIds.size(); i > sp->patchIndex; i--) {
        uint64_t pid = txn.patchIds[i - 1];
        auto pIt = m_patches.find(pid);
        if (pIt != m_patches.end() &&
            pIt->second.state == PatchLifecycleState::Applied) {
            pIt->second.state = PatchLifecycleState::RolledBack;
            m_stats.activePatches.fetch_sub(1);
        }
    }

    txn.patchIds.resize(sp->patchIndex);
    return PatchResult::ok("Rolled back to savepoint");
}

// ============================================================================
// Dependency & Conflict Management
// ============================================================================
PatchResult HotpatchControlPlane::addDependency(uint64_t patchId, uint64_t dependsOn) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return PatchResult::error("Patch not found", -1);
    if (m_patches.find(dependsOn) == m_patches.end()) return PatchResult::error("Dependency not found", -2);

    it->second.dependencies.push_back(dependsOn);

    // Check for cycles
    if (hasDependencyCycle()) {
        it->second.dependencies.pop_back();
        return PatchResult::error("Adding dependency would create a cycle", -3);
    }

    return PatchResult::ok("Dependency added");
}

PatchResult HotpatchControlPlane::removeDependency(uint64_t patchId, uint64_t dependsOn) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return PatchResult::error("Patch not found", -1);

    auto& deps = it->second.dependencies;
    deps.erase(std::remove(deps.begin(), deps.end(), dependsOn), deps.end());

    return PatchResult::ok("Dependency removed");
}

PatchResult HotpatchControlPlane::addConflict(uint64_t patchA, uint64_t patchB) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto itA = m_patches.find(patchA);
    auto itB = m_patches.find(patchB);
    if (itA == m_patches.end() || itB == m_patches.end()) {
        return PatchResult::error("One or both patches not found", -1);
    }

    itA->second.conflicts.push_back(patchB);
    itB->second.conflicts.push_back(patchA);

    return PatchResult::ok("Conflict registered");
}

PatchResult HotpatchControlPlane::removeConflict(uint64_t patchA, uint64_t patchB) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto itA = m_patches.find(patchA);
    auto itB = m_patches.find(patchB);

    if (itA != m_patches.end()) {
        auto& c = itA->second.conflicts;
        c.erase(std::remove(c.begin(), c.end(), patchB), c.end());
    }
    if (itB != m_patches.end()) {
        auto& c = itB->second.conflicts;
        c.erase(std::remove(c.begin(), c.end(), patchA), c.end());
    }

    return PatchResult::ok("Conflict removed");
}

bool HotpatchControlPlane::hasDependencyCycle() const {
    // DFS-based cycle detection
    std::unordered_set<uint64_t> visited;
    std::unordered_set<uint64_t> inStack;

    for (auto& [id, _] : m_patches) {
        if (visited.find(id) == visited.end()) {
            if (detectCycleFrom(id, visited, inStack)) return true;
        }
    }
    return false;
}

std::vector<uint64_t> HotpatchControlPlane::resolveDependencyOrder(uint64_t patchId) const {
    // Topological sort of dependency subgraph rooted at patchId
    std::vector<uint64_t> order;
    std::unordered_set<uint64_t> visited;

    std::function<void(uint64_t)> dfs = [&](uint64_t id) {
        if (visited.count(id)) return;
        visited.insert(id);

        auto it = m_patches.find(id);
        if (it == m_patches.end()) return;

        for (auto depId : it->second.dependencies) {
            dfs(depId);
        }
        order.push_back(id);
    };

    dfs(patchId);
    return order;
}

std::vector<uint64_t> HotpatchControlPlane::detectConflicts(uint64_t patchId) const {
    std::vector<uint64_t> result;
    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return result;

    for (auto conflictId : it->second.conflicts) {
        auto cIt = m_patches.find(conflictId);
        if (cIt != m_patches.end() &&
            cIt->second.state == PatchLifecycleState::Applied) {
            result.push_back(conflictId);
        }
    }
    return result;
}

// ============================================================================
// Validation Pipeline
// ============================================================================
PatchResult HotpatchControlPlane::addValidationRule(ValidationRule& rule) {
    std::lock_guard<std::mutex> lock(m_mutex);

    rule.ruleId = m_nextRuleId.fetch_add(1);
    m_validationRules[rule.ruleId] = rule;

    return PatchResult::ok("Validation rule added");
}

PatchResult HotpatchControlPlane::removeValidationRule(uint64_t ruleId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_validationRules.find(ruleId);
    if (it == m_validationRules.end()) return PatchResult::error("Rule not found", -1);

    m_validationRules.erase(it);
    return PatchResult::ok("Validation rule removed");
}

std::vector<ValidationResult> HotpatchControlPlane::runValidation(uint64_t patchId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ValidationResult> results;

    auto pIt = m_patches.find(patchId);
    if (pIt == m_patches.end()) return results;

    const auto& manifest = pIt->second;

    for (auto& [ruleId, rule] : m_validationRules) {
        if (static_cast<uint8_t>(manifest.safetyLevel) < static_cast<uint8_t>(rule.minLevel)) {
            continue;
        }

        if (rule.validate) {
            char msg[512] = {};
            bool passed = rule.validate(&manifest, msg, sizeof(msg));
            if (passed) {
                results.push_back(ValidationResult::pass(ruleId, rule.name));
            } else {
                results.push_back(ValidationResult::fail(ruleId, rule.name, msg));
            }
        }
    }

    return results;
}

std::vector<ValidationResult> HotpatchControlPlane::runAllValidations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ValidationResult> results;

    for (auto& [patchId, manifest] : m_patches) {
        for (auto& [ruleId, rule] : m_validationRules) {
            if (static_cast<uint8_t>(manifest.safetyLevel) < static_cast<uint8_t>(rule.minLevel)) {
                continue;
            }
            if (rule.validate) {
                char msg[512] = {};
                bool passed = rule.validate(&manifest, msg, sizeof(msg));
                if (passed) {
                    results.push_back(ValidationResult::pass(ruleId, rule.name));
                } else {
                    results.push_back(ValidationResult::fail(ruleId, rule.name, msg));
                }
            }
        }
    }

    return results;
}

// ============================================================================
// Query
// ============================================================================
const PatchManifest* HotpatchControlPlane::getPatch(uint64_t patchId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_patches.find(patchId);
    return (it != m_patches.end()) ? &it->second : nullptr;
}

std::vector<const PatchManifest*> HotpatchControlPlane::getAllPatches() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<const PatchManifest*> result;
    result.reserve(m_patches.size());
    for (auto& [id, p] : m_patches) result.push_back(&p);
    return result;
}

std::vector<const PatchManifest*> HotpatchControlPlane::getActivePatch() const {
    return getPatchesByState(PatchLifecycleState::Applied);
}

std::vector<const PatchManifest*> HotpatchControlPlane::getPatchesByState(PatchLifecycleState state) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<const PatchManifest*> result;
    for (auto& [id, p] : m_patches) {
        if (p.state == state) result.push_back(&p);
    }
    return result;
}

std::vector<const PatchManifest*> HotpatchControlPlane::getPatchesByLayer(PatchLayerTarget layer) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<const PatchManifest*> result;
    for (auto& [id, p] : m_patches) {
        if (hasLayer(p.targetLayers, layer)) result.push_back(&p);
    }
    return result;
}

const PatchTransaction* HotpatchControlPlane::getTransaction(uint64_t txnId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_transactions.find(txnId);
    return (it != m_transactions.end()) ? &it->second : nullptr;
}

// ============================================================================
// Audit Trail
// ============================================================================
std::vector<HotpatchAuditEntry> HotpatchControlPlane::getAuditLog(size_t maxEntries) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<HotpatchAuditEntry> result;
    size_t count = std::min(maxEntries, m_auditLog.size());
    auto startIt = m_auditLog.end() - static_cast<ptrdiff_t>(count);
    result.assign(startIt, m_auditLog.end());
    return result;
}

std::vector<HotpatchAuditEntry> HotpatchControlPlane::getAuditLogForPatch(uint64_t patchId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<HotpatchAuditEntry> result;
    for (auto& entry : m_auditLog) {
        if (entry.patchId == patchId) result.push_back(entry);
    }
    return result;
}

size_t HotpatchControlPlane::auditLogSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_auditLog.size();
}

// ============================================================================
// Version Management
// ============================================================================
PatchResult HotpatchControlPlane::upgradePatch(uint64_t oldPatchId, PatchManifest& newManifest) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto oldIt = m_patches.find(oldPatchId);
    if (oldIt == m_patches.end()) return PatchResult::error("Original patch not found", -1);

    if (!(newManifest.version > oldIt->second.version)) {
        return PatchResult::error("New version must be greater than current", -2);
    }

    // Register as new patch with link to old
    newManifest.patchId     = m_nextPatchId.fetch_add(1);
    newManifest.state       = PatchLifecycleState::Draft;
    newManifest.created     = std::chrono::steady_clock::now();
    newManifest.lastModified = newManifest.created;
    newManifest.rollbackChain.push_back(oldPatchId);

    m_patches[newManifest.patchId] = newManifest;
    m_versionHistory[newManifest.patchId] = m_versionHistory[oldPatchId];
    m_versionHistory[newManifest.patchId].push_back(newManifest.version);

    // Deprecate old patch
    oldIt->second.state = PatchLifecycleState::Deprecated;

    m_stats.totalPatches.fetch_add(1);

    recordAudit(newManifest.patchId, 0, PatchLifecycleState::Draft,
                PatchLifecycleState::Draft, "system",
                "Upgraded from patch " + std::to_string(oldPatchId));

    return PatchResult::ok("Patch upgraded");
}

std::vector<PatchVersion> HotpatchControlPlane::getVersionHistory(uint64_t patchId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_versionHistory.find(patchId);
    if (it == m_versionHistory.end()) return {};
    return it->second;
}

// ============================================================================
// Statistics
// ============================================================================
void HotpatchControlPlane::resetStats() {
    m_stats.totalPatches.store(0);
    m_stats.activePatches.store(0);
    m_stats.totalTransactions.store(0);
    m_stats.committedTransactions.store(0);
    m_stats.rolledBackTransactions.store(0);
    m_stats.validationsPassed.store(0);
    m_stats.validationsFailed.store(0);
    m_stats.conflictsDetected.store(0);
    m_stats.dependencyErrors.store(0);
    m_stats.auditEntries.store(0);
}

// ============================================================================
// Callbacks
// ============================================================================
void HotpatchControlPlane::setPatchStateCallback(PatchStateCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_patchStateCb   = cb;
    m_patchStateData = userData;
}

void HotpatchControlPlane::setTransactionCallback(TransactionCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_txnCb   = cb;
    m_txnData = userData;
}

void HotpatchControlPlane::setConflictCallback(ConflictCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_conflictCb   = cb;
    m_conflictData = userData;
}

// ============================================================================
// Serialization (JSON-manual — no external deps)
// ============================================================================
PatchResult HotpatchControlPlane::exportState(const char* filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream out(filePath);
    if (!out.is_open()) return PatchResult::error("Cannot open file for writing", -1);

    out << "{\n  \"patches\": [\n";
    bool first = true;
    for (auto& [id, p] : m_patches) {
        if (!first) out << ",\n";
        first = false;
        out << "    { \"id\": " << id
            << ", \"name\": \"" << p.name << "\""
            << ", \"version\": \"" << p.version.toString() << "\""
            << ", \"state\": " << static_cast<int>(p.state)
            << ", \"safety\": " << static_cast<int>(p.safetyLevel)
            << ", \"layers\": " << static_cast<int>(p.targetLayers)
            << " }";
    }
    out << "\n  ],\n  \"auditCount\": " << m_auditLog.size() << "\n}\n";
    out.close();

    return PatchResult::ok("State exported");
}

PatchResult HotpatchControlPlane::importState(const char* filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream in(filePath);
    if (!in.is_open()) return PatchResult::error("Cannot open file for reading", -1);

    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();

    // Parse our own JSON format:
    // { "patches": [ { "id": N, "name": "...", "version": "M.m.p.b",
    //                   "state": N, "safety": N, "layers": N } ... ],
    //   "auditCount": N }

    size_t arrStart = content.find("\"patches\"");
    if (arrStart == std::string::npos)
        return PatchResult::error("No patches array found", -2);

    size_t bracketStart = content.find('[', arrStart);
    if (bracketStart == std::string::npos)
        return PatchResult::error("Malformed patches array", -3);

    uint32_t imported = 0;
    size_t searchPos = bracketStart + 1;

    while (searchPos < content.size()) {
        size_t objStart = content.find('{', searchPos);
        if (objStart == std::string::npos) break;

        size_t objEnd = content.find('}', objStart);
        if (objEnd == std::string::npos) break;

        std::string obj = content.substr(objStart, objEnd - objStart + 1);

        // Extract fields from the object
        auto extractInt = [&](const std::string& key) -> int64_t {
            std::string needle = "\"" + key + "\":";
            size_t pos = obj.find(needle);
            if (pos == std::string::npos) { needle = "\"" + key + "\": "; pos = obj.find(needle); }
            if (pos == std::string::npos) return 0;
            size_t vs = pos + needle.size();
            while (vs < obj.size() && obj[vs] == ' ') vs++;
            return std::strtoll(obj.c_str() + vs, nullptr, 10);
        };

        auto extractStr = [&](const std::string& key) -> std::string {
            std::string needle = "\"" + key + "\": \"";
            size_t pos = obj.find(needle);
            if (pos == std::string::npos) { needle = "\"" + key + "\":\"" ; pos = obj.find(needle); }
            if (pos == std::string::npos) return "";
            size_t vs = pos + needle.size();
            size_t ve = obj.find('"', vs);
            if (ve == std::string::npos) return "";
            return obj.substr(vs, ve - vs);
        };

        uint64_t id = static_cast<uint64_t>(extractInt("id"));
        std::string name = extractStr("name");
        std::string versionStr = extractStr("version");
        int state = static_cast<int>(extractInt("state"));
        int safety = static_cast<int>(extractInt("safety"));
        int layers = static_cast<int>(extractInt("layers"));

        if (id > 0 && !name.empty()) {
            // Parse version string (M.m.p or M.m.p.b)
            PatchVersion ver = PatchVersion::make(0, 0, 0, 0);
            uint16_t vparts[4] = {};
            int vcount = 0;
            std::string vStr = versionStr;
            size_t vpos = 0;
            while (vpos < vStr.size() && vcount < 4) {
                size_t dot = vStr.find('.', vpos);
                if (dot == std::string::npos) dot = vStr.size();
                std::string part = vStr.substr(vpos, dot - vpos);
                vparts[vcount++] = static_cast<uint16_t>(std::strtoul(part.c_str(), nullptr, 10));
                vpos = dot + 1;
            }
            ver = PatchVersion::make(vparts[0], vparts[1], vparts[2],
                                     vcount > 3 ? vparts[3] : 0);

            // Check if patch already exists
            auto existing = m_patches.find(id);
            if (existing != m_patches.end()) {
                // Update existing patch state
                existing->second.state = static_cast<PatchLifecycleState>(state);
                existing->second.version = ver;
            } else {
                // Create new manifest
                PatchManifest manifest;
                manifest.patchId = id;
                manifest.name = name;
                manifest.version = ver;
                manifest.state = static_cast<PatchLifecycleState>(state);
                manifest.safetyLevel = static_cast<PatchSafetyLevel>(safety);
                manifest.targetLayers = static_cast<uint8_t>(layers);
                m_patches.emplace(id, std::move(manifest));
            }
            imported++;
        }

        searchPos = objEnd + 1;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "Imported %u patches from %s", imported, filePath);
    return PatchResult::ok(msg);
}

// ============================================================================
// Internal Helpers
// ============================================================================
void HotpatchControlPlane::recordAudit(uint64_t patchId, uint64_t txnId,
    PatchLifecycleState oldState, PatchLifecycleState newState,
    const std::string& actor, const std::string& reason) {
    // Called with lock held
    HotpatchAuditEntry entry;
    entry.entryId       = m_nextAuditId.fetch_add(1);
    entry.patchId       = patchId;
    entry.transactionId = txnId;
    entry.oldState      = oldState;
    entry.newState      = newState;
    entry.actor         = actor;
    entry.reason        = reason;
    entry.timestamp     = std::chrono::steady_clock::now();

    m_auditLog.push_back(entry);
    if (m_auditLog.size() > MAX_AUDIT_ENTRIES) {
        m_auditLog.pop_front();
    }

    m_stats.auditEntries.fetch_add(1);
}

bool HotpatchControlPlane::checkDependencies(uint64_t patchId) const {
    // Called with lock held
    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return false;

    for (auto depId : it->second.dependencies) {
        auto depIt = m_patches.find(depId);
        if (depIt == m_patches.end()) return false;
        if (depIt->second.state != PatchLifecycleState::Applied) return false;
    }
    return true;
}

bool HotpatchControlPlane::checkConflicts(uint64_t patchId) const {
    // Called with lock held — returns true if NO conflicts
    auto it = m_patches.find(patchId);
    if (it == m_patches.end()) return true;

    for (auto conflictId : it->second.conflicts) {
        auto cIt = m_patches.find(conflictId);
        if (cIt != m_patches.end() &&
            cIt->second.state == PatchLifecycleState::Applied) {
            return false; // Conflict found
        }
    }
    return true;
}

bool HotpatchControlPlane::detectCycleFrom(uint64_t patchId,
    std::unordered_set<uint64_t>& visited,
    std::unordered_set<uint64_t>& inStack) const {
    // Called with lock held
    visited.insert(patchId);
    inStack.insert(patchId);

    auto it = m_patches.find(patchId);
    if (it != m_patches.end()) {
        for (auto depId : it->second.dependencies) {
            if (inStack.count(depId)) return true;
            if (!visited.count(depId)) {
                if (detectCycleFrom(depId, visited, inStack)) return true;
            }
        }
    }

    inStack.erase(patchId);
    return false;
}
