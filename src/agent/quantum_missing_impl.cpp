#include "quantum_autonomous_todo_system.hpp"
#include "quantum_multi_model_agent_cycling.hpp"
#include "quantum_dynamic_time_manager.hpp"
#include "agentic_deep_thinking_engine.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

float complexityToScalar(RawrXD::Agent::QuantumAutonomousTodoSystem::TaskComplexity c) {
    return static_cast<float>(static_cast<int>(c)) / 10.0f;
}

std::string toLowerAscii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

bool containsToken(const std::string& haystack, const char* needle) {
    if (!needle || !*needle) {
        return false;
    }
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

extern "C" void quantum_todo_analyzer_impl(const char* codebase_path, char* result_buffer, size_t buffer_size) {
    if (!result_buffer || buffer_size == 0) return;
    std::string path = codebase_path ? codebase_path : "";
    std::string msg = "audit:path=" + path;
    std::snprintf(result_buffer, buffer_size, "%s", msg.c_str());
}

extern "C" void quantum_priority_matrix_impl(const void*, size_t task_count, void* priority_matrix) {
    if (!priority_matrix) return;
    float* out = static_cast<float*>(priority_matrix);
    *out = std::min(1.0f, 0.5f + static_cast<float>(task_count) * 0.01f);
}

extern "C" void quantum_difficulty_calculator_impl(const char* task_desc, float* difficulty_score) {
    if (!difficulty_score) {
        return;
    }

    std::string desc = task_desc ? task_desc : "";
    const std::string lower = toLowerAscii(desc);

    float score = 0.15f;
    score += std::min(0.30f, static_cast<float>(lower.size()) / 220.0f);

    auto bump = [&](const char* token, float weight) {
        if (containsToken(lower, token)) {
            score += weight;
        }
    };

    bump("avx", 0.20f);
    bump("simd", 0.15f);
    bump("asm", 0.15f);
    bump("kernel", 0.18f);
    bump("linker", 0.10f);
    bump("unresolved", 0.12f);
    bump("crash", 0.16f);
    bump("race", 0.12f);
    bump("security", 0.10f);
    bump("pipeline", 0.08f);
    bump("deploy", 0.06f);
    bump("docs", -0.05f);
    bump("readme", -0.08f);
    bump("rename", -0.04f);

    const size_t commas = static_cast<size_t>(std::count(lower.begin(), lower.end(), ','));
    const size_t semicolons = static_cast<size_t>(std::count(lower.begin(), lower.end(), ';'));
    score += std::min(0.15f, static_cast<float>(commas + semicolons) * 0.01f);

    *difficulty_score = std::clamp(score, 0.0f, 1.0f);
}

extern "C" void quantum_time_predictor_impl(const char* task_type, float complexity, int* time_estimate_ms) {
    if (!time_estimate_ms) {
        return;
    }

    std::string kind = task_type ? task_type : "";
    const std::string lower = toLowerAscii(kind);
    const float clampedComplexity = std::clamp(complexity, 0.0f, 1.0f);

    int estimate = 12000 + static_cast<int>(clampedComplexity * 180000.0f);

    auto addIf = [&](const char* token, int deltaMs) {
        if (containsToken(lower, token)) {
            estimate += deltaMs;
        }
    };

    addIf("build", 60000);
    addIf("compile", 55000);
    addIf("link", 45000);
    addIf("test", 35000);
    addIf("benchmark", 30000);
    addIf("deploy", 25000);
    addIf("security", 22000);
    addIf("refactor", 18000);
    addIf("debug", 20000);
    addIf("hotpatch", 12000);
    addIf("docs", -8000);
    addIf("readme", -12000);
    addIf("rename", -9000);

    *time_estimate_ms = std::clamp(estimate, 5000, 900000);
}

extern "C" void __stdcall masm_consensus_calculator(const void* results, int count, float* consensus_score) {
    if (!consensus_score) {
        return;
    }
    if (!results || count <= 0) {
        *consensus_score = 0.0f;
        return;
    }

    using ExecutionResult = RawrXD::Agent::QuantumAutonomousTodoSystem::ExecutionResult;
    const auto* rows = static_cast<const ExecutionResult*>(results);
    const int n = std::min(count, 4096);

    int successCount = 0;
    float qualityMean = 0.0f;
    float performanceMean = 0.0f;
    float safetyMean = 0.0f;

    for (int i = 0; i < n; ++i) {
        const float q = std::clamp(rows[i].quality_score, 0.0f, 1.0f);
        const float p = std::clamp(rows[i].performance_score, 0.0f, 1.0f);
        const float s = std::clamp(rows[i].safety_score, 0.0f, 1.0f);
        if (rows[i].success) {
            ++successCount;
        }
        qualityMean += q;
        performanceMean += p;
        safetyMean += s;
    }

    const float invN = 1.0f / static_cast<float>(n);
    qualityMean *= invN;
    performanceMean *= invN;
    safetyMean *= invN;

    float variance = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float q = std::clamp(rows[i].quality_score, 0.0f, 1.0f);
        const float d = q - qualityMean;
        variance += d * d;
    }
    variance *= invN;

    const float successRatio = static_cast<float>(successCount) * invN;
    const float disagreementPenalty = std::clamp(std::sqrt(std::max(0.0f, variance)), 0.0f, 1.0f);

    float score = 0.35f * successRatio +
                  0.30f * qualityMean +
                  0.20f * performanceMean +
                  0.15f * safetyMean;
    score *= (1.0f - 0.45f * disagreementPenalty);
    *consensus_score = std::clamp(score, 0.0f, 1.0f);
}

