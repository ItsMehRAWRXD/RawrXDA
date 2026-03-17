#include "agentic_engine.h"
#include "native_agent.hpp"
#include "code_analyzer.h"
#include "ide_diagnostic_system.h"
// Temporarily disabled due to raw JavaScript string literal issues
// #include "advanced_agent_features.hpp"
#include "reverse_engineering/RawrDumpBin.hpp"
#include "reverse_engineering/RawrCodex.hpp"
#include "reverse_engineering/RawrCompiler.hpp"
#include "cpu_inference_engine.h"
#include <iostream>
#include <sstream>
#include <algorithm>

AgenticEngine::AgenticEngine() : m_inferenceEngine(nullptr) {
    m_codeAnalyzer = std::make_shared<RawrXD::CodeAnalyzer>();
}

AgenticEngine::~AgenticEngine() {
    if (m_inferenceEngine) {
        delete m_inferenceEngine;
        m_inferenceEngine = nullptr;
    }
}

void AgenticEngine::initialize() {
    // Initialize CPU Inference Engine
    if (!m_inferenceEngine) {
        m_inferenceEngine = new RawrXD::CPUInferenceEngine();
        std::cout << "[AGENT] CPU Inference Engine initialized\n";
    }
    
    // Set default config
    m_config.temperature = 0.8f;
    m_config.topP = 0.9f;
    m_config.maxTokens = 2048;
    m_config.maxMode = false;
    m_config.deepThinking = false;
    m_config.deepResearch = false;
    m_config.noRefusal = false;
    m_config.autoCorrect = false;
    
    std::cout << "[AGENT] Agentic Engine fully initialized and ready\n";
}

std::string AgenticEngine::analyzeCode(const std::string& code) {
    return chat("Analyze this code:\n" + code);
}

std::string AgenticEngine::analyzeCodeQuality(const std::string& code) {
    return chat("Evaluate code quality for:\n" + code);
}

std::string AgenticEngine::detectPatterns(const std::string& code) {
    return chat("Detect patterns in:\n" + code);
}

std::string AgenticEngine::calculateMetrics(const std::string& code) {
    // Stubbed actual calculation
    return "{ \"complexity\": \"O(n)\", \"lines\": " + std::to_string(std::count(code.begin(), code.end(), '\n')) + " }";
}

std::string AgenticEngine::suggestImprovements(const std::string& code) {
    return chat("Suggest improvements for:\n" + code);
}

std::string AgenticEngine::generateCode(const std::string& prompt) {
    return chat("Generate code: " + prompt);
}

std::string AgenticEngine::generateFunction(const std::string& signature, const std::string& description) {
    return chat("Generate function " + signature + ": " + description);
}

std::string AgenticEngine::generateClass(const std::string& className, const std::string& spec) {
    return chat("Generate class " + className + " with spec: " + spec);
}

std::string AgenticEngine::generateTests(const std::string& code) {
    return chat("Generate unit tests for:\n" + code);
}

std::string AgenticEngine::refactorCode(const std::string& code, const std::string& refactoringType) {
    return chat("Refactor code (" + refactoringType + "):\n" + code);
}

std::string AgenticEngine::planTask(const std::string& goal) {
    return chat("Plan task: " + goal);
}

std::string AgenticEngine::decomposeTask(const std::string& task) {
    return chat("Decompose task: " + task);
}

std::string AgenticEngine::generateWorkflow(const std::string& project) {
    return chat("Generate workflow for project: " + project);
}

std::string AgenticEngine::estimateComplexity(const std::string& task) {
    return chat("Estimate complexity for: " + task);
}

std::string AgenticEngine::understandIntent(const std::string& userInput) {
    return chat("What is the intent of: " + userInput);
}

std::string AgenticEngine::extractEntities(const std::string& text) {
    return chat("Extract entities from: " + text);
}

std::string AgenticEngine::generateNaturalResponse(const std::string& query, const std::string& context) {
    return chat("Context: " + context + "\nQuery: " + query);
}

std::string AgenticEngine::summarizeCode(const std::string& code) {
    return chat("Summarize code:\n" + code);
}

std::string AgenticEngine::explainError(const std::string& errorMessage) {
    return chat("Explain error: " + errorMessage);
}

void AgenticEngine::collectFeedback(const std::string&, bool, const std::string&) {}
void AgenticEngine::trainFromFeedback() {}
std::string AgenticEngine::getLearningStats() const { return "{}"; }
void AgenticEngine::adaptToUserPreferences(const std::string&) {}

bool AgenticEngine::validateInput(const std::string&) { return true; }
std::string AgenticEngine::sanitizeCode(const std::string& code) { return code; }
bool AgenticEngine::isCommandSafe(const std::string&) { return true; }

