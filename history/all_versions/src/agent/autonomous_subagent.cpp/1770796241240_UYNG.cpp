// ============================================================================
// autonomous_subagent.cpp — Autonomous SubAgent for Repetitive Bulk Fixes
// ============================================================================
// Architecture: C++20, Win32, no Qt
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "autonomous_subagent.hpp"
#include "../subagent_core.h"
#include "../agentic_engine.h"
#include "agentic_failure_detector.hpp"
#include "agentic_puppeteer.hpp"

#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>
#include <thread>
#include <condition_variable>

// ============================================================================
// BulkFixProgress::summary
// ============================================================================
std::string BulkFixProgress::summary() const {
    std::ostringstream oss;
    oss << "BulkFix: " << completed << "/" << totalTargets
        << " done (" << failed << " failed, " << skipped << " skipped, "
        << verified << " verified, " << retries << " retries) "
        << std::fixed << std::setprecision(1) << (percentComplete * 100.0f) << "% "
        << elapsedMs << "ms";
    return oss.str();
}

// ============================================================================
// BulkFixResult factories
// ============================================================================
BulkFixResult BulkFixResult::ok(const std::string& id, const std::string& strategy,
                                 const std::vector<BulkFixTarget>& targets,
                                 const std::string& merged) {
    BulkFixResult r;
    r.success = true;
    r.bulkFixId = id;
    r.strategyName = strategy;
    r.targets = targets;
    r.mergedResult = merged;
    return r;
}

BulkFixResult BulkFixResult::error(const std::string& id, const std::string& msg) {
    BulkFixResult r;
    r.success = false;
    r.bulkFixId = id;
    r.error = msg;
    return r;
}

int BulkFixResult::fixedCount() const {
    int count = 0;
    for (const auto& t : targets) {
        if (t.status == BulkFixTarget::Status::Fixed ||
            t.status == BulkFixTarget::Status::Verified) {
            count++;
        }
    }
    return count;
}

int BulkFixResult::failedCount() const {
    int count = 0;
    for (const auto& t : targets) {
        if (t.status == BulkFixTarget::Status::Failed) count++;
    }
    return count;
}

std::vector<BulkFixTarget> BulkFixResult::failedTargets() const {
    std::vector<BulkFixTarget> result;
    for (const auto& t : targets) {
        if (t.status == BulkFixTarget::Status::Failed) result.push_back(t);
    }
    return result;
}

// ============================================================================
// AutonomousSubAgent — Constructor / Destructor
// ============================================================================

AutonomousSubAgent::AutonomousSubAgent(
    SubAgentManager* manager,
    AgenticEngine* engine,
    AgenticFailureDetector* detector,
    AgenticPuppeteer* puppeteer)
    : m_manager(manager)
    , m_engine(engine)
    , m_detector(detector)
    , m_puppeteer(puppeteer)
{
}

AutonomousSubAgent::~AutonomousSubAgent() {
    cancel();
}

// ============================================================================
// Configuration
// ============================================================================

void AutonomousSubAgent::setStrategy(const BulkFixStrategy& strategy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_strategy = strategy;
}

void AutonomousSubAgent::setTargets(const std::vector<BulkFixTarget>& targets) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_targets = targets;
}

void AutonomousSubAgent::addTarget(const BulkFixTarget& target) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_targets.push_back(target);
}

// ============================================================================
// UUID
// ============================================================================

std::string AutonomousSubAgent::generateId() const {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    ss << "bf-";
    ss << std::setw(8) << (dis(gen) & 0xFFFFFFFF) << "-";
    ss << std::setw(4) << (dis(gen) & 0xFFFF);
    return ss.str();
}

// ============================================================================
// Execute — Synchronous bulk fix
// ============================================================================