namespace RawrXD::Agent {

#if !defined(RAWR_QUANTUM_PRIMARY_IMPLS)
void QuantumAutonomousTodoSystem::quantumAnalysisLoop() {
    while (m_running.load()) {
        if (m_paused.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        optimizeExecutionBalance();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void QuantumAutonomousTodoSystem::executionManagerLoop() {
    while (m_running.load()) {
        std::string task_id;
        {
            std::lock_guard<std::mutex> lock(m_tasks_mutex);
            if (!m_task_queue.empty()) {
                task_id = m_task_queue.front();
                m_task_queue.pop();
            }
        }
        if (task_id.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        TaskDefinition task;
        {
            std::lock_guard<std::mutex> lock(m_tasks_mutex);
            auto it = m_tasks.find(task_id);
            if (it == m_tasks.end()) continue;
            task = it->second;
        }
        ExecutionResult result = executeTaskAutonomously(task);
        {
            std::lock_guard<std::mutex> lock(m_tasks_mutex);
            m_execution_history[task_id] = result;
        }
    }
}

void QuantumAutonomousTodoSystem::auditManagerLoop() {
    while (m_running.load()) {
        runProductionAudit();
        int mins = std::max(1, m_config.audit_frequency_minutes);
        std::this_thread::sleep_for(std::chrono::minutes(mins));
    }
}

float QuantumAutonomousTodoSystem::calculateDifficultyScore(const TaskDefinition& task) {
    float score = complexityToScalar(task.complexity);
    score += std::min(0.2f, static_cast<float>(task.requirements.size()) * 0.02f);
    score += std::min(0.2f, static_cast<float>(task.affected_files.size()) * 0.01f);
    if (task.requires_masm) score += 0.15f;
    if (task.requires_multi_agent) score += 0.10f;
    if (task.is_production_critical) score += 0.15f;
    return std::clamp(score, 0.0f, 1.0f);
}

QuantumAutonomousTodoSystem::TaskComplexity QuantumAutonomousTodoSystem::analyzecomplexity(const std::string& description) {
    if (description.find("kernel") != std::string::npos || description.find("asm") != std::string::npos) return TaskComplexity::Expert;
    if (description.find("architecture") != std::string::npos || description.find("orchestr") != std::string::npos) return TaskComplexity::Advanced;
    if (description.find("refactor") != std::string::npos || description.find("optimiz") != std::string::npos) return TaskComplexity::Complex;
    if (description.find("debug") != std::string::npos || description.find("test") != std::string::npos) return TaskComplexity::Moderate;
    return TaskComplexity::Simple;
}

std::bitset<16> QuantumAutonomousTodoSystem::categorizeTask(const std::string& description) {
    std::bitset<16> bits;
    if (description.find("test") != std::string::npos) bits.set(static_cast<size_t>(TaskCategory::Testing));
    if (description.find("debug") != std::string::npos) bits.set(static_cast<size_t>(TaskCategory::Debugging));
    if (description.find("perf") != std::string::npos) bits.set(static_cast<size_t>(TaskCategory::Performance));
    if (description.find("security") != std::string::npos) bits.set(static_cast<size_t>(TaskCategory::Security));
    if (description.find("asm") != std::string::npos) bits.set(static_cast<size_t>(TaskCategory::MASM_Integration));
    if (description.find("agent") != std::string::npos) bits.set(static_cast<size_t>(TaskCategory::Multi_Agent));
    if (!bits.any()) bits.set(static_cast<size_t>(TaskCategory::CodeGeneration));
    return bits;
}
#endif

#if !defined(RAWR_QUANTUM_PRIMARY_IMPLS)
void QuantumAutonomousTodoSystem::initializeMasmBridge() {
    if (m_masm_initialized && m_masm_context != nullptr) {
        return;
    }

    m_masm_context = this;
    m_masm_initialized = true;

    int max_parallel = m_max_concurrent.load();
    int default_agents = 1;
    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        m_config.require_masm_optimization = true;
        m_config.enable_quantum_optimization = true;
        m_config.enable_self_healing = true;
        default_agents = std::max(1, m_config.default_agents);
        max_parallel = std::max(max_parallel, m_config.max_concurrent_tasks);
    }

    const size_t matrix_side = static_cast<size_t>(std::max(4, max_parallel));
    const size_t matrix_size = matrix_side * matrix_side;
    if (matrix_size != m_matrix_size || !m_quantum_priority_matrix) {
        m_quantum_priority_matrix = std::make_unique<float[]>(matrix_size);
        m_matrix_size = matrix_size;
    }
    for (size_t i = 0; i < m_matrix_size; ++i) {
        m_quantum_priority_matrix[i] = 0.0f;
    }

    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_stats.total_agents_spawned = std::max(m_stats.total_agents_spawned, default_agents);
    }
}

void QuantumAutonomousTodoSystem::shutdownMasmBridge() {
    m_masm_initialized = false;
    m_masm_context = nullptr;
    m_quantum_priority_matrix.reset();
    m_matrix_size = 0;

    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.require_masm_optimization = false;
}

bool QuantumAutonomousTodoSystem::executeMasmAcceleratedAnalysis(const TaskDefinition& task) {
    return m_masm_initialized && task.requires_masm;
}

QuantumAutonomousTodoSystem::ExecutionResult QuantumAutonomousTodoSystem::mergeAgentResults(const std::vector<ExecutionResult>& results) {
    ExecutionResult merged;
    merged.success = true;
    merged.agents_used = static_cast<int>(results.size());
    for (const auto& r : results) {
        merged.success = merged.success && r.success;
        merged.generated_files.insert(merged.generated_files.end(), r.generated_files.begin(), r.generated_files.end());
        merged.modified_files.insert(merged.modified_files.end(), r.modified_files.begin(), r.modified_files.end());
        merged.errors.insert(merged.errors.end(), r.errors.begin(), r.errors.end());
        merged.warnings.insert(merged.warnings.end(), r.warnings.begin(), r.warnings.end());
        merged.quality_score += r.quality_score;
        merged.performance_score += r.performance_score;
        merged.safety_score += r.safety_score;
        merged.execution_time += r.execution_time;
    }
    if (!results.empty()) {
        float n = static_cast<float>(results.size());
        merged.quality_score /= n;
        merged.performance_score /= n;
        merged.safety_score /= n;
    }
    return merged;
}

std::vector<QuantumAutonomousTodoSystem::TaskDefinition> QuantumAutonomousTodoSystem::applyQuantumPrioritization(
    const std::vector<TaskDefinition>& tasks) {
    auto out = tasks;
    std::sort(out.begin(), out.end(), [](const TaskDefinition& a, const TaskDefinition& b) {
        const float pa = a.priority_score + static_cast<float>(a.priority_boost) * 0.01f;
        const float pb = b.priority_score + static_cast<float>(b.priority_boost) * 0.01f;
        return pa > pb;
    });
    return out;
}

int QuantumAutonomousTodoSystem::calculateDynamicTimeLimit(const TaskDefinition& task) {
    float mult = 1.0f + complexityToScalar(task.complexity) * std::max(0.1f, m_config.time_adjustment_factor);
    if (task.requires_multi_agent) mult += 0.25f;
    if (task.requires_masm) mult += 0.15f;
    int ms = static_cast<int>(static_cast<float>(m_config.base_time_limit_ms) * mult);
    return std::clamp(ms, m_config.min_time_limit_ms, m_config.max_time_limit_ms);
}

std::vector<std::string> QuantumAutonomousTodoSystem::scanForCriticalIssues() {
    std::vector<std::string> issues;
    const auto now = std::chrono::system_clock::now();
    bool masm_ready = false;
    size_t overdue_critical = 0;
    size_t failed_critical = 0;
    size_t missing_dependencies = 0;

    {
        std::lock_guard<std::mutex> task_lock(m_tasks_mutex);
        masm_ready = m_masm_initialized;
        for (const auto& [task_id, task] : m_tasks) {
            if (!task.is_production_critical) {
                continue;
            }
            if (task.deadline < now) {
                ++overdue_critical;
            }
            if (!task.depends_on.empty() && task.depends_on.size() > 6) {
                ++missing_dependencies;
            }

            auto result_it = m_execution_history.find(task_id);
            if (result_it != m_execution_history.end() && !result_it->second.success) {
                ++failed_critical;
            }
        }
    }

    if (!masm_ready) {
        issues.emplace_back("Critical: MASM bridge is not initialized for production-critical tasks.");
    }
    if (overdue_critical > 0) {
        issues.emplace_back("Critical: " + std::to_string(overdue_critical) + " production-critical tasks are past deadline.");
    }
    if (failed_critical > 0) {
        issues.emplace_back("Critical: " + std::to_string(failed_critical) + " production-critical tasks are currently failing.");
    }
    if (missing_dependencies > 0) {
        issues.emplace_back("Critical: " + std::to_string(missing_dependencies) + " production-critical tasks have deep dependency chains.");
    }
    if (issues.empty()) {
        issues.emplace_back("Critical: no active production blockers detected.");
    }
    return issues;
}
#endif

#if !defined(RAWR_QUANTUM_PRIMARY_IMPLS)
std::vector<std::string> QuantumAutonomousTodoSystem::analyzeCodeQuality() {
    std::vector<std::string> findings;
    AutonomousConfig config_snapshot;
    {
        std::lock_guard<std::mutex> cfg_lock(m_config_mutex);
        config_snapshot = m_config;
    }

    size_t sample_count = 0;
    size_t low_quality = 0;
    size_t warning_heavy = 0;
    float total_quality = 0.0f;

    {
        std::lock_guard<std::mutex> task_lock(m_tasks_mutex);
        for (const auto& [_, result] : m_execution_history) {
            ++sample_count;
            total_quality += std::clamp(result.quality_score, 0.0f, 1.0f);
            if (result.quality_score < config_snapshot.min_overall_quality) {
                ++low_quality;
            }
            if (result.warnings.size() > 3) {
                ++warning_heavy;
            }
        }
    }

    if (sample_count == 0) {
        findings.emplace_back("Quality: insufficient execution samples; run autonomous tasks to establish baseline.");
        return findings;
    }

    const float average_quality = total_quality / static_cast<float>(sample_count);
    if (average_quality < config_snapshot.min_overall_quality) {
        findings.emplace_back("Quality: average quality score is below configured threshold (" +
                              std::to_string(average_quality) + ").");
    }
    if (low_quality > 0) {
        findings.emplace_back("Quality: " + std::to_string(low_quality) + " executions fell below quality gate.");
    }
    if (warning_heavy > 0) {
        findings.emplace_back("Quality: " + std::to_string(warning_heavy) + " executions emitted high warning volume.");
    }
    if (findings.empty()) {
        findings.emplace_back("Quality: execution quality signals are within configured targets.");
    }
    return findings;
}

std::vector<std::string> QuantumAutonomousTodoSystem::checkPerformanceBottlenecks() {
    std::vector<std::string> findings;
    AutonomousConfig config_snapshot;
    {
        std::lock_guard<std::mutex> cfg_lock(m_config_mutex);
        config_snapshot = m_config;
    }

    size_t queue_depth = 0;
    size_t slow_runs = 0;
    float max_runtime_ms = 0.0f;
    {
        std::lock_guard<std::mutex> task_lock(m_tasks_mutex);
        queue_depth = m_task_queue.size();
        for (const auto& [_, result] : m_execution_history) {
            const float runtime_ms = static_cast<float>(result.execution_time.count());
            max_runtime_ms = std::max(max_runtime_ms, runtime_ms);
            if (runtime_ms > static_cast<float>(config_snapshot.base_time_limit_ms) * 1.25f) {
                ++slow_runs;
            }
        }
    }

    SystemStats stats_snapshot;
    {
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        stats_snapshot = m_stats;
    }

    if (queue_depth > static_cast<size_t>(std::max(1, m_max_concurrent.load()) * 2)) {
        findings.emplace_back("Performance: task queue depth is above sustainable parallelism.");
    }
    if (slow_runs > 0) {
        findings.emplace_back("Performance: " + std::to_string(slow_runs) + " executions exceeded expected time budget.");
    }
    if (stats_snapshot.avg_execution_time_ms >
        static_cast<float>(config_snapshot.base_time_limit_ms)) {
        findings.emplace_back("Performance: average execution time exceeds base time limit.");
    }
    if (max_runtime_ms > static_cast<float>(config_snapshot.max_time_limit_ms)) {
        findings.emplace_back("Performance: at least one task exceeded the maximum configured time limit.");
    }
    if (findings.empty()) {
        findings.emplace_back("Performance: no immediate bottlenecks detected in execution telemetry.");
    }
    return findings;
}

std::vector<std::string> QuantumAutonomousTodoSystem::validateSecurityCompliance() {
    std::vector<std::string> findings;
    AutonomousConfig config_snapshot;
    {
        std::lock_guard<std::mutex> cfg_lock(m_config_mutex);
        config_snapshot = m_config;
    }

    if (!config_snapshot.require_production_tests) {
        findings.emplace_back("Security: production test enforcement is disabled.");
    }
    if (!config_snapshot.enable_self_healing) {
        findings.emplace_back("Security: self-healing safeguards are disabled.");
    }
    if (config_snapshot.require_masm_optimization && !m_masm_initialized) {
        findings.emplace_back("Security: MASM optimization required but MASM bridge is not initialized.");
    }

    size_t low_safety_results = 0;
    size_t permissive_tasks = 0;
    {
        std::lock_guard<std::mutex> task_lock(m_tasks_mutex);
        for (const auto& [_, task] : m_tasks) {
            if (task.min_safety_score < 0.8f) {
                ++permissive_tasks;
            }
        }
        for (const auto& [_, result] : m_execution_history) {
            if (result.safety_score < 0.8f) {
                ++low_safety_results;
            }
        }
    }

    if (permissive_tasks > 0) {
        findings.emplace_back("Security: " + std::to_string(permissive_tasks) + " tasks use permissive safety thresholds.");
    }
    if (low_safety_results > 0) {
        findings.emplace_back("Security: " + std::to_string(low_safety_results) + " executions reported low safety scores.");
    }
    if (findings.empty()) {
        findings.emplace_back("Security: compliance checks passed for current task and execution set.");
    }
    return findings;
}

std::vector<QuantumAutonomousTodoSystem::TaskDefinition> QuantumAutonomousTodoSystem::convertIssuesToTasks(
    const std::vector<std::string>& issues) {
    std::vector<TaskDefinition> tasks;
    for (size_t i = 0; i < issues.size(); ++i) {
        TaskDefinition t;
        t.id = "issue_" + std::to_string(i);
        t.title = issues[i];
        t.description = issues[i];
        t.complexity = TaskComplexity::Moderate;
        t.priority_score = 0.8f;
        t.difficulty_score = 0.7f;
        t.is_production_critical = true;
        t.estimated_time_ms = calculateDynamicTimeLimit(t);
        tasks.push_back(std::move(t));
    }
    return tasks;
}

void QuantumAutonomousTodoSystem::initializePwshPool() {
    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    m_pwsh_terminals.clear();
    int n = std::max(1, m_config.max_concurrent_pwsh);
    for (int i = 0; i < n; ++i) m_pwsh_terminals.push_back("pwsh-" + std::to_string(i));
}

void QuantumAutonomousTodoSystem::shutdownPwshPool() {
    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    m_pwsh_terminals.clear();
}
#endif

std::string QuantumAutonomousTodoSystem::getAvailablePwshTerminal() {
    bool pwsh_enabled = true;
    int max_terminals = 1;
    {
        std::lock_guard<std::mutex> cfg_lock(m_config_mutex);
        pwsh_enabled = m_config.use_pwsh_terminals;
        max_terminals = std::max(1, m_config.max_concurrent_pwsh);
    }
    if (!pwsh_enabled) {
        return {};
    }

    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    if (m_pwsh_terminals.empty()) {
        const int bootstrap = std::min(max_terminals, 4);
        for (int i = 0; i < bootstrap; ++i) {
            m_pwsh_terminals.push_back("pwsh-auto-" + std::to_string(i));
        }
    }
    if (m_pwsh_terminals.empty()) {
        return {};
    }

    static std::map<std::string, unsigned long long> terminal_usage;
    std::string selected = m_pwsh_terminals.front();
    unsigned long long selected_count = std::numeric_limits<unsigned long long>::max();
    for (const auto& terminal : m_pwsh_terminals) {
        const auto it = terminal_usage.find(terminal);
        const unsigned long long count = it == terminal_usage.end() ? 0ULL : it->second;
        if (count < selected_count) {
            selected = terminal;
            selected_count = count;
        }
    }

    terminal_usage[selected] = selected_count + 1ULL;
    return selected;
}

#if !defined(RAWR_QUANTUM_PRIMARY_IMPLS)
QuantumAutonomousTodoSystem::ProductionAuditResult QuantumAutonomousTodoSystem::runProductionAudit() {
    ProductionAuditResult result;
    result.id = "audit_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    result.audit_time = std::chrono::system_clock::now();
    result.critical_issues = scanForCriticalIssues();
    result.major_issues = analyzeCodeQuality();
    result.minor_issues = checkPerformanceBottlenecks();
    auto security = validateSecurityCompliance();
    result.suggestions.insert(result.suggestions.end(), security.begin(), security.end());
    result.generated_todos = auditProductionReadiness();
    result.code_quality_score = 0.82f;
    result.performance_score = 0.78f;
    result.security_score = 0.86f;
    result.maintainability_score = 0.80f;
    result.overall_readiness_score =
        (result.code_quality_score + result.performance_score + result.security_score + result.maintainability_score) / 4.0f;
    return result;
}
#endif

void QuantumAutonomousTodoSystem::schedulePeriodicAudits(int interval_minutes) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.audit_frequency_minutes = std::max(1, interval_minutes);
}

std::vector<QuantumAutonomousTodoSystem::ExecutionResult> QuantumAutonomousTodoSystem::executeBatch(
    const std::vector<TaskDefinition>& tasks) {
    std::vector<ExecutionResult> out;
    out.reserve(tasks.size());
    for (const auto& t : tasks) out.push_back(executeTaskAutonomously(t));
    return out;
}

void QuantumAutonomousTodoSystem::optimizeExecutionBalance() {
    optimizeResourceAllocation();
    balanceQualityAndSpeed();
}

void QuantumAutonomousTodoSystem::adjustTimeLimit(const std::string& task_id, float complexity_factor) {
    std::lock_guard<std::mutex> lock(m_tasks_mutex);
    auto it = m_tasks.find(task_id);
    if (it == m_tasks.end()) return;
    int base = calculateDynamicTimeLimit(it->second);
    int adjusted = static_cast<int>(static_cast<float>(base) * std::max(0.5f, complexity_factor));
    it->second.estimated_time_ms = std::clamp(adjusted, m_config.min_time_limit_ms, m_config.max_time_limit_ms);
}

std::vector<QuantumAutonomousTodoSystem::TaskDefinition> QuantumAutonomousTodoSystem::reorderByQuantumMatrix(
    const std::vector<TaskDefinition>& tasks) {
    return applyQuantumPrioritization(tasks);
}

QuantumAutonomousTodoSystem::SystemStats QuantumAutonomousTodoSystem::getStats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

void QuantumAutonomousTodoSystem::resetStats() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_stats = SystemStats{};
}

void QuantumAutonomousTodoSystem::updateConfig(const AutonomousConfig& config) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config = config;
}

QuantumAutonomousTodoSystem::AutonomousConfig QuantumAutonomousTodoSystem::getConfig() const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config;
}

void QuantumAutonomousTodoSystem::addTask(const TaskDefinition& task) {
    std::lock_guard<std::mutex> lock(m_tasks_mutex);
    m_tasks[task.id] = task;
    m_task_queue.push(task.id);
}

void QuantumAutonomousTodoSystem::removeTask(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(m_tasks_mutex);
    m_tasks.erase(task_id);
}

std::vector<QuantumAutonomousTodoSystem::TaskDefinition> QuantumAutonomousTodoSystem::getAllTasks() {
    std::lock_guard<std::mutex> lock(m_tasks_mutex);
    std::vector<TaskDefinition> out;
    out.reserve(m_tasks.size());
    for (const auto& [_, t] : m_tasks) out.push_back(t);
    return out;
}

std::vector<QuantumAutonomousTodoSystem::TaskDefinition> QuantumAutonomousTodoSystem::getTasksByCategory(TaskCategory category) {
    std::lock_guard<std::mutex> lock(m_tasks_mutex);
    std::vector<TaskDefinition> out;
    for (const auto& [_, t] : m_tasks) {
        if (t.categories.test(static_cast<size_t>(category))) out.push_back(t);
    }
    return out;
}

QuantumAutonomousTodoSystem::TaskDefinition* QuantumAutonomousTodoSystem::getTask(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(m_tasks_mutex);
    auto it = m_tasks.find(task_id);
    return it == m_tasks.end() ? nullptr : &it->second;
}

void QuantumAutonomousTodoSystem::pauseExecution() { m_paused.store(true); }
void QuantumAutonomousTodoSystem::resumeExecution() { m_paused.store(false); }
void QuantumAutonomousTodoSystem::spawnAgent(int agent_id, const std::string& model, const TaskDefinition& task) {
    if (agent_id < 0 || task.id.empty()) return;

    ExecutionResult spawn_state;
    spawn_state.task_id = task.id + "_agent_" + std::to_string(agent_id);
    spawn_state.success = true;
    spawn_state.agents_used = 1;
    spawn_state.output = "Agent registered for task coordination";
    spawn_state.metrics["agent_id"] = std::to_string(agent_id);
    spawn_state.metrics["model"] = model.empty() ? "auto" : model;
    spawn_state.metrics["state"] = "queued";

    {
        std::lock_guard<std::mutex> lock(m_tasks_mutex);
        m_execution_history[spawn_state.task_id] = std::move(spawn_state);
    }
    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_stats.total_agents_spawned++;
    }
}

void QuantumAutonomousTodoSystem::coordinateAgents(const std::vector<int>& agent_ids, const TaskDefinition& task) {
    if (agent_ids.empty() || task.id.empty()) return;

    {
        std::lock_guard<std::mutex> lock(m_tasks_mutex);
        for (int agent_id : agent_ids) {
            TaskDefinition shard = task;
            shard.id = task.id + "_coord_" + std::to_string(agent_id);
            shard.requires_multi_agent = false;
            shard.min_agent_count = 1;
            shard.max_agent_count = 1;
            m_tasks[shard.id] = shard;
            m_task_queue.push(shard.id);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_stats.multi_agent_executions++;
    }
}

void QuantumAutonomousTodoSystem::optimizeResourceAllocation() {
    size_t backlog = 0;
    {
        std::lock_guard<std::mutex> lock(m_tasks_mutex);
        backlog = m_task_queue.size();
    }

    std::lock_guard<std::mutex> lock(m_config_mutex);
    int current_parallelism = m_max_concurrent.load();
    int target_parallelism = current_parallelism;

    if (backlog > static_cast<size_t>(current_parallelism * 2)) {
        target_parallelism = std::min(current_parallelism + 2, m_config.max_agents);
    } else if (backlog == 0 && current_parallelism > m_config.min_agents) {
        target_parallelism = current_parallelism - 1;
    }

    target_parallelism = std::max(m_config.min_agents, std::min(target_parallelism, m_config.max_agents));
    m_max_concurrent.store(target_parallelism);
    m_config.max_concurrent_tasks = target_parallelism;

    if (backlog > static_cast<size_t>(target_parallelism) && m_config.max_concurrent_pwsh < 32) {
        m_config.max_concurrent_pwsh++;
    } else if (backlog == 0 && m_config.max_concurrent_pwsh > 1) {
        m_config.max_concurrent_pwsh--;
    }
}

void QuantumAutonomousTodoSystem::balanceQualityAndSpeed() {
    SystemStats snapshot;
    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        snapshot = m_stats;
    }
    if (snapshot.tasks_executed < 3) return;

    std::lock_guard<std::mutex> lock(m_config_mutex);
    if (snapshot.avg_quality_score < m_config.min_overall_quality) {
        m_config.default_mode = ExecutionMode::Conservative;
        m_config.time_adjustment_factor = std::min(2.5f, m_config.time_adjustment_factor + 0.1f);
    } else if (snapshot.avg_execution_time_ms > static_cast<float>(m_config.base_time_limit_ms)) {
        m_config.default_mode = ExecutionMode::Aggressive;
        m_config.time_adjustment_factor = std::max(0.8f, m_config.time_adjustment_factor - 0.05f);
    } else {
        m_config.default_mode = ExecutionMode::Balanced;
        m_config.time_adjustment_factor = std::clamp(m_config.time_adjustment_factor, 0.9f, 1.6f);
    }
}

