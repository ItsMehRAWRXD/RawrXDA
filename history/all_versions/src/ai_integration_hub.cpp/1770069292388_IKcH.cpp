#include "../include/ai_integration_hub.h"
#include <iostream>
#include <filesystem>
#include "../src/agentic_engine.h"

// Component Headers
#include "../include/CompletionEngine.h"
#include "../include/CodebaseContextAnalyzer.h"
#include "../include/SmartRewriteEngine.h"
#include "../include/MultiModalModelRouter.h"
#include "../include/LanguageServerIntegration.h"
#include "../include/PerformanceOptimizer.h"
#include "../include/AdvancedCodingAgent.h"

// Namespaces
using namespace RawrXD;
// using namespace RawrXD::IDE;

AIIntegrationHub::AIIntegrationHub() {
    m_logger = std::make_shared<Logger>("AIHub");
    m_metrics = std::make_shared<Metrics>();
    m_tracer = std::make_shared<Tracer>();
    m_initialized = false;
    m_loading = false;
}

AIIntegrationHub::~AIIntegrationHub() {
    if (m_backgroundThread && m_backgroundThread->joinable()) {
        m_backgroundThread->join();
    }
}

bool AIIntegrationHub::initialize(const std::string& defaultModel) {
    if (m_initialized) return true;

    m_loading = true;
    m_currentModel = defaultModel;

    try {
        // Initialize Core Infrastructure
        m_formatRouter = std::make_unique<FormatRouter>();
        m_modelLoader = std::make_unique<EnhancedModelLoader>();
        // Use concrete CPU Engine
        m_inferenceEngine = std::make_unique<CPUInferenceEngine>(); 

        // Initialize IDE Components
        m_completionEngine = std::make_unique<IDE::IntelligentCompletionEngine>();
        m_contextAnalyzer = std::make_unique<IDE::CodebaseContextAnalyzer>();
        m_rewriteEngine = std::make_unique<IDE::SmartRewriteEngine>();
        m_modelRouter = std::make_unique<IDE::MultiModalModelRouter>();
        m_languageServer = std::make_unique<IDE::LanguageServerIntegration>();
        m_performanceOptimizer = std::make_unique<IDE::PerformanceOptimizer>();
        m_codingAgent = std::make_unique<IDE::AdvancedCodingAgent>();

        if (!defaultModel.empty()) {
            if (!loadModel(defaultModel)) {
                if (m_logger) m_logger->warn("Failed to load default model: " + defaultModel);
            }
        }

        initializeAIComponents();
        startBackgroundServices();

        m_initialized = true;
    } catch (const std::exception& e) {
        if (m_logger) m_logger->error(std::string("Initialization failed: ") + e.what());
        m_initialized = false;
    }

    m_loading = false;
    return m_initialized;
}

bool AIIntegrationHub::loadModel(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(m_modelMutex);
    m_loading = true;

    bool success = false;
    if (m_modelLoader && m_inferenceEngine) {
        // Attempt load via inference engine directly or loader helper
        auto result = m_inferenceEngine->loadModel(modelPath);
        success = result.has_value();
        
        if (success) {
            m_currentModel = modelPath;
            // setupModelRouting(); // This might be missing too
        }
    }

    m_loading = false;
    return success;
}

bool AIIntegrationHub::unloadModel() {
    std::lock_guard<std::mutex> lock(m_modelMutex);
    m_currentModel.clear();
    // Logic to unload if engine supports it
    return true;
}

std::vector<CodeCompletion> AIIntegrationHub::getCompletions(
    const std::string& filePath,
    const std::string& prefix,
    const std::string& suffix,
    int cursorPosition
) {
    if (!m_completionEngine || !isReady()) return {};

    RawrXD::IDE::CompletionContext ctx;
    ctx.filePath = filePath;
    ctx.linePrefix = prefix;
    ctx.lineSuffix = suffix;
    // ctx.lineNumber = ...; // Would need fuller context
    
    auto suggestions = m_completionEngine->getCompletions(ctx);
    
    std::vector<CodeCompletion> result;
    for (const auto& s : suggestions) {
        CodeCompletion cc;
        cc.text = s.text;
        cc.detail = s.description;
        cc.confidence = s.confidence;
        cc.kind = s.kind;
        result.push_back(cc);
    }
    
    return result;
}

std::vector<CodeSuggestion> AIIntegrationHub::getSuggestions(
    const std::string& code,
    const std::string& context
) {
    if (m_rewriteEngine) {
        auto smells = m_rewriteEngine->detectCodeSmells(code, "cpp");
        std::vector<CodeSuggestion> result;
        for (const auto& smell : smells) {
             CodeSuggestion cs;
             cs.original = ""; // Can't easily map back to span without line numbers logic
             cs.suggested = smell.suggestedFix;
             cs.explanation = smell.description;
             cs.confidence = (double)smell.confidence;
             result.push_back(cs);
        }
        if (!result.empty()) return result;
    }
    return {};
}

std::string AIIntegrationHub::chat(const std::string& message) {
    if (m_agenticEngine) {
        return m_agenticEngine->chat(message);
    }
    // Minimal fallback
    if (!m_inferenceEngine) return "Error: Engine not initialized.";
    auto result = m_inferenceEngine->generate(message);
    return result.has_value() ? result.value().text : "Error: Generation failed.";
}

std::string AIIntegrationHub::planTask(const std::string& task) {
    if (m_agenticEngine) return m_agenticEngine->planTask(task);
    if (!m_inferenceEngine) return "Error: Agentic Engine not initialized.";
    // Fallback: simple generation
    auto result = m_inferenceEngine->generate("Plan: " + task);
    return result.has_value() ? result.value().text : "Error";
}