BulkFixResult AutonomousSubAgent::execute(const std::string& parentId) {
    // Only generate a new ID if one wasn't already assigned (e.g., by executeAsync)
    if (m_bulkFixId.empty()) {
        m_bulkFixId = generateId();
    }
    m_running.store(true);
    m_cancelled.store(false);

    m_progress = {};
    m_progress.totalTargets = (int)m_targets.size();
    m_progress.startTime = std::chrono::steady_clock::now();

    if (m_targets.empty()) {
        m_running.store(false);
        return BulkFixResult::error(m_bulkFixId, "No targets provided");
    }

    if (m_strategy.promptTemplate.empty()) {
        m_running.store(false);
        return BulkFixResult::error(m_bulkFixId, "No strategy prompt template set");
    }

    // ---- Parallel dispatch with concurrency limit ----
    std::vector<std::thread> workers;
    std::mutex batchMutex;
    std::condition_variable batchCV;
    std::atomic<int> running{0};
    std::atomic<bool> anyFatalFail{false};
    int nextIdx = 0;

    auto workerFn = [&](int idx) {
        BulkFixTarget& target = m_targets[idx];
        target.status = BulkFixTarget::Status::InProgress;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_progress.inProgress++;
        }

        if (m_onTargetStarted) {
            m_onTargetStarted(m_bulkFixId, target);
        }

        bool success = processTarget(target, parentId);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_progress.inProgress--;
            if (success) {
                m_progress.completed++;
            } else {
                m_progress.failed++;
                if (m_strategy.failFast) anyFatalFail.store(true);
            }
            m_progress.retries += target.retryCount;
        }
        updateProgress();

        if (m_onTargetCompleted) {
            m_onTargetCompleted(m_bulkFixId, target, success);
        }

        running--;
        {
            std::lock_guard<std::mutex> lk(batchMutex);
        }
        batchCV.notify_one();
    };

    for (int i = 0; i < (int)m_targets.size(); i++) {
        if (m_cancelled.load()) break;
        if (anyFatalFail.load()) break;

        {
            std::unique_lock<std::mutex> lock(batchMutex);
            batchCV.wait(lock, [&]() {
                return running.load() < m_strategy.maxParallel
                    || m_cancelled.load()
                    || anyFatalFail.load();
            });
        }

        if (m_cancelled.load() || anyFatalFail.load()) break;

        running++;
        workers.emplace_back(workerFn, i);
    }

    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }

    // Mark remaining as skipped if cancelled
    for (auto& target : m_targets) {
        if (target.status == BulkFixTarget::Status::Pending) {
            target.status = BulkFixTarget::Status::Skipped;
            std::lock_guard<std::mutex> lock(m_mutex);
            m_progress.skipped++;
        }
    }

    // Final progress update
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto now = std::chrono::steady_clock::now();
        m_progress.elapsedMs = (int)std::chrono::duration_cast<
            std::chrono::milliseconds>(now - m_progress.startTime).count();
        m_progress.percentComplete = 1.0f;
    }

    // Merge results
    std::string merged = mergeResults(parentId);

    m_running.store(false);

    BulkFixResult result = BulkFixResult::ok(m_bulkFixId, m_strategy.name,
                                              m_targets, merged);
    result.progress = m_progress;

    if (m_onCompleted) {
        m_onCompleted(result);
    }

    return result;
}

// ============================================================================
// Execute Async
// ============================================================================

std::string AutonomousSubAgent::executeAsync(const std::string& parentId,
                                              BulkFixCompletedCb onComplete) {
    // Generate ID once here; execute() will reuse it since m_bulkFixId is already set
    m_bulkFixId = generateId();
    std::string id = m_bulkFixId;

    std::thread([this, parentId, onComplete, id]() {
        // execute() will see m_bulkFixId is already set and reuse it
        BulkFixResult result = execute(parentId);
        if (onComplete) onComplete(result);
    }).detach();

    return id;
}

// ============================================================================
// Cancel
// ============================================================================

void AutonomousSubAgent::cancel() {
    m_cancelled.store(true);
}

// ============================================================================
// processTarget — Core fix logic per target
// ============================================================================

