// ============================================================================
// autonomous_recovery_orchestrator.hpp — T4: Source-Level Self-Hosting
// ============================================================================
//
// Closes the recovery loop:
//   DivergenceDetected(FailureClass)
//     → SymbolizeAddress()           (PDB GSI hash → file:line)
//     → SelectStrategy()             (FailureClass → RecoveryStrategy)
//     → ExecuteRecovery()            (hotpatch / rollback / source edit)
//     → DeterministicReplayEngine    (verify fix)
//     → GitCommit()                  (permanentize)
//
// This is the bridge from T3 (observability) to T4 (autonomy).
// Without this, the agent can detect divergence but cannot self-heal.
//
// Integration Points:
//   - DeterministicReplayEngine  (DivergenceEvent, FailureClass, TelemetrySnapshot)
//   - HotpatchTelemetrySafety    (safePatch, safeBatch, rollback)
//   - AgentSelfRepair            (function-level redirect, CRC verify)
//   - GSIHashTable / PDB         (address → source symbolization)
//   - RawrXD_SourceEdit_Kernel   (MASM64: atomic file replace + git commit)
//
// Architecture: C++20, no exceptions, PatchResult pattern.
// Threading: mutex-protected, all recovery strategies are serialized.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>

#include "DeterministicReplayEngine.h"

// Forward declarations — avoid circular includes
namespace RawrXD { namespace Safety {
    class HotpatchTelemetrySafety;
    struct SafePatchResult;
} }

namespace RawrXD { namespace PDB {
    class GSIHashTable;
} }

class AgentSelfRepair;
struct PatchResult;
struct LivePatchUnit;

namespace RawrXD {
namespace Agent {

// ============================================================================
// SourceLocation — Result of PDB symbolization (addr → file:line:function)
// ============================================================================
struct SourceLocation {
    std::string filePath;       // e.g. "src/agentic/agent_self_repair.cpp"
    std::string functionName;   // e.g. "AgentSelfRepair::scanSelf"
    int         lineNumber = 0; // Source line number (1-based)
    uint32_t    rva        = 0; // Relative virtual address in PE
    bool        valid      = false;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["file"]     = filePath;
        j["function"] = functionName;
        j["line"]     = lineNumber;
        j["rva"]      = rva;
        j["valid"]    = valid;
        return j;
    }
};

// ============================================================================
// RecoveryStrategy — What the orchestrator will attempt for each FailureClass
// ============================================================================
enum class RecoveryStrategy : uint8_t {
    None                = 0,    // No recovery (Unknown classification)
    HotpatchRedirect    = 1,    // Redirect function to known-good fallback
    BatchRollback       = 2,    // Rollback all recent hotpatches
    ReduceBatchSize     = 3,    // OOM mitigation: shrink inference batch
    FreezeHotpatching   = 4,    // HotpatchCascade: disable patching for cooldown
    SourceEdit          = 5,    // T4: LLM-generate fix → atomic replace → verify
    SwapKVToDisk        = 6,    // OOM mitigation: evict KV-cache to NanoDisk
    FullRollbackAndHalt = 7     // Critical: revert everything, stop execution
};

inline const char* RecoveryStrategyString(RecoveryStrategy s) {
    switch (s) {
        case RecoveryStrategy::HotpatchRedirect:    return "hotpatch_redirect";
        case RecoveryStrategy::BatchRollback:       return "batch_rollback";
        case RecoveryStrategy::ReduceBatchSize:     return "reduce_batch_size";
        case RecoveryStrategy::FreezeHotpatching:   return "freeze_hotpatching";
        case RecoveryStrategy::SourceEdit:          return "source_edit";
        case RecoveryStrategy::SwapKVToDisk:        return "swap_kv_to_disk";
        case RecoveryStrategy::FullRollbackAndHalt: return "full_rollback_and_halt";
        default:                                     return "none";
    }
}

// ============================================================================
// RecoveryResult — Outcome of an autonomous recovery attempt
// ============================================================================
struct RecoveryResult {
    bool                success;
    const char*         detail;
    int                 errorCode;

