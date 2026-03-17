// hotpatch_control_plane.hpp — Phase 14: Advanced Hotpatch Control Plane
// Versioned patch management with dependency graphs, atomic multi-layer
// transactions, rollback chains, validation pipelines, and safety verification.
//
// Architecture: Extends the three-layer hotpatch system with enterprise-grade
//               lifecycle management. Coordinates Memory, Byte-Level, and
//               Server layers through atomic transaction boundaries.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured PatchResult returns only.
// Rule: All threading via std::mutex + std::lock_guard. No recursive locks.
#pragma once

#ifndef RAWRXD_HOTPATCH_CONTROL_PLANE_HPP
#define RAWRXD_HOTPATCH_CONTROL_PLANE_HPP

#include "model_memory_hotpatch.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

// ============================================================================
// Patch Version Identifier — Semantic version for each patch
// ============================================================================
struct PatchVersion {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    uint32_t build;     // Monotonic build number

    bool operator==(const PatchVersion& o) const {
        return major == o.major && minor == o.minor &&
               patch == o.patch && build == o.build;
    }

    bool operator<(const PatchVersion& o) const {
        if (major != o.major) return major < o.major;
        if (minor != o.minor) return minor < o.minor;
        if (patch != o.patch) return patch < o.patch;
        return build < o.build;
    }

    bool operator>(const PatchVersion& o) const { return o < *this; }
    bool operator<=(const PatchVersion& o) const { return !(o < *this); }
    bool operator>=(const PatchVersion& o) const { return !(*this < o); }

    static PatchVersion make(uint16_t maj, uint16_t min, uint16_t pat, uint32_t bld = 0) {
        return { maj, min, pat, bld };
    }

    // String representation: "1.2.3+42"
    std::string toString() const {
        char buf[64];
        snprintf(buf, sizeof(buf), "%u.%u.%u+%u", major, minor, patch, build);
        return std::string(buf);
    }
};

// ============================================================================
// Patch Layer Target — Which layer(s) this patch targets
// ============================================================================
enum class PatchLayerTarget : uint8_t {
    Memory      = 0x01,
    ByteLevel   = 0x02,
    Server      = 0x04,
    All         = 0x07
};

inline uint8_t operator|(PatchLayerTarget a, PatchLayerTarget b) {
    return static_cast<uint8_t>(a) | static_cast<uint8_t>(b);
}

inline bool hasLayer(uint8_t flags, PatchLayerTarget layer) {
    return (flags & static_cast<uint8_t>(layer)) != 0;
}

// ============================================================================
// Patch Lifecycle State
// ============================================================================
enum class PatchLifecycleState : uint8_t {
    Draft       = 0,    // Created, not yet validated
    Validated   = 1,    // Passed validation pipeline
    Staged      = 2,    // Ready for application
    Applied     = 3,    // Currently active
    Suspended   = 4,    // Temporarily deactivated
    RolledBack  = 5,    // Reverted to previous state
    Deprecated  = 6,    // Marked for removal
    Archived    = 7     // Permanently stored, not active
};

// ============================================================================
// Patch Safety Level
// ============================================================================
enum class PatchSafetyLevel : uint8_t {
    Safe        = 0,    // No risk — metadata-only changes
    Low         = 1,    // Parameter adjustments
    Medium      = 2,    // Behavioral changes
    High        = 3,    // Memory layout changes
    Critical    = 4     // Requires downtime or quiesce
};

// ============================================================================
// Patch Manifest — Complete description of a patch
// ============================================================================
struct PatchManifest {
    uint64_t            patchId;
    std::string         name;
    std::string         description;
    std::string         author;
    PatchVersion        version;
    PatchLifecycleState state;
    PatchSafetyLevel    safetyLevel;
    uint8_t             targetLayers;       // Bitmask of PatchLayerTarget
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point lastModified;

    // Dependencies — other patch IDs that must be applied first
    std::vector<uint64_t> dependencies;

    // Conflicts — other patch IDs that cannot coexist
    std::vector<uint64_t> conflicts;

    // Minimum version requirements
    PatchVersion        minCompatVersion;
    PatchVersion        maxCompatVersion;

    // Rollback chain — ordered list of patches to revert to reach this state
    std::vector<uint64_t> rollbackChain;

    // Validation results
    bool                validated;
    std::string         validationReport;

    // Layer-specific payloads (opaque)
    void*               memoryPayload;
    size_t              memoryPayloadSize;
    void*               bytePayload;
    size_t              bytePayloadSize;
    void*               serverPayload;
    size_t              serverPayloadSize;

