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
    if (!m_inferenceEngine) return "Error: Engine not initialized.";

    if (message.rfind("/agent ", 0) == 0) {
        std::string task = message.substr(7);
        if (m_codingAgent) {
             auto res = m_codingAgent->implementFeature(task, "cpp", "", {}); // Simple call
             return "[Agent] " + res.generatedCode;
        }
        // Fallback if no agent
        std::string prompt = "Act as an agent and " + task;
        auto result = m_inferenceEngine->generate(prompt);
        return result.has_value() ? result.value().text : "Error";
    }

    if (message.rfind("/plan ", 0) == 0) {
        std::string goal = message.substr(6);
        std::string prompt = "Create a detailed implementation plan for: " + goal;
        auto result = m_inferenceEngine->generate(prompt);
        return result.has_value() ? "[Plan]\n" + result.value().text : "Error";
    }

    if (message.rfind("/ask ", 0) == 0) {
        std::string q = message.substr(5);
        auto result = m_inferenceEngine->generate(q);
        return result.has_value() ? result.value().text : "Error";
    }

    if (message.rfind("/edit ", 0) == 0) {
        std::string instr = message.substr(6);
        std::string prompt = "Edit the code according to: " + instr; // Simplified
        auto result = m_inferenceEngine->generate(prompt);
        return result.has_value() ? "[Edit]\n" + result.value().text : "Error";
    }

    if (message.rfind("/bugreport ", 0) == 0) {
        std::string target = message.substr(11);
        std::string prompt = "Review the following code and generate a bug report:\n" + target;
        auto result = m_inferenceEngine->generate(prompt);
        return result.has_value() ? "[Bug Report]\n" + result.value().text : "Error: Inference failed during bug reporting.";
    }

    if (message.rfind("/suggest", 0) == 0) {
        std::string target = message.length() > 9 ? message.substr(9) : "codebase";
        std::string prompt = "Provide code suggestions and improvements for: " + target;
        auto result = m_inferenceEngine->generate(prompt);
         if (result.has_value()) {
            return "[Suggestions]\n" + result.value().text;
        }
        return "Error: Suggestions failed.";
    }

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

    if (message.rfind("/create ", 0) == 0) { // e.g., create a react server
        std::string desc = message.substr(8);
        std::string prompt = "Act as an Expert Developer. Create a complete, functional implementation (files and code) for: " + desc;
        
        // Pass to agent if available for "Deep Thinking"
        if (m_codingAgent) {
             auto res = m_codingAgent->implementFeature(desc, "Create", "full-stack", {}); // context?
             if (res.success) return res.result; 
        }
        
        // Fallback
        auto result = m_inferenceEngine->generate(prompt, 0.7f, 0.95f, 4096);
        return result.has_value() ? result.value().text : "Error: Creation failed.";
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
    // Link components together if needed
}

void AIIntegrationHub::startBackgroundServices() {
    // Start monitoring threads if needed
}


