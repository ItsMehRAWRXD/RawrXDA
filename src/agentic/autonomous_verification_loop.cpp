// =============================================================================
// autonomous_verification_loop.cpp
// RawrXD v14.2.1 Cathedral — Phase A: Closed-Loop Verification
//
// The core autonomy loop: Generate → Compile → Fix → Test → Lint → Commit
// Retries up to policy.maxRetries, tracks all telemetry, supports rollback.
// =============================================================================

#include "autonomous_verification_loop.hpp"

#include <thread>
#include <algorithm>
#include <cstdio>

namespace RawrXD {
namespace Autonomy {

// =============================================================================
// Singleton
// =============================================================================

AutonomousVerifier& AutonomousVerifier::instance() {
    static AutonomousVerifier s_instance;
    return s_instance;
}

AutonomousVerifier::AutonomousVerifier()
    : m_codeGenerator(nullptr), m_codeGenUserData(nullptr)
    , m_compiler(nullptr),      m_compilerUserData(nullptr)
    , m_testRunner(nullptr),    m_testRunnerUserData(nullptr)
    , m_errorFixer(nullptr),    m_errorFixerUserData(nullptr)
    , m_linter(nullptr),        m_linterUserData(nullptr)
    , m_formatter(nullptr),     m_formatterUserData(nullptr)
    , m_committer(nullptr),     m_committerUserData(nullptr)
    , m_stageCb(nullptr),       m_stageCbData(nullptr)
    , m_compileErrCb(nullptr),  m_compileErrCbData(nullptr)
    , m_testFailCb(nullptr),    m_testFailCbData(nullptr)
    , m_fixApprovalCb(nullptr), m_fixApprovalCbData(nullptr)
{
    m_policy = VerificationPolicy::defaults();
    std::memset(&m_stats, 0, sizeof(m_stats));
}

AutonomousVerifier::~AutonomousVerifier() {
    shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

void AutonomousVerifier::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running.store(true);
}

void AutonomousVerifier::shutdown() {
    m_running.store(false);
}

// =============================================================================
// Policy
// =============================================================================

void AutonomousVerifier::setPolicy(const VerificationPolicy& policy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_policy = policy;
}

VerificationPolicy AutonomousVerifier::getPolicy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_policy;
}

// =============================================================================
// Sub-system Injection
// =============================================================================

void AutonomousVerifier::setCodeGenerator(CodeGeneratorFn fn, void* ud)  { m_codeGenerator = fn; m_codeGenUserData = ud; }
void AutonomousVerifier::setCompiler(CompilerFn fn, void* ud)            { m_compiler = fn; m_compilerUserData = ud; }
void AutonomousVerifier::setTestRunner(TestRunnerFn fn, void* ud)        { m_testRunner = fn; m_testRunnerUserData = ud; }
void AutonomousVerifier::setErrorFixer(ErrorFixerFn fn, void* ud)        { m_errorFixer = fn; m_errorFixerUserData = ud; }
void AutonomousVerifier::setLinter(LinterFn fn, void* ud)                { m_linter = fn; m_linterUserData = ud; }
void AutonomousVerifier::setFormatter(FormatterFn fn, void* ud)          { m_formatter = fn; m_formatterUserData = ud; }
void AutonomousVerifier::setCommitter(CommitFn fn, void* ud)             { m_committer = fn; m_committerUserData = ud; }

void AutonomousVerifier::setStageCallback(VerificationStageCallback cb, void* ud)   { m_stageCb = cb; m_stageCbData = ud; }
void AutonomousVerifier::setCompileErrorCallback(CompileErrorCallback cb, void* ud) { m_compileErrCb = cb; m_compileErrCbData = ud; }
void AutonomousVerifier::setTestFailureCallback(TestFailureCallback cb, void* ud)   { m_testFailCb = cb; m_testFailCbData = ud; }
void AutonomousVerifier::setFixApprovalCallback(FixApprovalCallback cb, void* ud)   { m_fixApprovalCb = cb; m_fixApprovalCbData = ud; }

// =============================================================================
// Error Pattern Database
// =============================================================================

void AutonomousVerifier::addErrorFixPattern(const ErrorFixPattern& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_patterns.push_back(pattern);
}

int AutonomousVerifier::getPatternCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_patterns.size());
}

