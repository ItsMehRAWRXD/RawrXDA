// =============================================================================
// autonomous_verification_loop.hpp
// RawrXD v14.2.1 Cathedral — Phase A: Closed-Loop Verification
//
// Implements the Write → Compile → Test → Fix retry loop.
// Agents generate code and this system AUTONOMOUSLY verifies correctness
// by compiling, running tests, interpreting failures, and auto-fixing.
//
// Pattern: PatchResult factories, std::mutex+lock_guard, singleton instance()
// No exceptions. No STL allocators in hot path.
// =============================================================================
#pragma once
#ifndef RAWRXD_AUTONOMOUS_VERIFICATION_LOOP_HPP
#define RAWRXD_AUTONOMOUS_VERIFICATION_LOOP_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <chrono>

namespace RawrXD {
namespace Autonomy {

// =============================================================================
// Result Types (factory-static pattern — never brace-initialize)
// =============================================================================

struct VerificationResult {
    bool   success;
    const char* detail;
    int    errorCode;
    int    retryCount;
    double durationMs;

    static VerificationResult ok(const char* msg, int retries = 0, double ms = 0.0) {
        VerificationResult r;
        r.success    = true;
        r.detail     = msg;
        r.errorCode  = 0;
        r.retryCount = retries;
        r.durationMs = ms;
        return r;
    }

    static VerificationResult error(const char* msg, int code = -1, int retries = 0) {
        VerificationResult r;
        r.success    = false;
        r.detail     = msg;
        r.errorCode  = code;
        r.retryCount = retries;
        r.durationMs = 0.0;
        return r;
    }
};

struct CompileResult {
    bool   compiled;
    const char* rawOutput;
    int    errorCount;
    int    warningCount;

    struct ErrorEntry {
        char file[260];
        int  line;
        int  col;
        char message[512];
        char errorId[64];
    };

    ErrorEntry errors[64];

    static CompileResult ok(const char* output, int warnings = 0) {
        CompileResult r;
        r.compiled     = true;
        r.rawOutput    = output;
        r.errorCount   = 0;
        r.warningCount = warnings;
        return r;
    }

    static CompileResult fail(const char* output, int errs) {
        CompileResult r;
        r.compiled     = false;
        r.rawOutput    = output;
        r.errorCount   = errs;
        r.warningCount = 0;
        return r;
    }
};

struct TestResult {
    bool   allPassed;
    int    totalTests;
    int    passedTests;
    int    failedTests;
    int    skippedTests;
    const char* rawOutput;

    struct TestFailure {
        char testName[256];
        char expected[512];
        char actual[512];
        char file[260];
        int  line;
    };

    TestFailure failures[32];

    static TestResult ok(int total, int passed, const char* output) {
        TestResult r;
        r.allPassed    = true;
        r.totalTests   = total;
        r.passedTests  = passed;
        r.failedTests  = 0;
        r.skippedTests = total - passed;
        r.rawOutput    = output;
        return r;
    }

    static TestResult fail(int total, int passed, int failed, const char* output) {
        TestResult r;
        r.allPassed    = false;
        r.totalTests   = total;
        r.passedTests  = passed;
        r.failedTests  = failed;
        r.skippedTests = total - passed - failed;
        r.rawOutput    = output;
        return r;
    }
};

struct LintResult {
    bool   clean;
    int    issueCount;
    int    autoFixed;
    const char* rawOutput;

    static LintResult ok(const char* output) {
        LintResult r;
        r.clean      = true;
        r.issueCount = 0;
        r.autoFixed  = 0;
        r.rawOutput  = output;
        return r;
    }

