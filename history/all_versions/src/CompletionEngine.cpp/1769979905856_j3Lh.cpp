#include "CompletionEngine.h"
#include "cpu_inference_engine.h"
#include "utils/InferenceSettingsManager.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <regex>
#include <nlohmann/json.hpp>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

namespace RawrXD {
namespace IDE {

// Global static engine for completion to ensure persistent model state
static std::unique_ptr<RawrXD::CPUInferenceEngine> g_completionEngine = nullptr;

static void EnsureEngineLoaded() {
    if (!g_completionEngine) {
        g_completionEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
    }
    
    // Check if model is loaded or needs update
    // For now we assume if it's created, we try to load the default model
    auto& settings = RawrXD::InferenceSettingsManager::getInstance();
    std::string modelPath = settings.getCurrentModelPath();
    
    if (!g_completionEngine->isModelLoaded() && !modelPath.empty()) {
        g_completionEngine->LoadModel(modelPath);
    }
}

IntelligentCompletionEngine::IntelligentCompletionEngine()
    : m_confidenceThreshold(0.5f), m_maxSuggestions(10),
      m_modelUrl("http://localhost:11434"), m_embeddingUrl("http://localhost:11434") {
}

std::vector<CompletionSuggestion> IntelligentCompletionEngine::getCompletions(
    const CompletionContext& context, int maxSuggestions) {
    
    std::string cacheKey = context.filePath + ":" + context.linePrefix;
    
    // Check cache
    auto it = m_cache.find(cacheKey);
    if (it != m_cache.end()) {
        auto elapsed = std::chrono::system_clock::now() - it->second.timestamp;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 5) {
            return it->second.suggestions;
        }
    }
    
    // Get completions from model
    auto suggestions = inferCompletions(context);
    
    // Score and rank
    scoreCompletions(suggestions, context);
    
    // Sort by priority
    std::sort(suggestions.begin(), suggestions.end(),
        [](const CompletionSuggestion& a, const CompletionSuggestion& b) {
            return a.priority > b.priority;
        });
    
    // Limit results
    if (suggestions.size() > (size_t)maxSuggestions) {
        suggestions.resize(maxSuggestions);
    }
    
    // Cache result
    CachedCompletion cached{suggestions, std::chrono::system_clock::now()};
    m_cache[cacheKey] = cached;
    
