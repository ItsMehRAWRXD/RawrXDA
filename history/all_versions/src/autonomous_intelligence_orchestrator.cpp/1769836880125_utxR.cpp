#include "autonomous_intelligence_orchestrator.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <future>

namespace fs = std::filesystem;
using json = nlohmann::json;

AutonomousIntelligenceOrchestrator::AutonomousIntelligenceOrchestrator(void* parent) {
    // Instantiate all subsystems
    // Construction order: Model -> Codebase -> Cloud -> Feature (Feature depends on others usually)
    modelManager = std::make_unique<AutonomousModelManager>();
    codebaseEngine = std::make_unique<IntelligentCodebaseEngine>();
    cloudManager = std::make_unique<HybridCloudManager>(parent);
    featureEngine = std::make_unique<AutonomousFeatureEngine>(parent);
    
    // Wire up dependencies
    featureEngine->setCodebaseEngine(codebaseEngine.get());
    featureEngine->setHybridCloudManager(cloudManager.get());
    
    operationMode = "hybrid";
}

AutonomousIntelligenceOrchestrator::~AutonomousIntelligenceOrchestrator() {
    // smart pointers handle deletion
}

bool AutonomousIntelligenceOrchestrator::initialize(const std::string& projectPath) {
    currentProjectPath = projectPath;
    
    // Perform initial system check
    SystemAnalysis sys = modelManager->analyzeSystemCapabilities();
    if (sys.hasGPU) {
        std::cout << "[Orchestrator] Detected GPU: " << sys.gpuType << std::endl;
    }
    
    return true;
}

bool AutonomousIntelligenceOrchestrator::loadConfiguration(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        if (file.is_open()) {
            json j;
            file >> j;
            if (j.contains("mode")) operationMode = j["mode"];
            if (j.contains("autoAnalysis")) autoAnalysisEnabled = j["autoAnalysis"];
            return true;
        }
    } catch (...) {}
    return false;
}

bool AutonomousIntelligenceOrchestrator::saveConfiguration() {
    // Stub
    return true;
}

bool AutonomousIntelligenceOrchestrator::startAutonomousMode(const std::string& projectPath) {
    if (autoAnalysisEnabled) {
        std::thread([this, projectPath]() {
            analyzeProject(projectPath);
        }).detach();
    }
    return true;
}

bool AutonomousIntelligenceOrchestrator::stopAutonomousMode() {
    return true;
}

bool AutonomousIntelligenceOrchestrator::pauseAutonomousMode() {
    return true;
}

bool AutonomousIntelligenceOrchestrator::resumeAutonomousMode() {
    return true;
}

bool AutonomousIntelligenceOrchestrator::analyzeProject(const std::string& projectPath) {
    currentProjectPath = projectPath;
    if (codebaseEngine) {
        return codebaseEngine->analyzeEntireCodebase(projectPath);
    }
    return false;
}

bool AutonomousIntelligenceOrchestrator::analyzeFile(const std::string& filePath) {
    if (codebaseEngine) {
        return codebaseEngine->analyzeFile(filePath);
    }
    return false;
}

bool AutonomousIntelligenceOrchestrator::analyzeFunction(const std::string& functionName, const std::string& filePath) {
    if (codebaseEngine) {
        return codebaseEngine->analyzeFunction(functionName, filePath);
    }
    return false;
}

nlohmann::json AutonomousIntelligenceOrchestrator::getProjectIntelligence() {
    json j;
    if (codebaseEngine) {
        j["complexity"] = 0; // Stub, assuming we'd serialize CodeComplexity
        j["files"] = {}; 
    }
    return j;
}

nlohmann::json AutonomousIntelligenceOrchestrator::getIntelligentRecommendations() {
    json j = json::array();
    if (featureEngine) {
         // featureEngine could return recommendations
         // For now return empty
    }
    return j;
}

std::vector<Optimization> AutonomousIntelligenceOrchestrator::getOptimizationSuggestions() {
    if (codebaseEngine) return codebaseEngine->suggestOptimizations();
    return {};
}

std::vector<RefactoringOpportunity> AutonomousIntelligenceOrchestrator::getRefactoringSuggestions() {
    if (codebaseEngine) return codebaseEngine->discoverRefactoringOpportunities();
    return {};
}

std::vector<BugReport> AutonomousIntelligenceOrchestrator::getBugReports() {
    if (codebaseEngine) return codebaseEngine->detectBugs();
    return {};
}

bool AutonomousIntelligenceOrchestrator::autoSelectBestModel(const std::string& taskType) {
    if (modelManager) {
        ModelRecommendation rec = modelManager->autoDetectBestModel(taskType, "cpp");
        activeModel = rec.modelId;
        return true;
    }
    return false;
}

bool AutonomousIntelligenceOrchestrator::autoGenerateTests() {
    // Stub: delegate to feature engine
    return true;
}

bool AutonomousIntelligenceOrchestrator::autoFixBugs() {
    // Stub
    return true;
}

bool AutonomousIntelligenceOrchestrator::autoOptimizeCode() {
    // Stub
    return true;
}

bool AutonomousIntelligenceOrchestrator::autoRefactor() {
    // Stub
    return true;
}

ModelRecommendation AutonomousIntelligenceOrchestrator::getModelRecommendation(const std::string& taskType) {
    if (modelManager) return modelManager->recommendModelForTask(taskType, currentLanguage);
    return ModelRecommendation();
}

bool AutonomousIntelligenceOrchestrator::switchModel(const std::string& modelId) {
    activeModel = modelId;
    return true;
}

nlohmann::json AutonomousIntelligenceOrchestrator::getAvailableModels() {
    if (modelManager) return modelManager->getAvailableModels();
    return json::array();
}

bool AutonomousIntelligenceOrchestrator::enableCloudMode() {
    operationMode = "cloud";
    return true;
}

bool AutonomousIntelligenceOrchestrator::enableLocalMode() {
    operationMode = "local";
    return true;
}

bool AutonomousIntelligenceOrchestrator::enableHybridMode() {
    operationMode = "hybrid";
    return true;
}

std::string AutonomousIntelligenceOrchestrator::getCurrentMode() {
    return operationMode;
}

double AutonomousIntelligenceOrchestrator::getCodeQualityScore() {
    if (codebaseEngine) return codebaseEngine->getCodeQualityScore();
    return 0.0;
}

double AutonomousIntelligenceOrchestrator::getTestCoverage() {
    if (codebaseEngine) return codebaseEngine->getTestCoverage();
    return 0.0;
}

double AutonomousIntelligenceOrchestrator::getMaintainabilityIndex() {
    return 0.0; // Stub
}

nlohmann::json AutonomousIntelligenceOrchestrator::getQualityReport() {
    return json::object();
}

bool AutonomousIntelligenceOrchestrator::enableEnterpriseMode() {
    return true;
}

bool AutonomousIntelligenceOrchestrator::setupTeamCollaboration(const std::string& teamId) {
    return true;
}

nlohmann::json AutonomousIntelligenceOrchestrator::getEnterpriseReport() {
    return json::object();
}