    RecoveryStrategy    strategy;
    FailureClass        failureClass;
    SourceLocation      sourceLocation;     // Symbolized addr (if applicable)
    TelemetrySnapshot   preRecovery;        // Counter state before recovery
    TelemetrySnapshot   postRecovery;       // Counter state after recovery
    bool                verifyPassed;       // DeterministicReplayEngine re-verify
    bool                gitCommitted;       // Source fix committed to repo
    std::string         gitCommitHash;      // SHA if committed
    std::string         generatedFix;       // LLM-generated source patch (if any)
    uint64_t            timestampMs;
    uint64_t            recoveryDurationMs;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["success"]             = success;
        j["detail"]              = detail ? detail : "";
        j["error_code"]          = errorCode;
        j["strategy"]            = RecoveryStrategyString(strategy);
        j["failure_class"]       = FailureClassString(failureClass);
        if (sourceLocation.valid)
            j["source_location"] = sourceLocation.toJson();
        if (preRecovery.valid)
            j["pre_recovery"]    = preRecovery.toJson();
        if (postRecovery.valid)
            j["post_recovery"]   = postRecovery.toJson();
        j["verify_passed"]       = verifyPassed;
        j["git_committed"]       = gitCommitted;
        if (!gitCommitHash.empty())
            j["git_commit"]      = gitCommitHash;
        j["timestamp_ms"]        = timestampMs;
        j["recovery_duration_ms"]= recoveryDurationMs;
        return j;
    }

    static RecoveryResult ok(const char* msg, RecoveryStrategy strat) {
        RecoveryResult r{};
        r.success       = true;
        r.detail        = msg;
        r.errorCode     = 0;
        r.strategy      = strat;
        r.verifyPassed  = false;
        r.gitCommitted  = false;
        return r;
    }

    static RecoveryResult error(const char* msg, int code = -1) {
        RecoveryResult r{};
        r.success       = false;
        r.detail        = msg;
        r.errorCode     = code;
        r.strategy      = RecoveryStrategy::None;
        r.verifyPassed  = false;
        r.gitCommitted  = false;
        return r;
    }
};

// ============================================================================
// RecoveryEvent — Logged to the recovery journal for audit
// ============================================================================
struct RecoveryEvent {
    uint64_t            timestampMs;
    FailureClass        failureClass;
    RecoveryStrategy    strategyAttempted;
    bool                success;
    std::string         detail;
    SourceLocation      location;
    std::string         commitHash;
};

// ============================================================================
// Cooldown tracking — prevents recovery storms
// ============================================================================
struct RecoveryCooldown {
    uint64_t            hotpatchFreezeUntilMs    = 0;  // HotpatchCascade cooldown
    uint64_t            lastRecoveryMs           = 0;
    uint32_t            consecutiveFailures      = 0;
    uint32_t            maxConsecutiveFailures    = 5;  // Halt after this many
    uint64_t            minRecoveryIntervalMs    = 2000; // Min 2s between recoveries
};

// ============================================================================
// RecoveryConfig — Tunable parameters
// ============================================================================
struct RecoveryConfig {
    bool        enableSourceEdit        = false; // T4: LLM-driven source fixes
    bool        enableGitCommit         = false; // Auto-commit source fixes
    bool        enableBinaryHotpatch    = true;  // Binary-level hotpatch
    bool        enableBatchRollback     = true;  // Rollback multiple patches
    bool        enableFunctionRedirect  = true;  // AgentSelfRepair redirects
    bool        verifyAfterRecovery     = true;  // Re-run replay engine
    int         hotpatchCooldownSec     = 60;    // Freeze hotpatching duration
    int         maxBatchRollbackAgeMs   = 30000; // Rollback patches < 30s old
    int         reducedBatchSize        = 1;     // OOM: reduce to this
    std::string pdbPath;                          // Path to .pdb for symbolization
    std::string repoRoot;                         // Git repo root for source edits
    std::string gitExecutable = "git.exe";        // Path to git binary
    std::string llmEndpoint;                      // Ollama/model endpoint for fix gen
    std::string llmModel = "qwen2.5-coder:7b";   // Model for code generation
};