    static LintResult issues(int count, int fixed, const char* output) {
        LintResult r;
        r.clean      = (count == fixed);
        r.issueCount = count;
        r.autoFixed  = fixed;
        r.rawOutput  = output;
        return r;
    }
};

// =============================================================================
// Enums
// =============================================================================

enum class VerificationStage : uint8_t {
    Idle,
    Generating,
    Compiling,
    Fixing,
    Testing,
    Linting,
    Formatting,
    Committing,
    Complete,
    Failed,
    Aborted
};

enum class FixStrategy : uint8_t {
    LLMRewrite,      // Ask LLM to fix based on error messages
    PatternMatch,    // Known error → known fix mapping
    Rollback,        // Revert to last working version
    Incremental,     // Fix one error at a time
    Bisect           // Binary search for the breaking change
};

// =============================================================================
// Callbacks (C function pointers for hot path)
// =============================================================================

typedef void (*VerificationStageCallback)(VerificationStage stage, const char* detail, void* userData);
typedef void (*CompileErrorCallback)(const CompileResult::ErrorEntry* error, void* userData);
typedef void (*TestFailureCallback)(const TestResult::TestFailure* failure, void* userData);
typedef bool (*FixApprovalCallback)(const char* originalCode, const char* fixedCode, const char* reason, void* userData);

// =============================================================================
// Configuration
// =============================================================================

struct VerificationPolicy {
    int          maxRetries;         // Default: 5
    int          maxFixAttempts;     // Default: 3
    double       compileTimeoutMs;   // Default: 120000 (2 min)
    double       testTimeoutMs;      // Default: 300000 (5 min)
    bool         autoCommitOnPass;   // Default: false
    bool         autoLint;           // Default: true
    bool         autoFormat;         // Default: true
    bool         rollbackOnFailure;  // Default: true
    FixStrategy  primaryStrategy;    // Default: LLMRewrite
    FixStrategy  fallbackStrategy;   // Default: PatternMatch
    const char*  buildTarget;        // Default: "RawrXD-Shell"
    const char*  testTarget;         // Default: "self_test_gate"
    const char*  lintCommand;        // Default: nullptr (disabled)
    const char*  formatCommand;      // Default: nullptr (disabled)

    static VerificationPolicy defaults() {
        VerificationPolicy p;
        p.maxRetries        = 5;
        p.maxFixAttempts    = 3;
        p.compileTimeoutMs  = 120000.0;
        p.testTimeoutMs     = 300000.0;
        p.autoCommitOnPass  = false;
        p.autoLint          = true;
        p.autoFormat        = true;
        p.rollbackOnFailure = true;
        p.primaryStrategy   = FixStrategy::LLMRewrite;
        p.fallbackStrategy  = FixStrategy::PatternMatch;
        p.buildTarget       = "RawrXD-Shell";
        p.testTarget        = "self_test_gate";
        p.lintCommand       = nullptr;
        p.formatCommand     = nullptr;
        return p;
    }
};

// =============================================================================
// Error → Fix Mapping (pattern-based auto-repair)
// =============================================================================

struct ErrorFixPattern {
    char errorPattern[256];     // Regex or substring to match in error message
    char fixTemplate[1024];     // Code transformation template
    int  confidence;            // 0-100 confidence this fix is correct
    int  appliedCount;          // How many times this pattern has been used
    int  successCount;          // How many times this fix worked
};

// =============================================================================
// Verification Task
// =============================================================================

struct VerificationTask {
    uint64_t        taskId;
    char            description[512];
    char            targetFile[260];
    std::string     generatedCode;
    std::string     originalCode;        // Snapshot for rollback
    VerificationStage currentStage;
    int             retryCount;
    int             fixAttemptCount;
    double          totalDurationMs;

    CompileResult   lastCompile;
    TestResult      lastTest;
    LintResult      lastLint;
    VerificationResult  finalResult;

    std::chrono::steady_clock::time_point startTime;
};

// =============================================================================
// Core Class: AutonomousVerifier
// =============================================================================

class AutonomousVerifier {
public:
    static AutonomousVerifier& instance();

    // ── Lifecycle ──────────────────────────────────────────────────────────
    void initialize();
    void shutdown();
    bool isRunning() const { return m_running.load(); }

    // ── Policy ─────────────────────────────────────────────────────────────
    void setPolicy(const VerificationPolicy& policy);
    VerificationPolicy getPolicy() const;

    // ── Core Verification Loop ─────────────────────────────────────────────
    // Generates code for a task, then autonomously verifies it.
    // Returns only after the loop completes (success or exhausted retries).
    VerificationResult generateAndVerify(const char* taskDescription,
                                         const char* targetFile);

    // Verify already-generated code (no generation step)
    VerificationResult verifyCode(const char* code,
                                  const char* targetFile);

    // Async variants
    uint64_t generateAndVerifyAsync(const char* taskDescription,
                                     const char* targetFile);
    uint64_t verifyCodeAsync(const char* code,
                             const char* targetFile);

    // ── Query ──────────────────────────────────────────────────────────────
    VerificationStage getTaskStage(uint64_t taskId) const;
    VerificationResult getTaskResult(uint64_t taskId) const;
    bool abortTask(uint64_t taskId);

    // ── Sub-system Injection ───────────────────────────────────────────────
    // Code generator (LLM bridge)
    typedef std::string (*CodeGeneratorFn)(const char* task, const char* context, void* userData);
    void setCodeGenerator(CodeGeneratorFn fn, void* userData);