std::string AgenticEngine::grepFiles(const std::string&, const std::string&) { return ""; }
std::string AgenticEngine::readFile(const std::string&, int, int) { return ""; }
std::string AgenticEngine::searchFiles(const std::string& query, const std::string& path) { return ""; }
std::string AgenticEngine::referenceSymbol(const std::string&) { return ""; }

void AgenticEngine::updateConfig(const GenerationConfig& config) {
    m_config = config;
    if (m_inferenceEngine) {
        m_inferenceEngine->SetMaxMode(config.maxMode);
        m_inferenceEngine->SetDeepThinking(config.deepThinking);
        m_inferenceEngine->SetDeepResearch(config.deepResearch);
        // noRefusal is handled in the agent prompt builder
    }
}

std::string AgenticEngine::runDumpbin(const std::string& filePath, const std::string& mode) {
    RawrXD::ReverseEngineering::RawrDumpBin db;
    if (mode == "headers") return db.DumpHeaders(filePath);
    if (mode == "imports") return db.DumpImports(filePath);
    if (mode == "exports") return db.DumpExports(filePath);
    return db.DumpHeaders(filePath); // default
}

std::string AgenticEngine::runCodex(const std::string& filePath) {
    RawrXD::ReverseEngineering::RawrCodex codex;
    if (codex.LoadBinary(filePath)) {
        return codex.AnalyzeCodeStructure();
    }
    return "Error: Could not analyze with Codex.";
}

std::string AgenticEngine::runCompiler(const std::string& sourceFile, const std::string& target) {
    RawrXD::ReverseEngineering::RawrCompiler compiler;
    auto result = compiler.CompileSource(sourceFile);
    if (result.success) {
        return "Compilation Successful: " + result.objectFile;
    } else {
        std::string errs;
        for (const auto& e : result.errors) errs += e + "\n";
        return "Compilation Failed:\n" + errs;
    }
}

std::string AgenticEngine::chat(const std::string& message) {
    // Zero-Sim Mode: Intelligent mock responses based on message analysis
    if (!m_inferenceEngine || !m_inferenceEngine->IsModelLoaded()) {
        std::string response = "[Agent Mock] ";
        
        // Pattern-based intelligent responses
        if (message.find("analyze") != std::string::npos || message.find("code quality") != std::string::npos) {
            response += "Code Analysis:\n";
            response += "- Cyclomatic Complexity: O(n)\n";
            response += "- Maintainability: High\n";
            response += "- Test Coverage: Recommended\n";
            response += "- Security: No obvious vulnerabilities detected\n";
        }
        else if (message.find("generate") != std::string::npos && message.find("function") != std::string::npos) {
            response += "Generated Function Template:\n\n";
            response += "```cpp\nvoid generatedFunction() {\n    // Implementation\n    std::cout << \"Function executed\" << std::endl;\n}\n```";
        }
        else if (message.find("refactor") != std::string::npos) {
            response += "Refactoring Suggestions:\n";
            response += "1. Extract method for repeated logic\n";
            response += "2. Apply SOLID principles\n";
            response += "3. Improve naming conventions\n";
            response += "4. Add error handling\n";
        }
        else if (message.find("plan") != std::string::npos || message.find("task") != std::string::npos) {
            response += "Task Breakdown:\n";
            response += "Phase 1: Requirements Analysis\n";
            response += "Phase 2: Design & Architecture\n";
            response += "Phase 3: Implementation\n";
            response += "Phase 4: Testing & Validation\n";
            response += "Phase 5: Deployment\n";
        }
        else {
            response += "Processed: " + message.substr(0, 50) + (message.length() > 50 ? "..." : "");
        }
        
        return response;
    }
    
    // Real Inference Mode
    RawrXD::NativeAgent agent(m_inferenceEngine);
    
    // Configure Agent from Engine config
    agent.SetMaxMode(m_config.maxMode);
    agent.SetDeepThink(m_config.deepThinking);
    agent.SetDeepResearch(m_config.deepResearch);
    agent.SetNoRefusal(m_config.noRefusal);

    std::string response;
    agent.SetOutputCallback([&](const std::string& token) {
        response += token;
    });
    
    agent.Ask(message);
    return response;
}

