#pragma once


#include <memory>
#include "autonomous_model_manager.h"
#include "intelligent_codebase_engine.h"
#include "autonomous_feature_engine.h"

/**
 * @brief Master Autonomous Intelligence Orchestrator
 * 
 * Coordinates all autonomous systems to provide seamless, intelligent
 * IDE experience comparable to Cursor. This is the "brain" that makes
 * the IDE truly autonomous.
 */
class AutonomousIntelligenceOrchestrator : public void {

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
    bool autoAnalysisEnabled = true;
    bool autoTestGenerationEnabled = true;
    bool autoBugFixEnabled = true;
    bool intelligentModelSwitchingEnabled = true;
    
public:
    explicit AutonomousIntelligenceOrchestrator(void* parent = nullptr);
    ~AutonomousIntelligenceOrchestrator();
    
    // Initialization
    bool initialize(const std::string& projectPath);
    bool loadConfiguration(const void*& config);
    bool saveConfiguration();
    
    // Autonomous operations
    bool startAutonomousMode(const std::string& projectPath);
    bool stopAutonomousMode();
    bool pauseAutonomousMode();
    bool resumeAutonomousMode();
    
    // Intelligent analysis
    void* analyzeProject(const std::string& projectPath);
    void* analyzeFile(const std::string& filePath);
    void* analyzeFunction(const std::string& functionName, const std::string& filePath);
    void* getProjectIntelligence();
    
    // Intelligent recommendations
    void* getIntelligentRecommendations();
    void* getOptimizationSuggestions();
    void* getRefactoringSuggestions();
    void* getBugReports();
    
    // Automatic actions
    bool autoSelectBestModel(const std::string& taskType = "");
    bool autoGenerateTests();
    bool autoFixBugs();
    bool autoOptimizeCode();
    bool autoRefactor();
    
    // Model intelligence
    ModelRecommendation getModelRecommendation(const std::string& taskType);
    bool switchModel(const std::string& modelId);
    void* getAvailableModels();
    
    // Cloud integration
    bool enableCloudMode();
    bool enableLocalMode();
    bool enableHybridMode();
    std::string getCurrentMode();
    
    // Quality metrics
    double getCodeQualityScore();
    double getTestCoverage();
    double getMaintainabilityIndex();
    void* getQualityReport();
    
    // Enterprise features
    bool enableEnterpriseMode();
    bool setupTeamCollaboration(const std::string& teamId);
    void* getEnterpriseReport();
    

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
};

