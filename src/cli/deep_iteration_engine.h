// ============================================================================
// deep_iteration_engine.h — Beyond Copilot/Cursor: Multi-Pass Audit/Code Cycles
// ============================================================================
//
// Enables a model to run audit→code→audit→code cycles indefinitely without
// losing complexity or slowing down. Supports full model chains and
// complexity-preserving context retention.
//
// Key capabilities:
//   • Configurable iteration count (1–50+)
//   • Model chaining: different models per phase (audit, code, verify)
//   • Full context retention across passes (no degradation)
//   • Convergence detection: stop when no new findings
//   • Complexity scoring: prevent simplification over iterations
//   • Per-iteration budgets: maintain speed across many passes
//   • Structured audit output → actionable coding tasks
//
// Pattern: Structured results, no exceptions, thread-safe.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>
#include <sstream>

class AgenticEngine;
class SubAgentManager;

// ============================================================================
// ENUMS
// ============================================================================

enum class IterationPhase : uint8_t {
    Audit    = 0,  // Analyze code, produce findings
    Code     = 1,  // Generate/apply changes from findings
    Verify   = 2,  // Validate changes
    Converged= 3,  // No more improvements
    Aborted  = 4,  // User or limit stopped
    Error    = 5,
};

enum class AuditSeverity : uint8_t {
    Critical = 0,  // Must fix
    High     = 1,
    Medium   = 2,
    Low      = 3,
    Info     = 4,
};

// ============================================================================
// STRUCTS
// ============================================================================

struct AuditFinding {
    std::string id;
    AuditSeverity severity;
    std::string category;      // e.g., "style", "bug", "perf", "security"
    std::string location;      // file:line or region
    std::string description;
    std::string suggestion;    // Concrete fix suggestion
    std::string codeSnippet;   // Affected code
    float confidence;          // 0.0–1.0
    int iterationDiscovered;   // Which pass found this
};

struct AuditReport {
    int iteration;
    std::vector<AuditFinding> findings;
    int totalCritical;
    int totalHigh;
    int totalMedium;
    int totalLow;
    float complexityScore;     // Preserved/improved complexity
    std::string summary;
    bool converged;            // No new actionable findings
};

struct CodeChange {
    std::string filePath;
    std::string changeType;    // "edit", "insert", "delete", "refactor"
    std::string beforeContent;
    std::string afterContent;
    int startLine;
    int endLine;
    std::string rationale;
    std::string findingIds;    // Comma-separated IDs this addresses
};

struct IterationResult {
    int iteration;
    IterationPhase phase;
    AuditReport auditReport;
    std::vector<CodeChange> changes;
    bool verificationPassed;
    std::string verificationOutput;
    int durationMs;
    float complexityDelta;     // Change in complexity (+ = more complex, - = simplified)
    std::string modelUsed;
};

struct ModelChainConfig {
    std::string auditModel;    // Model for audit phase (empty = default)
    std::string codeModel;     // Model for code phase (empty = default)
    std::string verifyModel;   // Model for verify phase (empty = default)
    bool useSeparateModels() const {
        return !auditModel.empty() || !codeModel.empty() || !verifyModel.empty();
    }
};

struct DeepIterationConfig {
    int maxIterations;             // 1–50+ (0 = unlimited until convergence)
    int minImprovementIterations;  // Require N iterations with improvements before allowing converge
    int convergenceWindow;         // No new findings for N consecutive audits = converge
    float minComplexityPreservation; // Reject changes that drop complexity below this ratio (0.0–1.0)
    int maxTokensPerPhase;         // Budget per phase to avoid slowdown
    int maxContextTokens;          // Max context to retain (summarize beyond this)
    ModelChainConfig modelChain;
    bool preserveFullContext;      // Retain full prior context (vs. summarization)
    bool stopOnFirstConverge;      // Stop as soon as converge detected
    bool verboseLogging;
    bool writeResultToFile;        // Write final code back to targetPath when run completes

    DeepIterationConfig()
        : maxIterations(10)
        , minImprovementIterations(1)
        , convergenceWindow(2)
        , minComplexityPreservation(0.85f)
        , maxTokensPerPhase(4096)
        , maxContextTokens(16384)
        , preserveFullContext(true)
        , stopOnFirstConverge(true)
        , verboseLogging(false)
        , writeResultToFile(false)
    {}
};

struct DeepIterationStats {
    std::atomic<uint64_t> totalIterations{0};
    std::atomic<uint64_t> auditsRun{0};
    std::atomic<uint64_t> codePhasesRun{0};
    std::atomic<uint64_t> verificationsRun{0};
    std::atomic<uint64_t> findingsTotal{0};
    std::atomic<uint64_t> changesApplied{0};
    std::atomic<uint64_t> convergenceCount{0};
    std::atomic<uint64_t> complexityRejects{0};
    std::atomic<uint64_t> totalDurationMs{0};
};

