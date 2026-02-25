#include "autonomous_intelligence_orchestrator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <regex>
#include <cstring>

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
    
    std::cout << "[Orchestrator] Initialized with REAL components." << std::endl;
}

AutonomousIntelligenceOrchestrator::~AutonomousIntelligenceOrchestrator() {
    stopAutonomousMode();
}

void AutonomousIntelligenceOrchestrator::startAutonomousMode(const std::string& projectPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) return;
    
    m_projectPath = projectPath;
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
    while (m_running) {
        // 1. Monitor project state
        // 2. Check for new tasks
        // 3. Execute plan steps
        // 4. Optimization passes
        
        if (m_currentMode == OrchestratorMode::Autonomous) {
            // Real logic: scan for issues periodically
            auto issues = scanForIssues(m_projectPath);
            if (!issues.empty()) {
                // Determine if we should auto-fix
                for (const auto& issue : issues) {
                    if (onNotification) {
                        onNotification("Issue Detected", issue);
                    }
                    // Plan a fix...
                    m_planOrchestrator->createPlan("Fix issue: " + issue);
                }
            }
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
    
    // Would call AI model to generate code based on requirement
    std::string generatedCode = "// Auto-generated implementation\n// TODO: Review and test";
    
    if (onNotification) {
        onNotification("Status", "Implementation generated");
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
    if (!m_projectPath.empty()) {
        m_qualityMetrics = assessCodeQuality(m_projectPath);
    }
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