    // Compiler bridge
    typedef CompileResult (*CompilerFn)(const char* file, const char* buildTarget, void* userData);
    void setCompiler(CompilerFn fn, void* userData);

    // Test runner bridge
    typedef TestResult (*TestRunnerFn)(const char* testTarget, void* userData);
    void setTestRunner(TestRunnerFn fn, void* userData);

    // LLM-based error fixer
    typedef std::string (*ErrorFixerFn)(const char* code, const CompileResult* errors,
                                        const TestResult* testFailures, void* userData);
    void setErrorFixer(ErrorFixerFn fn, void* userData);

    // Linter / formatter
    typedef LintResult (*LinterFn)(const char* file, void* userData);
    void setLinter(LinterFn fn, void* userData);

    typedef bool (*FormatterFn)(const char* file, void* userData);
    void setFormatter(FormatterFn fn, void* userData);

    // VCS commit
    typedef bool (*CommitFn)(const char* file, const char* message, void* userData);
    void setCommitter(CommitFn fn, void* userData);

    // ── Callbacks ──────────────────────────────────────────────────────────
    void setStageCallback(VerificationStageCallback cb, void* userData);
    void setCompileErrorCallback(CompileErrorCallback cb, void* userData);
    void setTestFailureCallback(TestFailureCallback cb, void* userData);
    void setFixApprovalCallback(FixApprovalCallback cb, void* userData);

    // ── Error Pattern Database ─────────────────────────────────────────────
    void addErrorFixPattern(const ErrorFixPattern& pattern);
    int  getPatternCount() const;
    void clearPatterns();

    // ── Statistics ──────────────────────────────────────────────────────────
    struct Stats {
        uint64_t totalTasks;
        uint64_t successfulTasks;
        uint64_t failedTasks;
        uint64_t totalRetries;
        uint64_t totalFixAttempts;
        uint64_t patternsApplied;
        uint64_t llmFixesApplied;
        double   avgRetriesPerTask;
        double   avgDurationMs;
        double   successRate;      // 0.0 – 1.0
    };

    Stats getStats() const;
    void  resetStats();

    // ── JSON Serialization ─────────────────────────────────────────────────
    std::string statsToJson() const;
    std::string taskToJson(uint64_t taskId) const;
    std::string patternsToJson() const;

private:
    AutonomousVerifier();
    ~AutonomousVerifier();
    AutonomousVerifier(const AutonomousVerifier&) = delete;
    AutonomousVerifier& operator=(const AutonomousVerifier&) = delete;

    // Internal loop steps
    CompileResult  compileStep(VerificationTask& task);
    TestResult     testStep(VerificationTask& task);
    LintResult     lintStep(VerificationTask& task);
    bool           formatStep(VerificationTask& task);
    std::string    generateCode(const char* taskDescription, const char* context);
    std::string    fixCode(VerificationTask& task);
    std::string    applyPatternFix(const char* code, const CompileResult& errors);
    bool           commitCode(VerificationTask& task);
    void           rollback(VerificationTask& task);
    void           emitStage(VerificationStage stage, const char* detail);

    // ── State ──────────────────────────────────────────────────────────────
    mutable std::mutex m_mutex;
    std::atomic<bool>  m_running{false};
    std::atomic<uint64_t> m_nextTaskId{1};

    VerificationPolicy m_policy;
    std::unordered_map<uint64_t, VerificationTask> m_tasks;
    std::vector<ErrorFixPattern> m_patterns;

    // ── Injected Subsystems ────────────────────────────────────────────────
    CodeGeneratorFn m_codeGenerator;     void* m_codeGenUserData;
    CompilerFn      m_compiler;          void* m_compilerUserData;
    TestRunnerFn    m_testRunner;        void* m_testRunnerUserData;
    ErrorFixerFn    m_errorFixer;        void* m_errorFixerUserData;
    LinterFn        m_linter;            void* m_linterUserData;
    FormatterFn     m_formatter;         void* m_formatterUserData;
    CommitFn        m_committer;         void* m_committerUserData;

    // ── Callbacks ──────────────────────────────────────────────────────────
    VerificationStageCallback m_stageCb;       void* m_stageCbData;
    CompileErrorCallback      m_compileErrCb;  void* m_compileErrCbData;
    TestFailureCallback       m_testFailCb;    void* m_testFailCbData;
    FixApprovalCallback       m_fixApprovalCb; void* m_fixApprovalCbData;

    // ── Telemetry ──────────────────────────────────────────────────────────
    alignas(64) Stats m_stats;
};

} // namespace Autonomy
} // namespace RawrXD

#endif // RAWRXD_AUTONOMOUS_VERIFICATION_LOOP_HPP
