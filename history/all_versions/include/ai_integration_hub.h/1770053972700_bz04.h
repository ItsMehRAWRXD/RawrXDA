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
#include "logging/logger.h"
#include "metrics/metrics.h"
#include "tracing/tracer.h"

// Forward declarations for AI components
namespace RawrXD {
    // class InferenceEngine; // Removed forward decl
    namespace IDE {
        class IntelligentCompletionEngine;
        class CodebaseContextAnalyzer;
        class SmartRewriteEngine;
        class MultiModalModelRouter;
        class LanguageServerIntegration;
        class PerformanceOptimizer;
        class AdvancedCodingAgent;
    }
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
    // Core infrastructure
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    std::shared_ptr<Tracer> m_tracer;

    std::unique_ptr<FormatRouter> m_formatRouter;
    std::unique_ptr<EnhancedModelLoader> m_modelLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_inferenceEngine;

    // AI Components
    std::unique_ptr<RawrXD::IDE::IntelligentCompletionEngine> m_completionEngine;
    // std::unique_ptr<RawrXD::IDE::CodebaseContextAnalyzer> m_contextAnalyzer;
    // std::unique_ptr<RawrXD::IDE::SmartRewriteEngine> m_rewriteEngine;
    // std::unique_ptr<RawrXD::IDE::MultiModalModelRouter> m_modelRouter;
    // std::unique_ptr<RawrXD::IDE::LanguageServerIntegration> m_languageServer;
    // std::unique_ptr<RawrXD::IDE::PerformanceOptimizer> m_performanceOptimizer;
    // std::unique_ptr<RawrXD::IDE::AdvancedCodingAgent> m_codingAgent;

    // State management
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_loading{false};
    std::string m_currentModel;
    ModelFormat m_currentFormat;

    // Threading
    std::unique_ptr<std::thread> m_backgroundThread;
    std::mutex m_modelMutex;

public:
    AIIntegrationHub();
    ~AIIntegrationHub();

    // Initialization
    bool initialize(const std::string& defaultModel = "llama3:latest");
    bool loadModel(const std::string& modelPath);
    bool unloadModel();

    // Real-time AI features
    std::vector<CodeCompletion> getCompletions(
        const std::string& filePath,
        const std::string& prefix,
        const std::string& suffix,
        int cursorPosition
    );

    std::vector<CodeSuggestion> getSuggestions(
        const std::string& code,
        const std::string& context = ""
    );

    // Agent features
    std::string generateDocumentation(const std::string& code);
    std::vector<GeneratedTestCase> generateTests(const std::string& function);
    std::vector<BugReport> findBugs(const std::string& code);
    std::vector<Optimization> optimizeCode(const std::string& code);

    // Context awareness
    void indexCodebase(const std::string& rootPath);

    // Performance
    void setLatencyTarget(int milliseconds);

    // Status
    bool isReady() const { return m_initialized && !m_loading; }
    std::string getCurrentModel() const { return m_currentModel; }
    std::vector<std::string> getAvailableModels() const;

private:
    void backgroundInitialization();
    bool validateModelCompatibility(const std::string& modelPath);
    void setupModelRouting();
    void initializeAIComponents();
    void startBackgroundServices();
};