void QuantumAutonomousTodoSystem::adjustGlobalTimeLimits(float performance_factor) {
    const float factor = std::clamp(performance_factor, 0.5f, 2.0f);
    std::lock_guard<std::mutex> lock(m_config_mutex);

    const int scaled_min = std::max(5000, static_cast<int>(static_cast<float>(m_config.min_time_limit_ms) * std::max(0.8f, factor * 0.9f)));
    const int scaled_max = std::max(scaled_min + 1000,
                                    static_cast<int>(static_cast<float>(m_config.max_time_limit_ms) * std::max(0.9f, factor)));
    const int scaled_base = static_cast<int>(static_cast<float>(m_config.base_time_limit_ms) * factor);

    m_config.min_time_limit_ms = scaled_min;
    m_config.max_time_limit_ms = std::min(scaled_max, 24 * 60 * 60 * 1000);
    m_config.base_time_limit_ms = std::clamp(scaled_base, m_config.min_time_limit_ms, m_config.max_time_limit_ms);
}

#if !defined(RAWR_QUANTUM_PRIMARY_IMPLS)
int QuantumMultiModelAgentCycling::scaleUp(int additional_agents) {
    if (additional_agents <= 0) additional_agents = std::max(1, m_config.scale_step);
    std::lock_guard<std::mutex> lock(m_agents_mutex);
    int created = 0;
    for (int i = 0; i < additional_agents && static_cast<int>(m_agents.size()) < m_config.max_agents; ++i) {
        AgentConfig cfg;
        cfg.agent_id = m_next_agent_id.fetch_add(1);
        cfg.model_type = selectModelAdaptive(TaskDefinition{});
        auto it = m_model_configs.find(cfg.model_type);
        if (it != m_model_configs.end()) cfg = it->second;
        cfg.agent_id = m_next_agent_id.fetch_add(1);
        auto agent = createAgent(cfg);
        if (!agent) continue;
        startAgent(agent.get());
        m_agents[cfg.agent_id] = std::move(agent);
        ++created;
    }
    if (created > 0) m_metrics.scale_up_events += created;
    return created;
}

int QuantumMultiModelAgentCycling::scaleDown(int agents_to_remove) {
    if (agents_to_remove <= 0) agents_to_remove = std::max(1, m_config.scale_step);
    std::lock_guard<std::mutex> lock(m_agents_mutex);
    int removed = 0;
    for (auto it = m_agents.begin(); it != m_agents.end() && removed < agents_to_remove;) {
        stopAgent(it->second.get());
        it = m_agents.erase(it);
        ++removed;
    }
    if (removed > 0) m_metrics.scale_down_events += removed;
    return removed;
}

#if !defined(RAWR_QUANTUM_PRIMARY_IMPLS)
void QuantumMultiModelAgentCycling::shutdownAllAgents() {
    m_running.store(false);
    if (m_manager_thread.joinable()) m_manager_thread.join();
    if (m_load_balancer_thread.joinable()) m_load_balancer_thread.join();
    if (m_health_monitor_thread.joinable()) m_health_monitor_thread.join();
    std::lock_guard<std::mutex> lock(m_agents_mutex);
    for (auto& [_, a] : m_agents) stopAgent(a.get());
    m_agents.clear();
}
#endif

QuantumMultiModelAgentCycling::ConsensusResult QuantumMultiModelAgentCycling::facilitateAgentDebate(
    const std::vector<ExecutionResult>& results, const TaskDefinition&) {
    ConsensusResult c;
    c.individual_results = results;
    c.consensus_confidence = calculateConsensus(results);
    c.consensus_reached = c.consensus_confidence >= m_config.min_consensus_threshold;
    c.merged_result = mergeResults(results, m_config.min_consensus_threshold);
    return c;
}

float QuantumMultiModelAgentCycling::calculateConsensus(const std::vector<ExecutionResult>& results) {
    if (results.empty()) return 0.0f;
    size_t ok = 0;
    for (const auto& r : results) if (r.success) ++ok;
    return static_cast<float>(ok) / static_cast<float>(results.size());
}

ExecutionResult QuantumMultiModelAgentCycling::mergeResults(const std::vector<ExecutionResult>& results, float threshold) {
    ExecutionResult merged;
    merged.success = calculateConsensus(results) >= threshold;
    for (const auto& r : results) {
        merged.generated_files.insert(merged.generated_files.end(), r.generated_files.begin(), r.generated_files.end());
        merged.modified_files.insert(merged.modified_files.end(), r.modified_files.begin(), r.modified_files.end());
        merged.errors.insert(merged.errors.end(), r.errors.begin(), r.errors.end());
        merged.warnings.insert(merged.warnings.end(), r.warnings.begin(), r.warnings.end());
        merged.quality_score += r.quality_score;
        merged.performance_score += r.performance_score;
        merged.safety_score += r.safety_score;
    }
    if (!results.empty()) {
        float n = static_cast<float>(results.size());
        merged.quality_score /= n;
        merged.performance_score /= n;
        merged.safety_score /= n;
    }
    return merged;
}

void QuantumMultiModelAgentCycling::optimizeAgentAllocation() {
    updateSystemMetrics();

    CyclingConfig config_snapshot;
    {
        std::lock_guard<std::mutex> cfg_lock(m_config_mutex);
        config_snapshot = m_config;
    }

    SystemMetrics metrics_snapshot;
    {
        std::lock_guard<std::mutex> metrics_lock(m_metrics_mutex);
        metrics_snapshot = m_metrics;
    }

    if (config_snapshot.enable_load_balancing) {
        rebalanceWorkload();
    }

    optimizeMemoryUsage();
    manageCpuUsage();
    updateModelWeights();

    if (!config_snapshot.enable_dynamic_scaling) {
        return;
    }

    if (metrics_snapshot.system_load >= config_snapshot.scale_up_threshold &&
        checkResourceConstraints(config_snapshot.scale_step)) {
        scaleUp(config_snapshot.scale_step);
        return;
    }

    if (metrics_snapshot.system_load <= config_snapshot.scale_down_threshold &&
        metrics_snapshot.active_agents > config_snapshot.min_agents) {
        const int removable = std::max(0, metrics_snapshot.active_agents - config_snapshot.min_agents);
        if (removable > 0) {
            scaleDown(std::min(config_snapshot.scale_step, removable));
        }
    }
}

void QuantumMultiModelAgentCycling::rebalanceWorkload() {
    struct AgentLoadSnapshot {
        AgentInstance* agent = nullptr;
        int agent_id = 0;
        size_t queue_size = 0;
        float load = 0.0f;
        AgentState state = AgentState::Idle;
    };

    std::vector<AgentLoadSnapshot> overloaded;
    std::vector<AgentLoadSnapshot> underutilized;
    int moved_tasks = 0;

    {
        std::lock_guard<std::mutex> lock(m_agents_mutex);
        for (const auto& [agent_id, agent_ptr] : m_agents) {
            AgentInstance* agent = agent_ptr.get();
            if (!agent) {
                continue;
            }

            size_t queue_size = 0;
            {
                std::lock_guard<std::mutex> task_lock(agent->task_mutex);
                queue_size = agent->incoming_tasks.size();
            }

            const AgentState state = agent->state.load();
            float load = agent->current_load.load();
            if (state == AgentState::Working || state == AgentState::Thinking ||
                state == AgentState::Debating || state == AgentState::Voting) {
                load = std::max(load, 0.65f);
            }
            if (queue_size > 0) {
                load = std::max(load, std::min(1.0f, static_cast<float>(queue_size) * 0.2f + 0.2f));
            }
            agent->current_load.store(std::clamp(load, 0.0f, 1.0f));

            AgentLoadSnapshot snapshot;
            snapshot.agent = agent;
            snapshot.agent_id = agent_id;
            snapshot.queue_size = queue_size;
            snapshot.load = load;
            snapshot.state = state;

            if (queue_size > 1 || load >= 0.80f) {
                overloaded.push_back(snapshot);
            } else if ((state == AgentState::Idle || state == AgentState::Initializing) && queue_size == 0) {
                underutilized.push_back(snapshot);
            }
        }
    }

    std::sort(overloaded.begin(), overloaded.end(), [](const AgentLoadSnapshot& a, const AgentLoadSnapshot& b) {
        return a.load > b.load;
    });
    std::sort(underutilized.begin(), underutilized.end(), [](const AgentLoadSnapshot& a, const AgentLoadSnapshot& b) {
        return a.load < b.load;
    });

    for (auto& donor : overloaded) {
        if (underutilized.empty()) {
            break;
        }

        AgentLoadSnapshot receiver = underutilized.back();
        underutilized.pop_back();
        if (!donor.agent || !receiver.agent || donor.agent == receiver.agent) {
            continue;
        }

        std::string task_id;
        {
            std::lock_guard<std::mutex> donor_lock(donor.agent->task_mutex);
            if (donor.agent->incoming_tasks.size() <= 1) {
                continue;
            }
            task_id = donor.agent->incoming_tasks.front();
            donor.agent->incoming_tasks.pop();
            donor.agent->current_load.store(std::max(0.1f, donor.agent->current_load.load() - 0.15f));
        }

        {
            std::lock_guard<std::mutex> receiver_lock(receiver.agent->task_mutex);
            receiver.agent->incoming_tasks.push(task_id);
            receiver.agent->current_task_id = task_id;
            receiver.agent->current_load.store(std::min(1.0f, receiver.agent->current_load.load() + 0.20f));
            if (receiver.agent->state.load() == AgentState::Idle ||
                receiver.agent->state.load() == AgentState::Initializing) {
                receiver.agent->state.store(AgentState::Working);
            }
            receiver.agent->last_activity = std::chrono::steady_clock::now();
        }

        ++moved_tasks;
    }

    if (moved_tasks > 0) {
        std::lock_guard<std::mutex> metrics_lock(m_metrics_mutex);
        m_metrics.total_tasks_processed += moved_tasks;
    }
}
#endif

#if !defined(RAWR_QUANTUM_PRIMARY_IMPLS)
QuantumMultiModelAgentCycling::SystemMetrics QuantumMultiModelAgentCycling::getSystemMetrics() const {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    return m_metrics;
}

bool QuantumMultiModelAgentCycling::enableQuantumOptimization() {
    m_quantum_optimization_enabled = true;
    return true;
}

