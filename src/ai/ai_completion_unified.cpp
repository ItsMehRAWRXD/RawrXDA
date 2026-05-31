// UNIFIED AI Completion - Merges both implementations
#include "ai_completion_provider_real.hpp"
#include "../cpu_inference_engine.h"
#include "logging/logger.h"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <regex>
#include <filesystem>

static Logger s_completionLogger("AICompletion");

// Merged from src/ai_completion_real.cpp (1474 lines) + src/ai/ai_completion_provider_real.cpp (557 lines)
// Single unified implementation combining Windows integration + cross-platform provider

class AICompletionEngineUnified {
private:
    AICompletionProvider* m_provider;
    std::mutex m_mutex;
    bool m_initialized;
    
public:
    AICompletionEngineUnified() : m_provider(new AICompletionProvider()), m_initialized(false) {}
    
    ~AICompletionEngineUnified() { delete m_provider; }
    
    bool initialize(const std::string& modelPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return true;
        m_initialized = m_provider->initialize(modelPath, "");
        if (m_initialized) {
            s_completionLogger.info("AI Completion initialized with model: {}", modelPath);
        } else {
            s_completionLogger.error("Failed to initialize AI Completion");
        }
        return m_initialized;
    }
    
    std::vector<AICompletionProvider::CompletionSuggestion> getCompletions(
        const std::string& code, int cursorPos, const std::string& language) {
        
        if (!m_initialized) {
            s_completionLogger.warn("getCompletions called before initialization");
            return {};
        }
        
        AICompletionProvider::CompletionContext ctx;
        ctx.currentFile = "untitled";
        ctx.language = language;
        ctx.cursorPosition = cursorPos;
        
        auto lines = splitLines(code);
        ctx.allLines = lines;
        ctx.totalLines = static_cast<int>(lines.size());
        
        if (!lines.empty()) {
            size_t lineNum = 0;
            size_t pos = 0;
            for (size_t i = 0; i < lines.size() && pos < static_cast<size_t>(cursorPos); ++i) {
                lineNum = i;
                pos += lines[i].length() + 1;
            }
            ctx.currentLine = lines[lineNum];
            if (lineNum > 0) ctx.previousLine = lines[lineNum - 1];
            if (lineNum + 1 < lines.size()) ctx.nextLine = lines[lineNum + 1];
        }
        
        return m_provider->getCompletions(ctx);
    }
    
private:
    std::vector<std::string> splitLines(const std::string& text) {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
        return lines;
    }
};

// Global instance
static AICompletionEngineUnified* g_completionEngine = nullptr;
static std::mutex g_engineMutex;

extern "C" {
    __declspec(dllexport) bool AICompletion_Initialize(const char* modelPath) {
        std::lock_guard<std::mutex> lock(g_engineMutex);
        if (!g_completionEngine) {
            g_completionEngine = new AICompletionEngineUnified();
        }
        return g_completionEngine->initialize(modelPath ? modelPath : "");
    }
    
    __declspec(dllexport) void AICompletion_Shutdown() {
        std::lock_guard<std::mutex> lock(g_engineMutex);
        delete g_completionEngine;
        g_completionEngine = nullptr;
    }
}