bool AutonomousSubAgent::processTarget(BulkFixTarget& target,
                                        const std::string& parentId) {
    auto startTime = std::chrono::steady_clock::now();

    for (int attempt = 0; attempt <= m_strategy.maxRetries; attempt++) {
        if (m_cancelled.load()) {
            target.status = BulkFixTarget::Status::Skipped;
            return false;
        }

        target.retryCount = attempt;
        std::string prompt = buildPrompt(target);

        // Spawn subagent via SubAgentManager for proper lifecycle tracking
        std::string desc = "[BulkFix:" + m_strategy.name + "] " + target.id;
        std::string agentId;

        if (m_manager) {
            agentId = m_manager->spawnSubAgent(parentId, desc, prompt);
            bool done = m_manager->waitForSubAgent(agentId, m_strategy.perTargetTimeoutMs);
            if (!done) {
                target.result = "[Timeout] SubAgent timed out after " +
                                std::to_string(m_strategy.perTargetTimeoutMs) + "ms";
                if (attempt < m_strategy.maxRetries) continue;
                target.status = BulkFixTarget::Status::Failed;
                break;
            }
            target.result = m_manager->getSubAgentResult(agentId);
        } else if (m_engine) {
            // Direct engine call fallback
            target.result = m_engine->chat(prompt);
        } else {
            target.status = BulkFixTarget::Status::Failed;
            target.result = "[Error] No engine or manager available";
            break;
        }

        // ---- Self-healing: detect failures in the response ----
        if (m_strategy.selfHeal && m_detector) {
            auto failure = m_detector->detectFailure(target.result);
            if (failure.type != AgentFailureType::None &&
                failure.confidence >= 0.6) {
                // Try puppeteer correction
                std::string healed = selfHeal(target.result, target);
                if (!healed.empty() && healed != target.result) {
                    target.result = healed;
                } else if (attempt < m_strategy.maxRetries) {
                    // Retry with modified prompt
                    continue;
                }
            }
        }

        // ---- Custom validator ----
        if (m_strategy.customValidator) {
            if (!m_strategy.customValidator(target.id, target.result)) {
                if (attempt < m_strategy.maxRetries) continue;
                target.status = BulkFixTarget::Status::Failed;
                target.result = "[Validation Failed] " + target.result;
                break;
            }
        }

        // ---- Auto-verify ----
        if (m_strategy.autoVerify && !m_strategy.verificationPrompt.empty()) {
            bool verified = verifyFix(target, target.result, parentId);
            if (verified) {
                target.status = BulkFixTarget::Status::Verified;
                std::lock_guard<std::mutex> lock(m_mutex);
                m_progress.verified++;
            } else if (attempt < m_strategy.maxRetries) {
                continue;
            } else {
                target.status = BulkFixTarget::Status::Fixed;
            }
        } else {
            target.status = BulkFixTarget::Status::Fixed;
        }

        auto endTime = std::chrono::steady_clock::now();
        target.elapsedMs = (int)std::chrono::duration_cast<
            std::chrono::milliseconds>(endTime - startTime).count();
        return true;
    }

    // Exhausted retries
    auto endTime = std::chrono::steady_clock::now();
    target.elapsedMs = (int)std::chrono::duration_cast<
        std::chrono::milliseconds>(endTime - startTime).count();

    if (target.status != BulkFixTarget::Status::Failed) {
        target.status = BulkFixTarget::Status::Failed;
    }
    return false;
}

// ============================================================================
// buildPrompt — Template substitution
// ============================================================================

std::string AutonomousSubAgent::buildPrompt(const BulkFixTarget& target) const {
    std::string prompt = m_strategy.promptTemplate;

    auto replaceAll = [](std::string& str, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.size(), to);
            pos += to.size();
        }
    };

    replaceAll(prompt, "{{target}}", target.id);
    replaceAll(prompt, "{{target_id}}", target.id);
    replaceAll(prompt, "{{path}}", target.path);
    replaceAll(prompt, "{{context}}", target.context);
    replaceAll(prompt, "{{category}}", target.category);
    replaceAll(prompt, "{{strategy}}", m_strategy.name);
    replaceAll(prompt, "{{retry}}", std::to_string(target.retryCount));

    return prompt;
}

// ============================================================================
// verifyFix — Spawn verification subagent
// ============================================================================

bool AutonomousSubAgent::verifyFix(const BulkFixTarget& target,
                                    const std::string& result,
                                    const std::string& parentId) {
    if (m_strategy.verificationPrompt.empty()) return true;

    std::string verifyPrompt = m_strategy.verificationPrompt;
    auto replaceAll = [](std::string& str, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.size(), to);
            pos += to.size();
        }
    };

    replaceAll(verifyPrompt, "{{target}}", target.id);
    replaceAll(verifyPrompt, "{{path}}", target.path);
    replaceAll(verifyPrompt, "{{result}}", result);
    replaceAll(verifyPrompt, "{{context}}", target.context);

    std::string verifyResult;
    if (m_manager) {
        std::string desc = "[Verify:" + m_strategy.name + "] " + target.id;
        std::string agentId = m_manager->spawnSubAgent(parentId, desc, verifyPrompt);
        m_manager->waitForSubAgent(agentId, 30000);
        verifyResult = m_manager->getSubAgentResult(agentId);
    } else if (m_engine) {
        verifyResult = m_engine->chat(verifyPrompt);
    } else {
        return true; // Can't verify, assume ok
    }

    // Simple heuristic: check if verification response indicates success
    std::string lower = verifyResult;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return (lower.find("pass") != std::string::npos ||
            lower.find("correct") != std::string::npos ||
            lower.find("verified") != std::string::npos ||
            lower.find("success") != std::string::npos ||
            lower.find("yes") != std::string::npos ||
            lower.find("true") != std::string::npos);
}