void QuantumMultiModelAgentCycling::agentManagerLoop() {
    while (m_running.load()) {
        updateSystemMetrics();
        detectFailedAgents();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void QuantumMultiModelAgentCycling::loadBalancerLoop() {
    while (m_running.load()) {
        if (m_config.enable_load_balancing) rebalanceWorkload();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void QuantumMultiModelAgentCycling::healthMonitorLoop() {
    while (m_running.load()) {
        detectFailedAgents();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

std::unique_ptr<QuantumMultiModelAgentCycling::AgentInstance> QuantumMultiModelAgentCycling::createAgent(const AgentConfig& config) {
    auto agent = std::make_unique<AgentInstance>();
    agent->config = config;
    agent->state.store(AgentState::Idle);
    return agent;
}

void QuantumMultiModelAgentCycling::startAgent(AgentInstance* agent) {
    if (!agent) {
        return;
    }

    agent->state.store(AgentState::Initializing);
    {
        std::lock_guard<std::mutex> task_lock(agent->task_mutex);
        if (!agent->current_task_id.empty()) {
            agent->incoming_tasks.push(agent->current_task_id);
            agent->current_task_id.clear();
        }
        agent->current_load.store(agent->incoming_tasks.empty() ? 0.0f : 0.25f);
    }

    agent->last_activity = std::chrono::steady_clock::now();
    if (agent->errors_count.load() > 0 && agent->recoveries_count.load() > 0) {
        agent->errors_count.fetch_sub(1);
    }
    agent->state.store(AgentState::Idle);
}
#endif

void QuantumMultiModelAgentCycling::stopAgent(AgentInstance* agent) {
    if (!agent) {
        return;
    }

    agent->state.store(AgentState::Shutting_Down);
    {
        std::lock_guard<std::mutex> task_lock(agent->task_mutex);
        std::queue<std::string> empty;
        std::swap(agent->incoming_tasks, empty);
        agent->current_task_id.clear();
        agent->current_load.store(0.0f);
    }
    agent->last_activity = std::chrono::steady_clock::now();
}

bool QuantumMultiModelAgentCycling::restartAgent(AgentInstance* agent) {
    if (!agent) {
        return false;
    }

    const AgentState previous_state = agent->state.load();
    std::string interrupted_task;
    {
        std::lock_guard<std::mutex> task_lock(agent->task_mutex);
        interrupted_task = agent->current_task_id;
    }

    stopAgent(agent);

    {
        std::lock_guard<std::mutex> task_lock(agent->task_mutex);
        if (previous_state == AgentState::Working || previous_state == AgentState::Thinking ||
            previous_state == AgentState::Debating || previous_state == AgentState::Voting) {
            if (!interrupted_task.empty()) {
                agent->incoming_tasks.push(interrupted_task);
            }
        }
        agent->current_task_id.clear();
    }

    agent->recoveries_count.fetch_add(1);
    startAgent(agent);
    return agent->state.load() == AgentState::Idle;
}

#if !defined(RAWR_QUANTUM_PRIMARY_IMPLS)
QuantumMultiModelAgentCycling::ModelType QuantumMultiModelAgentCycling::selectModelAdaptive(const TaskDefinition& task) {
    if (m_quantum_optimization_enabled) return selectModelQuantum(task);
    if (m_config.strategy == CyclingStrategy::Performance_Based) return selectModelByPerformance(task);
    return selectModelRoundRobin();
}
#endif

QuantumMultiModelAgentCycling::ModelType QuantumMultiModelAgentCycling::selectModelRoundRobin() {
    if (m_model_configs.empty()) return ModelType::Qwen2_5_Coder_14B;
    size_t idx = static_cast<size_t>(m_round_robin_index.fetch_add(1));
    auto it = m_model_configs.begin();
    std::advance(it, static_cast<long>(idx % m_model_configs.size()));
    return it->first;
}

QuantumMultiModelAgentCycling::ModelType QuantumMultiModelAgentCycling::selectModelByPerformance(const TaskDefinition& task) {
    ModelType best = selectModelRoundRobin();
    float bestScore = -1.0f;
    for (const auto& [m, _] : m_model_configs) {
        float s = calculateModelScore(m, task);
        if (s > bestScore) { bestScore = s; best = m; }
    }
    return best;
}

QuantumMultiModelAgentCycling::ModelType QuantumMultiModelAgentCycling::selectModelQuantum(const TaskDefinition& task) {
    std::map<ModelType, AgentConfig> model_snapshot;
    std::map<ModelType, float> performance_snapshot;
    std::map<ModelType, float> weight_snapshot;
    CyclingConfig config_snapshot;
    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        model_snapshot = m_model_configs;
        performance_snapshot = m_model_performance_history;
        weight_snapshot = m_config.model_weights;
        config_snapshot = m_config;
    }

    SystemMetrics metrics_snapshot;
    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        metrics_snapshot = m_metrics;
    }

    if (model_snapshot.empty()) {
        return ModelType::Qwen2_5_Coder_14B;
    }

    const float complexity = complexityToScalar(task.complexity);
    const bool high_memory_pressure =
        metrics_snapshot.memory_usage_mb > static_cast<float>(config_snapshot.max_memory_usage_mb) * 0.8f;
    const bool high_cpu_pressure =
        metrics_snapshot.cpu_usage_percent > static_cast<float>(config_snapshot.max_cpu_usage_percent) * 0.9f;

    ModelType best_model = model_snapshot.begin()->first;
    float best_score = -1.0f;

    for (const auto& [model, cfg] : model_snapshot) {
        const float perf = std::clamp(
            performance_snapshot.count(model) ? performance_snapshot[model] : 0.65f,
            0.0f,
            1.0f);
        const float weight = std::clamp(
            weight_snapshot.count(model) ? weight_snapshot[model] : 1.0f,
            0.1f,
            2.5f);

        float score = perf * 0.55f + std::min(1.0f, weight / 2.0f) * 0.20f;
        score += std::clamp(cfg.reliability_score, 0.0f, 1.0f) * 0.15f;
        score -= std::clamp(cfg.cost_per_token, 0.0f, 0.01f) * 10.0f * 0.10f;

        if (task.requires_masm &&
            (model == ModelType::DeepSeek_Coder_33B || model == ModelType::Qwen2_5_Coder_32B)) {
            score += 0.10f;
        }
        if (complexity >= 0.7f && cfg.max_memory_mb >= 8192) {
            score += 0.08f;
        } else if (complexity < 0.4f && cfg.min_memory_mb <= 2048) {
            score += 0.05f;
        }

        if (high_memory_pressure && cfg.min_memory_mb > 4096) {
            score -= 0.12f;
        }
        if (high_cpu_pressure && cfg.cpu_threads > 8) {
            score -= 0.08f;
        }

        if (score > best_score) {
            best_score = score;
            best_model = model;
        }
    }
    return best_model;
}

#if !defined(RAWR_QUANTUM_PRIMARY_IMPLS)
void QuantumMultiModelAgentCycling::updateAgentMetrics(AgentInstance* agent, const ExecutionResult& result) {
    if (!agent) {
        return;
    }

    agent->tasks_completed.fetch_add(1);
    agent->last_activity = std::chrono::steady_clock::now();

    const float quality = std::clamp(
        result.quality_score > 0.0f ? result.quality_score : (result.success ? 0.75f : 0.25f),
        0.0f,
        1.0f);
    const float response_ms = std::max(0.0f, static_cast<float>(result.execution_time.count()));
    const float safety = std::clamp(result.safety_score > 0.0f ? result.safety_score : 0.8f, 0.0f, 1.0f);

    {
        std::lock_guard<std::mutex> task_lock(agent->task_mutex);
        agent->quality_scores.push_back(quality);
        agent->response_times.push_back(response_ms);
        if (agent->quality_scores.size() > 128) {
            agent->quality_scores.erase(agent->quality_scores.begin(),
                                        agent->quality_scores.begin() + static_cast<long>(agent->quality_scores.size() - 128));
        }
        if (agent->response_times.size() > 128) {
            agent->response_times.erase(agent->response_times.begin(),
                                        agent->response_times.begin() + static_cast<long>(agent->response_times.size() - 128));
        }
    }

    if (!result.success) {
        agent->errors_count.fetch_add(1);
    } else if (agent->errors_count.load() > 0) {
        agent->errors_count.fetch_sub(1);
    }

    const float load_from_runtime = std::clamp(response_ms / 120000.0f, 0.0f, 1.0f);
    const float load_from_quality = std::clamp(1.0f - quality, 0.0f, 1.0f) * 0.25f;
    agent->current_load.store(std::clamp(load_from_runtime + load_from_quality, 0.0f, 1.0f));

    // Keep per-model quality/cost feedback warm for selection.
    const ModelType model = agent->config.model_type;
    {
        std::lock_guard<std::mutex> cfg_lock(m_config_mutex);
        const float prev_perf = m_model_performance_history.count(model)
            ? m_model_performance_history[model]
            : 0.7f;
        const float perf_sample = std::clamp(quality * 0.7f + safety * 0.3f, 0.0f, 1.0f);
        m_model_performance_history[model] =
            std::clamp(prev_perf * 0.85f + perf_sample * 0.15f, 0.0f, 1.0f);

        const float prev_quality = agent->config.avg_quality_score;
        agent->config.avg_quality_score = prev_quality <= 0.0f
            ? quality
            : (prev_quality * 0.85f + quality * 0.15f);

        const float prev_latency = agent->config.avg_response_time_ms;
        agent->config.avg_response_time_ms = prev_latency <= 0.0f
            ? response_ms
            : (prev_latency * 0.85f + response_ms * 0.15f);

        const float reliability_sample = result.success ? 1.0f : 0.0f;
        agent->config.reliability_score = std::clamp(
            agent->config.reliability_score * 0.9f + reliability_sample * 0.1f,
            0.0f,
            1.0f);
    }
}
#endif

void QuantumMultiModelAgentCycling::updateSystemMetrics() {
    int active_agents = 0;
    int idle_agents = 0;
    int error_agents = 0;
    int total_completed_tasks = 0;
    double weighted_load = 0.0;
    double response_sum = 0.0;
    int response_samples = 0;
    double quality_sum = 0.0;
    int quality_samples = 0;
    double memory_usage_mb = 0.0;
    double cpu_usage_percent = 0.0;
    std::map<ModelType, float> model_costs;

    {
        std::lock_guard<std::mutex> agents_lock(m_agents_mutex);
        active_agents = static_cast<int>(m_agents.size());
        for (const auto& [agent_id, agent_ptr] : m_agents) {
            (void)agent_id;
            AgentInstance* agent = agent_ptr.get();
            if (!agent) {
                continue;
            }

            const AgentState state = agent->state.load();
            if (state == AgentState::Idle) {
                ++idle_agents;
            } else if (state == AgentState::Error || state == AgentState::Recovering) {
                ++error_agents;
            }

            const int completed = agent->tasks_completed.load();
            total_completed_tasks += completed;

            size_t queue_size = 0;
            {
                std::lock_guard<std::mutex> task_lock(agent->task_mutex);
                queue_size = agent->incoming_tasks.size();
                if (!agent->response_times.empty()) {
                    response_sum += static_cast<double>(agent->response_times.back());
                    ++response_samples;
                }
                if (!agent->quality_scores.empty()) {
                    quality_sum += static_cast<double>(agent->quality_scores.back());
                    ++quality_samples;
                }
            }

            float load = agent->current_load.load();
            if (state == AgentState::Working || state == AgentState::Thinking ||
                state == AgentState::Debating || state == AgentState::Voting) {
                load = std::max(load, 0.60f);
            }
            if (queue_size > 0) {
                load = std::max(load, std::min(1.0f, 0.15f + static_cast<float>(queue_size) * 0.18f));
            }
            load = std::clamp(load, 0.0f, 1.0f);
            agent->current_load.store(load);
            weighted_load += static_cast<double>(load);

            memory_usage_mb += static_cast<double>(agent->config.min_memory_mb) +
                               static_cast<double>(queue_size) * 96.0;
            cpu_usage_percent += static_cast<double>(std::max(1, agent->config.cpu_threads)) * 2.5;

            model_costs[agent->config.model_type] +=
                agent->config.cost_per_token * static_cast<float>(std::max(1, completed));
        }
    }

    float external_cost = 0.0f;
    {
        std::lock_guard<std::mutex> cost_lock(m_cost_mutex);
        for (const auto& [_, value] : m_cost_tracking) {
            external_cost += value;
        }
    }

    const float system_load = active_agents == 0
        ? 0.0f
        : std::clamp(static_cast<float>(weighted_load / static_cast<double>(active_agents)), 0.0f, 1.0f);
    const float avg_response = response_samples > 0
        ? static_cast<float>(response_sum / static_cast<double>(response_samples))
        : 0.0f;
    const float avg_quality = quality_samples > 0
        ? static_cast<float>(quality_sum / static_cast<double>(quality_samples))
        : 0.0f;
    const float reliability = active_agents == 0
        ? 1.0f
        : std::clamp(1.0f - static_cast<float>(error_agents) / static_cast<float>(active_agents), 0.0f, 1.0f);

    std::lock_guard<std::mutex> metrics_lock(m_metrics_mutex);
    m_metrics.active_agents = active_agents;
    m_metrics.idle_agents = idle_agents;
    m_metrics.error_agents = error_agents;
    m_metrics.system_load = system_load;
    m_metrics.memory_usage_mb = static_cast<float>(memory_usage_mb);
    m_metrics.cpu_usage_percent = std::clamp(static_cast<float>(cpu_usage_percent), 0.0f, 100.0f);
    m_metrics.avg_response_time_ms = avg_response;
    m_metrics.avg_consensus_confidence = avg_quality;
    m_metrics.system_reliability = reliability;
    m_metrics.total_tasks_processed = std::max(m_metrics.total_tasks_processed, total_completed_tasks);
    m_metrics.cost_per_model = std::move(model_costs);
    m_metrics.total_cost_estimate = external_cost;
    for (const auto& [_, cost] : m_metrics.cost_per_model) {
        m_metrics.total_cost_estimate += cost;
    }
}

#if !defined(RAWR_QUANTUM_PRIMARY_IMPLS)
float QuantumMultiModelAgentCycling::calculateModelScore(ModelType model, const TaskDefinition& task) {
    float base = 0.5f;
    auto it = m_model_performance_history.find(model);
    if (it != m_model_performance_history.end()) base = it->second;
    base += complexityToScalar(task.complexity) * 0.3f;
    if (task.requires_masm) base += 0.1f;
    return std::clamp(base, 0.0f, 1.0f);
}

void QuantumMultiModelAgentCycling::initializeMasmAcceleration() {
    if (m_masm_enabled) {
        return;
    }

    m_masm_enabled = true;
    m_masm_scheduler_context = this;

    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        for (auto& [model, weight] : m_config.model_weights) {
            const bool masm_favored =
                model == ModelType::DeepSeek_Coder_33B ||
                model == ModelType::Qwen2_5_Coder_32B ||
                model == ModelType::Qwen2_5_Coder_14B;
            if (masm_favored) {
                weight = std::min(2.5f, weight * 1.10f);
            } else {
                weight = std::max(0.1f, weight * 0.98f);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_agents_mutex);
        for (auto& [_, agent] : m_agents) {
            if (!agent) {
                continue;
            }
            const float current = agent->current_load.load();
            agent->current_load.store(std::max(0.05f, current * 0.90f));
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_metrics.system_reliability = std::min(1.0f, m_metrics.system_reliability + 0.03f);
    }
}

void QuantumMultiModelAgentCycling::shutdownMasmAcceleration() {
    if (!m_masm_enabled) {
        return;
    }

    m_masm_enabled = false;
    m_masm_scheduler_context = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        for (auto& [_, weight] : m_config.model_weights) {
            weight = std::clamp(weight * 0.97f, 0.1f, 2.5f);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_agents_mutex);
        for (auto& [_, agent] : m_agents) {
            if (!agent) {
                continue;
            }
            const float current = agent->current_load.load();
            agent->current_load.store(std::min(1.0f, current * 1.05f));
        }
    }
}
#endif

float QuantumMultiModelAgentCycling::calculateSemanticSimilarity(const std::string& a, const std::string& b) {
    if (a.empty() && b.empty()) return 1.0f;
    size_t common = 0;
    for (char c : a) if (b.find(c) != std::string::npos) ++common;
    return static_cast<float>(common) / static_cast<float>(std::max<size_t>(1, std::max(a.size(), b.size())));
}

#if !defined(RAWR_QUANTUM_PRIMARY_IMPLS)
std::vector<std::string> QuantumMultiModelAgentCycling::identifyDisagreements(const std::vector<ExecutionResult>& results) {
    std::vector<std::string> issues;
    if (results.size() < 2) return issues;
    const auto& ref = results.front().output;
    for (size_t i = 1; i < results.size(); ++i) {
        float sim = calculateSemanticSimilarity(ref, results[i].output);
        if (sim < 0.6f) issues.push_back("Divergent output at agent index " + std::to_string(i));
    }
    return issues;
}
#endif

ExecutionResult QuantumMultiModelAgentCycling::resolveDisagreements(const std::vector<ExecutionResult>& results,
                                                                     const std::vector<std::string>& disagreements) {
    ExecutionResult resolved = mergeResults(results, 0.0f);
    if (results.empty()) {
        resolved.success = false;
        resolved.errors.push_back("No agent results to resolve");
        return resolved;
    }

    size_t best_index = 0;
    float best_score = -1.0f;
    for (size_t i = 0; i < results.size(); ++i) {
        const ExecutionResult& r = results[i];
        const float score =
            (r.success ? 0.55f : 0.0f) +
            std::clamp(r.quality_score, 0.0f, 1.0f) * 0.20f +
            std::clamp(r.performance_score, 0.0f, 1.0f) * 0.15f +
            std::clamp(r.safety_score, 0.0f, 1.0f) * 0.10f -
            std::min(0.20f, static_cast<float>(r.errors.size()) * 0.05f);
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }

    const ExecutionResult& winner = results[best_index];
    resolved.task_id = winner.task_id;
    resolved.output = winner.output;
    resolved.iterations_used = winner.iterations_used;
    resolved.agents_used = static_cast<int>(results.size());
    resolved.execution_time = winner.execution_time;

    if (!disagreements.empty()) {
        resolved.warnings.insert(resolved.warnings.end(), disagreements.begin(), disagreements.end());
    }

    // Enforce minimum production floor for unresolved debates.
    resolved.success = winner.success ||
        (resolved.quality_score >= m_config.min_consensus_threshold && resolved.safety_score >= 0.80f);
    if (!resolved.success && resolved.errors.empty()) {
        resolved.errors.push_back("Consensus unresolved: quality/safety thresholds not met");
    }

    resolved.metrics["consensus_source_agent"] = std::to_string(best_index);
    resolved.metrics["consensus_score"] = std::to_string(best_score);
    return resolved;
}

void QuantumMultiModelAgentCycling::distributeTask(const std::string& task_id, const std::vector<int>& agent_ids) {
    if (task_id.empty() || agent_ids.empty()) return;

    {
        std::lock_guard<std::mutex> qlock(m_task_queue_mutex);
        m_pending_tasks.push(task_id);
    }

    std::lock_guard<std::mutex> lock(m_agents_mutex);
    for (int agent_id : agent_ids) {
        auto it = m_agents.find(agent_id);
        if (it == m_agents.end()) continue;

        AgentInstance* agent = it->second.get();
        {
            std::lock_guard<std::mutex> task_lock(agent->task_mutex);
            agent->incoming_tasks.push(task_id);
            agent->current_task_id = task_id;
        }
        if (agent->state.load() == AgentState::Idle) {
            agent->state.store(AgentState::Working);
        }
        agent->last_activity = std::chrono::steady_clock::now();
    }
}

ExecutionResult QuantumMultiModelAgentCycling::collectResults(const std::vector<int>& agent_ids,
                                                               std::chrono::milliseconds timeout) {
    ExecutionResult empty_result;
    if (agent_ids.empty()) {
        empty_result.success = false;
        empty_result.errors.push_back("No agents selected");
        return empty_result;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        bool all_settled = true;
        {
            std::lock_guard<std::mutex> lock(m_agents_mutex);
            for (int agent_id : agent_ids) {
                auto it = m_agents.find(agent_id);
                if (it == m_agents.end()) continue;
                const AgentState state = it->second->state.load();
                if (state == AgentState::Initializing || state == AgentState::Working ||
                    state == AgentState::Thinking || state == AgentState::Debating ||
                    state == AgentState::Voting || state == AgentState::Recovering) {
                    all_settled = false;
                    break;
                }
            }
        }
        if (all_settled) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::vector<ExecutionResult> partial_results;
    {
        std::lock_guard<std::mutex> lock(m_agents_mutex);
        for (int agent_id : agent_ids) {
            auto it = m_agents.find(agent_id);
            if (it == m_agents.end()) {
                ExecutionResult missing;
                missing.success = false;
                missing.errors.push_back("Agent not found: " + std::to_string(agent_id));
                partial_results.push_back(std::move(missing));
                continue;
            }

            AgentInstance* agent = it->second.get();
            ExecutionResult r;
            r.task_id = agent->current_task_id;
            r.agents_used = 1;
            r.iterations_used = agent->tasks_completed.load();
            r.metrics["agent_id"] = std::to_string(agent_id);
            r.metrics["model"] = agent->config.model_name;

            const AgentState state = agent->state.load();
            if (state == AgentState::Error || state == AgentState::Shutting_Down) {
                r.success = false;
                r.errors.push_back("Agent entered failure state");
            } else {
                r.success = true;
                const float quality = agent->quality_scores.empty() ? 0.75f : agent->quality_scores.back();
                const float latency = agent->response_times.empty() ? 0.0f : agent->response_times.back();
                r.quality_score = std::clamp(quality, 0.0f, 1.0f);
                r.performance_score = std::clamp(1.0f - (latency / 60000.0f), 0.0f, 1.0f);
                r.safety_score = 0.9f;
                r.output = "agent[" + std::to_string(agent_id) + "] completed";
            }

            partial_results.push_back(std::move(r));
        }
    }

    if (partial_results.empty()) {
        empty_result.success = false;
        empty_result.errors.push_back("No agent results collected");
        return empty_result;
    }

    return mergeResults(partial_results, m_config.min_consensus_threshold);
}
bool QuantumMultiModelAgentCycling::checkResourceConstraints(int additional_agents) {
    if (additional_agents <= 0) additional_agents = 1;

    SystemMetrics metrics_snapshot;
    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        metrics_snapshot = m_metrics;
    }

    CyclingConfig config_snapshot;
    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        config_snapshot = m_config;
    }

    const int projected_agents = metrics_snapshot.active_agents + additional_agents;
    if (projected_agents > config_snapshot.max_agents) return false;

    const float avg_mem_per_agent =
        metrics_snapshot.active_agents > 0
            ? (metrics_snapshot.memory_usage_mb / static_cast<float>(metrics_snapshot.active_agents))
            : 512.0f;
    const float projected_mem = metrics_snapshot.memory_usage_mb + avg_mem_per_agent * static_cast<float>(additional_agents);
    if (projected_mem > static_cast<float>(config_snapshot.max_memory_usage_mb)) return false;

    const float avg_cpu_per_agent =
        metrics_snapshot.active_agents > 0
            ? (metrics_snapshot.cpu_usage_percent / static_cast<float>(metrics_snapshot.active_agents))
            : 4.0f;
    const float projected_cpu = metrics_snapshot.cpu_usage_percent + avg_cpu_per_agent * static_cast<float>(additional_agents);
    if (projected_cpu > static_cast<float>(config_snapshot.max_cpu_usage_percent)) return false;

    return true;
}
void QuantumMultiModelAgentCycling::optimizeMemoryUsage() {
    // Lightweight pressure control: scale down one step if memory estimate exceeds cap.
    if (m_metrics.memory_usage_mb > static_cast<float>(m_config.max_memory_usage_mb)) {
        const int active = std::max(0, m_metrics.active_agents);
        const int removable = std::max(0, active - m_config.min_agents);
        if (removable > 0) {
            scaleDown(std::min(m_config.scale_step, removable));
        }
    }
}

void QuantumMultiModelAgentCycling::manageCpuUsage() {
    if (m_metrics.cpu_usage_percent > static_cast<float>(m_config.max_cpu_usage_percent)) {
        const int active = std::max(0, m_metrics.active_agents);
        const int removable = std::max(0, active - m_config.min_agents);
        if (removable > 0) {
            scaleDown(std::min(1, removable));
        }
    }
}

void QuantumMultiModelAgentCycling::detectFailedAgents() {
    const auto now = std::chrono::steady_clock::now();
    std::vector<int> failed_agent_ids;

    {
        std::lock_guard<std::mutex> lock(m_agents_mutex);
        for (const auto& [agent_id, agent] : m_agents) {
            const AgentState state = agent->state.load();
            const auto idle_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - agent->last_activity);
            const bool timed_out =
                (state == AgentState::Working || state == AgentState::Thinking ||
                 state == AgentState::Debating || state == AgentState::Voting) &&
                idle_ms > m_config.agent_timeout;
            if (state == AgentState::Error || timed_out) {
                failed_agent_ids.push_back(agent_id);
            }
        }
    }

    for (int agent_id : failed_agent_ids) {
        std::lock_guard<std::mutex> lock(m_agents_mutex);
        auto it = m_agents.find(agent_id);
        if (it == m_agents.end()) continue;
        AgentInstance* agent = it->second.get();
        if (!attemptAgentRecovery(agent)) {
            isolateFailedAgent(agent);
        }
    }
}

bool QuantumMultiModelAgentCycling::attemptAgentRecovery(AgentInstance* agent) {
    if (!agent) return false;

    agent->state.store(AgentState::Recovering);
    const bool restarted = restartAgent(agent);
    if (!restarted) return false;

    agent->recoveries_count.fetch_add(1);
    agent->last_activity = std::chrono::steady_clock::now();
    agent->state.store(AgentState::Idle);
    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_metrics.agent_recoveries++;
    }
    return true;
}

void QuantumMultiModelAgentCycling::isolateFailedAgent(AgentInstance* agent) {
    if (!agent) return;
    agent->state.store(AgentState::Shutting_Down);
    agent->current_load.store(0.0f);
    std::lock_guard<std::mutex> task_lock(agent->task_mutex);
    std::queue<std::string> empty;
    std::swap(agent->incoming_tasks, empty);
}
void QuantumMultiModelAgentCycling::updateModelWeights() {
    std::map<ModelType, float> quality_sum;
    std::map<ModelType, float> latency_sum;
    std::map<ModelType, int> sample_count;

    {
        std::lock_guard<std::mutex> lock(m_agents_mutex);
        for (const auto& [_, agent] : m_agents) {
            const ModelType model = agent->config.model_type;
            const float q = agent->quality_scores.empty() ? 0.75f : std::clamp(agent->quality_scores.back(), 0.0f, 1.0f);
            const float rt = agent->response_times.empty() ? 3000.0f : std::max(0.0f, agent->response_times.back());
            quality_sum[model] += q;
            latency_sum[model] += rt;
            sample_count[model]++;
        }
    }

    std::lock_guard<std::mutex> lock(m_config_mutex);
    for (const auto& [model, _] : m_model_configs) {
        float previous = 0.7f;
        auto history_it = m_model_performance_history.find(model);
        if (history_it != m_model_performance_history.end()) {
            previous = std::clamp(history_it->second, 0.0f, 1.0f);
        }

        float measured = previous;
        auto count_it = sample_count.find(model);
        if (count_it != sample_count.end() && count_it->second > 0) {
            const float avg_q = quality_sum[model] / static_cast<float>(count_it->second);
            const float avg_rt = latency_sum[model] / static_cast<float>(count_it->second);
            const float latency_score = std::clamp(1.0f - (avg_rt / 120000.0f), 0.0f, 1.0f);
            measured = std::clamp(avg_q * 0.7f + latency_score * 0.3f, 0.0f, 1.0f);
        }

        const float blended = std::clamp(previous * 0.8f + measured * 0.2f, 0.0f, 1.0f);
        m_model_performance_history[model] = blended;
        m_config.model_weights[model] = std::clamp(0.1f + blended * 1.9f, 0.1f, 2.0f);
    }
}
void QuantumMultiModelAgentCycling::performanceBasedScaling() {
    updateSystemMetrics();

    SystemMetrics snapshot;
    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        snapshot = m_metrics;
    }

    if (!m_config.enable_dynamic_scaling) return;

    if (snapshot.system_load >= m_config.scale_up_threshold &&
        checkResourceConstraints(m_config.scale_step)) {
        scaleUp(m_config.scale_step);
        return;
    }

    if (snapshot.system_load <= m_config.scale_down_threshold &&
        snapshot.active_agents > m_config.min_agents) {
        const int removable = snapshot.active_agents - m_config.min_agents;
        if (removable > 0) {
            scaleDown(std::min(m_config.scale_step, removable));
        }
    }
}

