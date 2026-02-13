// ============================================================================
// local_reasoning_integration.hpp — Hook LocalReasoningEngine into IDE
// ============================================================================

#pragma once
#include "local_reasoning_engine.hpp"
#include <memory>

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
};
