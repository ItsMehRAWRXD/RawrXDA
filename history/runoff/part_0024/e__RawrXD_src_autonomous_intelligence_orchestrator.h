#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include "autonomous_model_manager.h"
#include "intelligent_codebase_engine.h"
#include "autonomous_feature_engine.h"
#include "cpu_inference_engine.h" // Include the header for CPUInferenceEngine

/**
 * @brief Master Autonomous Intelligence Orchestrator
 * 
 * Coordinates all autonomous systems to provide seamless, intelligent
 * IDE experience comparable to Cursor. This is the "brain" that makes
 * the IDE truly autonomous.
 */
class AutonomousIntelligenceOrchestrator : public QObject {
    Q_OBJECT
    
private:
    // Core autonomous systems
    std::unique_ptr<AutonomousModelManager> modelManager;
    std::unique_ptr<IntelligentCodebaseEngine> codebaseEngine;
    std::unique_ptr<AutonomousFeatureEngine> featureEngine;
    std::unique_ptr<HybridCloudManager> cloudManager;
    std::unique_ptr<CPUInference::CPUInferenceEngine> inferenceEngine; // Added
    
    // Current state
    QString currentProjectPath;
    QString currentLanguage;
    QString activeModel;
    QString operationMode; // "local", "cloud", "hybrid"
    
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
    explicit AutonomousIntelligenceOrchestrator(QObject* parent = nullptr);
    ~AutonomousIntelligenceOrchestrator();
    
    // Initialization
    bool initialize(const QString& projectPath);
    bool loadConfiguration(const QJsonObject& config);
    bool saveConfiguration();
    
    // Autonomous operations
    bool startAutonomousMode(const QString& projectPath);
    bool stopAutonomousMode();
    bool pauseAutonomousMode();
    bool resumeAutonomousMode();
    
    // Intelligent analysis
    QJsonObject analyzeProject(const QString& projectPath);
    QJsonObject analyzeFile(const QString& filePath);
    QJsonObject analyzeFunction(const QString& functionName, const QString& filePath);
    QJsonObject getProjectIntelligence();
    
    // Intelligent recommendations
    QJsonObject getIntelligentRecommendations();
    QJsonObject getOptimizationSuggestions();
    QJsonObject getRefactoringSuggestions();
    QJsonObject getBugReports();
    
    // Automatic actions
    bool autoSelectBestModel(const QString& taskType = "");
    bool autoGenerateTests();
    bool autoFixBugs();
    bool autoOptimizeCode();
    bool autoRefactor();
    
    // Model intelligence
    ModelRecommendation getModelRecommendation(const QString& taskType);
    bool switchModel(const QString& modelId);
    QJsonArray getAvailableModels();
    
    // Cloud integration
    bool enableCloudMode();
    bool enableLocalMode();
    bool enableHybridMode();
    QString getCurrentMode();
    
    // Quality metrics
    double getCodeQualityScore();
    double getTestCoverage();
    double getMaintainabilityIndex();
    QJsonObject getQualityReport();
    
    // Enterprise features
    bool enableEnterpriseMode();
    bool setupTeamCollaboration(const QString& teamId);
    QJsonObject getEnterpriseReport();
    
signals:
    void autonomousModeStarted();
    void autonomousModeStopped();
    void analysisCompleted(const QJsonObject& results);
    void recommendationsReady(const QJsonObject& recommendations);
    void modelSwitched(const QString& newModel);
    void qualityScoreUpdated(double score);
    void optimizationsFound(int count);
    void bugsDetected(int count);
    void testsGenerated(int count);
    void intelligenceReportReady(const QJsonObject& report);
    
private:
    void setupConnections();
    void performContinuousAnalysis();
    void performIntelligentModelSelection();
    void performAutomaticOptimization();
    void updateIntelligenceMetrics();
    
    QJsonObject generateIntelligenceReport();
    bool shouldSwitchToCloudModel();
    bool shouldSwitchToLocalModel();
};