std::string AIIntegrationHub::executePlan(const std::string& plan) {
    if (m_agenticEngine) return m_agenticEngine->executePlan(plan);
    return "";
}

std::string AIIntegrationHub::analyzeCode(const std::string& code) {
    if (m_agenticEngine) return m_agenticEngine->analyzeCode(code);
    return "";
}

std::string AIIntegrationHub::bugReport(const std::string& code, const std::string& error) {
    if (m_agenticEngine) return m_agenticEngine->bugReport(code, error);
    if (!m_inferenceEngine) return "Error: No agent.";
    auto result = m_inferenceEngine->generate("Bug Report for:\n" + code + "\nError: " + error);
    return result.has_value() ? result.value().text : "Error";
}

std::string AIIntegrationHub::generateDocumentation(const std::string& code) {
    if (!m_inferenceEngine) return "";
    
    std::string prompt = "Generate documentation for the following code:\n```cpp\n" + code + "\n```\nDocumentation:";
    auto result = m_inferenceEngine->generate(prompt, 0.7f, 0.9f, 512);
    
    if (result.has_value()) {
        return result.value().text;
    }
    return "";
}

std::vector<GeneratedTestCase> AIIntegrationHub::generateTests(const std::string& function) {
    if (m_codingAgent) {
        auto tests = m_codingAgent->generateTests(function, "cpp"); // Assuming language
        std::vector<GeneratedTestCase> results;
        for (const auto& t : tests) {
             GeneratedTestCase tc;
             tc.name = t.name;
             tc.code = t.testCode;
             tc.description = t.setup; 
             tc.assertions.push_back(t.assertions);
             results.push_back(tc);
        }
        if (!results.empty()) return results;
    }

    if (!m_inferenceEngine) return {};
    
    std::string prompt = "Generate unit tests for this function:\n```cpp\n" + function + "\n```\nTests:";
    auto result = m_inferenceEngine->generate(prompt, 0.7f, 0.9f, 1024);
    
    if (result.has_value()) {
        GeneratedTestCase test;
        test.name = "AI_Generated_Test";
        test.code = result.value().text;
        test.description = "Automatically generated by RawrXD Engine";
        return {test};
    }
    return {};
}

std::vector<BugReport> AIIntegrationHub::findBugs(const std::string& code) {
    if (m_rewriteEngine) {
        auto issues = m_rewriteEngine->detectBugs(code, "cpp");
        std::vector<BugReport> reports;
        for (const auto& issue : issues) {
            BugReport report;
            report.type = issue.type;
            report.severity = issue.severity;
            report.description = issue.description;
            report.location = "Line " + std::to_string(issue.lineNumber);
            if (!issue.suggestedFix.empty()) {
                report.suggestions.push_back(issue.suggestedFix);
            }
            reports.push_back(report);
        }
        if (!reports.empty()) return reports;
    }

    if (!m_inferenceEngine) return {};

    std::string prompt = "Find bugs in this code:\n```cpp\n" + code + "\n```\nBugs:";
    auto result = m_inferenceEngine->generate(prompt);
    
    if (result.has_value()) {
        BugReport report;
        report.type = "AI Analysis";
        report.severity = "Medium";
        report.description = result.value().text;
        return {report};
    }
    return {};
}

std::vector<Optimization> AIIntegrationHub::optimizeCode(const std::string& code) {
    // Prefer using main inference engine for generation tasks as SmartRewriteEngine needs model injection
    if (!m_inferenceEngine) return {};
    
    std::string prompt = "Optimize this code to improve performance:\n```cpp\n" + code + "\n```\nOptimized Code:";
    auto result = m_inferenceEngine->generate(prompt, 0.7f, 0.9f, 1024);

    if (result.has_value()) {
         Optimization opt;
         opt.type = "AI Optimization";
         opt.description = "Performance improvement suggestion generated by " + m_currentModel;
         opt.originalCode = code;
         opt.optimizedCode = result.value().text;
         opt.expectedImprovement = 0.5; // Estimated
         
         if (opt.optimizedCode != code && !opt.optimizedCode.empty()) {
             return {opt};
         }
    }
    return {};
}

void AIIntegrationHub::indexCodebase(const std::string& rootPath) {
    if (m_backgroundThread && m_backgroundThread->joinable()) {
        m_backgroundThread->join();
    }
    m_backgroundThread = std::make_unique<std::thread>([this, rootPath]() {
        if (m_contextAnalyzer) {
            m_contextAnalyzer->initialize(rootPath);
        }
    });
}




void AIIntegrationHub::setLatencyTarget(int milliseconds) {
    // Logic to adjust inference engine parameters
}

std::vector<std::string> AIIntegrationHub::getAvailableModels() const {
    /*if (m_modelRouter) {
        // return m_modelRouter->GetAvailableModels();
    }*/
    return {"local-default"};
}

void AIIntegrationHub::backgroundInitialization() {
    // Warmup caches, etc.
}

bool AIIntegrationHub::validateModelCompatibility(const std::string& modelPath) {
    return true;
}

void AIIntegrationHub::setupModelRouting() {
    if (m_formatRouter && !m_currentModel.empty()) {
        // m_formatRouter->Configure(m_currentModel);
    }
}

void AIIntegrationHub::initializeAIComponents() {
    m_agenticEngine = std::make_shared<AgenticEngine>();
    if (m_inferenceEngine) {
        m_agenticEngine->setInferenceEngine(m_inferenceEngine.get());
    }
}

void AIIntegrationHub::startBackgroundServices() {
    // Start monitoring threads if needed
}