// ============================================================================
// selfHeal — Use failure detector + puppeteer to correct bad output
// ============================================================================

std::string AutonomousSubAgent::selfHeal(const std::string& failedResult,
                                          const BulkFixTarget& target) {
    if (!m_puppeteer) return failedResult;

    CorrectionResult correction = m_puppeteer->correctResponse(failedResult,
        "Bulk fix for target: " + target.id + " using strategy: " + m_strategy.name);

    if (correction.success) {
        return correction.correctedOutput;
    }
    return failedResult;
}

// ============================================================================
// mergeResults
// ============================================================================

std::string AutonomousSubAgent::mergeResults(const std::string& parentId) {
    switch (m_strategy.mergeMode) {
        case BulkFixStrategy::MergeMode::Individual:
        {
            std::ostringstream oss;
            for (int i = 0; i < (int)m_targets.size(); i++) {
                const auto& t = m_targets[i];
                oss << "=== [" << t.statusString() << "] " << t.id << " ===\n"
                    << t.result << "\n\n";
            }
            return oss.str();
        }

        case BulkFixStrategy::MergeMode::Concatenate:
        {
            std::ostringstream oss;
            for (const auto& t : m_targets) {
                if (t.status == BulkFixTarget::Status::Fixed ||
                    t.status == BulkFixTarget::Status::Verified) {
                    oss << t.result << "\n";
                }
            }
            return oss.str();
        }

        case BulkFixStrategy::MergeMode::Summarize:
        {
            std::ostringstream all;
            all << "You are given the results of " << m_targets.size()
                << " bulk fix operations using strategy '" << m_strategy.name << "'.\n"
                << "Summarize the overall outcome and highlight any failures.\n\n";
            for (int i = 0; i < (int)m_targets.size(); i++) {
                const auto& t = m_targets[i];
                all << "--- Target " << (i + 1) << " [" << t.statusString() << "] "
                    << t.id << " ---\n" << t.result << "\n\n";
            }
            if (m_engine) return m_engine->chat(all.str());
            return all.str();
        }
    }
    return {};
}

// ============================================================================
// Progress
// ============================================================================

BulkFixProgress AutonomousSubAgent::getProgress() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    BulkFixProgress p = m_progress;
    if (p.totalTargets > 0) {
        p.percentComplete = (float)(p.completed + p.failed + p.skipped)
                          / (float)p.totalTargets;
    }
    auto now = std::chrono::steady_clock::now();
    p.elapsedMs = (int)std::chrono::duration_cast<
        std::chrono::milliseconds>(now - p.startTime).count();
    return p;
}

std::string AutonomousSubAgent::getProgressJSON() const {
    auto p = getProgress();
    std::ostringstream oss;
    oss << "{\"bulkFixId\":\"" << m_bulkFixId
        << "\",\"strategy\":\"" << m_strategy.name
        << "\",\"total\":" << p.totalTargets
        << ",\"completed\":" << p.completed
        << ",\"failed\":" << p.failed
        << ",\"skipped\":" << p.skipped
        << ",\"verified\":" << p.verified
        << ",\"inProgress\":" << p.inProgress
        << ",\"retries\":" << p.retries
        << ",\"percent\":" << std::fixed << std::setprecision(1) << (p.percentComplete * 100)
        << ",\"elapsedMs\":" << p.elapsedMs << "}";
    return oss.str();
}

void AutonomousSubAgent::updateProgress() {
    if (m_onProgress) {
        m_onProgress(m_bulkFixId, getProgress());
    }
}

std::string AutonomousSubAgent::getStatusSummary() const {
    return getProgress().summary();
}

// ============================================================================
// BulkFixOrchestrator — Constructor / Destructor
// ============================================================================

BulkFixOrchestrator::BulkFixOrchestrator(
    SubAgentManager* manager,
    AgenticEngine* engine,
    AgenticFailureDetector* detector,
    AgenticPuppeteer* puppeteer)
    : m_manager(manager)
    , m_engine(engine)
    , m_detector(detector)
    , m_puppeteer(puppeteer)
{
}

BulkFixOrchestrator::~BulkFixOrchestrator() {
    cancelAll();
}

// ============================================================================
// Predefined Strategy Factories
// ============================================================================