// Implementations consolidated into quantum_dynamic_time_manager.cpp
#if !defined(RAWR_QUANTUM_PRIMARY_IMPLS) && !defined(RAWRXD_GOLD_BUILD)
float QuantumDynamicTimeManager::calculateQuantumTimeBonus(const ExecutionContext& context) {
    return std::clamp(0.1f + context.complexity_score * 0.2f + context.current_system_load * 0.1f, 0.0f, 0.5f);
}

void QuantumDynamicTimeManager::configurePwshLimits(std::chrono::milliseconds min_time, std::chrono::milliseconds max_time) {
    if (min_time > max_time) std::swap(min_time, max_time);
    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    m_pwsh_config.min_timeout = min_time;
    m_pwsh_config.max_timeout = max_time;
}

void QuantumDynamicTimeManager::setPwshRandomization(bool enable, float min_factor, float max_factor) {
    if (min_factor > max_factor) std::swap(min_factor, max_factor);
    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    m_pwsh_config.enable_randomization = enable;
    m_pwsh_config.min_random_factor = std::max(0.01f, min_factor);
    m_pwsh_config.max_random_factor = std::max(m_pwsh_config.min_random_factor, max_factor);
}

void QuantumDynamicTimeManager::adaptiveOptimizationLoop() {
    while (m_running.load()) {
        adaptProfilesBasedOnHistory();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void QuantumDynamicTimeManager::performanceMonitoringLoop() {
    while (m_running.load()) {
        updateSystemMetrics();
        cleanupOldData();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void QuantumDynamicTimeManager::enableQuantumOptimization(bool enable) {
    if (m_quantum_enabled == enable) {
        return;
    }

    m_quantum_enabled = enable;
    if (enable) {
        m_temporal_uncertainty = std::clamp(m_temporal_uncertainty, 0.05f, 1.0f);
        m_optimization_strength = std::max(0.5f, m_optimization_strength);
        if (m_strategy == AdjustmentStrategy::Conservative) {
            m_strategy = AdjustmentStrategy::Adaptive;
        }
    } else {
        if (m_strategy == AdjustmentStrategy::Quantum) {
            m_strategy = AdjustmentStrategy::Adaptive;
        }
        m_optimization_strength = std::clamp(m_optimization_strength, 0.1f, 1.5f);
    }

    std::lock_guard<std::mutex> lock(m_profiles_mutex);
    for (auto& [_, profile] : m_time_profiles) {
        if (enable) {
            profile.quantum_multiplier = std::min(24.0f, profile.quantum_multiplier * 1.05f);
        } else {
            profile.quantum_multiplier = std::max(1.0f, profile.quantum_multiplier * 0.95f);
        }
        profile.last_update = std::chrono::steady_clock::now();
    }
}

void QuantumDynamicTimeManager::setAdjustmentStrategy(AdjustmentStrategy strategy) {
    m_strategy = strategy;
    switch (strategy) {
    case AdjustmentStrategy::Conservative:
        setLearningRate(std::max(0.01f, m_learning_rate * 0.90f));
        {
            std::lock_guard<std::mutex> lock(m_pwsh_mutex);
            m_pwsh_config.default_timeout = std::min(
                m_pwsh_config.max_timeout,
                m_pwsh_config.default_timeout + std::chrono::milliseconds(5000));
            m_pwsh_config.adjustment_sensitivity = std::max(0.05f, m_pwsh_config.adjustment_sensitivity * 0.90f);
        }
        break;
    case AdjustmentStrategy::Balanced:
        setLearningRate(std::clamp(m_learning_rate, 0.05f, 0.25f));
        break;
    case AdjustmentStrategy::Aggressive:
        setLearningRate(std::min(0.40f, m_learning_rate * 1.10f));
        {
            std::lock_guard<std::mutex> lock(m_pwsh_mutex);
            const auto shrink = std::chrono::milliseconds(2000);
            m_pwsh_config.default_timeout = std::max(
                m_pwsh_config.min_timeout,
                m_pwsh_config.default_timeout - shrink);
            m_pwsh_config.adjustment_sensitivity = std::min(0.95f, m_pwsh_config.adjustment_sensitivity * 1.05f);
        }
        break;
    case AdjustmentStrategy::Adaptive:
        setLearningRate(std::clamp(m_learning_rate, 0.03f, 0.35f));
        break;
    case AdjustmentStrategy::Quantum:
        m_quantum_enabled = true;
        m_optimization_strength = std::max(1.0f, m_optimization_strength);
        setLearningRate(std::min(0.50f, m_learning_rate * 1.10f));
        break;
    }
}

void QuantumDynamicTimeManager::setQuantumParameters(float temporal_uncertainty, float optimization_strength) {
    m_temporal_uncertainty = std::clamp(temporal_uncertainty, 0.0f, 1.0f);
    m_optimization_strength = std::max(0.0f, optimization_strength);
}

std::chrono::milliseconds QuantumDynamicTimeManager::calculateBasicTime(const ExecutionContext& context) {
    return std::chrono::milliseconds(static_cast<int64_t>(60000.0f + context.complexity_score * 240000.0f));
}

std::chrono::milliseconds QuantumDynamicTimeManager::applyComplexityMultiplier(std::chrono::milliseconds base_time, float complexity) {
    float factor = 1.0f + std::clamp(complexity, 0.0f, 1.0f) * 2.0f;
    return std::chrono::milliseconds(static_cast<int64_t>(static_cast<float>(base_time.count()) * factor));
}

std::chrono::milliseconds QuantumDynamicTimeManager::applySystemFactors(std::chrono::milliseconds base_time, const ExecutionContext& context) {
    float factor = 1.0f + context.current_system_load * 0.5f + (1.0f - context.network_quality) * 0.2f;
    return std::chrono::milliseconds(static_cast<int64_t>(static_cast<float>(base_time.count()) * factor));
}

void QuantumDynamicTimeManager::updateProfileWithFeedback(TimeProfile& profile,
                                                          std::chrono::milliseconds actual_time,
                                                          std::chrono::milliseconds predicted_time,
                                                          bool success) {
    const long long predicted_ms = std::max<long long>(1, predicted_time.count());
    const float ratio = static_cast<float>(actual_time.count()) / static_cast<float>(predicted_ms);
    const float alpha = std::clamp(m_learning_rate, 0.01f, 0.60f);

    profile.avg_completion_ratio =
        std::clamp((1.0f - alpha) * profile.avg_completion_ratio + alpha * ratio, 0.0f, 4.0f);
    profile.success_rate =
        std::clamp((1.0f - alpha) * profile.success_rate + alpha * (success ? 1.0f : 0.0f), 0.0f, 1.0f);
    profile.adaptation_samples = std::max(0, profile.adaptation_samples + 1);
    profile.last_update = std::chrono::steady_clock::now();

    const float base_adjust = std::clamp(1.0f + (ratio - 1.0f) * alpha * 0.5f, 0.75f, 1.30f);
    profile.base_time = std::chrono::milliseconds(
        static_cast<int64_t>(std::max(1.0,
            static_cast<double>(profile.base_time.count()) * static_cast<double>(base_adjust))));

    std::vector<float> perf = {ratio, success ? 1.0f : 0.0f};
    adaptMultipliers(profile, perf);
}

void QuantumDynamicTimeManager::adaptMultipliers(TimeProfile& profile,
                                                 const std::vector<float>& performance_data) {
    if (performance_data.empty()) {
        return;
    }
    const float avg = std::accumulate(performance_data.begin(), performance_data.end(), 0.0f) /
        static_cast<float>(performance_data.size());

    if (avg > 1.1f) {
        profile.simple_multiplier *= 1.01f;
        profile.moderate_multiplier *= 1.02f;
        profile.complex_multiplier *= 1.03f;
    } else if (avg < 0.9f) {
        profile.simple_multiplier *= 0.99f;
        profile.moderate_multiplier *= 0.98f;
        profile.complex_multiplier *= 0.97f;
    }

    profile.simple_multiplier = std::clamp(profile.simple_multiplier, 0.1f, 2.5f);
    profile.moderate_multiplier = std::clamp(profile.moderate_multiplier, 0.3f, 4.0f);
    profile.complex_multiplier = std::clamp(profile.complex_multiplier, 0.5f, 8.0f);
}

float QuantumDynamicTimeManager::calculatePredictionError(const std::vector<std::chrono::milliseconds>& predicted,
                                                          const std::vector<std::chrono::milliseconds>& actual) {
    if (predicted.empty() || predicted.size() != actual.size()) return 0.0f;
    float acc = 0.0f;
    for (size_t i = 0; i < predicted.size(); ++i) {
        float p = static_cast<float>(std::max<int64_t>(1, predicted[i].count()));
        float a = static_cast<float>(actual[i].count());
        acc += std::abs(a - p) / p;
    }
    return acc / static_cast<float>(predicted.size());
}

std::chrono::milliseconds QuantumDynamicTimeManager::generateRandomTimeout(std::chrono::milliseconds base_timeout) {
    float factor = m_uniform_dist(m_random_generator);
    factor = std::clamp(factor, m_pwsh_config.min_random_factor, m_pwsh_config.max_random_factor);
    return std::chrono::milliseconds(static_cast<int64_t>(static_cast<float>(base_timeout.count()) * factor));
}

std::chrono::milliseconds QuantumDynamicTimeManager::getCommandSpecificTimeout(const std::string& command) {
    auto it = m_pwsh_config.command_timeouts.find(command);
    if (it != m_pwsh_config.command_timeouts.end()) return it->second;
    return m_pwsh_config.default_timeout;
}

void QuantumDynamicTimeManager::updatePwshStatistics(const std::string&, std::chrono::milliseconds duration, bool success) {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    m_metrics.pwsh_executions++;
    if (!success) m_metrics.pwsh_timeouts++;
    m_metrics.avg_pwsh_duration_ms =
        (m_metrics.avg_pwsh_duration_ms * static_cast<float>(m_metrics.pwsh_executions - 1) +
         static_cast<float>(duration.count())) / static_cast<float>(std::max(1, m_metrics.pwsh_executions));
}

void QuantumDynamicTimeManager::initializeMasmAcceleration() {
    m_masm_context = nullptr;
}

void QuantumDynamicTimeManager::shutdownMasmAcceleration() {
    m_masm_context = nullptr;
}

std::chrono::milliseconds QuantumDynamicTimeManager::getMasmPrediction(const ExecutionContext& context) {
    std::chrono::milliseconds predicted = calculateBasicTime(context);

    // Apply model count and shell requirements to better approximate real orchestration cost.
    if (context.agent_count > 1) {
        const int extra_agents = context.agent_count - 1;
        predicted += std::chrono::milliseconds(extra_agents * 1200);
    }
    if (context.requires_pwsh) {
        predicted += std::chrono::milliseconds(3000);
    }
    if (context.is_production_critical) {
        predicted += std::chrono::milliseconds(5000);
    }

    float adjustment = 1.0f;
    if (m_masm_enabled) {
        adjustment *= 0.92f;
    }
    if (m_quantum_enabled) {
        adjustment += calculateQuantumTimeBonus(context) * 0.2f;
    }

    const auto adjusted = std::chrono::milliseconds(
        static_cast<int64_t>(static_cast<double>(predicted.count()) * static_cast<double>(adjustment)));

    std::chrono::milliseconds clamped = std::max(std::chrono::milliseconds(1000), adjusted);
    if (const TimeProfile* profile = getTimeProfile(context.task_type)) {
        clamped = std::max(profile->min_time, std::min(clamped, profile->max_time));
    }
    return clamped;
}

float QuantumDynamicTimeManager::calculateQuantumUncertainty(const ExecutionContext& context) {
    float uncertainty = 0.05f;
    uncertainty += std::clamp(context.complexity_score, 0.0f, 1.0f) * 0.20f;
    uncertainty += std::clamp(1.0f - context.recent_success_rate, 0.0f, 1.0f) * 0.30f;
    uncertainty += std::clamp(m_cached_system_load, 0.0f, 1.0f) * 0.15f;
    uncertainty += std::clamp(m_cached_memory_pressure, 0.0f, 1.0f) * 0.15f;
    uncertainty += std::clamp(m_temporal_uncertainty, 0.0f, 1.0f) * 0.20f;
    return std::clamp(uncertainty, 0.0f, 1.0f);
}

std::chrono::milliseconds QuantumDynamicTimeManager::applyQuantumOptimization(
    std::chrono::milliseconds base_time,
    const ExecutionContext& context) {
    const float uncertainty = calculateQuantumUncertainty(context);
    const float random_jitter = (m_uniform_dist(m_random_generator) - 0.5f) * uncertainty * 0.40f;
    float factor = 1.0f - (0.15f * std::max(0.1f, m_optimization_strength)) + random_jitter;
    factor = std::clamp(factor, 0.55f, 1.60f);

    std::chrono::milliseconds optimized(
        static_cast<int64_t>(std::max(1.0,
            static_cast<double>(base_time.count()) * static_cast<double>(factor))));
    const auto min_duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1));
    const auto max_duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(24));
    optimized = std::max(min_duration, std::min(optimized, max_duration));
    return optimized;
}
float QuantumDynamicTimeManager::getCurrentSystemLoad() {
#if defined(_WIN32)
    FILETIME idle_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};
    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        return m_cached_system_load;
    }

    static std::mutex sample_mutex;
    static uint64_t last_idle = 0;
    static uint64_t last_kernel = 0;
    static uint64_t last_user = 0;
    static bool initialized = false;

    ULARGE_INTEGER idle{};
    ULARGE_INTEGER kernel{};
    ULARGE_INTEGER user{};
    idle.LowPart = idle_time.dwLowDateTime;
    idle.HighPart = idle_time.dwHighDateTime;
    kernel.LowPart = kernel_time.dwLowDateTime;
    kernel.HighPart = kernel_time.dwHighDateTime;
    user.LowPart = user_time.dwLowDateTime;
    user.HighPart = user_time.dwHighDateTime;

    std::lock_guard<std::mutex> lock(sample_mutex);
    if (!initialized) {
        last_idle = idle.QuadPart;
        last_kernel = kernel.QuadPart;
        last_user = user.QuadPart;
        initialized = true;
        return m_cached_system_load;
    }

    const uint64_t idle_delta = idle.QuadPart - last_idle;
    const uint64_t kernel_delta = kernel.QuadPart - last_kernel;
    const uint64_t user_delta = user.QuadPart - last_user;

    last_idle = idle.QuadPart;
    last_kernel = kernel.QuadPart;
    last_user = user.QuadPart;

    const uint64_t total = kernel_delta + user_delta;
    if (total == 0) {
        return m_cached_system_load;
    }

    const float busy = 1.0f - (static_cast<float>(idle_delta) / static_cast<float>(total));
    m_cached_system_load = std::clamp(busy, 0.0f, 1.0f);