    PatchManifest()
        : patchId(0), state(PatchLifecycleState::Draft)
        , safetyLevel(PatchSafetyLevel::Low), targetLayers(0)
        , validated(false)
        , memoryPayload(nullptr), memoryPayloadSize(0)
        , bytePayload(nullptr), bytePayloadSize(0)
        , serverPayload(nullptr), serverPayloadSize(0) {}
};

// ============================================================================
// Transaction — Atomic multi-patch operation
// ============================================================================
struct PatchTransaction {
    uint64_t            transactionId;
    std::string         description;
    std::vector<uint64_t> patchIds;         // Patches in this transaction
    bool                committed;
    bool                rolledBack;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;

    // Savepoints for partial rollback
    struct Savepoint {
        uint64_t    savepointId;
        size_t      patchIndex;     // Index in patchIds up to which we've applied
        std::string label;
    };
    std::vector<Savepoint> savepoints;

    PatchTransaction()
        : transactionId(0), committed(false), rolledBack(false) {}
};

// ============================================================================
// Validation Rule — Pluggable patch validation
// ============================================================================
struct ValidationRule {
    uint64_t    ruleId;
    std::string name;
    std::string description;
    bool      (*validate)(const PatchManifest* manifest, char* outMsg, size_t msgLen);
    PatchSafetyLevel minLevel;  // Only apply to patches at or above this level

    ValidationRule()
        : ruleId(0), validate(nullptr), minLevel(PatchSafetyLevel::Safe) {}
};

// ============================================================================
// Validation Result
// ============================================================================
struct ValidationResult {
    bool        passed;
    uint64_t    ruleId;
    std::string ruleName;
    std::string message;

    static ValidationResult pass(uint64_t id, const std::string& name) {
        return { true, id, name, "Passed" };
    }

    static ValidationResult fail(uint64_t id, const std::string& name, const std::string& msg) {
        return { false, id, name, msg };
    }
};

// ============================================================================
// Hotpatch Audit Entry
// ============================================================================
struct HotpatchAuditEntry {
    uint64_t                                  entryId;
    uint64_t                                  patchId;
    uint64_t                                  transactionId;
    PatchLifecycleState                       oldState;
    PatchLifecycleState                       newState;
    std::string                               actor;       // Who initiated
    std::string                               reason;
    std::chrono::steady_clock::time_point     timestamp;
};

// ============================================================================
// Control Plane Statistics
// ============================================================================
struct ControlPlaneStats {
    std::atomic<uint64_t> totalPatches{0};
    std::atomic<uint64_t> activePatches{0};
    std::atomic<uint64_t> totalTransactions{0};
    std::atomic<uint64_t> committedTransactions{0};
    std::atomic<uint64_t> rolledBackTransactions{0};
    std::atomic<uint64_t> validationsPassed{0};
    std::atomic<uint64_t> validationsFailed{0};
    std::atomic<uint64_t> conflictsDetected{0};
    std::atomic<uint64_t> dependencyErrors{0};
    std::atomic<uint64_t> auditEntries{0};
};

// ============================================================================
// Callbacks
// ============================================================================
using PatchStateCallback    = void(*)(uint64_t patchId, PatchLifecycleState newState, void* userData);
using TransactionCallback   = void(*)(uint64_t txnId, bool committed, void* userData);
using ConflictCallback      = void(*)(uint64_t patchA, uint64_t patchB, void* userData);

// ============================================================================
// HotpatchControlPlane — Main class
// ============================================================================
class HotpatchControlPlane {
public:
    static HotpatchControlPlane& instance();

    // ── Patch Registration ──
    PatchResult registerPatch(PatchManifest& manifest);
    PatchResult unregisterPatch(uint64_t patchId);
    PatchResult updatePatchMetadata(uint64_t patchId, const std::string& name,
                                     const std::string& desc, const std::string& author);

    // ── Lifecycle Management ──
    PatchResult validatePatch(uint64_t patchId);
    PatchResult stagePatch(uint64_t patchId);
    PatchResult applyPatch(uint64_t patchId, const std::string& actor, const std::string& reason);
    PatchResult suspendPatch(uint64_t patchId, const std::string& actor, const std::string& reason);
    PatchResult rollbackPatch(uint64_t patchId, const std::string& actor, const std::string& reason);
    PatchResult deprecatePatch(uint64_t patchId);
    PatchResult archivePatch(uint64_t patchId);

    // ── Transaction Support ──
    uint64_t beginTransaction(const std::string& description);
    PatchResult addToTransaction(uint64_t txnId, uint64_t patchId);
    PatchResult commitTransaction(uint64_t txnId, const std::string& actor);
    PatchResult rollbackTransaction(uint64_t txnId, const std::string& actor);
    PatchResult createSavepoint(uint64_t txnId, const std::string& label);
    PatchResult rollbackToSavepoint(uint64_t txnId, uint64_t savepointId);