BulkFixStrategy BulkFixOrchestrator::makeCompileErrorFixStrategy() {
    BulkFixStrategy s;
    s.name = "compile_error_fix";
    s.description = "Fix compile errors in source files";
    s.promptTemplate =
        "You are an expert C++ developer. Fix ALL compile errors in the following file.\n"
        "File: {{path}}\n"
        "Error context: {{context}}\n\n"
        "Rules:\n"
        "- Only fix compile errors, do not change logic\n"
        "- Preserve all existing functionality\n"
        "- Use C++20 idioms\n"
        "- No exceptions (use PatchResult pattern)\n"
        "- Return the corrected code sections only\n";
    s.verificationPrompt =
        "Verify this fix is correct for {{path}}:\n{{result}}\n"
        "Original error: {{context}}\n"
        "Does this fix resolve the error without introducing new issues? Answer yes/no with explanation.";
    s.maxRetries = 2;
    s.maxParallel = 4;
    s.autoVerify = true;
    s.selfHeal = true;
    return s;
}

BulkFixStrategy BulkFixOrchestrator::makeFormatEnforcementStrategy() {
    BulkFixStrategy s;
    s.name = "format_enforcement";
    s.description = "Enforce coding style and format across files";
    s.promptTemplate =
        "Reformat the following file to match project coding standards.\n"
        "File: {{path}}\n"
        "Context: {{context}}\n\n"
        "Standards:\n"
        "- 4-space indentation\n"
        "- PascalCase for types, camelCase for functions\n"
        "- snake_case for local variables\n"
        "- Braces on same line for functions\n"
        "- Maximum 120 chars per line\n"
        "- Return formatted code\n";
    s.maxRetries = 1;
    s.maxParallel = 8;
    s.autoVerify = false;
    s.selfHeal = false;
    return s;
}

BulkFixStrategy BulkFixOrchestrator::makeRefactorRenameStrategy(
    const std::string& oldName, const std::string& newName) {
    BulkFixStrategy s;
    s.name = "refactor_rename";
    s.description = "Rename '" + oldName + "' to '" + newName + "' across codebase";
    s.promptTemplate =
        "Rename all occurrences of '" + oldName + "' to '" + newName + "' in this file.\n"
        "File: {{path}}\n"
        "Context: {{context}}\n\n"
        "Rules:\n"
        "- Rename in declarations, definitions, references, and comments\n"
        "- Handle header include guards if applicable\n"
        "- Preserve all logic\n"
        "- Return only the changed sections\n";
    s.verificationPrompt =
        "Verify the rename from '" + oldName + "' to '" + newName + "' in {{path}}:\n"
        "{{result}}\n"
        "Are all occurrences correctly renamed? Answer yes/no.";
    s.maxRetries = 1;
    s.maxParallel = 6;
    s.autoVerify = true;
    s.selfHeal = false;
    return s;
}

BulkFixStrategy BulkFixOrchestrator::makeStubImplementationStrategy() {
    BulkFixStrategy s;
    s.name = "stub_implementation";
    s.description = "Replace stubs with real implementations";
    s.promptTemplate =
        "Implement the stub function(s) in this file with real, production-quality logic.\n"
        "File: {{path}}\n"
        "Target stub: {{target}}\n"
        "Context: {{context}}\n\n"
        "Rules:\n"
        "- C++20, no exceptions (use PatchResult or error codes)\n"
        "- Thread-safe with std::mutex / std::atomic where needed\n"
        "- Full working implementation, not another stub\n"
        "- Use Win32 APIs where applicable\n"
        "- Return the complete function implementation\n";
    s.verificationPrompt =
        "Verify this stub implementation for {{target}} in {{path}}:\n{{result}}\n"
        "Is this a real implementation (not another stub)? Does it compile? Answer yes/no.";
    s.maxRetries = 3;
    s.maxParallel = 4;
    s.autoVerify = true;
    s.selfHeal = true;
    return s;
}

BulkFixStrategy BulkFixOrchestrator::makeHeaderIncludeFixStrategy() {
    BulkFixStrategy s;
    s.name = "header_include_fix";
    s.description = "Fix missing or incorrect #include directives";
    s.promptTemplate =
        "Fix all #include issues in this file. Add missing includes, remove unused ones.\n"
        "File: {{path}}\n"
        "Errors: {{context}}\n\n"
        "Rules:\n"
        "- Include what you use (IWYU)\n"
        "- Use angle brackets for system headers\n"
        "- Use quotes for project headers\n"
        "- No circular includes\n"
        "- Return the corrected #include block\n";
    s.maxRetries = 2;
    s.maxParallel = 8;
    s.autoVerify = false;
    s.selfHeal = true;
    return s;
}

