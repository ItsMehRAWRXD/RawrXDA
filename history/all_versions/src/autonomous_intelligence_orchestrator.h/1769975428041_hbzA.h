#pragma once


#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "autonomous_model_manager.h"
#include "intelligent_codebase_engine.h"
#include "autonomous_feature_engine.h"
#include "hybrid_cloud_manager.h"

class AgenticEngine; // Forward Declaration

struct OrchestratorConfig {
    bool autoAnalysis = true;
    bool autoTestGen = true;
    int checkIntervalSeconds = 30;
    std::string mode = "hybrid";
};

/**
 * @brief Master Autonomous Intelligence Orchestrator
 * 
 * Coordinates all autonomous systems to provide seamless, intelligent
 * IDE experience comparable to Cursor. This is the "brain" that makes
 * the IDE truly autonomous.
 */
class AutonomousIntelligenceOrchestrator {

private:
    // Core autonomous systems
    std::unique_ptr<AutonomousModelManager> modelManager;
    std::unique_ptr<IntelligentCodebaseEngine> codebaseEngine;
    std::unique_ptr<AutonomousFeatureEngine> featureEngine;
    std::unique_ptr<HybridCloudManager> cloudManager;
    
    // Current state
    std::string currentProjectPath;
    std::string currentLanguage;
    std::string activeModel;
    std::string operationMode; // "local", "cloud", "hybrid"
    
    // Intelligence metrics
    double codeQualityScore = 0.0;
    double testCoverage = 0.0;
    double maintainabilityIndex = 0.0;
    int totalOptimizationsFound = 0;
    int totalBugsDetected = 0;
    
    // Configuration
    int m_checkInterval = 30;
    bool autoAnalysisEnabled = true;
    bool autoTestGenerationEnabled = true;
    bool autoBugFixEnabled = true;
    bool intelligentModelSwitchingEnabled = true;
    
public:
    explicit AutonomousIntelligenceOrchestrator(void* parent = nullptr);
    ~AutonomousIntelligenceOrchestrator();
    
    // Initialization
    bool initialize(const std::string& projectPath);
    bool loadConfiguration(const std::string& configPath);
    bool saveConfiguration();
    
    // Autonomous operations
    bool startAutonomousMode(const std::string& projectPath);
    bool stopAutonomousMode();
    bool pauseAutonomousMode();
    bool resumeAutonomousMode();
    
    // Intelligent analysis
    bool analyzeProject(const std::string& projectPath);
    bool analyzeFile(const std::string& filePath);
    bool analyzeFunction(const std::string& functionName, const std::string& filePath);
    nlohmann::json getProjectIntelligence();
    
    // Intelligent recommendations
    nlohmann::json getIntelligentRecommendations();
    std::vector<Optimization> getOptimizationSuggestions();
    std::vector<RefactoringOpportunity> getRefactoringSuggestions();
    std::vector<BugReport> getBugReports();
    
    // Automatic actions
    bool autoSelectBestModel(const std::string& taskType = "");
    bool autoGenerateTests();
    bool autoFixBugs();
    bool autoOptimizeCode();
    bool autoRefactor();
    
    // Model intelligence
    ModelRecommendation getModelRecommendation(const std::string& taskType);
    bool switchModel(const std::string& modelId);
    nlohmann::json getAvailableModels();
    
    // Cloud integration
    bool enableCloudMode();
    bool enableLocalMode();
    bool enableHybridMode();
    std::string getCurrentMode();
    
    // Quality metrics
    double getCodeQualityScore();
    double getTestCoverage();
    double getMaintainabilityIndex();
    nlohmann::json getQualityReport();
    
    // Enterprise features
    bool enableEnterpriseMode();
    bool setupTeamCollaboration(const std::string& teamId);
    nlohmann::json getEnterpriseReport();
    
    // Callbacks
    std::function<void(const std::string& type, const std::string& message)> onNotification;
    
    // [AGENTIC] EXPLICIT LOGIC: Connection to AgenticEngine for automated fixes
    void setAgenticEngine(AgenticEngine* engine) { m_agenticEngine = engine; }

    void autonomousModeStarted();
    void autonomousModeStopped();
    void analysisCompleted(const void*& results);
    void recommendationsReady(const void*& recommendations);
    void modelSwitched(const std::string& newModel);
    void qualityScoreUpdated(double score);
    void optimizationsFound(int count);
    void bugsDetected(int count);
    void testsGenerated(int count);
    void intelligenceReportReady(const void*& report);
    
private:
    void setupConnections();
    void performContinuousAnalysis();
    void performIntelligentModelSelection();
    void performAutomaticOptimization();
    void updateIntelligenceMetrics();
    
    void* generateIntelligenceReport();
    bool shouldSwitchToCloudModel();
    bool shouldSwitchToLocalModel();

private:
    AgenticEngine* m_agenticEngine = nullptr;
};

