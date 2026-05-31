// ============================================================================
// system_integrity_audit_trail.h — Persistent Audit Logging for Integrity Checks
// ============================================================================
// Persistent audit trail for System Integrity Prover results, with EventBus
// integration for real-time event propagation.
//
// Features:
//   - Persistent RocksDB-backed audit log (if available)
//   - In-memory fallback circular buffer for immediate queries
//   - EventBus emits IntegrityAuditEvent for external subscribers
//   - Thread-safe lock-free reads, atomic writes
//   - Structured JSON events for compliance/forensics
//
// Threading:
//   - Async appends to backing store (non-blocking)
//   - Lock-free queries over in-memory entries
//   - EventBus is thread-safe
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>
#include <memory>
#include <functional>
#include <atomic>

namespace rawrxd {

// ============================================================================
// IntegrityAuditEvent — audit log entry
// ============================================================================
struct IntegrityAuditEvent {
    // Timestamp (milliseconds since epoch)
    uint64_t timestampMs;

    // Operation context (e.g., "startup", "before_critical_op", "periodic_check")
    std::string context;

    // 5-layer verification results (bitmask)
    //   bit 0: physical layer
    //   bit 1: logic layer
    //   bit 2: security layer
    //   bit 3: persistence layer
    //   bit 4: visibility layer
    uint8_t layerResults;

    // Overall pass/fail
    bool passed;

    // Summary message
    std::string message;

    // Sequence number (for linearization)
    uint64_t seqNo;

    // Convert to JSON string for storage/transmission
    std::string toJSON() const;

    // Parse from JSON string
    static IntegrityAuditEvent fromJSON(const std::string& json);
};

// ============================================================================
// IntegrityAuditTrail — Persistent Audit Log Manager
// ============================================================================
class IntegrityAuditTrail {
public:
    static IntegrityAuditTrail& Instance() {
        static IntegrityAuditTrail instance;
        return instance;
    }

    // Initialize audit trail (creates backing store if needed)
    bool initialize(const std::string& storePath = "");

    // Record an integrity check result
    void recordCheck(const IntegrityAuditEvent& event);

    // Query recent audit entries (most recent first)
    std::vector<IntegrityAuditEvent> getRecentEntries(size_t count = 100) const;

    // Get all entries passing a predicate filter
    std::vector<IntegrityAuditEvent> getEntriesWhere(
        std::function<bool(const IntegrityAuditEvent&)> predicate) const;

    // Get last N failed checks
    std::vector<IntegrityAuditEvent> getFailedChecks(size_t count = 50) const;

    // Get entries within time range [startMs, endMs]
    std::vector<IntegrityAuditEvent> getEntriesSince(uint64_t untilMs) const;

    // Export audit log to JSON file (for forensics/compliance)
    bool exportToFile(const std::string& filepath) const;

    // Get total number of recorded entries
    uint64_t totalEntries() const { return m_seqNo.load(std::memory_order_acquire); }

    // Get statistics
    struct Stats {
        uint64_t total_checks;
        uint64_t total_passed;
        uint64_t total_failed;
        uint64_t failed_physical;
        uint64_t failed_logic;
        uint64_t failed_security;
        uint64_t failed_persistence;
        uint64_t failed_visibility;
    };
    Stats getStats() const;

    // Clear all entries (for testing)
    void clear();

private:
    IntegrityAuditTrail() = default;

    std::atomic<uint64_t> m_seqNo{0};
    mutable std::mutex m_mu;

    // In-memory circular buffer (fallback and fast path)
    static constexpr size_t MAX_BUFFER_SIZE = 1000;
    std::deque<IntegrityAuditEvent> m_buffer;

    // Backing store path (empty = in-memory only)
    std::string m_storePath;
    std::atomic<bool> m_initialized{false};

    // Async append to backing store
    void appendToBacking(const IntegrityAuditEvent& event);
};

// ============================================================================
// IntegrityProverWithAudit — Enhanced System Integrity Prover with Auditing
// ============================================================================
class IntegrityProverWithAudit {
public:
    static IntegrityProverWithAudit& Instance() {
        static IntegrityProverWithAudit instance;
        return instance;
    }

    // Initialize with audit trail backing store
    bool initialize(const std::string& auditStorePath = "");

    // Enhanced RunBeforeCriticalOp with automatic audit logging
    bool runBeforeCriticalOpWithAudit(const std::string& opName,
                                       uint8_t& outLayerResults);

    // Enhanced startup verification with audit logging
    bool runOnStartupWithAudit(bool force = false);

    // Get audit trail for this prover
    IntegrityAuditTrail& getAuditTrail() { return IntegrityAuditTrail::Instance(); }

    // Register callbacks that fire on integrity events (EventBus style)
    using IntegrityEventCallback = std::function<void(const IntegrityAuditEvent&)>;
    void registerAuditCallback(IntegrityEventCallback cb) {
        std::lock_guard<std::mutex> lk(m_cbMutex);
        m_callbacks.push_back(std::move(cb));
    }

private:
    IntegrityProverWithAudit() = default;

    std::mutex m_cbMutex;
    std::vector<IntegrityEventCallback> m_callbacks;

    void fireCallbacks(const IntegrityAuditEvent& event) {
        std::lock_guard<std::mutex> lk(m_cbMutex);
        for (auto& cb : m_callbacks) {
            cb(event);
        }
    }

    // Helper to build audit event from layer results
    IntegrityAuditEvent buildAuditEvent(const std::string& context,
                                        uint8_t layerResults,
                                        bool passed,
                                        const std::string& message) const;
};

}  // namespace rawrxd