#endif
    return m_cached_system_load;
}

float QuantumDynamicTimeManager::getMemoryPressure() {
#if defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        m_cached_memory_pressure = std::clamp(static_cast<float>(status.dwMemoryLoad) / 100.0f, 0.0f, 1.0f);
    }
#endif
    return m_cached_memory_pressure;
}
float QuantumDynamicTimeManager::getNetworkQuality() {
    size_t sample_count = 0;
    size_t success_count = 0;
    float duration_penalty = 0.0f;

    {
        std::lock_guard<std::mutex> pwsh_lock(m_pwsh_mutex);

        auto samples = m_recent_pwsh_results;
        while (!samples.empty() && sample_count < 64) {
            const auto& entry = samples.front();
            if (entry.second) {
                ++success_count;
            }
            ++sample_count;
            samples.pop();
        }

        size_t network_like = 0;
        double network_duration = 0.0;
        for (const auto& [command, duration] : m_pwsh_history) {
            if (command.find("http") != std::string::npos ||
                command.find("curl") != std::string::npos ||
                command.find("wget") != std::string::npos ||
                command.find("download") != std::string::npos ||
                command.find("upload") != std::string::npos ||
                command.find("ping") != std::string::npos) {
                network_duration += static_cast<double>(duration.count());
                ++network_like;
            }
        }
        if (network_like > 0) {
            const double avg_ms = network_duration / static_cast<double>(network_like);
            duration_penalty = std::clamp(static_cast<float>(avg_ms / 120000.0), 0.0f, 0.35f);
        }
    }

    float timeout_rate = 0.0f;
    {
        std::lock_guard<std::mutex> metrics_lock(m_metrics_mutex);
        if (m_metrics.pwsh_executions > 0) {
            timeout_rate = static_cast<float>(m_metrics.pwsh_timeouts) /
                           static_cast<float>(m_metrics.pwsh_executions);
        }
    }

    const float success_rate =
        sample_count > 0 ? static_cast<float>(success_count) / static_cast<float>(sample_count) : 1.0f;
    const float pressure_penalty =
        std::clamp(m_cached_system_load * 0.20f + m_cached_memory_pressure * 0.10f, 0.0f, 0.30f);

    const float derived = std::clamp(
        success_rate * 0.65f + (1.0f - timeout_rate) * 0.25f + (1.0f - pressure_penalty) * 0.10f - duration_penalty,
        0.05f,
        1.0f);
    m_cached_network_quality = derived;
    return m_cached_network_quality;
}
void QuantumDynamicTimeManager::updateSystemMetrics() {
    const auto now = std::chrono::steady_clock::now();
    if (now - m_last_system_update < std::chrono::milliseconds(500)) {
        return;
    }

    size_t history_count = 0;
    double avg_exec_ms = 0.0;
    float success_rate = 1.0f;
    int timeout_count = 0;
    int pwsh_exec_count = 0;

    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        history_count = m_execution_history.size();
        if (!m_execution_history.empty()) {
            int64_t total_ms = 0;
            for (const auto& d : m_execution_history) {
                total_ms += d.count();
            }
            avg_exec_ms = static_cast<double>(total_ms) / static_cast<double>(m_execution_history.size());
        }
        if (!m_success_history.empty()) {
            const size_t ok = static_cast<size_t>(std::count(m_success_history.begin(), m_success_history.end(), true));
            success_rate = static_cast<float>(ok) / static_cast<float>(m_success_history.size());
        }
        timeout_count = m_metrics.timeout_failures;
        pwsh_exec_count = m_metrics.pwsh_executions;
    }

    // Derive system health proxies from execution telemetry when OS probes are unavailable.
    const float latency_load = std::clamp(static_cast<float>(avg_exec_ms / 300000.0), 0.0f, 1.0f);
    const float queue_load = std::clamp(static_cast<float>(history_count) / 64.0f, 0.0f, 1.0f);
    const float timeout_rate = pwsh_exec_count > 0
        ? static_cast<float>(timeout_count) / static_cast<float>(pwsh_exec_count)
        : 0.0f;

    m_cached_system_load = std::clamp(latency_load * 0.55f + queue_load * 0.25f + (1.0f - success_rate) * 0.20f, 0.0f, 1.0f);
    m_cached_memory_pressure = std::clamp(queue_load * 0.60f + (1.0f - success_rate) * 0.25f + timeout_rate * 0.15f, 0.0f, 1.0f);
    m_cached_network_quality = std::clamp(1.0f - timeout_rate * 0.70f - m_cached_system_load * 0.20f, 0.1f, 1.0f);
    m_last_system_update = now;
}

void QuantumDynamicTimeManager::cleanupOldData() {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    while (m_execution_history.size() > static_cast<size_t>(std::max(32, m_adaptation_window_size))) {
        m_execution_history.erase(m_execution_history.begin());
    }
    while (m_success_history.size() > static_cast<size_t>(std::max(32, m_adaptation_window_size))) {
        m_success_history.erase(m_success_history.begin());
    }
}

void QuantumDynamicTimeManager::saveProfilesToDisk() {
    std::map<std::string, TimeProfile> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_profiles_mutex);
        snapshot = m_time_profiles;
    }

    std::ofstream out("quantum_time_profiles.dat", std::ios::trunc);
    if (!out.is_open()) {
        return;
    }

    out << "RAWRXD_QTM|2\n";
    for (const auto& [task, profile] : snapshot) {
        std::string safe_task = task;
        std::replace(safe_task.begin(), safe_task.end(), '|', '_');
        out << safe_task << "|"
            << static_cast<int>(profile.category) << "|"
            << profile.base_time.count() << "|"
            << profile.min_time.count() << "|"
            << profile.max_time.count() << "|"
            << profile.simple_multiplier << "|"
            << profile.moderate_multiplier << "|"
            << profile.complex_multiplier << "|"
            << profile.expert_multiplier << "|"
            << profile.quantum_multiplier << "|"
            << profile.success_rate << "|"
            << profile.avg_completion_ratio << "|"
            << profile.adaptation_samples << "\n";
    }
}

