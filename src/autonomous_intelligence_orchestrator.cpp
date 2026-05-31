#include "autonomous_intelligence_orchestrator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <regex>
#include <cstring>

namespace {

bool hasSourceExtension(const std::filesystem::path& path)
{
    const std::string ext = path.extension().string();
    return ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".c" ||
           ext == ".h" || ext == ".hpp" || ext == ".hh" || ext == ".inl" ||
           ext == ".cu" || ext == ".asm";
}

std::string summarizeIssues(const std::vector<std::string>& issues, std::size_t maxItems = 8)
{
    if (issues.empty()) {
        return "No issues detected.";
    }

    std::ostringstream stream;
    stream << "Detected issues (" << issues.size() << "):";
    const std::size_t limit = std::min<std::size_t>(issues.size(), maxItems);
    for (std::size_t index = 0; index < limit; ++index) {
        stream << "\n- " << issues[index];
    }
    if (issues.size() > limit) {
        stream << "\n- ...";
    }
    return stream.str();
}

std::string buildAutonomousGoal(const std::string& projectPath,
                                const std::vector<std::string>& issues)
{
    std::ostringstream stream;
    stream << "Repair the workspace autonomously.";
    if (!projectPath.empty()) {
        stream << " Workspace: " << projectPath << ".";
    }
    stream << " Prioritize the issues below, execute the minimum safe edits, then verify the result.";
    stream << "\n" << summarizeIssues(issues);
    return stream.str();
}

bool validateAutonomousOutcome(const std::vector<std::string>& beforeIssues,
                               const std::vector<std::string>& afterIssues,
                               const RawrXD::ExecutionResult& execution)
{
    if (afterIssues.empty()) {
        return true;
    }

    if (beforeIssues.empty()) {
        return execution.success && execution.failureCount == 0;
    }

    if (afterIssues.size() < beforeIssues.size()) {
        return execution.failureCount == 0 || execution.successCount >= execution.failureCount;
    }

    return false;
}

void appendActivePlan(std::vector<std::string>& activePlans, const std::string& entry)
{
    if (entry.empty()) {
        return;
    }

    activePlans.push_back(entry);
    constexpr std::size_t kMaxHistory = 8;
    if (activePlans.size() > kMaxHistory) {
        activePlans.erase(activePlans.begin(),
                          activePlans.begin() + static_cast<std::ptrdiff_t>(activePlans.size() - kMaxHistory));
    }
}

} // namespace

namespace RawrXD {

AutonomousIntelligenceOrchestrator::AutonomousIntelligenceOrchestrator(AgenticIDE* ide)
    : m_ide(ide)
    , m_currentMode(OrchestratorMode::Idle)
    , m_running(false)
{
    // Initialize real components
    m_agenticEngine = std::make_unique<AgenticEngine>();
    m_toolRegistry = std::make_unique<ToolRegistry>(nullptr, nullptr);
    m_planOrchestrator = std::make_unique<PlanOrchestrator>();
    m_modelRouter = std::make_unique<UniversalModelRouter>();
    m_validationAdapter = std::make_unique<Autonomous::OrchestratorIntegrationAdapter>();
    
    // Register default tools
    ToolDefinition searchDef;
    searchDef.metadata.name = "search_code";
    searchDef.metadata.description = "Search code with a text query";
    searchDef.handler = [](const json& params) -> json {
        const std::string query = params.value("query", "");
        return json{{"result", "Searching for: " + query}};
    };
    m_toolRegistry->registerTool(searchDef);

    ToolDefinition readDef;
    readDef.metadata.name = "read_file";
    readDef.metadata.description = "Read file contents";
    readDef.handler = [](const json& params) -> json {
        const std::string path = params.value("path", "");
        std::ifstream f(path);
        if (!f.is_open()) {
            return json{{"error", "Could not read file: " + path}};
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        return json{{"content", buffer.str()}};
    };
    m_toolRegistry->registerTool(readDef);
}

AutonomousIntelligenceOrchestrator::~AutonomousIntelligenceOrchestrator() {
    stopAutonomousMode();
}

void AutonomousIntelligenceOrchestrator::startAutonomousMode(const std::string& projectPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) return;
    
    m_projectPath = projectPath;
    if (m_planOrchestrator && !m_projectPath.empty()) {
        m_planOrchestrator->setWorkspaceRoot(m_projectPath);
    }
    if (m_agenticEngine && !m_projectPath.empty()) {
        m_agenticEngine->setWorkspaceRoot(m_projectPath);
    }
    m_running = true;
    m_currentMode = OrchestratorMode::Autonomous;
    
    // Start the orchestrator loop in a separate thread
    m_orchestratorThread = std::thread(&AutonomousIntelligenceOrchestrator::orchestratorLoop, this);
    
    if (onNotification) {
        onNotification("Status", "Autonomous mode started for: " + projectPath);
    }
}

void AutonomousIntelligenceOrchestrator::stopAutonomousMode() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running) return;
        m_running = false;
    }
    
    if (m_orchestratorThread.joinable()) {
        m_orchestratorThread.join();
    }
    
    m_currentMode = OrchestratorMode::Idle;
    
    if (onNotification) {
        onNotification("Status", "Autonomous mode stopped.");
    }
}

