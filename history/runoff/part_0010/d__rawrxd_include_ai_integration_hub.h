#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>

#include "format_router.h"
#include "enhanced_model_loader.h"
#include "../src/cpu_inference_engine.h"
#include "../src/agentic_engine.h"
#include "logging/logger.h"
#include "metrics/metrics.h"
#include "tracing/tracer.h"

namespace RawrXD {

namespace IDE {
    class IntelligentCompletionEngine;
    class CodebaseContextAnalyzer;
    class SmartRewriteEngine;
    class MultiModalModelRouter;
    class LanguageServerIntegration;
    class PerformanceOptimizer;
    class AdvancedCodingAgent;
}

// Completion structs
struct CodeCompletion {
    std::string text;
    std::string detail;
    double confidence;
    std::string kind;
    int insertTextLength;
    int cursorOffset;
};

using Completion = CodeCompletion;

struct CodeSuggestion {
    std::string original;
    std::string suggested;
    std::string explanation;
    double confidence;
    std::vector<std::string> affectedFiles;
};

struct BugReport {
    std::string severity;
    std::string type;
    std::string description;
    std::string location;
    std::vector<std::string> suggestions;
};

struct GeneratedTestCase {
    std::string name;
    std::string code;
    std::string description;
    std::vector<std::string> assertions;
};

struct Optimization {
    std::string type;
    std::string description;
    std::string originalCode;
    std::string optimizedCode;
    double expectedImprovement;
};

class AIIntegrationHub {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    std::shared_ptr<Tracer> m_tracer;

    std::unique_ptr<FormatRouter> m_formatRouter;
    std::unique_ptr<EnhancedModelLoader> m_modelLoader;
    std::unique_ptr<CPUInferenceEngine> m_inferenceEngine;

    std::unique_ptr<IDE::IntelligentCompletionEngine> m_completionEngine;
    std::unique_ptr<IDE::CodebaseContextAnalyzer> m_contextAnalyzer;
    std::unique_ptr<IDE::SmartRewriteEngine> m_rewriteEngine;
    std::unique_ptr<IDE::MultiModalModelRouter> m_modelRouter;
    std::unique_ptr<IDE::LanguageServerIntegration> m_languageServer;
    std::unique_ptr<IDE::PerformanceOptimizer> m_performanceOptimizer;
    std::unique_ptr<IDE::AdvancedCodingAgent> m_codingAgent;
    
    std::shared_ptr<AgenticEngine> m_agenticEngine;

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_loading{false};
    std::string m_currentModel;
    ModelFormat m_currentFormat;

    std::unique_ptr<std::thread> m_backgroundThread;
    std::mutex m_modelMutex;

public:
    AIIntegrationHub();
    ~AIIntegrationHub();

    bool initialize(const std::string& defaultModel = "llama3:latest");
    void shutdown();
    
    bool loadModel(const std::string& modelPath);
    bool unloadModel();
    bool isReady() const { return m_initialized && !m_loading; }
    std::string getCurrentModel() const { return m_currentModel; }
    std::vector<std::string> getAvailableModels() const;

    std::vector<CodeCompletion> getCompletions(const std::string& filePath, const std::string& prefix, const std::string& suffix, int cursorPosition);
    std::vector<CodeSuggestion> getSuggestions(const std::string& code, const std::string& context = "");

    std::string chat(const std::string& message);
    std::string generateDocumentation(const std::string& code);
    std::vector<GeneratedTestCase> generateTests(const std::string& function);
    std::vector<BugReport> findBugs(const std::string& code);
    std::vector<Optimization> optimizeCode(const std::string& code);

    std::string planTask(const std::string& task);
    std::string executePlan(const std::string& plan);
    std::string analyzeCode(const std::string& code);
    std::string bugReport(const std::string& code, const std::string& error);
    
    std::shared_ptr<AgenticEngine> getAgent() { return m_agenticEngine; }
    void updateAgentConfig(const GenerationConfig& config);

    void indexCodebase(const std::string& rootPath);
    void setLatencyTarget(int milliseconds);

private:
    void backgroundInitialization();
    bool validateModelCompatibility(const std::string& modelPath);
    void setupModelRouting();
    void initializeAIComponents();
    void startBackgroundServices();
};

}