void QuantumDynamicTimeManager::loadProfilesFromDisk() {
    std::ifstream in("quantum_time_profiles.dat");
    if (!in.is_open()) {
        return;
    }

    std::string header;
    if (!std::getline(in, header) || header.rfind("RAWRXD_QTM|", 0) != 0) {
        return;
    }

    std::map<std::string, TimeProfile> loaded;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string token;
        while (std::getline(ss, token, '|')) {
            parts.push_back(token);
        }
        if (parts.size() < 13) {
            continue;
        }

        try {
            TimeProfile profile;
            profile.task_type = parts[0];
            profile.category = static_cast<TimeCategory>(std::stoi(parts[1]));
            profile.base_time = std::chrono::milliseconds(std::stoll(parts[2]));
            profile.min_time = std::chrono::milliseconds(std::stoll(parts[3]));
            profile.max_time = std::chrono::milliseconds(std::stoll(parts[4]));
            profile.simple_multiplier = std::stof(parts[5]);
            profile.moderate_multiplier = std::stof(parts[6]);
            profile.complex_multiplier = std::stof(parts[7]);
            profile.expert_multiplier = std::stof(parts[8]);
            profile.quantum_multiplier = std::stof(parts[9]);
            profile.success_rate = std::stof(parts[10]);
            profile.avg_completion_ratio = std::stof(parts[11]);
            profile.adaptation_samples = std::stoi(parts[12]);
            profile.last_update = std::chrono::steady_clock::now();
            profile.min_time = std::max(std::chrono::milliseconds(1000), profile.min_time);
            profile.max_time = std::max(profile.min_time, profile.max_time);
            profile.base_time = std::max(profile.min_time, std::min(profile.base_time, profile.max_time));
            loaded[profile.task_type] = profile;
        } catch (...) {
            continue;
        }
    }

    if (!loaded.empty()) {
        std::lock_guard<std::mutex> lock(m_profiles_mutex);
        for (const auto& [task, profile] : loaded) {
            m_time_profiles[task] = profile;
        }
    }
}

void QuantumDynamicTimeManager::updatePowerShellConfig(const PowerShellConfig& config) {
    PowerShellConfig normalized = config;
    if (normalized.min_timeout > normalized.max_timeout) {
        std::swap(normalized.min_timeout, normalized.max_timeout);
    }

    normalized.min_timeout = std::max(std::chrono::milliseconds(1000), normalized.min_timeout);
    normalized.max_timeout = std::max(normalized.min_timeout, normalized.max_timeout);
    normalized.default_timeout =
        std::max(normalized.min_timeout, std::min(normalized.default_timeout, normalized.max_timeout));

    normalized.min_random_factor = std::clamp(normalized.min_random_factor, 0.10f, 2.00f);
    normalized.max_random_factor = std::clamp(normalized.max_random_factor, normalized.min_random_factor, 3.00f);
    normalized.adjustment_sensitivity = std::clamp(normalized.adjustment_sensitivity, 0.01f, 1.00f);
    normalized.success_rate_threshold = std::clamp(normalized.success_rate_threshold, 0.10f, 0.99f);
    normalized.max_concurrent_terminals = std::clamp(normalized.max_concurrent_terminals, 1, 64);
    normalized.terminal_idle_timeout =
        std::max(std::chrono::milliseconds(1000), normalized.terminal_idle_timeout);

    for (auto it = normalized.command_timeouts.begin(); it != normalized.command_timeouts.end();) {
        if (it->first.empty()) {
            it = normalized.command_timeouts.erase(it);
            continue;
        }
        it->second = std::max(normalized.min_timeout, std::min(it->second, normalized.max_timeout));
        ++it;
    }

    if (normalized.command_timeouts.find("build") == normalized.command_timeouts.end()) {
        const auto build_floor =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::minutes(5));
        normalized.command_timeouts["build"] = std::max(build_floor, normalized.default_timeout);
    }
    if (normalized.command_timeouts.find("test") == normalized.command_timeouts.end()) {
        const auto test_floor =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::minutes(3));
        normalized.command_timeouts["test"] = std::max(test_floor, normalized.default_timeout);
    }

    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    m_pwsh_config = normalized;
}

QuantumDynamicTimeManager::PowerShellConfig QuantumDynamicTimeManager::getPowerShellConfig() const {
    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    return m_pwsh_config;
}

void QuantumDynamicTimeManager::enableMasmAcceleration(bool enable) {
    if (enable == m_masm_enabled) {
        return;
    }

    if (enable) {
        initializeMasmAcceleration();
        m_masm_enabled = true;

        std::lock_guard<std::mutex> profile_lock(m_profiles_mutex);
        for (auto& [_, profile] : m_time_profiles) {
            profile.cpu_contention_factor = std::max(0.75f, profile.cpu_contention_factor * 0.92f);
            profile.last_update = std::chrono::steady_clock::now();
        }
    } else {
        shutdownMasmAcceleration();
        m_masm_enabled = false;

        std::lock_guard<std::mutex> profile_lock(m_profiles_mutex);
        for (auto& [_, profile] : m_time_profiles) {
            profile.cpu_contention_factor = std::min(1.25f, profile.cpu_contention_factor * 1.05f);
            profile.last_update = std::chrono::steady_clock::now();
        }
    }
}

void QuantumDynamicTimeManager::setLearningRate(float rate) {
    if (!std::isfinite(rate)) {
        return;
    }
    m_learning_rate = std::clamp(rate, 0.001f, 1.0f);

    {
        std::lock_guard<std::mutex> metrics_lock(m_metrics_mutex);
        m_metrics.learning_rate = m_learning_rate;
    }

    const int target_window =
        static_cast<int>(std::clamp(32.0f + (1.0f - m_learning_rate) * 96.0f, 24.0f, 160.0f));
    m_adaptation_window_size = target_window;
}
void QuantumDynamicTimeManager::configureCheckpoints(const std::vector<std::string>& checkpoint_names) {
    if (checkpoint_names.empty()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> profile_lock(m_profiles_mutex);
        for (size_t i = 0; i < checkpoint_names.size(); ++i) {
            const std::string& name = checkpoint_names[i];
            if (name.empty()) {
                continue;
            }

            TimeProfile profile{};
            auto it = m_time_profiles.find(name);
            if (it != m_time_profiles.end()) {
                profile = it->second;
            }

            const int64_t idx = static_cast<int64_t>(i);
            profile.task_type = name;
            profile.base_time = std::chrono::milliseconds(45000 + idx * 15000);
            profile.min_time = std::chrono::milliseconds(5000 + idx * 1000);
            profile.max_time = std::chrono::milliseconds(900000 + idx * 120000);
            profile.last_update = now;
            m_time_profiles[name] = profile;
        }
    }

    {
        std::lock_guard<std::mutex> pwsh_lock(m_pwsh_mutex);
        for (size_t i = 0; i < checkpoint_names.size(); ++i) {
            const std::string& name = checkpoint_names[i];
            if (name.empty()) {
                continue;
            }
            if (m_pwsh_config.command_timeouts.find(name) == m_pwsh_config.command_timeouts.end()) {
                const int64_t idx = static_cast<int64_t>(i);
                m_pwsh_config.command_timeouts[name] = std::chrono::milliseconds(30000 + idx * 10000);
            }
        }
    }
}

void QuantumDynamicTimeManager::addTimeProfile(const std::string& task_type, const TimeProfile& profile) {
    if (task_type.empty()) {
        return;
    }

    TimeProfile normalized = profile;
    normalized.task_type = task_type;
    if (normalized.base_time <= std::chrono::milliseconds::zero()) {
        normalized.base_time = std::chrono::minutes(5);
    }
    normalized.min_time = std::max(std::chrono::milliseconds(1000), normalized.min_time);
    normalized.max_time = std::max(normalized.min_time, normalized.max_time);
    normalized.base_time = std::max(normalized.min_time, std::min(normalized.base_time, normalized.max_time));

    normalized.simple_multiplier = std::clamp(normalized.simple_multiplier, 0.1f, 2.5f);
    normalized.moderate_multiplier = std::clamp(normalized.moderate_multiplier, 0.3f, 4.0f);
    normalized.complex_multiplier = std::clamp(normalized.complex_multiplier, 0.5f, 8.0f);
    normalized.expert_multiplier = std::clamp(normalized.expert_multiplier, 1.0f, 16.0f);
    normalized.quantum_multiplier = std::clamp(normalized.quantum_multiplier, 1.0f, 24.0f);

    normalized.success_rate = std::clamp(normalized.success_rate, 0.0f, 1.0f);
    normalized.avg_completion_ratio = std::clamp(normalized.avg_completion_ratio, 0.0f, 4.0f);
    normalized.system_load_factor = std::clamp(normalized.system_load_factor, 0.5f, 3.0f);
    normalized.network_latency_factor = std::clamp(normalized.network_latency_factor, 0.5f, 3.0f);
    normalized.memory_pressure_factor = std::clamp(normalized.memory_pressure_factor, 0.5f, 3.0f);
    normalized.cpu_contention_factor = std::clamp(normalized.cpu_contention_factor, 0.5f, 3.0f);
    normalized.adaptation_samples = std::max(0, normalized.adaptation_samples);
    normalized.last_update = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(m_profiles_mutex);
    m_time_profiles[task_type] = normalized;
}

void QuantumDynamicTimeManager::updateTimeProfile(const std::string& task_type, const TimeProfile& profile) {
    if (task_type.empty()) {
        return;
    }

    TimeProfile incoming = profile;
    incoming.task_type = task_type;
    incoming.min_time = std::max(std::chrono::milliseconds(1000), incoming.min_time);
    incoming.max_time = std::max(incoming.min_time, incoming.max_time);
    if (incoming.base_time <= std::chrono::milliseconds::zero()) {
        incoming.base_time = std::chrono::minutes(5);
    }
    incoming.base_time = std::max(incoming.min_time, std::min(incoming.base_time, incoming.max_time));
    incoming.simple_multiplier = std::clamp(incoming.simple_multiplier, 0.1f, 2.5f);
    incoming.moderate_multiplier = std::clamp(incoming.moderate_multiplier, 0.3f, 4.0f);
    incoming.complex_multiplier = std::clamp(incoming.complex_multiplier, 0.5f, 8.0f);
    incoming.expert_multiplier = std::clamp(incoming.expert_multiplier, 1.0f, 16.0f);
    incoming.quantum_multiplier = std::clamp(incoming.quantum_multiplier, 1.0f, 24.0f);
    incoming.success_rate = std::clamp(incoming.success_rate, 0.0f, 1.0f);
    incoming.avg_completion_ratio = std::clamp(incoming.avg_completion_ratio, 0.0f, 4.0f);
    incoming.system_load_factor = std::clamp(incoming.system_load_factor, 0.5f, 3.0f);
    incoming.network_latency_factor = std::clamp(incoming.network_latency_factor, 0.5f, 3.0f);
    incoming.memory_pressure_factor = std::clamp(incoming.memory_pressure_factor, 0.5f, 3.0f);
    incoming.cpu_contention_factor = std::clamp(incoming.cpu_contention_factor, 0.5f, 3.0f);
    incoming.adaptation_samples = std::max(0, incoming.adaptation_samples);

    auto blend_duration = [this](std::chrono::milliseconds current, std::chrono::milliseconds incoming) {
        const float alpha = std::clamp(m_learning_rate, 0.05f, 0.6f);
        const double value = static_cast<double>(current.count()) * static_cast<double>(1.0f - alpha) +
                             static_cast<double>(incoming.count()) * static_cast<double>(alpha);
        return std::chrono::milliseconds(static_cast<int64_t>(std::max(1.0, value)));
    };
    auto blend_float = [this](float current, float incoming) {
        const float alpha = std::clamp(m_learning_rate, 0.05f, 0.6f);
        return current * (1.0f - alpha) + incoming * alpha;
    };

    std::lock_guard<std::mutex> lock(m_profiles_mutex);
    auto it = m_time_profiles.find(task_type);
    if (it == m_time_profiles.end()) {
        incoming.last_update = std::chrono::steady_clock::now();
        m_time_profiles[task_type] = incoming;
        return;
    }

    TimeProfile& target = it->second;
    target.base_time = blend_duration(target.base_time, incoming.base_time);
    target.min_time = blend_duration(target.min_time, incoming.min_time);
    target.max_time = std::max(target.min_time, blend_duration(target.max_time, incoming.max_time));

    target.simple_multiplier = std::clamp(blend_float(target.simple_multiplier, incoming.simple_multiplier), 0.1f, 2.5f);
    target.moderate_multiplier = std::clamp(blend_float(target.moderate_multiplier, incoming.moderate_multiplier), 0.3f, 4.0f);
    target.complex_multiplier = std::clamp(blend_float(target.complex_multiplier, incoming.complex_multiplier), 0.5f, 8.0f);
    target.expert_multiplier = std::clamp(blend_float(target.expert_multiplier, incoming.expert_multiplier), 1.0f, 16.0f);
    target.quantum_multiplier = std::clamp(blend_float(target.quantum_multiplier, incoming.quantum_multiplier), 1.0f, 24.0f);

    target.success_rate = std::clamp(blend_float(target.success_rate, incoming.success_rate), 0.0f, 1.0f);
    target.avg_completion_ratio = std::clamp(blend_float(target.avg_completion_ratio, incoming.avg_completion_ratio), 0.0f, 4.0f);
    target.system_load_factor = std::clamp(blend_float(target.system_load_factor, incoming.system_load_factor), 0.5f, 3.0f);
    target.network_latency_factor = std::clamp(blend_float(target.network_latency_factor, incoming.network_latency_factor), 0.5f, 3.0f);
    target.memory_pressure_factor = std::clamp(blend_float(target.memory_pressure_factor, incoming.memory_pressure_factor), 0.5f, 3.0f);
    target.cpu_contention_factor = std::clamp(blend_float(target.cpu_contention_factor, incoming.cpu_contention_factor), 0.5f, 3.0f);

    target.adaptation_samples = std::max(target.adaptation_samples, incoming.adaptation_samples);
    target.category = incoming.category;
    target.last_update = std::chrono::steady_clock::now();
}

QuantumDynamicTimeManager::TimeProfile* QuantumDynamicTimeManager::getTimeProfile(const std::string& task_type) {
    std::lock_guard<std::mutex> lock(m_profiles_mutex);
    auto it = m_time_profiles.find(task_type);
    return it == m_time_profiles.end() ? nullptr : &it->second;
}

