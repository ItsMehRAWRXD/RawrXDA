// ============================================================================
// safe_refactor_engine.hpp — Safe-by-Default Bulk Refactor Engine
// ============================================================================
// Architecture: C++20, Win32, no exceptions, no Qt
//
// Provides diff previews + rollback baked into every refactoring operation.
// Every bulk modification goes through:
//   1. Snapshot (atomic backup of all affected files)
//   2. Diff preview (unified diff generated before commit)
//   3. Verification gate (semantic + syntactic checks)
//   4. Approval gate (optional human review)
//   5. Commit or rollback (atomic)
//
// Diff-Aware Verification Gates:
//   - Parse the diff to understand what changed (not just "did files change")
//   - Verify that only intended changes were made (no collateral damage)
//   - Check that no new errors were introduced
//   - Enforced correctness: CRC32 + line-count + symbol preservation checks
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include "../core/model_memory_hotpatch.hpp"

// ============================================================================
// DiffHunk — One change region in a file
// ============================================================================
struct DiffHunk {
    std::string filePath;
    int         oldStartLine;
    int         oldLineCount;
    int         newStartLine;
    int         newLineCount;
    std::vector<std::string> removedLines;
    std::vector<std::string> addedLines;
    std::string context;            // Surrounding unchanged lines for readability
};

// ============================================================================
// DiffReport — Complete change analysis
// ============================================================================
struct DiffReport {
    std::string     refactorId;
    int             filesChanged;
    int             filesAdded;
    int             filesDeleted;
    int             totalLinesAdded;
    int             totalLinesRemoved;
    std::vector<DiffHunk> hunks;
    std::string     unifiedDiff;        // Full unified diff text
    std::string     summary;            // Human-readable summary

    struct FileStats {
        std::string filePath;
        int linesAdded;
        int linesRemoved;
        uint32_t oldCRC32;
        uint32_t newCRC32;
    };
    std::vector<FileStats> perFileStats;
};

// ============================================================================
// VerificationGate — Diff-aware verification checks
// ============================================================================
struct VerificationGateConfig {
    bool            checkSyntax;            // Use compiler to verify syntax
    bool            checkSymbols;           // Verify no symbols were accidentally removed
    bool            checkIncludeGuards;     // Verify include guards preserved
    bool            checkNoNewErrors;       // Verify no new compile errors
    bool            checkLineCountBounds;   // Flag if change is disproportionate
    bool            checkCRC;               // CRC32 file integrity
    bool            checkCollateralDamage;  // Verify only targeted areas changed
    float           maxChangeRatio;         // Max ratio of changed lines to total (0.0-1.0)
    int             maxNewErrors;           // Max tolerated new errors (usually 0)
    std::vector<std::string> protectedSymbols; // Symbols that must survive refactor

    VerificationGateConfig()
        : checkSyntax(true),
          checkSymbols(true),
          checkIncludeGuards(true),
          checkNoNewErrors(true),
          checkLineCountBounds(true),
          checkCRC(true),
          checkCollateralDamage(true),
          maxChangeRatio(0.3f),  // Max 30% of file can change
          maxNewErrors(0) {}
};

// ============================================================================
// VerificationResult — Outcome of a verification gate
// ============================================================================
struct VerificationResult {
    bool            passed;
    std::string     summary;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    std::vector<std::string> symbolsLost;       // Symbols that were accidentally removed
    int             newCompileErrors;
    float           changeRatio;                 // Actual ratio of changed lines

    static VerificationResult pass(const std::string& msg) {
        VerificationResult r;
        r.passed = true;
        r.summary = msg;
        r.newCompileErrors = 0;
        r.changeRatio = 0;
        return r;
    }

    static VerificationResult fail(const std::string& msg) {
        VerificationResult r;
        r.passed = false;
        r.summary = msg;
        r.newCompileErrors = 0;
        r.changeRatio = 0;
        return r;
    }
};

// ============================================================================
// RefactorSnapshot — Atomic backup for rollback
// ============================================================================
struct RefactorSnapshot {
    std::string     snapshotId;
    std::string     refactorName;
    std::chrono::steady_clock::time_point timestamp;

    struct FileBackup {
        std::string     filePath;
        std::vector<uint8_t> content;
        size_t          size;
        uint32_t        crc32;
        int             lineCount;
    };
    std::vector<FileBackup> files;
    bool valid;

    // Pre-computed symbols from the original codebase (for symbol preservation check)
    std::unordered_set<std::string> originalSymbols;
};

// ============================================================================
// RefactorResult — Final outcome of a safe refactor
// ============================================================================
struct RefactorResult {
    std::string         refactorId;
    bool                success;
    bool                rolledBack;
    std::string         detail;
    DiffReport          diff;
    VerificationResult  verification;
    double              totalDurationMs;