// NEW: Advanced Analysis Methods
std::string AgenticEngine::performCompleteCodeAudit(const std::string& code) {
    if (!m_codeAnalyzer) return "Error: Code analyzer not initialized";
    
    std::ostringstream oss;
    oss << "=== Complete Code Audit ===\n\n";
    
    // Metrics
    auto metrics = m_codeAnalyzer->CalculateMetrics(code);
    oss << "Metrics:\n";
    oss << "  Lines of Code: " << metrics.lines_of_code << "\n";
    oss << "  Functions: " << metrics.functions << "\n";
    oss << "  Classes: " << metrics.classes << "\n";
    oss << "  Cyclomatic Complexity: " << metrics.cyclomatic_complexity << "\n";
    oss << "  Maintainability Index: " << std::fixed << std::setprecision(2) << metrics.maintainability_index << "%\n";
    oss << "  Duplication Ratio: " << metrics.duplication_ratio * 100 << "%\n\n";
    
    // Security Analysis
    auto security_issues = m_codeAnalyzer->SecurityAudit(code);
    oss << "Security Issues: " << security_issues.size() << "\n";
    for (const auto& issue : security_issues) {
        oss << "  [" << issue.code << "] " << issue.message << "\n";
        oss << "    Suggestion: " << issue.suggestion << "\n";
    }
    oss << "\n";
    
    // Performance Analysis
    auto perf_issues = m_codeAnalyzer->PerformanceAudit(code);
    oss << "Performance Issues: " << perf_issues.size() << "\n";
    for (const auto& issue : perf_issues) {
        oss << "  [" << issue.code << "] " << issue.message << "\n";
        oss << "    Suggestion: " << issue.suggestion << "\n";
    }
    oss << "\n";
    
    // Style Checks
    auto style_issues = m_codeAnalyzer->CheckStyle(code);
    oss << "Style Issues: " << style_issues.size() << "\n";
    for (const auto& issue : style_issues) {
        oss << "  [" << issue.code << "] " << issue.message << "\n";
    }
    
    return oss.str();
}

std::string AgenticEngine::getSecurityAssessment(const std::string& code) {
    if (!m_codeAnalyzer) return "Error: Code analyzer not initialized";
    
    auto issues = m_codeAnalyzer->SecurityAudit(code);
    
    std::ostringstream oss;
    oss << "=== Security Assessment ===\n";
    oss << "Total Issues: " << issues.size() << "\n\n";
    
    int critical = 0, high = 0, medium = 0, low = 0;
    for (const auto& issue : issues) {
        if (issue.severity == RawrXD::CodeIssue::Critical) critical++;
        else if (issue.severity == RawrXD::CodeIssue::Error) high++;
        else if (issue.severity == RawrXD::CodeIssue::Warning) medium++;
        else low++;
    }
    
    oss << "Breakdown:\n";
    oss << "  Critical: " << critical << "\n";
    oss << "  High: " << high << "\n";
    oss << "  Medium: " << medium << "\n";
    oss << "  Low: " << low << "\n\n";
    
    if (critical > 0) {
        oss << "CRITICAL: This code has security vulnerabilities that must be fixed!\n";
    } else if (high > 0) {
        oss << "WARNING: This code has significant security concerns.\n";
    } else if (medium > 0) {
        oss << "NOTICE: This code has some security improvements recommended.\n";
    } else {
        oss << "OK: No major security issues detected.\n";
    }
    
    return oss.str();
}

std::string AgenticEngine::getPerformanceRecommendations(const std::string& code) {
    if (!m_codeAnalyzer) return "Error: Code analyzer not initialized";
    
    auto issues = m_codeAnalyzer->PerformanceAudit(code);
    
    std::ostringstream oss;
    oss << "=== Performance Recommendations ===\n\n";
    
    if (issues.empty()) {
        oss << "No performance issues detected!\n";
        return oss.str();
    }
    
    oss << "Found " << issues.size() << " potential performance issues:\n\n";
    
    for (size_t i = 0; i < issues.size(); ++i) {
        oss << i + 1 << ". " << issues[i].message << "\n";
        oss << "   Recommendation: " << issues[i].suggestion << "\n\n";
    }
    
    return oss.str();
}

void AgenticEngine::integrateWithDiagnostics(RawrXD::IDEDiagnosticSystem* diagnostics) {
    m_diagnosticSystem = diagnostics;
    
    if (m_diagnosticSystem) {
        // Register a callback for agentic engine events
        m_diagnosticSystem->RegisterDiagnosticListener([this](const RawrXD::DiagnosticEvent& event) {
            // Can log or process diagnostic events here
            std::cout << "[Diagnostic] " << event.message << "\n";
        });
    }
}

std::string AgenticEngine::getIDEHealthReport() {
    if (!m_diagnosticSystem) return "Diagnostic system not initialized";
    
    std::ostringstream oss;
    oss << "=== IDE Health Report ===\n";
    oss << "Health Score: " << m_diagnosticSystem->GetHealthScore() << "%\n";
    oss << "Errors: " << m_diagnosticSystem->CountErrors() << "\n";
    oss << "Warnings: " << m_diagnosticSystem->CountWarnings() << "\n";
    oss << "\n";
    oss << m_diagnosticSystem->GetPerformanceReport();
    
    return oss.str();
}
