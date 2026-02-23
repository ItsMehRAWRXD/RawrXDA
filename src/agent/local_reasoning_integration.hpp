// ============================================================================
// local_reasoning_integration.hpp — Hook LocalReasoningEngine into IDE
// ============================================================================

#pragma once
#include "local_reasoning_engine.hpp"
#include <memory>
#include <string>

/**
 * Singleton accessor for LocalReasoningEngine
 * Provides API-free code analysis with kernel RE expertise
 */
class LocalReasoningIntegration {
public:
    static LocalReasoningEngine& instance() {
        static LocalReasoningEngine s_engine;
        return s_engine;
    }

    // Convenience wrappers
    static LocalReasoningEngine::AnalysisResult analyzeCode(
        const std::string& code,
        const std::string& language,
        bool deepAnalysis = false,
        bool x64Mode = true
    ) {
        LocalReasoningEngine::AnalysisContext context;
        context.sourceCode = code;
        context.language = language;
        context.deepAnalysis = deepAnalysis;
        context.x64Mode = x64Mode;
        context.includeAssembly = (language == "asm" || language == "masm");
        
        return instance().analyze(context);
    }

    static void enableVerboseMode() {
        instance().enableVerboseLogging();
    }

    static void disableVerboseMode() {
        instance().disableVerboseLogging();
    }

    /** One-shot analysis of prompt/context text; returns summary string for routing. */
    static std::string analyzeCurrentContext(const std::string& prompt) {
        auto result = analyzeCode(prompt, "text", true, true);
        return result.summary.empty() ? "[No analysis]" : result.summary;
    }
};