typedef void (*DeepIterationProgressCallback)(int iteration, IterationPhase phase,
    const char* detail, void* userData);
typedef void (*DeepIterationCompleteCallback)(bool success, const char* summary, void* userData);

// ============================================================================
// DeepIterationEngine — Main class
// ============================================================================

class DeepIterationEngine {
public:
    static DeepIterationEngine& instance();

    void setAgenticEngine(AgenticEngine* engine);
    void setSubAgentManager(SubAgentManager* mgr);

    // Inject chat per phase (for model chaining). When set, overrides default.
    using ChatFn = std::function<std::string(const std::string& msg, const std::string& modelHint)>;
    void setChatProvider(ChatFn fn) { m_chatProvider = std::move(fn); }
    void clearChatProvider() { m_chatProvider = nullptr; }

    // ---- Configuration ----
    void setConfig(const DeepIterationConfig& cfg);
    DeepIterationConfig getConfig() const;

    // ---- Execution ----
    // Run full audit→code→audit cycle up to maxIterations.
    bool run(const std::string& targetPath, const std::string& initialPrompt,
        std::string* outFinalCode = nullptr);

    // Run single audit phase.
    AuditReport runAudit(const std::string& code, const std::string& context, int iteration);

    // Run single code phase (apply findings → new code).
    std::vector<CodeChange> runCodePhase(const std::string& code, const AuditReport& report,
        int iteration, std::string* outNewCode = nullptr);

    // Run verification phase.
    bool runVerify(const std::string& code, const std::vector<CodeChange>& changes,
        std::string* outLog = nullptr);

    // ---- Lifecycle ----
    void startAsync(const std::string& targetPath, const std::string& initialPrompt);
    void cancel();
    bool isRunning() const { return m_running.load(); }

    // ---- Statistics ----
    const DeepIterationStats& getStats() const;
    void resetStats();
    std::vector<IterationResult> getIterationHistory(int count = 20) const;

    // ---- Callbacks ----
    void setProgressCallback(DeepIterationProgressCallback cb, void* userData = nullptr);
    void setCompleteCallback(DeepIterationCompleteCallback cb, void* userData = nullptr);

    // ---- Context Management (complexity preservation) ----
    void pushContext(const std::string& key, const std::string& value);
    std::string getContextSummary(int maxTokens = 2048) const;
    void clearContext();

    // ---- Status ----
    std::string getStatusString() const;

    // ---- Last started/finished phase and iteration point (observability) ----
    int getLastStartedIteration() const { return m_lastStartedIteration; }
    IterationPhase getLastStartedPhase() const { return m_lastStartedPhase; }
    int getLastFinishedIteration() const { return m_lastFinishedIteration; }
    IterationPhase getLastFinishedPhase() const { return m_lastFinishedPhase; }

private:
    DeepIterationEngine();
    ~DeepIterationEngine();
    DeepIterationEngine(const DeepIterationEngine&) = delete;
    DeepIterationEngine& operator=(const DeepIterationEngine&) = delete;

    std::string chatForPhase(const std::string& msg, IterationPhase phase);
    float computeComplexityScore(const std::string& code) const;
    bool shouldConverge(const std::vector<AuditReport>& recentAudits) const;
    std::string compressContextForTokens(const std::string& full, int maxTokens) const;
    void emitProgress(int iter, IterationPhase phase, const std::string& detail);

    void asyncRunImpl(const std::string& targetPath, const std::string& initialPrompt);

    mutable std::mutex m_mutex;
    std::atomic<bool> m_running;
    std::atomic<bool> m_cancelRequested;
    std::thread m_asyncThread;

    AgenticEngine* m_engine;
    SubAgentManager* m_subAgentMgr;
    ChatFn m_chatProvider;

    DeepIterationConfig m_config;
    DeepIterationStats m_stats;

    std::deque<IterationResult> m_iterationHistory;
    static constexpr size_t MAX_HISTORY = 100;

    std::deque<std::pair<std::string, std::string>> m_contextStack;
    static constexpr size_t MAX_CONTEXT_ENTRIES = 50;

    DeepIterationProgressCallback m_progressCb;
    void* m_progressUserData;
    DeepIterationCompleteCallback m_completeCb;
    void* m_completeUserData;

    int m_lastStartedIteration = 0;
    IterationPhase m_lastStartedPhase = IterationPhase::Audit;
    int m_lastFinishedIteration = 0;
    IterationPhase m_lastFinishedPhase = IterationPhase::Audit;
};