void AutonomousVerifier::clearPatterns() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_patterns.clear();
}

// =============================================================================
// Internal Helpers
// =============================================================================

void AutonomousVerifier::emitStage(VerificationStage stage, const char* detail) {
    if (m_stageCb) {
        m_stageCb(stage, detail, m_stageCbData);
    }
}

std::string AutonomousVerifier::generateCode(const char* taskDescription, const char* context) {
    if (!m_codeGenerator) {
        return "";
    }
    return m_codeGenerator(taskDescription, context, m_codeGenUserData);
}

CompileResult AutonomousVerifier::compileStep(VerificationTask& task) {
    if (!m_compiler) {
        return CompileResult::ok("No compiler configured — skipping");
    }
    return m_compiler(task.targetFile, m_policy.buildTarget, m_compilerUserData);
}

TestResult AutonomousVerifier::testStep(VerificationTask& task) {
    if (!m_testRunner) {
        return TestResult::ok(0, 0, "No test runner configured — skipping");
    }
    return m_testRunner(m_policy.testTarget, m_testRunnerUserData);
}

LintResult AutonomousVerifier::lintStep(VerificationTask& task) {
    if (!m_linter) {
        return LintResult::ok("No linter configured — skipping");
    }
    return m_linter(task.targetFile, m_linterUserData);
}

bool AutonomousVerifier::formatStep(VerificationTask& task) {
    if (!m_formatter) return true;
    return m_formatter(task.targetFile, m_formatterUserData);
}

std::string AutonomousVerifier::fixCode(VerificationTask& task) {
    // Try pattern-based fix first
    std::string patternFix = applyPatternFix(task.generatedCode.c_str(), task.lastCompile);
    if (!patternFix.empty()) {
        m_stats.patternsApplied++;
        return patternFix;
    }

    // Fall back to LLM-based fix
    if (m_errorFixer) {
        std::string fixed = m_errorFixer(
            task.generatedCode.c_str(),
            &task.lastCompile,
            &task.lastTest,
            m_errorFixerUserData
        );
        if (!fixed.empty()) {
            m_stats.llmFixesApplied++;
            return fixed;
        }
    }

    return "";
}

std::string AutonomousVerifier::applyPatternFix(const char* code, const CompileResult& errors) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (int e = 0; e < errors.errorCount && e < 64; e++) {
        const char* errMsg = errors.errors[e].message;

        for (auto& pattern : m_patterns) {
            // Simple substring match (could be upgraded to regex)
            if (std::strstr(errMsg, pattern.errorPattern) != nullptr) {
                pattern.appliedCount++;
                // Return the fix template — caller applies it
                return std::string(pattern.fixTemplate);
            }
        }
    }

    return "";
}

bool AutonomousVerifier::commitCode(VerificationTask& task) {
    if (!m_committer) return false;

    char msg[512];
    std::snprintf(msg, sizeof(msg),
        "[autonomous] Verified %s (retries=%d, duration=%.1fms)",
        task.description, task.retryCount, task.totalDurationMs);

    return m_committer(task.targetFile, msg, m_committerUserData);
}

void AutonomousVerifier::rollback(VerificationTask& task) {
    if (task.originalCode.empty()) return;

    // Write original code back to disk
    FILE* f = std::fopen(task.targetFile, "wb");
    if (f) {
        std::fwrite(task.originalCode.c_str(), 1, task.originalCode.size(), f);
        std::fclose(f);
    }

    task.currentStage = VerificationStage::Failed;
    emitStage(VerificationStage::Failed, "Rolled back to original");
}

// =============================================================================
// Core Verification Loop
// =============================================================================

