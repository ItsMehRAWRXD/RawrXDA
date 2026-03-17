#pragma once
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include "agentic_engine.h"
#include "tool_registry.hpp"
#include "plan_orchestrator.h"
#include "universal_model_router.h"

namespace RawrXD {

enum class OrchestratorMode {
    Idle,
    Monitoring,
    Coding,
    Debugging,
    Optimizing,
    Autonomous
};

struct QualityMetrics {
    float codeQualityScore = 0.0f;
    float maintainabilityScore = 0.0f;
    float performanceScore = 0.0f;
    float securityScore = 0.0f;
    std::vector<std::string> issues;
};

class AutonomousIntelligenceOrchestrator {
public:
    AutonomousIntelligenceOrchestrator(AgenticIDE* ide);
    ~AutonomousIntelligenceOrchestrator();
    
    // Non-copyable
    AutonomousIntelligenceOrchestrator(const AutonomousIntelligenceOrchestrator&) = delete;
    AutonomousIntelligenceOrchestrator& operator=(const AutonomousIntelligenceOrchestrator&) = delete;
    
    // Core functionality
    void startAutonomousMode(const std::string& projectPath);
    void stopAutonomousMode();
    
    // Real decision making
    void analyzeCodebase(const std::string& path);
    void generateImplementation(const std::string& requirement);
    void debugIssue(const std::string& errorDescription);
    void optimizePerformance();
    
    // Quality assessment (real logic)
    QualityMetrics assessCodeQuality(const std::string& filePath);
    QualityMetrics getQualityReport() const { return m_qualityMetrics; }
    
    // Status
    OrchestratorMode getCurrentMode() const { return m_currentMode.load(); }
    json getStatus() const;
    
    // Notification system
    std::function<void(const std::string&, const std::string&)> onNotification;
    
private:
    AgenticIDE* m_ide;
    std::atomic<OrchestratorMode> m_currentMode{OrchestratorMode::Idle};
    std::atomic<bool> m_running{false};
    mutable std::mutex m_mutex;
    
    // Real components
    std::unique_ptr<AgenticEngine> m_agenticEngine;
    std::unique_ptr<ToolRegistry> m_toolRegistry;
    std::unique_ptr<PlanOrchestrator> m_planOrchestrator;
    std::unique_ptr<UniversalModelRouter> m_modelRouter;
    
    // State
    QualityMetrics m_qualityMetrics;
    std::string m_projectPath;
    std::vector<std::string> m_activePlans;
    std::thread m_orchestratorThread;
    
    // Real implementation methods
    void orchestratorLoop();
    std::string makeDecision(const std::string& context);
    void executePlan(const std::string& plan);
    void updateQualityMetrics();
    
    // File system monitoring
    void monitorFileChanges(const std::string& path);
    std::vector<std::string> scanForIssues(const std::string& path);
    
    // Real code analysis
    std::vector<std::string> parseAST(const std::string& code);
    std::vector<std::string> detectBugs(const std::string& code);
    std::vector<std::string> suggestOptimizations(const std::string& code);
    
    float calculateQualityScore(const std::string& code);
    float calculateMaintainabilityScore(const std::string& code);
    float calculatePerformanceScore(const std::string& code);
    float calculateSecurityScore(const std::string& code);
    std::vector<std::string> detectSecurityVulnerabilities(const std::string& code);
};

} // namespace RawrXD