BulkFixStrategy BulkFixOrchestrator::makeLintFixStrategy() {
    BulkFixStrategy s;
    s.name = "lint_fix";
    s.description = "Fix linting warnings and code quality issues";
    s.promptTemplate =
        "Fix all linting/static analysis warnings in this file.\n"
        "File: {{path}}\n"
        "Warnings: {{context}}\n\n"
        "Rules:\n"
        "- Fix the specific warnings listed\n"
        "- Do not change unrelated code\n"
        "- Preserve all existing logic\n"
        "- Return corrected code\n";
    s.maxRetries = 2;
    s.maxParallel = 6;
    s.autoVerify = false;
    s.selfHeal = true;
    return s;
}

BulkFixStrategy BulkFixOrchestrator::makeTestGenerationStrategy() {
    BulkFixStrategy s;
    s.name = "test_generation";
    s.description = "Generate unit tests for functions";
    s.promptTemplate =
        "Generate comprehensive C++20 unit tests for the following target.\n"
        "Target: {{target}}\n"
        "File: {{path}}\n"
        "Context: {{context}}\n\n"
        "Requirements:\n"
        "- Test edge cases, normal cases, and error paths\n"
        "- Use assertions (no external test framework needed)\n"
        "- Test thread safety where applicable\n"
        "- Return complete test code\n";
    s.maxRetries = 2;
    s.maxParallel = 4;
    s.autoVerify = false;
    s.selfHeal = true;
    s.mergeMode = BulkFixStrategy::MergeMode::Concatenate;
    return s;
}

BulkFixStrategy BulkFixOrchestrator::makeDocCommentStrategy() {
    BulkFixStrategy s;
    s.name = "doc_comments";
    s.description = "Add documentation comments to functions and classes";
    s.promptTemplate =
        "Add Doxygen-style documentation comments to all public functions/classes in:\n"
        "File: {{path}}\n"
        "Target: {{target}}\n\n"
        "Requirements:\n"
        "- @brief, @param, @return, @note where applicable\n"
        "- Describe thread safety guarantees\n"
        "- Describe error handling behavior\n"
        "- Return the documented code\n";
    s.maxRetries = 1;
    s.maxParallel = 8;
    s.autoVerify = false;
    s.selfHeal = false;
    return s;
}

BulkFixStrategy BulkFixOrchestrator::makeSecurityAuditFixStrategy() {
    BulkFixStrategy s;
    s.name = "security_audit_fix";
    s.description = "Fix security vulnerabilities found by audit";
    s.promptTemplate =
        "Fix the security vulnerability in this code.\n"
        "File: {{path}}\n"
        "Vulnerability: {{target}}\n"
        "Details: {{context}}\n\n"
        "Rules:\n"
        "- Fix the specific vulnerability\n"
        "- Add bounds checking, input validation, or sanitization as needed\n"
        "- Do not introduce new functionality\n"
        "- Use safe Win32 APIs (e.g., StringCchCopy over strcpy)\n"
        "- Return the corrected code\n";
    s.verificationPrompt =
        "Verify this security fix for {{target}} in {{path}}:\n{{result}}\n"
        "Is the vulnerability properly mitigated? Answer yes/no.";
    s.maxRetries = 3;
    s.maxParallel = 4;
    s.autoVerify = true;
    s.selfHeal = true;
    return s;
}

BulkFixStrategy BulkFixOrchestrator::makeCustomStrategy(
    const std::string& name, const std::string& promptTemplate) {
    BulkFixStrategy s;
    s.name = name;
    s.description = "Custom bulk fix: " + name;
    s.promptTemplate = promptTemplate;
    s.maxRetries = 2;
    s.maxParallel = 4;
    s.autoVerify = false;
    s.selfHeal = true;
    return s;
}

// ============================================================================
// fixCompileErrors — High-level: scan + fix compile errors in files
// ============================================================================

BulkFixResult BulkFixOrchestrator::fixCompileErrors(
    const std::string& parentId,
    const std::vector<std::string>& filePaths)
{
    std::vector<BulkFixTarget> targets;
    for (int i = 0; i < (int)filePaths.size(); i++) {
        BulkFixTarget t;
        t.id = "file_" + std::to_string(i);
        t.path = filePaths[i];
        t.context = "Compile error in " + filePaths[i];
        t.category = "compile";
        targets.push_back(t);
    }

    BulkFixStrategy strategy = makeCompileErrorFixStrategy();
    return applyBulkRefactor(parentId, strategy, targets);
}

// ============================================================================
// applyBulkRefactor — Execute a full bulk fix strategy synchronously
// ============================================================================