VerificationResult AutonomousVerifier::generateAndVerify(
    const char* taskDescription,
    const char* targetFile)
{
    if (!m_running.load()) {
        return VerificationResult::error("Verifier not running", -1);
    }

    // ── Create task ────────────────────────────────────────────────────────
    uint64_t id = m_nextTaskId.fetch_add(1);
    VerificationTask task;
    task.taskId           = id;
    task.retryCount       = 0;
    task.fixAttemptCount  = 0;
    task.totalDurationMs  = 0.0;
    task.currentStage     = VerificationStage::Idle;
    task.startTime        = std::chrono::steady_clock::now();
    std::strncpy(task.description, taskDescription, sizeof(task.description) - 1);
    task.description[sizeof(task.description) - 1] = '\0';
    std::strncpy(task.targetFile, targetFile, sizeof(task.targetFile) - 1);
    task.targetFile[sizeof(task.targetFile) - 1] = '\0';

    // ── Snapshot original file for rollback ─────────────────────────────────
    {
        FILE* f = std::fopen(targetFile, "rb");
        if (f) {
            std::fseek(f, 0, SEEK_END);
            long sz = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (sz > 0) {
                task.originalCode.resize(static_cast<size_t>(sz));
                std::fread(&task.originalCode[0], 1, static_cast<size_t>(sz), f);
            }
            std::fclose(f);
        }
    }

    // ── Generate code ──────────────────────────────────────────────────────
    emitStage(VerificationStage::Generating, taskDescription);
    task.currentStage = VerificationStage::Generating;

    std::string code = generateCode(taskDescription, task.originalCode.c_str());
    if (code.empty()) {
        task.currentStage = VerificationStage::Failed;
        return VerificationResult::error("Code generation returned empty", -2);
    }
    task.generatedCode = code;

    // ── Write generated code to disk ───────────────────────────────────────
    {
        FILE* f = std::fopen(targetFile, "wb");
        if (!f) {
            return VerificationResult::error("Cannot write to target file", -3);
        }
        std::fwrite(code.c_str(), 1, code.size(), f);
        std::fclose(f);
    }

    // ── Verification retry loop ────────────────────────────────────────────
    VerificationPolicy pol;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pol = m_policy;
    }

    for (int attempt = 0; attempt <= pol.maxRetries; attempt++) {
        task.retryCount = attempt;

        // ── Step 1: Compile ────────────────────────────────────────────────
        emitStage(VerificationStage::Compiling, "Compiling generated code");
        task.currentStage = VerificationStage::Compiling;

        CompileResult cr = compileStep(task);
        task.lastCompile = cr;

        if (!cr.compiled) {
            // Emit individual errors
            if (m_compileErrCb) {
                for (int i = 0; i < cr.errorCount && i < 64; i++) {
                    m_compileErrCb(&cr.errors[i], m_compileErrCbData);
                }
            }

            // ── Try to fix ─────────────────────────────────────────────────
            if (task.fixAttemptCount < pol.maxFixAttempts) {
                emitStage(VerificationStage::Fixing, "Attempting auto-fix");
                task.currentStage = VerificationStage::Fixing;
                task.fixAttemptCount++;

                std::string fixed = fixCode(task);
                if (!fixed.empty()) {
                    // Check fix approval if callback set
                    bool approved = true;
                    if (m_fixApprovalCb) {
                        approved = m_fixApprovalCb(
                            task.generatedCode.c_str(),
                            fixed.c_str(),
                            "Auto-fix for compile errors",
                            m_fixApprovalCbData
                        );
                    }

                    if (approved) {
                        task.generatedCode = fixed;
                        // Write fixed code
                        FILE* f = std::fopen(targetFile, "wb");
                        if (f) {
                            std::fwrite(fixed.c_str(), 1, fixed.size(), f);
                            std::fclose(f);
                        }
                        continue; // Retry compilation
                    }
                }
            }

            // Exhausted fixes
            if (pol.rollbackOnFailure) {
                rollback(task);
            }

            auto elapsed = std::chrono::steady_clock::now() - task.startTime;
            task.totalDurationMs = std::chrono::duration<double, std::milli>(elapsed).count();

            m_stats.totalTasks++;
            m_stats.failedTasks++;
            m_stats.totalRetries += static_cast<uint64_t>(attempt);
            m_stats.totalFixAttempts += static_cast<uint64_t>(task.fixAttemptCount);

            task.finalResult = VerificationResult::error(
                "Compilation failed after all fix attempts",
                cr.errorCount, attempt
            );

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_tasks[id] = task;
            }

            return task.finalResult;
        }

        // ── Step 2: Test ───────────────────────────────────────────────────
        emitStage(VerificationStage::Testing, "Running test suite");
        task.currentStage = VerificationStage::Testing;

        TestResult tr = testStep(task);
        task.lastTest = tr;

        if (!tr.allPassed && tr.failedTests > 0) {
            // Emit failures
            if (m_testFailCb) {
                for (int i = 0; i < tr.failedTests && i < 32; i++) {
                    m_testFailCb(&tr.failures[i], m_testFailCbData);
                }
            }

            // Try to fix test failures
            if (task.fixAttemptCount < pol.maxFixAttempts) {
                emitStage(VerificationStage::Fixing, "Fixing test failures");
                task.currentStage = VerificationStage::Fixing;
                task.fixAttemptCount++;

                std::string fixed = fixCode(task);
                if (!fixed.empty()) {
                    task.generatedCode = fixed;
                    FILE* f = std::fopen(targetFile, "wb");
                    if (f) {
                        std::fwrite(fixed.c_str(), 1, fixed.size(), f);
                        std::fclose(f);
                    }
                    continue; // Retry from compile
                }
            }

            // Exhausted fixes on test failures
            if (pol.rollbackOnFailure) {
                rollback(task);
            }

            auto elapsed = std::chrono::steady_clock::now() - task.startTime;
            task.totalDurationMs = std::chrono::duration<double, std::milli>(elapsed).count();

            m_stats.totalTasks++;
            m_stats.failedTasks++;
            m_stats.totalRetries += static_cast<uint64_t>(attempt);

            task.finalResult = VerificationResult::error(
                "Tests failed after all fix attempts",
                tr.failedTests, attempt
            );

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_tasks[id] = task;
            }

            return task.finalResult;
        }

        // ── Step 3: Lint ───────────────────────────────────────────────────
        if (pol.autoLint) {
            emitStage(VerificationStage::Linting, "Running linter");
            task.currentStage = VerificationStage::Linting;
            task.lastLint = lintStep(task);
        }

        // ── Step 4: Format ─────────────────────────────────────────────────
        if (pol.autoFormat) {
            emitStage(VerificationStage::Formatting, "Auto-formatting");
            task.currentStage = VerificationStage::Formatting;
            formatStep(task);
        }

        // ── Step 5: Commit ─────────────────────────────────────────────────
        if (pol.autoCommitOnPass) {
            emitStage(VerificationStage::Committing, "Auto-committing");
            task.currentStage = VerificationStage::Committing;
            commitCode(task);
        }

        // ── Success ────────────────────────────────────────────────────────
        auto elapsed = std::chrono::steady_clock::now() - task.startTime;
        task.totalDurationMs = std::chrono::duration<double, std::milli>(elapsed).count();
        task.currentStage = VerificationStage::Complete;

        m_stats.totalTasks++;
        m_stats.successfulTasks++;
        m_stats.totalRetries += static_cast<uint64_t>(attempt);
        m_stats.totalFixAttempts += static_cast<uint64_t>(task.fixAttemptCount);

        // Update rolling averages
        if (m_stats.totalTasks > 0) {
            m_stats.avgRetriesPerTask = static_cast<double>(m_stats.totalRetries) /
                                        static_cast<double>(m_stats.totalTasks);
            m_stats.avgDurationMs     = (m_stats.avgDurationMs * (m_stats.totalTasks - 1) +
                                         task.totalDurationMs) / m_stats.totalTasks;
            m_stats.successRate       = static_cast<double>(m_stats.successfulTasks) /
                                        static_cast<double>(m_stats.totalTasks);
        }

        task.finalResult = VerificationResult::ok(
            "Verification passed",
            attempt,
            task.totalDurationMs
        );

        emitStage(VerificationStage::Complete, "All checks passed");

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_tasks[id] = task;
        }

        return task.finalResult;
    }

    // Should not reach here, but safety
    return VerificationResult::error("Retry loop exhausted", -99);
}