    return suggestions;
}

std::vector<std::string> IntelligentCompletionEngine::getMultiLineCompletions(
    const CompletionContext& context, int maxLines) {
    
    std::vector<std::string> results;
    
    std::string prompt = "Complete this code:\n" + context.contextBefore + 
                        "\n// Current line: " + context.currentLine +
                        "\n// Generate " + std::to_string(maxLines) + " lines of code";
    
    EnsureEngineLoaded();
    
    if (g_completionEngine && g_completionEngine->isModelLoaded()) {
        // Real Inference
        std::string generated = g_completionEngine->infer(prompt, maxLines * 20); // Approx 20 tokens per line
        if (!generated.empty()) {
            results.push_back(generated);
        }
    } else {
        // Fallback or empty if no model
        std::vector<CompletionSuggestion> single = getCompletions(context, 1);
        if (!single.empty()) {
            results.push_back(single[0].text);
        }
    }
    
    return results;
}

bool IntelligentCompletionEngine::validateCompletion(
    const std::string& completion, const std::string& language, const std::string& context) {
    
    // Basic syntax validation
    if (completion.empty()) return false;
    
    // Check for balanced braces/parens
    int braceCount = 0, parenCount = 0, bracketCount = 0;
    for (char c : completion) {
        if (c == '{') braceCount++;
        else if (c == '}') braceCount--;
        else if (c == '(') parenCount++;
        else if (c == ')') parenCount--;
        else if (c == '[') bracketCount++;
        else if (c == ']') bracketCount--;
    }
    
    return braceCount >= 0 && parenCount >= 0 && bracketCount >= 0;
}

std::string IntelligentCompletionEngine::getCompletionDocumentation(
    const std::string& completion) {
    // Extract function signature and provide documentation
    // This would integrate with codebase analyzer
    return "Documentation for: " + completion;
}

std::vector<std::string> IntelligentCompletionEngine::getCompletionParams(
    const std::string& functionName) {
    // Extract parameters from function signature
    // This would use codebase analyzer
    return {"param1", "param2"};
}

void IntelligentCompletionEngine::setMinConfidenceThreshold(float threshold) {
    m_confidenceThreshold = threshold;
}

void IntelligentCompletionEngine::setMaxSuggestions(int count) {
    m_maxSuggestions = count;
}

void IntelligentCompletionEngine::setLanguageModelUrl(const std::string& url) {
    m_modelUrl = url;
}

void IntelligentCompletionEngine::setEmbeddingModelUrl(const std::string& url) {
    m_embeddingUrl = url;
}

std::vector<CompletionSuggestion> IntelligentCompletionEngine::inferCompletions(
    const CompletionContext& context) {
    
    std::vector<CompletionSuggestion> suggestions;
    
    // Build prompt
    std::string prompt = enrichContext(context);
    
    EnsureEngineLoaded();

    if (g_completionEngine && g_completionEngine->isModelLoaded()) {
        // Run inference
        // Suggest completions for the current line
        std::string result = g_completionEngine->infer(prompt);
        
        // Parse result - assume model returns text that completes the line or multiple lines
        if (!result.empty()) {
            CompletionSuggestion s;
            s.text = result;
            s.label = result.substr(0, std::min(result.length(), (size_t)30));
            s.confidence = 0.85f; // From model
            s.priority = 100;
            s.kind = "ai-generated";
            suggestions.push_back(s);
        }
    }
    
    // If we didn't get results (or as fallback/addition), add structural suggestions
    if (suggestions.empty()) {
        std::vector<std::string> completions = {
            context.linePrefix + " = ",
            context.linePrefix + "()",
            "if (" + context.linePrefix + ") {\n}",
            "for (auto " + context.linePrefix + " : ",
        };
        
        for (const auto& comp : completions) {
            CompletionSuggestion s;
            s.text = comp;
            s.label = comp.substr(0, 30);
            s.confidence = 0.2f; // Low confidence for heuristics
            s.priority = 10;
            s.kind = "snippet";
            suggestions.push_back(s);
        }
    }
    
    return suggestions;
}

void IntelligentCompletionEngine::scoreCompletions(
    std::vector<CompletionSuggestion>& suggestions,
    const CompletionContext& context) {
    
    for (auto& sug : suggestions) {
        // Fuzzy match score
        float matchScore = fuzzyMatchScore(sug.text, context.linePrefix);
        
        // Boost by category
        if (sug.kind == "function") sug.priority += 20;
        else if (sug.kind == "class") sug.priority += 15;
        else if (sug.kind == "variable") sug.priority += 10;
        
        // Apply fuzzy score
        sug.priority = static_cast<int>(sug.priority * (0.5f + matchScore * 0.5f));
    }
}

float IntelligentCompletionEngine::fuzzyMatchScore(
    const std::string& suggestion, const std::string& userInput) {
    
    if (userInput.empty()) return 1.0f;
    if (suggestion.empty()) return 0.0f;
    
    size_t pos = suggestion.find(userInput);
    if (pos == std::string::npos) return 0.1f;
    
    // Boost for prefix match
    if (pos == 0) return 0.9f;
    
    // Partial match
    return 0.5f;
}

std::string IntelligentCompletionEngine::enrichContext(
    const CompletionContext& context) {
    
    std::stringstream ss;
    ss << "File: " << context.filePath << "\n";
    ss << "Language: " << context.fileLanguage << "\n";
    ss << "Context: \n" << context.contextBefore << "\n";
    ss << "Current: " << context.currentLine << "\n";
    ss << "Visible symbols: ";
    for (const auto& sym : context.visibleSymbols) {
        ss << sym << ", ";
    }
    ss << "\n";
    ss << "Complete the line.";
    
    return ss.str();
}

} // namespace IDE
} // namespace RawrXD