// ============================================================================
// LLM Fix Generation callback — pluggable LLM integration
// ============================================================================
// Instead of std::function in the hot path, use a function pointer + context.
// The callback receives source context and divergence info, returns a fix.
// ============================================================================
struct LLMFixRequest {
    const char*         sourceFile;     // File with the bug
    int                 lineNumber;     // Line of the divergence
    const char*         functionName;   // Enclosing function
    const char*         sourceContext;  // Surrounding source lines
    const DivergenceEvent* divergence;  // Full divergence context
};

struct LLMFixResponse {
    bool        success;
    std::string replacement;    // Replacement source code
    std::string explanation;    // LLM's reasoning
    float       confidence;     // 0.0 - 1.0
};

using LLMFixGenerator = LLMFixResponse(*)(const LLMFixRequest& req, void* userData);

// ============================================================================
// AutonomousRecoveryOrchestrator — The T4 Self-Healing Bridge
// ============================================================================
//
// State machine per recovery attempt:
//
//   ┌─────────┐    ┌────────────┐    ┌──────────┐    ┌────────┐
//   │ Classify │───→│ Symbolize  │───→│ Recover  │───→│ Verify │
//   └─────────┘    └────────────┘    └──────────┘    └────────┘
//       │               │                │                │
//       │               │                │                ▼
//       │               │                │          ┌──────────┐
//       │               │                └─────────→│  Commit  │
//       │               │                           └──────────┘
//       ▼               ▼                                ▼
//   ┌──────────────────────────────────────────────────────────┐
//   │                   RecoveryJournal                         │
//   └──────────────────────────────────────────────────────────┘
//
// ============================================================================
class AutonomousRecoveryOrchestrator {
public:
    static AutonomousRecoveryOrchestrator& instance();

    // ---- Lifecycle ----
    void initialize(const RecoveryConfig& config);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(); }

    // ---- Configuration ----
    void setConfig(const RecoveryConfig& config);
    const RecoveryConfig& getConfig() const;

    // ---- Core: Execute full recovery pipeline ----
    //
    // This is the main entry point. Given a DivergenceEvent (with
    // FailureClass already classified by T3-C), it:
    //   1. Selects a RecoveryStrategy based on FailureClass
    //   2. Symbolizes the divergence address (if applicable)
    //   3. Executes the recovery strategy
    //   4. Verifies the fix via DeterministicReplayEngine (if configured)
    //   5. Commits to git (if source edit and configured)
    //
    RecoveryResult executeRecovery(const DivergenceEvent& div);

    // ---- Strategy selection ----
    RecoveryStrategy selectStrategy(FailureClass fc) const;

    // ---- Individual recovery strategies ----
    RecoveryResult recoverLogicDrift(const DivergenceEvent& div);
    RecoveryResult recoverOOM(const TelemetrySnapshot& snap);
    RecoveryResult recoverTimeout(const DivergenceEvent& div);
    RecoveryResult recoverDiskPressure(const TelemetrySnapshot& snap);
    RecoveryResult recoverGpuStall(const TelemetrySnapshot& snap);
    RecoveryResult recoverHotpatchCascade(const DivergenceEvent& div);

    // ---- Source symbolization (PDB GSI) ----
    SourceLocation symbolizeAddress(uintptr_t addr);
    bool loadPDB(const std::string& pdbPath);
    bool isPDBLoaded() const { return m_pdbLoaded.load(); }

    // ---- Source-level editing (T4 exclusive) ----
    //
    // Reads source file, generates LLM fix, applies atomic replace,
    // re-verifies via replay, and commits to git.
    //
    RecoveryResult applySourceFix(const SourceLocation& loc,
                                   const DivergenceEvent& div);

    // ---- LLM integration ----
    void setLLMFixGenerator(LLMFixGenerator generator, void* userData);

    // ---- Git operations ----
    bool gitAdd(const std::string& filePath);
    bool gitCommit(const std::string& message);
    std::string gitGetCommitHash();

    // ---- Verification ----
    bool verifyWithReplayEngine(const ReplayConfig& config);

    // ---- Journal / History ----
    const std::vector<RecoveryEvent>& getJournal() const;
    size_t getSuccessCount() const { return m_successCount.load(); }
    size_t getFailureCount() const { return m_failureCount.load(); }
    size_t getTotalAttempts() const { return m_totalAttempts.load(); }

    // ---- Cooldown queries ----
    bool isHotpatchFrozen() const;
    bool isCoolingDown() const;
    uint64_t getTimeUntilNextRecoveryMs() const;

    // ---- Report ----
    nlohmann::json generateReport() const;