BulkFixResult BulkFixOrchestrator::applyBulkRefactor(
    const std::string& parentId,
    const BulkFixStrategy& strategy,
    const std::vector<BulkFixTarget>& targets)
{
    auto agent = std::make_shared<AutonomousSubAgent>(
        m_manager, m_engine, m_detector, m_puppeteer);
    agent->setStrategy(strategy);
    agent->setTargets(targets);

    std::string id;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.totalBulkOps++;
    }

    BulkFixResult result = agent->execute(parentId);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.totalTargetsProcessed += (int)targets.size();
        m_stats.totalFixed += result.fixedCount();
        m_stats.totalFailed += result.failedCount();
        m_stats.totalRetries += result.progress.retries;
        m_stats.totalVerified += result.progress.verified;
    }

    return result;
}

// ============================================================================
// dispatchBulkFix — Async dispatch 
// ============================================================================

std::string BulkFixOrchestrator::dispatchBulkFix(
    const std::string& parentId,
    const BulkFixStrategy& strategy,
    const std::vector<BulkFixTarget>& targets,
    BulkFixCompletedCb onComplete)
{
    auto agent = std::make_shared<AutonomousSubAgent>(
        m_manager, m_engine, m_detector, m_puppeteer);
    agent->setStrategy(strategy);
    agent->setTargets(targets);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.totalBulkOps++;
    }

    std::string id = agent->executeAsync(parentId,
        [this, onComplete, agent](const BulkFixResult& result) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stats.totalTargetsProcessed += (int)result.targets.size();
                m_stats.totalFixed += result.fixedCount();
                m_stats.totalFailed += result.failedCount();
                m_stats.totalRetries += result.progress.retries;
                m_stats.totalVerified += result.progress.verified;
                m_active.erase(result.bulkFixId);
            }
            if (onComplete) onComplete(result);
        });

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_active[id] = agent;
    }

    return id;
}

// ============================================================================
// parseBulkFixToolCall — Parse model output for bulk_fix tool calls
// ============================================================================

