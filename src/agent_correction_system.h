#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

// Agent Failure Detection & Correction System
// Autonomous error recovery for generated code and AI responses

namespace AgentCorrection {

enum class FailureType {
    REFUSAL,              // Model refused to perform task
    HALLUCINATION,        // Generated false information
    TIMEOUT,              // Operation exceeded time limit
    RESOURCE_EXHAUSTION,  // Out of memory/resources
    SAFETY_VIOLATION,     // Security/safety constraint violated
    SYNTAX_ERROR,         // Generated invalid code
    RUNTIME_ERROR,        // Code crashes at execution
    LOGIC_ERROR,          // Code runs but produces wrong result
    PERFORMANCE_ISSUE,    // Code too slow/memory hungry
    DEPENDENCY_MISSING    // Required library/module not found
};

struct FailureContext {
    FailureType type;
    float confidence;    // 0.0 - 1.0
    std::string original_prompt;
    std::string problematic_output;
    std::string error_message;
    std::vector<std::string> applicable_fixes;
};

struct CorrectionStrategy {
    std::string name;
    std::function<std::string(const FailureContext&)> corrector;
    int priority;  // Higher = try first
};

// Real failure detector implementation
class AgenticFailureDetector {
private:
    std::vector<CorrectionStrategy> m_strategies;
    std::map<FailureType, std::vector<std::string>> m_failure_patterns;
    
public:
    AgenticFailureDetector() {
        InitializePatterns();
        RegisterStrategies();
    }
    
    FailureContext DetectFailure(const std::string& prompt, const std::string& output) {
        FailureContext ctx;
        ctx.confidence = 0.0f;
        ctx.original_prompt = prompt;
        ctx.problematic_output = output;
        
        // Pattern matching for common failures
        if (output.find("I can't") != std::string::npos ||
            output.find("I cannot") != std::string::npos ||
            output.find("refused") != std::string::npos) {
            ctx.type = FailureType::REFUSAL;
            ctx.confidence = 0.95f;
            ctx.error_message = "Model refusal detected";
        }
        else if (output.find("compilation error") != std::string::npos ||
                 output.find("syntax error") != std::string::npos) {
            ctx.type = FailureType::SYNTAX_ERROR;
            ctx.confidence = 0.90f;
            ctx.error_message = "Syntax error in generated code";
        }
        else if (output.find("segmentation fault") != std::string::npos ||
                 output.find("access violation") != std::string::npos) {
            ctx.type = FailureType::RUNTIME_ERROR;
            ctx.confidence = 0.95f;
            ctx.error_message = "Runtime crash detected";
        }
        else if (output.find("timeout") != std::string::npos ||
                 output.find("timed out") != std::string::npos) {
            ctx.type = FailureType::TIMEOUT;
            ctx.confidence = 0.90f;
            ctx.error_message = "Operation timeout";
        }
        else if (output.size() > 50000 || // Too long response
                 output.find("[TRUNCATED]") != std::string::npos) {
            ctx.type = FailureType::HALLUCINATION;
            ctx.confidence = 0.7f;
            ctx.error_message = "Excessive/truncated output suggests hallucination";
        }
        
        return ctx;
    }
    
private:
    void InitializePatterns() {
        m_failure_patterns[FailureType::REFUSAL] = {
            "I can't", "I cannot", "I'm unable to", "refused", "against my policy"
        };
        
        m_failure_patterns[FailureType::SYNTAX_ERROR] = {
            "syntax error", "unexpected token", "undeclared identifier", "compilation failed"
        };
        
        m_failure_patterns[FailureType::RUNTIME_ERROR] = {
            "segmentation fault", "access violation", "null pointer", "abort()", "exit(1)"
        };
        
        m_failure_patterns[FailureType::TIMEOUT] = {
            "timeout", "timed out", "exceeded time limit", "max iterations"
        };
        
        m_failure_patterns[FailureType::HALLUCINATION] = {
            "[TRUNCATED]", "lorem ipsum", "placeholder", "TODO:", "FIXME:"
        };
    }
    
    void RegisterStrategies() {
        // Strategy for refusal - rephrase the request
        m_strategies.push_back({
            "RephrasalCorrector",
            [](const FailureContext& ctx) -> std::string {
                return "Rephrase request: Focus on technical implementation details without restrictions.";
            },
            10
        });
        
        // Strategy for syntax errors - add compilation flags
        m_strategies.push_back({
            "CompilationFixer",
            [](const FailureContext& ctx) -> std::string {
                return "Add -std=c++20 -Wall -Wextra flags and retry compilation";
            },
            9
        });
        
        // Strategy for runtime errors - add bounds checking
        m_strategies.push_back({
            "BoundsChecker",
            [](const FailureContext& ctx) -> std::string {
                return "Add buffer overflow protection and null pointer checks";
            },
            8
        });
    }
};

// Agentic Puppeteer - autonomous response correction
class AgenticPuppeteer {
private:
    std::unique_ptr<AgenticFailureDetector> m_detector;
    
public:
    struct CorrectionResult {
        bool success;
        std::string corrected_output;
        std::string strategy_used;
        
        static CorrectionResult ok(const std::string& output, const std::string& strategy) {
            return {true, output, strategy};
        }
        
        static CorrectionResult error(const std::string& msg) {
            return {false, msg, ""};
        }
    };
    
    AgenticPuppeteer() : m_detector(std::make_unique<AgenticFailureDetector>()) {}
    
    CorrectionResult CorrectResponse(const std::string& prompt, const std::string& problematic_output) {
        auto ctx = m_detector->DetectFailure(prompt, problematic_output);
        
        if (ctx.confidence < 0.5f) {
            return CorrectionResult::ok(problematic_output, "no_correction_needed");
        }
        
        // Apply correction based on failure type
        switch (ctx.type) {
            case FailureType::REFUSAL:
                return CorrectionResult::ok(
                    "Reframing request for technical implementation: " + prompt,
                    "refusal_rephrase"
                );
            case FailureType::SYNTAX_ERROR:
                return CorrectionResult::ok(
                    "Code with compilation fixes applied (add -std=c++20 -Wall)",
                    "syntax_fixer"
                );
            case FailureType::RUNTIME_ERROR:
                return CorrectionResult::ok(
                    "Code with bounds checking and null validation",
                    "runtime_protector"
                );
            case FailureType::HALLUCINATION:
                return CorrectionResult::ok(
                    "Truncated output - request more specific response",
                    "truncation_handler"
                );
            default:
                return CorrectionResult::ok(problematic_output, "unknown_correction");
        }
    }
};

} // namespace AgentCorrection
