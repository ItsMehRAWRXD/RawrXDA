#include "../include/ai_integration_hub.h"
#include <iostream>
#include <filesystem>

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
    // Coding Agent not implemented fully for suggestions yet
    return {};
}

std::string AIIntegrationHub::chat(const std::string& message) {
    if (!m_inferenceEngine) return "Error: Engine not initialized.";

    if (message.rfind("/audit", 0) == 0) {
        // Simple audit simulation
        std::string target = message.substr(7); 
        if (target.empty()) target = "codebase";
        
        std::string prompt = "Analyze the following code for bugs and security issues: " + target; // Ideally read file content
        
        auto result = m_inferenceEngine->generate(prompt);
         if (result.has_value()) {
            return "[Audit Report] " + result.value().text;
        }
        return "Error: Audit generation failed.";
    }

    // In a real generic hub, we might need to route this or format it as user/assistant
    // For now, pass direct prompt
    auto result = m_inferenceEngine->generate(message);
    if (result.has_value()) {
        return result.value().text;
    } else {
        return "Error: Generation failed.";
    }
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
    if (!m_rewriteEngine || !isReady()) return {};
    
    auto transformation = m_rewriteEngine->optimizeCode(code, "cpp");
    
    Optimization opt;
    opt.type = "Performance Optimization";
    opt.description = transformation.explanation;
    opt.originalCode = code;
    opt.optimizedCode = transformation.transformedCode;
    opt.expectedImprovement = (double)transformation.confidence; // Using confidence as proxy for improvement

    if (opt.optimizedCode.empty() || opt.optimizedCode == code) return {};

    return {opt};
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
    // Link components together if needed
}

void AIIntegrationHub::startBackgroundServices() {
    // Start monitoring threads if needed
}