    // ── Dependency & Conflict Management ──
    PatchResult addDependency(uint64_t patchId, uint64_t dependsOn);
    PatchResult removeDependency(uint64_t patchId, uint64_t dependsOn);
    PatchResult addConflict(uint64_t patchA, uint64_t patchB);
    PatchResult removeConflict(uint64_t patchA, uint64_t patchB);
    bool hasDependencyCycle() const;
    std::vector<uint64_t> resolveDependencyOrder(uint64_t patchId) const;
    std::vector<uint64_t> detectConflicts(uint64_t patchId) const;

    // ── Validation Pipeline ──
    PatchResult addValidationRule(ValidationRule& rule);
    PatchResult removeValidationRule(uint64_t ruleId);
    std::vector<ValidationResult> runValidation(uint64_t patchId) const;
    std::vector<ValidationResult> runAllValidations() const;

    // ── Query ──
    const PatchManifest* getPatch(uint64_t patchId) const;
    std::vector<const PatchManifest*> getAllPatches() const;
    std::vector<const PatchManifest*> getActivePatch() const;
    std::vector<const PatchManifest*> getPatchesByState(PatchLifecycleState state) const;
    std::vector<const PatchManifest*> getPatchesByLayer(PatchLayerTarget layer) const;
    const PatchTransaction* getTransaction(uint64_t txnId) const;

    // ── Audit Trail ──
    std::vector<HotpatchAuditEntry> getAuditLog(size_t maxEntries = 1000) const;
    std::vector<HotpatchAuditEntry> getAuditLogForPatch(uint64_t patchId) const;
    size_t auditLogSize() const;

    // ── Version Management ──
    PatchResult upgradePatch(uint64_t oldPatchId, PatchManifest& newManifest);
    std::vector<PatchVersion> getVersionHistory(uint64_t patchId) const;

    // ── Statistics ──
    const ControlPlaneStats& getStats() const { return m_stats; }
    void resetStats();

    // ── Callbacks ──
    void setPatchStateCallback(PatchStateCallback cb, void* userData);
    void setTransactionCallback(TransactionCallback cb, void* userData);
    void setConflictCallback(ConflictCallback cb, void* userData);

    // ── Serialization ──
    PatchResult exportState(const char* filePath) const;
    PatchResult importState(const char* filePath);

private:
    HotpatchControlPlane();
    ~HotpatchControlPlane();
    HotpatchControlPlane(const HotpatchControlPlane&) = delete;
    HotpatchControlPlane& operator=(const HotpatchControlPlane&) = delete;

    // Internal helpers
    void recordAudit(uint64_t patchId, uint64_t txnId,
                     PatchLifecycleState oldState, PatchLifecycleState newState,
                     const std::string& actor, const std::string& reason);
    bool checkDependencies(uint64_t patchId) const;
    bool checkConflicts(uint64_t patchId) const;
    bool detectCycleFrom(uint64_t patchId, std::unordered_set<uint64_t>& visited,
                         std::unordered_set<uint64_t>& inStack) const;

    // State
    mutable std::mutex                                  m_mutex;
    std::atomic<uint64_t>                               m_nextPatchId{1};
    std::atomic<uint64_t>                               m_nextTxnId{1};
    std::atomic<uint64_t>                               m_nextRuleId{1};
    std::atomic<uint64_t>                               m_nextAuditId{1};
    std::atomic<uint64_t>                               m_nextSavepointId{1};

    // Patch registry
    std::unordered_map<uint64_t, PatchManifest>         m_patches;

    // Transactions
    std::unordered_map<uint64_t, PatchTransaction>      m_transactions;

    // Validation rules
    std::unordered_map<uint64_t, ValidationRule>        m_validationRules;

    // Audit log (bounded ring buffer)
    std::deque<HotpatchAuditEntry>                      m_auditLog;
    static constexpr size_t MAX_AUDIT_ENTRIES = 10000;

    // Version history tracking
    std::unordered_map<uint64_t, std::vector<PatchVersion>> m_versionHistory;

    // Callbacks
    PatchStateCallback  m_patchStateCb      = nullptr;
    void*               m_patchStateData    = nullptr;
    TransactionCallback m_txnCb             = nullptr;
    void*               m_txnData           = nullptr;
    ConflictCallback    m_conflictCb        = nullptr;
    void*               m_conflictData      = nullptr;

    // Statistics
    ControlPlaneStats   m_stats;
};

#endif // RAWRXD_HOTPATCH_CONTROL_PLANE_HPP