VerificationResult AutonomousVerifier::verifyCode(
    const char* code,
    const char* targetFile)
{
    // Write code, then run the verification loop without generation
    FILE* f = std::fopen(targetFile, "wb");
    if (!f) {
        return VerificationResult::error("Cannot write target file", -3);
    }
    std::fwrite(code, 1, std::strlen(code), f);
    std::fclose(f);

    // Reuse the generate path with a no-op description
    // Temporarily override generator to return the code as-is
    auto savedGen = m_codeGenerator;
    auto savedData = m_codeGenUserData;

    struct PassthroughState { const char* code; };
    PassthroughState state{code};

    m_codeGenerator = [](const char*, const char*, void* ud) -> std::string {
        auto* s = static_cast<PassthroughState*>(ud);
        return std::string(s->code);
    };
    m_codeGenUserData = &state;

    auto result = generateAndVerify("Verify provided code", targetFile);

    m_codeGenerator   = savedGen;
    m_codeGenUserData = savedData;

    return result;
}

// =============================================================================
// Async Variants
// =============================================================================

uint64_t AutonomousVerifier::generateAndVerifyAsync(
    const char* taskDescription,
    const char* targetFile)
{
    uint64_t id = m_nextTaskId.load(); // Will be assigned in the loop

    std::string desc(taskDescription);
    std::string file(targetFile);

    std::thread([this, desc, file]() {
        generateAndVerify(desc.c_str(), file.c_str());
    }).detach();

    return id;
}