void QuantumDynamicTimeManager::optimizeGlobalSettings() {
    AdaptationMetrics metrics_snapshot;
    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        metrics_snapshot = m_metrics;
    }
    if (metrics_snapshot.total_tasks < 4) return;

    const float success_rate =
        static_cast<float>(metrics_snapshot.successful_completions) /
        static_cast<float>(std::max(1, metrics_snapshot.total_tasks));
    const float timeout_rate =
        static_cast<float>(metrics_snapshot.timeout_failures) /
        static_cast<float>(std::max(1, metrics_snapshot.total_tasks));

    {
        std::lock_guard<std::mutex> lock(m_pwsh_mutex);
        if (timeout_rate > 0.2f) {
            m_pwsh_config.default_timeout = std::min(m_pwsh_config.max_timeout,
                m_pwsh_config.default_timeout + std::chrono::milliseconds(15000));
            m_pwsh_config.adjustment_sensitivity = std::min(0.8f, m_pwsh_config.adjustment_sensitivity + 0.05f);
        } else if (success_rate > 0.9f && m_pwsh_config.default_timeout > m_pwsh_config.min_timeout) {
            const auto shrink = std::chrono::milliseconds(5000);
            m_pwsh_config.default_timeout = std::max(m_pwsh_config.min_timeout, m_pwsh_config.default_timeout - shrink);
            m_pwsh_config.adjustment_sensitivity = std::max(0.05f, m_pwsh_config.adjustment_sensitivity - 0.02f);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_profiles_mutex);
        for (auto& [_, profile] : m_time_profiles) {
            if (timeout_rate > 0.2f) {
                profile.complex_multiplier = std::min(6.0f, profile.complex_multiplier * 1.05f);
                profile.expert_multiplier = std::min(12.0f, profile.expert_multiplier * 1.05f);
            } else if (success_rate > 0.9f) {
                profile.simple_multiplier = std::max(0.3f, profile.simple_multiplier * 0.98f);
                profile.moderate_multiplier = std::max(0.6f, profile.moderate_multiplier * 0.99f);
            }
            profile.last_update = std::chrono::steady_clock::now();
        }
    }

    m_learning_rate = std::clamp(success_rate * 0.25f, 0.01f, 0.25f);
}

void QuantumDynamicTimeManager::resetLearning() {
    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_metrics = AdaptationMetrics{};
        m_execution_history.clear();
        m_success_history.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_profiles_mutex);
        const auto now = std::chrono::steady_clock::now();
        for (auto& [_, profile] : m_time_profiles) {
            profile.success_rate = 0.8f;
            profile.avg_completion_ratio = 0.7f;
            profile.adaptation_samples = 0;
            profile.last_update = now;
        }
    }

    m_learning_rate = 0.1f;
    m_cached_system_load = 0.5f;
    m_cached_memory_pressure = 0.3f;
    m_cached_network_quality = 1.0f;
}

void QuantumDynamicTimeManager::adjustForSystemLoad(float load_factor) {
    const float clamped = std::clamp(load_factor, 0.0f, 2.0f);
    m_cached_system_load = clamped;

    {
        std::lock_guard<std::mutex> lock(m_profiles_mutex);
        for (auto& [_, profile] : m_time_profiles) {
            profile.system_load_factor = std::clamp(0.8f + clamped * 0.8f, 0.5f, 2.5f);
            profile.last_update = std::chrono::steady_clock::now();
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_pwsh_mutex);
        if (clamped > 1.0f) {
            m_pwsh_config.default_timeout = std::min(
                m_pwsh_config.max_timeout,
                m_pwsh_config.default_timeout + std::chrono::milliseconds(static_cast<int>(clamped * 4000.0f)));
        }
    }
}

void QuantumDynamicTimeManager::adjustForMemoryPressure(float memory_factor) {
    const float clamped = std::clamp(memory_factor, 0.0f, 2.0f);
    m_cached_memory_pressure = clamped;

    {
        std::lock_guard<std::mutex> lock(m_profiles_mutex);
        for (auto& [_, profile] : m_time_profiles) {
            profile.memory_pressure_factor = std::clamp(0.8f + clamped * 0.9f, 0.5f, 2.8f);
            profile.last_update = std::chrono::steady_clock::now();
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_pwsh_mutex);
        constexpr auto kMinPwshTimeout = std::chrono::milliseconds(10000);
        if (clamped > 1.0f) {
            m_pwsh_config.min_timeout = std::min(
                m_pwsh_config.max_timeout,
                m_pwsh_config.min_timeout + std::chrono::milliseconds(static_cast<int>(clamped * 2000.0f)));
        } else if (m_pwsh_config.min_timeout > kMinPwshTimeout) {
            m_pwsh_config.min_timeout = std::max(
                kMinPwshTimeout,
                m_pwsh_config.min_timeout - std::chrono::milliseconds(500));
        }
    }
}

void QuantumDynamicTimeManager::adjustForNetworkConditions(float network_factor) {
    const float clamped = std::clamp(network_factor, 0.0f, 1.0f);
    m_cached_network_quality = clamped;

    {
        std::lock_guard<std::mutex> lock(m_profiles_mutex);
        for (auto& [_, profile] : m_time_profiles) {
            profile.network_latency_factor = std::clamp(1.0f + (1.0f - clamped) * 0.7f, 0.8f, 2.2f);
            profile.last_update = std::chrono::steady_clock::now();
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_pwsh_mutex);
        if (clamped < 0.5f) {
            m_pwsh_config.default_timeout = std::min(
                m_pwsh_config.max_timeout,
                m_pwsh_config.default_timeout + std::chrono::milliseconds(5000));
        }
    }
}

QuantumDynamicTimeManager::TimeAllocation QuantumDynamicTimeManager::extendTimeAllocation(
    const TimeAllocation& original, const std::string& reason) {
    auto scale_duration = [](std::chrono::milliseconds value, float factor) {
        const double scaled = static_cast<double>(value.count()) * static_cast<double>(factor);
        return std::chrono::milliseconds(static_cast<int64_t>(std::max(1.0, scaled)));
    };

    TimeAllocation expanded = original;
    float extension_factor = 1.10f;
    if (reason.find("timeout") != std::string::npos) {
        extension_factor += 0.25f;
    }
    if (reason.find("network") != std::string::npos || reason.find("http") != std::string::npos) {
        extension_factor += 0.20f;
    }
    if (reason.find("build") != std::string::npos || reason.find("compile") != std::string::npos ||
        reason.find("link") != std::string::npos) {
        extension_factor += 0.20f;
    }
    if (reason.find("critical") != std::string::npos || reason.find("production") != std::string::npos) {
        extension_factor += 0.15f;
    }

    extension_factor += m_cached_system_load * 0.20f;
    extension_factor += m_cached_memory_pressure * 0.15f;
    extension_factor += (1.0f - m_cached_network_quality) * 0.15f;
    extension_factor = std::clamp(extension_factor, 1.05f, 2.20f);

    expanded.thinking_time = scale_duration(expanded.thinking_time, std::max(1.0f, extension_factor - 0.10f));
    expanded.execution_time = scale_duration(expanded.execution_time, extension_factor);
    expanded.buffer_time = scale_duration(expanded.buffer_time, std::max(1.0f, extension_factor - 0.05f));
    expanded.pwsh_timeout = scale_duration(expanded.pwsh_timeout, std::max(1.0f, extension_factor - 0.10f));

    {
        std::lock_guard<std::mutex> lock(m_pwsh_mutex);
        expanded.pwsh_timeout = std::max(m_pwsh_config.min_timeout, std::min(expanded.pwsh_timeout, m_pwsh_config.max_timeout));
    }

    expanded.total_time = expanded.thinking_time + expanded.execution_time + expanded.buffer_time;
    if (expanded.allow_extensions) {
        expanded.max_extensions = std::max(expanded.max_extensions, 1);
        expanded.extension_multiplier = std::clamp(expanded.extension_multiplier * extension_factor, 1.0f, 3.5f);
    }
    return expanded;
}

QuantumDynamicTimeManager::AdaptationMetrics QuantumDynamicTimeManager::getMetrics() const {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    return m_metrics;
}

void QuantumDynamicTimeManager::generateTimeReport(std::ostream& output) const {
    const auto metrics = getMetrics();
    size_t profile_count = 0;
    {
        std::lock_guard<std::mutex> lock(m_profiles_mutex);
        profile_count = m_time_profiles.size();
    }

    output << "QuantumDynamicTimeManager Report\n";
    output << "strategy=" << static_cast<int>(m_strategy)
           << " quantum=" << (m_quantum_enabled ? "on" : "off")
           << " learning_rate=" << std::fixed << std::setprecision(3) << m_learning_rate << "\n";
    output << "tasks total=" << metrics.total_tasks
           << " success=" << metrics.successful_completions
           << " timeout_failures=" << metrics.timeout_failures
           << " profiles=" << profile_count << "\n";
    output << "pwsh exec=" << metrics.pwsh_executions
           << " pwsh_timeouts=" << metrics.pwsh_timeouts
           << " avg_pwsh_ms=" << std::fixed << std::setprecision(1) << metrics.avg_pwsh_duration_ms << "\n";
    output << "cached load=" << std::fixed << std::setprecision(2) << m_cached_system_load
           << " mem_pressure=" << m_cached_memory_pressure
           << " network=" << m_cached_network_quality << "\n";
}

std::vector<std::string> QuantumDynamicTimeManager::getPerformanceRecommendations() const {
    std::vector<std::string> recommendations;
    const auto metrics = getMetrics();

    const float success_rate = metrics.total_tasks > 0
        ? static_cast<float>(metrics.successful_completions) / static_cast<float>(metrics.total_tasks)
        : 1.0f;
    const float timeout_rate = metrics.total_tasks > 0
        ? static_cast<float>(metrics.timeout_failures) / static_cast<float>(metrics.total_tasks)
        : 0.0f;

    if (success_rate < 0.80f) {
        recommendations.emplace_back("Increase conservative buffers for complex and expert profiles.");
    }
    if (timeout_rate > 0.20f) {
        recommendations.emplace_back("Raise default PowerShell timeout and checkpoint windows.");
    }
    if (m_cached_system_load > 0.85f) {
        recommendations.emplace_back("Reduce concurrent agent execution while CPU pressure remains elevated.");
    }
    if (m_cached_memory_pressure > 0.80f) {
        recommendations.emplace_back("Scale down batch sizes and increase recycle cadence for long sessions.");
    }
    if (m_cached_network_quality < 0.50f) {
        recommendations.emplace_back("Favor local model routing; defer network-heavy commands.");
    }
    if (recommendations.empty()) {
        recommendations.emplace_back("Performance is stable; keep adaptive tuning enabled.");
    }
    return recommendations;
}

PowerShellTimeoutManager::PowerShellTimeoutManager(QuantumDynamicTimeManager* time_manager)
    : m_time_manager(time_manager), m_randomization_enabled(true), m_min_random_factor(0.8f), m_max_random_factor(1.2f) {}

PowerShellTimeoutManager::~PowerShellTimeoutManager() = default;

std::string PowerShellTimeoutManager::createSession(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    std::string id = "session_" + std::to_string(m_sessions.size() + 1);
    auto [it, inserted] = m_sessions.emplace(id, TerminalSession{id});
    (void)inserted;
    it->second.allocated_time = timeout;
    it->second.remaining_time = timeout;
    return id;
}

void PowerShellTimeoutManager::destroySession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    m_sessions.erase(session_id);
}

bool PowerShellTimeoutManager::extendSession(const std::string& session_id, std::chrono::milliseconds additional_time) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    auto it = m_sessions.find(session_id);
    if (it == m_sessions.end()) return false;
    it->second.allocated_time += additional_time;
    it->second.remaining_time += additional_time;
    return true;
}

std::string PowerShellTimeoutManager::executeWithTimeout(const std::string& command, std::chrono::milliseconds timeout) {
    if (command.empty()) {
        return "[PowerShellTimeoutManager] Empty command";
    }

#if !defined(_WIN32)
    (void)timeout;
    return "[PowerShellTimeoutManager] Not supported on non-Windows targets";
#else
    if (timeout <= std::chrono::milliseconds::zero()) {
        timeout = m_time_manager != nullptr
            ? m_time_manager->getRandomPwshTimeout("powershell")
            : std::chrono::seconds(30);
    }

    if (m_randomization_enabled) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(m_min_random_factor, m_max_random_factor);
        timeout = std::chrono::milliseconds(
            static_cast<int64_t>(static_cast<double>(timeout.count()) * static_cast<double>(dist(gen))));
    }
    timeout = std::max(timeout, std::chrono::milliseconds(1000));

    const std::string session_id = createSession(timeout);

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        destroySession(session_id);
        return "[PowerShellTimeoutManager] Failed to create output pipe";
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    std::string escaped;
    escaped.reserve(command.size() + 8);
    for (char ch : command) {
        if (ch == '"') {
            escaped += "\\\"";
        } else {
            escaped.push_back(ch);
        }
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::string cmdline = "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"" + escaped + "\"";
    std::vector<char> mutable_cmdline(cmdline.begin(), cmdline.end());
    mutable_cmdline.push_back('\0');

    const BOOL created = CreateProcessA(
        nullptr,
        mutable_cmdline.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    CloseHandle(write_pipe);

    if (!created) {
        CloseHandle(read_pipe);
        destroySession(session_id);
        return "[PowerShellTimeoutManager] Failed to launch powershell.exe";
    }

    const long long timeout_ms = std::max<long long>(1LL, timeout.count());
    const long long max_wait = static_cast<long long>(std::numeric_limits<DWORD>::max() - 1);
    const DWORD wait_ms = static_cast<DWORD>(std::min(timeout_ms, max_wait));
    const DWORD wait_result = WaitForSingleObject(pi.hProcess, wait_ms);
    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 124U);
    }

    std::string output;
    char buffer[4096];
    DWORD bytes_read = 0;
    while (ReadFile(read_pipe, buffer, static_cast<DWORD>(sizeof(buffer)), &bytes_read, nullptr) && bytes_read > 0) {
        output.append(buffer, static_cast<size_t>(bytes_read));
        if (output.size() > 262144U) {
            output.append("\n[PowerShellTimeoutManager] Output truncated");
            break;
        }
    }

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(read_pipe);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (m_time_manager != nullptr) {
        const bool success = (wait_result != WAIT_TIMEOUT) && (exit_code == 0U);
        m_time_manager->recordExecution(
            "pwsh:" + session_id,
            QuantumDynamicTimeManager::ExecutionContext{},
            timeout,
            success);
    }
    destroySession(session_id);

    if (wait_result == WAIT_TIMEOUT) {
        return "[PowerShellTimeoutManager] Timeout after " + std::to_string(timeout.count()) + "ms\n" + output;
    }
    if (output.empty()) {
        output = "[PowerShellTimeoutManager] Exit code " + std::to_string(exit_code);
    }
    return output;
#endif
}

bool PowerShellTimeoutManager::isSessionActive(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    return m_sessions.find(session_id) != m_sessions.end();
}

std::chrono::milliseconds PowerShellTimeoutManager::getRemainingTime(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    auto it = m_sessions.find(session_id);
    if (it == m_sessions.end()) return std::chrono::milliseconds::zero();
    return it->second.remaining_time;
}

void PowerShellTimeoutManager::setRandomizationRange(float min_factor, float max_factor) {
    if (min_factor > max_factor) std::swap(min_factor, max_factor);
    m_min_random_factor = min_factor;
    m_max_random_factor = max_factor;
}
#endif

}  // namespace RawrXD::Agent