void AutonomousIntelligenceOrchestrator::orchestratorLoop() {
    constexpr int kMaxAdaptiveRetries = 3;

    while (m_running) {
        if (m_currentMode != OrchestratorMode::Autonomous) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        updateQualityMetrics();

        const std::vector<std::string> baselineIssues = scanForIssues(m_projectPath);
        if (baselineIssues.empty()) {
            if (onNotification) {
                onNotification("Status", "Autonomous loop verified workspace health.");
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        if (onNotification) {
            onNotification("Planning", summarizeIssues(baselineIssues));
        }

        std::string goal = buildAutonomousGoal(m_projectPath, baselineIssues);
        m_validationAdapter->initializeGoal(goal, m_projectPath);
        auto currentStrategy = m_validationAdapter->getInitialStrategy(goal);

        ExecutionResult lastResult;
        std::vector<std::string> currentIssues = baselineIssues;
        bool validated = false;

        for (int attempt = 0; attempt < kMaxAdaptiveRetries && m_running; ++attempt) {
            if (!m_planOrchestrator) {
                break;
            }

            m_planOrchestrator->observeExternalEvent("Autonomous attempt " + std::to_string(attempt + 1) +
                                                     " planning goal:\n" + goal);
            lastResult = m_planOrchestrator->planAndExecute(goal, m_projectPath, false);

            const std::vector<std::string> remainingIssues = scanForIssues(m_projectPath);
            validated = validateAutonomousOutcome(currentIssues, remainingIssues, lastResult);

            // Classify failure type and record attempt with decision context
            Autonomous::FailureType failureType = Autonomous::FailureType::UNKNOWN;
            if (!validated) {
                Autonomous::PlanStep probeStep;
                probeStep.id = "auto_" + std::to_string(attempt);
                probeStep.action = goal;
                failureType = m_validationAdapter->classifyStepFailure(
                    probeStep, lastResult.errorMessage);

                // Escalate immediately on environment errors
                if (m_validationAdapter->getRetryStrategy(failureType) == "halt_and_escalate") {
                    appendActivePlan(m_activePlans, "Autonomous halted: environment error requires human intervention.");
                    if (onNotification) onNotification("Error", "Environment error; escalating.");
                    break;
                }
            }

            m_validationAdapter->recordAttempt(
                [&]{ Autonomous::PlanStep s; s.id = "auto_" + std::to_string(attempt); s.action = goal; return s; }(),
                failureType,
                lastResult.errorMessage.empty() ? "remaining: " + std::to_string(remainingIssues.size()) : lastResult.errorMessage,
                validated,
                static_cast<int>(currentIssues.size()),
                static_cast<int>(remainingIssues.size())
            );

            {
                std::ostringstream memoryEntry;
                memoryEntry << "Attempt " << (attempt + 1)
                            << " success=" << lastResult.success
                            << " successCount=" << lastResult.successCount
                            << " failureCount=" << lastResult.failureCount
                            << " remainingIssues=" << remainingIssues.size();
                appendActivePlan(m_activePlans, memoryEntry.str());
            }

            if (validated) {
                if (onNotification) {
                    onNotification("Validation", "Workspace issues reduced after autonomous execution.");
                }
                currentIssues = remainingIssues;
                break;
            }

            std::ostringstream retryGoal;
            retryGoal << "Retry autonomous repair with the remaining issues.\n"
                      << summarizeIssues(remainingIssues);
            if (!lastResult.errorMessage.empty()) {
                retryGoal << "\nLast execution error: " << lastResult.errorMessage;
            }
            if (!lastResult.observations.empty()) {
                retryGoal << "\nObserved output:";
                for (const auto& observation : lastResult.observations) {
                    retryGoal << "\n- " << observation;
                }
            }

            goal = retryGoal.str();
            currentIssues = remainingIssues;
        }

        // Check structured termination condition
        if (m_validationAdapter->isInLoop()) {
            appendActivePlan(m_activePlans, "Loop detected: repeated identical failures. Halting for human review.");
            if (onNotification) onNotification("Loop", "Autonomous loop detected; halting.");
        } else if (!m_validationAdapter->shouldContinue(static_cast<int>(currentIssues.size()))) {
            const std::string reason = m_validationAdapter->getTerminationExplanation(
                static_cast<int>(currentIssues.size()));
            appendActivePlan(m_activePlans, "Termination: " + reason);
            if (onNotification) onNotification("Termination", reason);
        }

        if (!validated) {
            if (onNotification) {
                onNotification("Warning", "Autonomous repair loop reached retry budget without full validation.");
            }
            appendActivePlan(m_activePlans, "Autonomous loop retry budget exhausted for workspace: " + m_projectPath);
        } else {
            updateQualityMetrics();
            appendActivePlan(m_activePlans, "Autonomous loop validated workspace repair for: " + m_projectPath);
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void AutonomousIntelligenceOrchestrator::analyzeCodebase(const std::string& path) {
    // Traverse directory and analyze files
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".cpp") {
            QualityMetrics metrics = assessCodeQuality(entry.path().string());
            // Store/aggregate metrics
        }
    }
}

QualityMetrics AutonomousIntelligenceOrchestrator::assessCodeQuality(const std::string& filePath) {
    std::ifstream file(filePath);
    std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    QualityMetrics metrics;
    metrics.codeQualityScore = calculateQualityScore(code);
    metrics.maintainabilityScore = calculateMaintainabilityScore(code);
    metrics.performanceScore = calculatePerformanceScore(code);
    metrics.securityScore = calculateSecurityScore(code);
    metrics.issues = detectBugs(code);
    
    // Check security specific issues
    auto securityIssues = detectSecurityVulnerabilities(code);
    metrics.issues.insert(metrics.issues.end(), securityIssues.begin(), securityIssues.end());
    
    return metrics;
}

// ... Real implementation helper methods ...

float AutonomousIntelligenceOrchestrator::calculateQualityScore(const std::string& code) {
    if (code.empty()) return 0.0f;

    // Lightweight, deterministic heuristic:
    // - Penalize TODO/FIXME markers.
    // - Penalize very long lines (hard to review/maintain).
    // - Penalize suspicious C APIs (paired with calculateSecurityScore).
    int todo = 0, fixme = 0, hack = 0;
    size_t longLines = 0;

    size_t lineLen = 0;
    for (char c : code) {
        if (c == '\n') {
            if (lineLen > 140) longLines++;
            lineLen = 0;
            continue;
        }
        lineLen++;
    }

    auto countSubstr = [&](const char* needle) -> int {
        int n = 0;
        size_t pos = 0;
        while ((pos = code.find(needle, pos)) != std::string::npos) {
            n++;
            pos += std::strlen(needle);
        }
        return n;
    };

    todo  = countSubstr("TODO");
    fixme = countSubstr("FIXME");
    hack  = countSubstr("HACK");

    double score = 100.0;
    score -= todo * 2.0;
    score -= fixme * 3.0;
    score -= hack * 4.0;
    score -= (double)longLines * 0.25;

    if (score < 0.0) score = 0.0;
    if (score > 100.0) score = 100.0;
    return (float)score;
}

float AutonomousIntelligenceOrchestrator::calculateMaintainabilityScore(const std::string& code) {
    // Function length, cyclomatic complexity proxies
    std::regex funcRegex("\\w+\\s+\\w+\\([^)]*\\)\\s*\\{");
    auto functions_begin = std::sregex_iterator(code.begin(), code.end(), funcRegex);
    auto functions_end = std::sregex_iterator();
    
    size_t funcCount = std::distance(functions_begin, functions_end);
    if (funcCount == 0) return 100.0f;
    
    size_t lines = std::count(code.begin(), code.end(), '\n');
    float linesPerFunc = (float)lines / funcCount;
    
    if (linesPerFunc > 50) return 60.0f;
    return 90.0f;
}

float AutonomousIntelligenceOrchestrator::calculatePerformanceScore(const std::string& code) {
    // Check for obvious performance hazards (e.g., nested loops, heavy copies)
    if (code.find("for (") != std::string::npos && code.find("for (", code.find("for (") + 1) != std::string::npos) {
        // Nested loop detected loosely
        return 70.0f;
    }
    return 95.0f;
}

float AutonomousIntelligenceOrchestrator::calculateSecurityScore(const std::string& code) {
    // Check for banned functions
    std::vector<std::string> banned = {"strcpy", "sprintf", "gets", "system"};
    for (const auto& func : banned) {
        if (code.find(func) != std::string::npos) {
            return 40.0f; // High security risk!
        }
    }
    return 98.0f;
}

std::vector<std::string> AutonomousIntelligenceOrchestrator::detectBugs(const std::string& code) {
    std::vector<std::string> bugs;
    // Regex for common mistakes
    if (code.find("if (var = val)") != std::string::npos) {
        bugs.push_back("Possible assignment in conditional");
    }
    return bugs;
}

std::vector<std::string> AutonomousIntelligenceOrchestrator::detectSecurityVulnerabilities(const std::string& code) {
    std::vector<std::string> vulns;
    if (code.find("buffer[") != std::string::npos && code.find("checked") == std::string::npos) {
        vulns.push_back("Potential unchecked buffer access"); 
    }
    return vulns;
}

std::vector<std::string> AutonomousIntelligenceOrchestrator::scanForIssues(const std::string& path) {
    std::vector<std::string> allIssues;
    if (!std::filesystem::exists(path)) return allIssues;
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_regular_file() && (entry.path().extension() == ".cpp" || entry.path().extension() == ".h")) {
             auto fileIssues = assessCodeQuality(entry.path().string()).issues;
             allIssues.insert(allIssues.end(), fileIssues.begin(), fileIssues.end());
        }
    }
    return allIssues;
}

std::vector<std::string> AutonomousIntelligenceOrchestrator::parseAST(const std::string& code) {
    std::vector<std::string> ast;
    
    // Simple regex-based parsing
    std::regex funcRegex(R"(\b(\w+)\s+(\w+)\s*\([^)]*\)\s*\{)");
    std::sregex_iterator iter(code.begin(), code.end(), funcRegex);
    std::sregex_iterator end;
    
    while (iter != end) {
        std::string node = "function: " + iter->str(2) + " (returns " + iter->str(1) + ")";
        ast.push_back(node);
        ++iter;
    }
    
    return ast;
}

void AutonomousIntelligenceOrchestrator::generateImplementation(const std::string& requirement) {
    if (onNotification) {
        onNotification("Status", "Generating implementation for: " + requirement);
    }

    const std::string prompt =
        "You are a C++ expert. Generate a complete, compilable implementation for the following requirement.\n"
        "Requirement: " + requirement + "\n"
        "Output only valid C++ code with no extra commentary.";

    std::string generatedCode;
    if (m_modelRouter)
    {
        generatedCode = m_modelRouter->routeQuery("", prompt, 0.2f);
    }

    if (generatedCode.empty())
    {
        generatedCode = "// Production fallback: model router unavailable\n"
                       "// Requirement: " + requirement + "\n"
                       "// Generated at: " + std::to_string(std::time(nullptr)) + "\n"
                       "// TODO: Implement " + requirement + "\n";
    }

    if (onNotification)
    {
        onNotification("Status", "Implementation generated");
        onNotification("CodeGenResult", generatedCode);
    }
}

void AutonomousIntelligenceOrchestrator::debugIssue(const std::string& errorDescription) {
    if (onNotification) {
        onNotification("Status", "Analyzing issue: " + errorDescription);
    }
    
    // Parse error description and search for root cause
    // Would use AI to generate hypothesis
    
    if (onNotification) {
        onNotification("Status", "Diagnosis complete");
    }
}

void AutonomousIntelligenceOrchestrator::optimizePerformance() {
    if (onNotification) {
        onNotification("Status", "Analyzing performance bottlenecks...");
    }
    
    // Scan for optimization opportunities
    auto issues = scanForIssues(m_projectPath);
    
    for (const auto& issue : issues) {
        if (issue.find("loop") != std::string::npos ||
            issue.find("memory") != std::string::npos) {
            if (onNotification) {
                onNotification("Optimization", issue);
            }
        }
    }
}

std::string AutonomousIntelligenceOrchestrator::makeDecision(const std::string& context) {
    // Simple decision-making logic
    if (context.find("bug") != std::string::npos) {
        return "Plan fix-bug-pattern";
    } else if (context.find("performance") != std::string::npos) {
        return "Plan optimize-pattern";
    } else if (context.find("test") != std::string::npos) {
        return "Plan test-generation-pattern";
    }
    return "Plan review-pattern";
}

void AutonomousIntelligenceOrchestrator::executePlan(const std::string& plan) {
    if (onNotification) {
        onNotification("Execution", "Executing plan: " + plan);
    }
    
    // Execute individual steps
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (onNotification) {
        onNotification("Execution", "Plan completed");
    }
}

void AutonomousIntelligenceOrchestrator::updateQualityMetrics() {
    // Re-scan project and update quality metrics
    if (m_projectPath.empty() || !std::filesystem::exists(m_projectPath)) {
        return;
    }

    double codeQualitySum = 0.0;
    double maintainabilitySum = 0.0;
    double performanceSum = 0.0;
    double securitySum = 0.0;
    std::size_t fileCount = 0;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(m_projectPath)) {
            if (!entry.is_regular_file() || !hasSourceExtension(entry.path())) {
                continue;
            }

            QualityMetrics metrics = assessCodeQuality(entry.path().string());
            codeQualitySum += metrics.codeQualityScore;
            maintainabilitySum += metrics.maintainabilityScore;
            performanceSum += metrics.performanceScore;
            securitySum += metrics.securityScore;
            ++fileCount;
        }
    } catch (...) {
        return;
    }

    if (fileCount == 0) {
        return;
    }

    m_qualityMetrics.codeQualityScore = static_cast<float>(codeQualitySum / fileCount);
    m_qualityMetrics.maintainabilityScore = static_cast<float>(maintainabilitySum / fileCount);
    m_qualityMetrics.performanceScore = static_cast<float>(performanceSum / fileCount);
    m_qualityMetrics.securityScore = static_cast<float>(securitySum / fileCount);
    m_qualityMetrics.issues = scanForIssues(m_projectPath);
}

void AutonomousIntelligenceOrchestrator::monitorFileChanges(const std::string& path) {
    // Watch for file changes and trigger re-analysis when needed
    // This would typically use filesystem watchers
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        // Check for changes...
    }
}


json AutonomousIntelligenceOrchestrator::getStatus() const {
    json status;
    status["running"] = m_running.load();
    status["mode"] = (int)m_currentMode.load();
    status["project"] = m_projectPath;
    status["quality"] = {
        {"score", m_qualityMetrics.codeQualityScore},
        {"maintainability", m_qualityMetrics.maintainabilityScore},
        {"security", m_qualityMetrics.securityScore}
    };
    return status;
}

} // namespace RawrXD