uint64_t AutonomousVerifier::verifyCodeAsync(
    const char* code,
    const char* targetFile)
{
    uint64_t id = m_nextTaskId.load();

    std::string codeStr(code);
    std::string file(targetFile);

    std::thread([this, codeStr, file]() {
        verifyCode(codeStr.c_str(), file.c_str());
    }).detach();

    return id;
}

// =============================================================================
// Query
// =============================================================================

VerificationStage AutonomousVerifier::getTaskStage(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) return VerificationStage::Idle;
    return it->second.currentStage;
}

VerificationResult AutonomousVerifier::getTaskResult(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) {
        return VerificationResult::error("Task not found", -404);
    }
    return it->second.finalResult;
}

bool AutonomousVerifier::abortTask(uint64_t taskId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) return false;
    it->second.currentStage = VerificationStage::Aborted;
    return true;
}

// =============================================================================
// Statistics
// =============================================================================

AutonomousVerifier::Stats AutonomousVerifier::getStats() const {
    // Cache-line aligned, direct read
    return m_stats;
}

void AutonomousVerifier::resetStats() {
    std::memset(&m_stats, 0, sizeof(m_stats));
}

// =============================================================================
// JSON Serialization
// =============================================================================

std::string AutonomousVerifier::statsToJson() const {
    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        R"({"totalTasks":%llu,"successfulTasks":%llu,"failedTasks":%llu,)"
        R"("totalRetries":%llu,"totalFixAttempts":%llu,)"
        R"("patternsApplied":%llu,"llmFixesApplied":%llu,)"
        R"("avgRetriesPerTask":%.2f,"avgDurationMs":%.2f,"successRate":%.4f})",
        (unsigned long long)m_stats.totalTasks,
        (unsigned long long)m_stats.successfulTasks,
        (unsigned long long)m_stats.failedTasks,
        (unsigned long long)m_stats.totalRetries,
        (unsigned long long)m_stats.totalFixAttempts,
        (unsigned long long)m_stats.patternsApplied,
        (unsigned long long)m_stats.llmFixesApplied,
        m_stats.avgRetriesPerTask,
        m_stats.avgDurationMs,
        m_stats.successRate
    );
    return std::string(buf);
}

std::string AutonomousVerifier::taskToJson(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) return R"({"error":"task not found"})";

    const auto& t = it->second;
    char buf[2048];
    std::snprintf(buf, sizeof(buf),
        R"({"taskId":%llu,"description":"%s","targetFile":"%s",)"
        R"("stage":%d,"retryCount":%d,"fixAttempts":%d,)"
        R"("durationMs":%.2f,"compiled":%s,"allTestsPassed":%s})",
        (unsigned long long)t.taskId,
        t.description,
        t.targetFile,
        static_cast<int>(t.currentStage),
        t.retryCount,
        t.fixAttemptCount,
        t.totalDurationMs,
        t.lastCompile.compiled ? "true" : "false",
        t.lastTest.allPassed ? "true" : "false"
    );
    return std::string(buf);
}

std::string AutonomousVerifier::patternsToJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string result = "[";
    for (size_t i = 0; i < m_patterns.size(); i++) {
        if (i > 0) result += ",";
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            R"({"pattern":"%s","confidence":%d,"applied":%d,"succeeded":%d})",
            m_patterns[i].errorPattern,
            m_patterns[i].confidence,
            m_patterns[i].appliedCount,
            m_patterns[i].successCount
        );
        result += buf;
    }
    result += "]";
    return result;
}

} // namespace Autonomy
} // namespace RawrXD