bool BulkFixOrchestrator::parseBulkFixToolCall(
    const std::string& modelOutput,
    BulkFixStrategy& strategy,
    std::vector<BulkFixTarget>& targets) const
{
    // Look for bulk_fix / bulkFix / TOOL:bulk_fix patterns in model output
    size_t pos = modelOutput.find("bulk_fix");
    if (pos == std::string::npos) pos = modelOutput.find("bulkFix");
    if (pos == std::string::npos) pos = modelOutput.find("TOOL:bulk_fix");
    if (pos == std::string::npos) pos = modelOutput.find("bulk_autonomous_fix");
    if (pos == std::string::npos) pos = modelOutput.find("autonomous_bulk");
    if (pos == std::string::npos) return false;

    // Extract JSON block after the keyword
    size_t jsonStart = modelOutput.find('{', pos);
    if (jsonStart == std::string::npos) return false;

    int depth = 0;
    size_t jsonEnd = jsonStart;
    for (size_t i = jsonStart; i < modelOutput.size(); i++) {
        if (modelOutput[i] == '{') depth++;
        else if (modelOutput[i] == '}') {
            depth--;
            if (depth == 0) { jsonEnd = i; break; }
        }
    }
    if (jsonEnd <= jsonStart) return false;

    std::string json = modelOutput.substr(jsonStart, jsonEnd - jsonStart + 1);

    // Simple field extraction
    auto extractField = [&](const std::string& fieldName) -> std::string {
        std::string key = "\"" + fieldName + "\"";
        size_t kpos = json.find(key);
        if (kpos == std::string::npos) return "";
        size_t colon = json.find(':', kpos + key.size());
        if (colon == std::string::npos) return "";
        size_t valStart = json.find('"', colon + 1);
        if (valStart == std::string::npos) return "";
        valStart++;
        std::string value;
        for (size_t i = valStart; i < json.size(); i++) {
            if (json[i] == '\\' && i + 1 < json.size()) {
                value += json[++i];
            } else if (json[i] == '"') {
                break;
            } else {
                value += json[i];
            }
        }
        return value;
    };

    // Strategy fields
    std::string strategyName = extractField("strategy");
    std::string prompt = extractField("prompt");
    std::string verifyPrompt = extractField("verify");

    if (strategyName.empty() && prompt.empty()) return false;

    // Map known strategy names
    if (strategyName == "compile_error_fix") {
        strategy = makeCompileErrorFixStrategy();
    } else if (strategyName == "format_enforcement") {
        strategy = makeFormatEnforcementStrategy();
    } else if (strategyName == "stub_implementation") {
        strategy = makeStubImplementationStrategy();
    } else if (strategyName == "header_include_fix") {
        strategy = makeHeaderIncludeFixStrategy();
    } else if (strategyName == "lint_fix") {
        strategy = makeLintFixStrategy();
    } else if (strategyName == "test_generation") {
        strategy = makeTestGenerationStrategy();
    } else if (strategyName == "doc_comments") {
        strategy = makeDocCommentStrategy();
    } else if (strategyName == "security_audit_fix") {
        strategy = makeSecurityAuditFixStrategy();
    } else if (!prompt.empty()) {
        strategy = makeCustomStrategy(
            strategyName.empty() ? "custom" : strategyName, prompt);
    } else {
        return false;
    }

    if (!verifyPrompt.empty()) {
        strategy.verificationPrompt = verifyPrompt;
    }

    // Parse targets array
    size_t targetsPos = json.find("\"targets\"");
    if (targetsPos == std::string::npos) targetsPos = json.find("\"files\"");
    if (targetsPos != std::string::npos) {
        size_t arrStart = json.find('[', targetsPos);
        if (arrStart != std::string::npos) {
            int adepth = 0;
            size_t arrEnd = arrStart;
            for (size_t i = arrStart; i < json.size(); i++) {
                if (json[i] == '[') adepth++;
                else if (json[i] == ']') {
                    adepth--;
                    if (adepth == 0) { arrEnd = i; break; }
                }
            }

            // Parse string array of paths or object array of targets
            std::string arr = json.substr(arrStart + 1, arrEnd - arrStart - 1);
            int targetIdx = 0;

            // Try object items first
            size_t objStart = 0;
            while ((objStart = arr.find('{', objStart)) != std::string::npos) {
                size_t objEnd = arr.find('}', objStart);
                if (objEnd == std::string::npos) break;
                std::string obj = arr.substr(objStart, objEnd - objStart + 1);

                auto extractObjField = [&](const std::string& key2) -> std::string {
                    std::string k = "\"" + key2 + "\"";
                    size_t p = obj.find(k);
                    if (p == std::string::npos) return "";
                    size_t c = obj.find(':', p + k.size());
                    if (c == std::string::npos) return "";
                    size_t vs = obj.find('"', c + 1);
                    if (vs == std::string::npos) return "";
                    vs++;
                    std::string v;
                    for (size_t i2 = vs; i2 < obj.size(); i2++) {
                        if (obj[i2] == '\\' && i2 + 1 < obj.size()) v += obj[++i2];
                        else if (obj[i2] == '"') break;
                        else v += obj[i2];
                    }
                    return v;
                };

                BulkFixTarget t;
                t.id = extractObjField("id");
                if (t.id.empty()) t.id = "target_" + std::to_string(targetIdx);
                t.path = extractObjField("path");
                if (t.path.empty()) t.path = extractObjField("file");
                t.context = extractObjField("context");
                if (t.context.empty()) t.context = extractObjField("error");
                t.category = extractObjField("category");
                if (!t.path.empty()) targets.push_back(t);
                targetIdx++;
                objStart = objEnd + 1;
            }

            // If no objects found, try plain string array
            if (targets.empty()) {
                size_t strStart = 0;
                while ((strStart = arr.find('"', strStart)) != std::string::npos) {
                    strStart++;
                    std::string val;
                    for (size_t i = strStart; i < arr.size(); i++) {
                        if (arr[i] == '\\' && i + 1 < arr.size()) val += arr[++i];
                        else if (arr[i] == '"') { strStart = i + 1; break; }
                        else val += arr[i];
                    }
                    if (!val.empty()) {
                        BulkFixTarget t;
                        t.id = "target_" + std::to_string(targetIdx++);
                        t.path = val;
                        targets.push_back(t);
                    }
                }
            }
        }
    }

    return !targets.empty();
}

// ============================================================================
// Active operation management
// ============================================================================

BulkFixProgress BulkFixOrchestrator::getProgress(const std::string& bulkFixId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_active.find(bulkFixId);
    if (it != m_active.end()) return it->second->getProgress();
    return {};
}

void BulkFixOrchestrator::cancelBulkFix(const std::string& bulkFixId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_active.find(bulkFixId);
    if (it != m_active.end()) it->second->cancel();
}

void BulkFixOrchestrator::cancelAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [id, agent] : m_active) agent->cancel();
}

std::vector<std::string> BulkFixOrchestrator::activeOperations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> ids;
    for (const auto& [id, _] : m_active) ids.push_back(id);
    return ids;
}

// ============================================================================
// Statistics
// ============================================================================

BulkFixOrchestrator::Stats BulkFixOrchestrator::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void BulkFixOrchestrator::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = {};
}