    static RefactorResult ok(const std::string& id, const DiffReport& diff,
                              const VerificationResult& vr) {
        RefactorResult r;
        r.refactorId = id;
        r.success = true;
        r.rolledBack = false;
        r.detail = "Refactor applied safely";
        r.diff = diff;
        r.verification = vr;
        r.totalDurationMs = 0;
        return r;
    }

    static RefactorResult fail(const std::string& id, const std::string& reason,
                                bool wasRolledBack = false) {
        RefactorResult r;
        r.refactorId = id;
        r.success = false;
        r.rolledBack = wasRolledBack;
        r.detail = reason;
        r.totalDurationMs = 0;
        return r;
    }
};

// ============================================================================
// Callbacks
// ============================================================================
using DiffPreviewCb     = std::function<bool(const std::string& refactorId,
                                              const DiffReport& diff)>;
using VerificationCb    = std::function<void(const std::string& refactorId,
                                              const VerificationResult& result)>;
using RefactorCompleteCb = std::function<void(const RefactorResult& result)>;

// ============================================================================
// SafeRefactorEngine — Singleton
// ============================================================================
class SafeRefactorEngine {
public:
    static SafeRefactorEngine& instance();

    // ---- Core Operations ----

    /// Execute a safe refactoring operation with full diff/verify/rollback cycle
    /// The `applyFn` callable performs the actual modifications.
    /// It is called between snapshot and verification.
    RefactorResult executeSafeRefactor(
        const std::string& name,
        const std::vector<std::string>& targetFiles,
        const VerificationGateConfig& verifyConfig,
        std::function<bool()> applyFn);

    /// Preview a refactor without committing (dry run)
    DiffReport previewRefactor(
        const std::string& name,
        const std::vector<std::string>& targetFiles,
        std::function<bool()> applyFn);

    /// Rollback the most recent refactor
    PatchResult rollbackLast();

    /// Rollback a specific refactor by ID
    PatchResult rollback(const std::string& refactorId);

    // ---- Verification Gates ----

    /// Run diff-aware verification against a snapshot
    VerificationResult runVerificationGate(
        const RefactorSnapshot& snapshot,
        const VerificationGateConfig& config);

    /// Check if specific symbols still exist in the modified files
    std::vector<std::string> findLostSymbols(
        const RefactorSnapshot& snapshot,
        const std::vector<std::string>& symbolsToCheck);

    // ---- Diff Generation ----

    /// Generate a full diff report between a snapshot and current state
    DiffReport generateDiffReport(const RefactorSnapshot& snapshot);

    /// Compute CRC32 for a file
    static uint32_t computeCRC32(const void* data, size_t len);

    // ---- Snapshot Management ----
    RefactorSnapshot takeSnapshot(const std::string& refactorName,
                                   const std::vector<std::string>& files);
    PatchResult restoreSnapshot(const RefactorSnapshot& snapshot);

    // ---- Callbacks ----
    void setDiffPreviewCallback(DiffPreviewCb cb) { m_diffPreviewCb = cb; }
    void setVerificationCallback(VerificationCb cb) { m_verificationCb = cb; }
    void setRefactorCompleteCallback(RefactorCompleteCb cb) { m_completeCb = cb; }

    // ---- History ----
    std::vector<RefactorResult> getHistory(int maxCount = 50) const;
    void clearHistory();

    // ---- Statistics ----
    struct Stats {
        std::atomic<uint64_t> totalRefactors{0};
        std::atomic<uint64_t> successfulRefactors{0};
        std::atomic<uint64_t> rolledBackRefactors{0};
        std::atomic<uint64_t> verificationFailures{0};
        std::atomic<uint64_t> symbolLossDetections{0};
    };
    const Stats& getStats() const { return m_stats; }
    void resetStats();

private:
    SafeRefactorEngine();
    ~SafeRefactorEngine() = default;
    SafeRefactorEngine(const SafeRefactorEngine&) = delete;
    SafeRefactorEngine& operator=(const SafeRefactorEngine&) = delete;

    std::string generateRefactorId() const;
    int countLines(const std::vector<uint8_t>& content) const;
    std::unordered_set<std::string> extractSymbols(const std::string& filePath) const;

    mutable std::mutex m_mutex;

    // Snapshot stack for nested rollback
    std::vector<std::pair<std::string, RefactorSnapshot>> m_snapshotStack;

    // History of completed refactors
    std::vector<RefactorResult> m_history;
    static constexpr size_t kMaxHistory = 100;

    Stats m_stats;

    // Callbacks
    DiffPreviewCb       m_diffPreviewCb;
    VerificationCb      m_verificationCb;
    RefactorCompleteCb  m_completeCb;
};