private:
    AutonomousRecoveryOrchestrator();
    ~AutonomousRecoveryOrchestrator();
    AutonomousRecoveryOrchestrator(const AutonomousRecoveryOrchestrator&) = delete;
    AutonomousRecoveryOrchestrator& operator=(const AutonomousRecoveryOrchestrator&) = delete;

    // ---- Internal helpers ----
    TelemetrySnapshot captureSnapshot() const;
    bool checkCooldown();
    void recordEvent(const RecoveryEvent& event);
    void updateCooldown(bool success);

    // ---- Source file I/O ----
    std::string readSourceFile(const std::string& path);
    std::string extractSourceContext(const std::string& source, int line, int radius = 15);
    bool atomicWriteSourceFile(const std::string& path, const std::string& content);

    // ---- Process execution (git, compiler) ----
    int executeProcess(const std::string& command, std::string* stdoutCapture = nullptr,
                        int timeoutMs = 30000);

    // ---- State ----
    mutable std::mutex          m_mutex;
    std::atomic<bool>           m_initialized{false};
    std::atomic<bool>           m_pdbLoaded{false};
    RecoveryConfig              m_config;
    RecoveryCooldown            m_cooldown;
    std::vector<RecoveryEvent>  m_journal;

    std::atomic<uint64_t>       m_successCount{0};
    std::atomic<uint64_t>       m_failureCount{0};
    std::atomic<uint64_t>       m_totalAttempts{0};

    // PDB integration (lazy-loaded)
    RawrXD::PDB::GSIHashTable*  m_gsiTable = nullptr;
    uint8_t*                    m_pdbSymbolStream = nullptr;
    uint32_t                    m_pdbSymbolStreamSize = 0;

    // LLM fix generator (pluggable)
    LLMFixGenerator             m_llmGenerator = nullptr;
    void*                       m_llmUserData  = nullptr;
};

// ============================================================================
// MASM64 Source Edit Kernel — Extern declarations
// ============================================================================
// Defined in RawrXD_SourceEdit_Kernel.asm
// Provides atomic file replacement with backup for safe source modification.
// ============================================================================
#ifdef RAWR_HAS_MASM
extern "C" {
    // Atomic source file replace:
    //   1. CreateFile(backup path) → write old content
    //   2. CreateFile(temp path)   → write new content
    //   3. MoveFileEx(original → temp.old, MOVEFILE_REPLACE_EXISTING)
    //   4. MoveFileEx(temp → original, MOVEFILE_REPLACE_EXISTING)
    //   5. DeleteFile(temp.old)
    //
    // Returns: 0 on success, GetLastError() on failure.
    // On failure, original file is guaranteed untouched (transactional).
    uint64_t SourceEdit_AtomicReplace(
        const wchar_t*  originalPath,       // Source file to modify
        const wchar_t*  backupPath,         // Where to write backup
        const void*     newContent,         // New file content bytes
        uint64_t        newContentLen       // Length of new content
    );

    // Execute git command via CreateProcess:
    //   Launches git.exe with given args, waits for completion.
    //   Captures stdout into provided buffer.
    //
    // Returns: process exit code (0 = success), -1 on launch failure.
    int64_t SourceEdit_GitCommand(
        const wchar_t*  gitExePath,         // Path to git.exe
        const wchar_t*  workingDir,         // Repo root
        const wchar_t*  args,               // Command line args (e.g. L"add src/foo.cpp")
        wchar_t*        stdoutBuf,          // Output buffer (nullable)
        uint64_t        stdoutBufLen        // Buffer size in wchar_t
    );
}
#endif

} // namespace Agent
} // namespace RawrXD
