// RawrXD Copilot Bridge - Pure Win32/C++ Implementation
// Replaces: agentic_copilot_bridge.cpp and all Qt-dependent copilot components
// Zero Qt dependencies - just Win32 API + STL

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <regex>
#include <sstream>

// ============================================================================
// COPILOT REQUEST/RESPONSE STRUCTURES
// ============================================================================

struct CompletionRequest {
    std::wstring context;
    std::wstring prefix;
    std::wstring suffix;
    std::wstring language;
    int max_tokens;
    float temperature;
    std::chrono::steady_clock::time_point timestamp;
};

struct CompletionResponse {
    std::wstring completion;
    float confidence;
    int tokens_used;
    std::chrono::milliseconds latency;
    bool success;
    std::wstring error;
};

struct ConversationMessage {
    enum Role { USER, ASSISTANT, SYSTEM };
    Role role;
    std::wstring content;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::wstring, std::wstring> metadata;
};

struct AnalysisResult {
    std::wstring summary;
    std::vector<std::wstring> suggestions;
    std::vector<std::wstring> warnings;
    std::vector<std::wstring> errors;
    int complexity_score;
    int line_count;
    int function_count;
};

// ============================================================================
// EVENT CALLBACKS (REPLACE QT SIGNALS)
// ============================================================================

template<typename... Args>
class EventCallback {
private:
    std::vector<std::function<void(Args...)>> m_handlers;
    std::mutex m_mutex;
    
public:
    void Connect(std::function<void(Args...)> handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers.push_back(handler);
    }
    
    void Emit(Args... args) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& handler : m_handlers) {
            try {
                handler(args...);
            } catch (...) {
                OutputDebugStringW(L"[CopilotBridge] Handler exception\n");
            }
        }
    }
    
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers.clear();
    }
};

// ============================================================================
// COPILOT BRIDGE
// ============================================================================

class CopilotBridge {
private:
    // Thread safety
    mutable std::mutex m_mutex;
    
    // State
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_enabled;
    std::wstring m_workspaceRoot;
    
    // Conversation history
    std::vector<ConversationMessage> m_conversationHistory;
    size_t m_maxHistorySize;
    std::wstring m_lastContext;
    
    // Component references (not owned)
    // void* m_agenticEngine;
    // void* m_chatInterface;
    // void* m_editor;
    // void* m_terminalPool;
    
    // Statistics
    std::atomic<int> m_completionCount;
    std::atomic<int> m_analysisCount;
    std::atomic<int> m_conversationCount;
    std::chrono::steady_clock::time_point m_sessionStart;
    
    // Code context cache
    std::map<std::wstring, std::wstring> m_codeCache;
    std::map<std::wstring, AnalysisResult> m_analysisCache;
    
public:
    // Events (replace Qt signals)
    EventCallback<std::wstring> CompletionReady;
    EventCallback<std::wstring> AnalysisReady;
    EventCallback<std::wstring> ErrorOccurred;
    EventCallback<int, std::wstring> TrainingProgress;
    EventCallback<std::wstring> TrainingCompleted;
    
    CopilotBridge()
        : m_initialized(false),
          m_enabled(true),
          m_maxHistorySize(1000),
          m_completionCount(0),
          m_analysisCount(0),
          m_conversationCount(0),
          m_sessionStart(std::chrono::steady_clock::now()) {
        
        InitializePaths();
        LogInfo(L"Copilot Bridge initialized");
    }
    
    ~CopilotBridge() {
        Cleanup();
        LogInfo(L"Copilot Bridge destroyed");
    }
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    bool Initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_initialized.load()) {
            LogWarning(L"Copilot Bridge already initialized");
            return true;
        }
        
        // Initialize conversation history
        m_conversationHistory.clear();
        m_conversationHistory.reserve(m_maxHistorySize);
        
        // Add system message
        ConversationMessage sysMsg;
        sysMsg.role = ConversationMessage::SYSTEM;
        sysMsg.content = L"You are RawrXD Copilot, an advanced AI coding assistant. "
                        L"Provide clear, accurate, and production-ready code suggestions.";
        sysMsg.timestamp = std::chrono::system_clock::now();
        m_conversationHistory.push_back(sysMsg);
        
        m_initialized = true;
        LogInfo(L"Copilot Bridge initialization complete");
        
        return true;
    }
    
    void Cleanup() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_conversationHistory.clear();
        m_codeCache.clear();
        m_analysisCache.clear();
        
        CompletionReady.Clear();
        AnalysisReady.Clear();
        ErrorOccurred.Clear();
        TrainingProgress.Clear();
        TrainingCompleted.Clear();
        
        m_initialized = false;
    }
    
    // ========================================================================
    // CODE COMPLETION
    // ========================================================================
    
    CompletionResponse GenerateCodeCompletion(const CompletionRequest& request) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto startTime = std::chrono::steady_clock::now();
        CompletionResponse response;
        response.success = false;
        
        if (!m_initialized.load()) {
            response.error = L"Copilot Bridge not initialized";
            ErrorOccurred.Emit(response.error);
            return response;
        }
        
        if (!m_enabled.load()) {
            response.error = L"Copilot disabled";
            return response;
        }
        
        LogInfo(L"Generating code completion for prefix: " + request.prefix.substr(0, 50) + L"...");
        
        // Build completion prompt
        std::wstring prompt = L"Based on this code context:\n" + 
                             request.context + L"\n\n" +
                             L"Complete the following code starting with: " + request.prefix + L"\n" +
                             L"Provide ONLY the completion code, no explanation.";
        
        // Generate completion (simulated for now - integrate with actual inference engine)
        response.completion = SimulateCompletion(request);
        
        // Apply quality improvements
        response.completion = HotpatchCompletion(response.completion, request.context);
        
        // Calculate metrics
        auto endTime = std::chrono::steady_clock::now();
        response.latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        response.tokens_used = static_cast<int>(response.completion.length() / 4); // Rough estimate
        response.confidence = CalculateConfidence(response.completion, request.context);
        response.success = true;
        
        m_completionCount++;
        
        // Emit event
        CompletionReady.Emit(response.completion);
        
        LogInfo(L"Completion generated in " + std::to_wstring(response.latency.count()) + L" ms");
        
        return response;
    }
    
    std::wstring GenerateInlineCompletion(const std::wstring& context, const std::wstring& prefix) {
        CompletionRequest request;
        request.context = context;
        request.prefix = prefix;
        request.max_tokens = 256;
        request.temperature = 0.3f;
        request.timestamp = std::chrono::steady_clock::now();
        
        CompletionResponse response = GenerateCodeCompletion(request);
        return response.success ? response.completion : L"";
    }
    
    // ========================================================================
    // CODE ANALYSIS
    // ========================================================================
    
    AnalysisResult AnalyzeActiveFile(const std::wstring& code, const std::wstring& filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        AnalysisResult result;
        result.complexity_score = 0;
        result.line_count = 0;
        result.function_count = 0;
        
        if (code.empty()) {
            result.summary = L"No code to analyze";
            return result;
        }
        
        LogInfo(L"Analyzing file: " + filePath + L" (" + std::to_wstring(code.length()) + L" chars)");
        
        // Line count
        result.line_count = static_cast<int>(std::count(code.begin(), code.end(), L'\n')) + 1;
        
        // Function count (simple regex)
        std::wregex funcRegex(L"(void|bool|int|double|float|auto|std::\\w+|DWORD|BOOL)\\s+\\w+\\s*\\(");
        auto funcs_begin = std::wsregex_iterator(code.begin(), code.end(), funcRegex);
        auto funcs_end = std::wsregex_iterator();
        result.function_count = static_cast<int>(std::distance(funcs_begin, funcs_end));
        
        // Complexity score (rough estimate)
        result.complexity_score = result.line_count / 10 + result.function_count * 2;
        
        // Code style detection
        bool uses_spaces = code.find(L"    ") != std::wstring::npos;
        bool uses_tabs = code.find(L"\t") != std::wstring::npos;
        
        // Build analysis summary
        result.summary = L"File Analysis:\n";
        result.summary += L"  Lines: " + std::to_wstring(result.line_count) + L"\n";
        result.summary += L"  Functions: " + std::to_wstring(result.function_count) + L"\n";
        result.summary += L"  Complexity: " + std::to_wstring(result.complexity_score) + L"\n";
        result.summary += L"  Indentation: " + std::wstring(uses_spaces ? L"Spaces" : uses_tabs ? L"Tabs" : L"Mixed") + L"\n";
        
        // Generate suggestions
        if (result.complexity_score > 50) {
            result.suggestions.push_back(L"Consider breaking down complex functions");
        }
        if (result.line_count > 1000) {
            result.suggestions.push_back(L"File is large - consider splitting into modules");
        }
        if (uses_spaces && uses_tabs) {
            result.warnings.push_back(L"Mixed indentation detected - standardize to spaces or tabs");
        }
        if (result.function_count == 0 && result.line_count > 50) {
            result.warnings.push_back(L"No functions detected - check code structure");
        }
        
        // Check for common issues
        if (code.find(L"malloc") != std::wstring::npos && code.find(L"free") == std::wstring::npos) {
            result.warnings.push_back(L"malloc without matching free - possible memory leak");
        }
        if (code.find(L"new ") != std::wstring::npos && code.find(L"delete") == std::wstring::npos) {
            result.warnings.push_back(L"new without matching delete - possible memory leak");
        }
        
        // Cache analysis
        m_analysisCache[filePath] = result;
        m_analysisCount++;
        
        // Build full analysis string
        std::wstring fullAnalysis = result.summary;
        if (!result.suggestions.empty()) {
            fullAnalysis += L"\nSuggestions:\n";
            for (const auto& s : result.suggestions) {
                fullAnalysis += L"  • " + s + L"\n";
            }
        }
        if (!result.warnings.empty()) {
            fullAnalysis += L"\nWarnings:\n";
            for (const auto& w : result.warnings) {
                fullAnalysis += L"  ⚠ " + w + L"\n";
            }
        }
        
        AnalysisReady.Emit(fullAnalysis);
        
        return result;
    }
    
    std::wstring SuggestRefactoring(const std::wstring& code) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        LogInfo(L"Generating refactoring suggestions");
        
        std::wstring suggestions = L"Refactoring Suggestions:\n\n";
        
        // Analyze code patterns
        bool hasLongFunctions = false;
        bool hasDuplicateCode = false;
        bool hasComplexConditions = false;
        
        // Check for long functions
        std::wregex funcBlockRegex(L"\\{[^}]{1000,}\\}");
        if (std::regex_search(code, funcBlockRegex)) {
            hasLongFunctions = true;
            suggestions += L"1. Break down long functions into smaller, focused functions\n";
            suggestions += L"   - Each function should do one thing well\n";
            suggestions += L"   - Aim for functions under 50 lines\n\n";
        }
        
        // Check for repeated patterns
        if (code.find(L"if") != std::wstring::npos) {
            size_t ifCount = 0;
            size_t pos = 0;
            while ((pos = code.find(L"if", pos)) != std::wstring::npos) {
                ifCount++;
                pos += 2;
            }
            if (ifCount > 10) {
                hasComplexConditions = true;
                suggestions += L"2. Simplify complex conditional logic\n";
                suggestions += L"   - Use lookup tables or polymorphism\n";
                suggestions += L"   - Extract conditions into named functions\n\n";
            }
        }
        
        // Check for magic numbers
        std::wregex magicNumberRegex(L"\\b[0-9]{3,}\\b");
        if (std::regex_search(code, magicNumberRegex)) {
            suggestions += L"3. Replace magic numbers with named constants\n";
            suggestions += L"   - Use const or constexpr for clarity\n";
            suggestions += L"   - Makes code more maintainable\n\n";
        }
        
        // Check for error handling
        if (code.find(L"try") == std::wstring::npos && code.find(L"if") != std::wstring::npos) {
            suggestions += L"4. Add error handling and validation\n";
            suggestions += L"   - Check return values and pointers\n";
            suggestions += L"   - Use RAII for resource management\n\n";
        }
        
        if (suggestions == L"Refactoring Suggestions:\n\n") {
            suggestions += L"Code looks well-structured! No immediate refactoring needed.\n";
        }
        
        return suggestions;
    }
    
    std::wstring GenerateTestsForCode(const std::wstring& code, const std::wstring& framework) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        LogInfo(L"Generating unit tests");
        
        // Extract function names
        std::vector<std::wstring> functions;
        std::wregex funcRegex(L"(\\w+)\\s+(\\w+)\\s*\\([^)]*\\)");
        auto funcs_begin = std::wsregex_iterator(code.begin(), code.end(), funcRegex);
        auto funcs_end = std::wsregex_iterator();
        
        for (std::wsregex_iterator i = funcs_begin; i != funcs_end; ++i) {
            std::wsmatch match = *i;
            if (match.size() >= 3) {
                functions.push_back(match[2].str());
            }
        }
        
        // Generate test skeleton
        std::wstring tests = L"// Generated Unit Tests\n";
        tests += L"// Framework: " + framework + L"\n\n";
        tests += L"#include <cassert>\n";
        tests += L"#include <iostream>\n\n";
        
        for (const auto& func : functions) {
            tests += L"void test_" + func + L"() {\n";
            tests += L"    // TODO: Test normal case\n";
            tests += L"    // TODO: Test edge cases\n";
            tests += L"    // TODO: Test error conditions\n";
            tests += L"    std::cout << \"test_" + func + L" passed\" << std::endl;\n";
            tests += L"}\n\n";
        }
        
        tests += L"int main() {\n";
        for (const auto& func : functions) {
            tests += L"    test_" + func + L"();\n";
        }
        tests += L"    std::cout << \"All tests passed!\" << std::endl;\n";
        tests += L"    return 0;\n";
        tests += L"}\n";
        
        return tests;
    }
    
    // ========================================================================
    // CONVERSATION MANAGEMENT
    // ========================================================================
    
    void AddUserMessage(const std::wstring& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ConversationMessage msg;
        msg.role = ConversationMessage::USER;
        msg.content = message;
        msg.timestamp = std::chrono::system_clock::now();
        
        m_conversationHistory.push_back(msg);
        PruneConversationHistory();
        
        m_conversationCount++;
    }
    
    void AddAssistantMessage(const std::wstring& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ConversationMessage msg;
        msg.role = ConversationMessage::ASSISTANT;
        msg.content = message;
        msg.timestamp = std::chrono::system_clock::now();
        
        m_conversationHistory.push_back(msg);
        PruneConversationHistory();
    }
    
    std::vector<ConversationMessage> GetConversationHistory(size_t maxMessages = 100) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t start = (m_conversationHistory.size() > maxMessages) ? 
                      (m_conversationHistory.size() - maxMessages) : 0;
        
        return std::vector<ConversationMessage>(
            m_conversationHistory.begin() + start,
            m_conversationHistory.end()
        );
    }
    
    void ClearConversationHistory() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_conversationHistory.clear();
        LogInfo(L"Conversation history cleared");
    }
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    struct Statistics {
        int completion_count;
        int analysis_count;
        int conversation_count;
        std::chrono::milliseconds uptime;
        size_t history_size;
        size_t cache_size;
    };
    
    Statistics GetStatistics() const {
        Statistics stats;
        stats.completion_count = m_completionCount.load();
        stats.analysis_count = m_analysisCount.load();
        stats.conversation_count = m_conversationCount.load();
        
        auto now = std::chrono::steady_clock::now();
        stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_sessionStart);
        
        stats.history_size = m_conversationHistory.size();
        stats.cache_size = m_codeCache.size() + m_analysisCache.size();
        
        return stats;
    }
    
    std::wstring GetStatusReport() const {
        auto stats = GetStatistics();
        
        std::wstring report = L"Copilot Bridge Status:\n";
        report += L"  Initialized: " + std::wstring(m_initialized.load() ? L"Yes" : L"No") + L"\n";
        report += L"  Enabled: " + std::wstring(m_enabled.load() ? L"Yes" : L"No") + L"\n";
        report += L"  Completions: " + std::to_wstring(stats.completion_count) + L"\n";
        report += L"  Analyses: " + std::to_wstring(stats.analysis_count) + L"\n";
        report += L"  Conversations: " + std::to_wstring(stats.conversation_count) + L"\n";
        report += L"  History Size: " + std::to_wstring(stats.history_size) + L" messages\n";
        report += L"  Uptime: " + std::to_wstring(stats.uptime.count()) + L" ms\n";
        
        return report;
    }
    
    void SetEnabled(bool enabled) {
        m_enabled = enabled;
        LogInfo(enabled ? L"Copilot enabled" : L"Copilot disabled");
    }
    
    bool IsEnabled() const {
        return m_enabled.load();
    }
    
private:
    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================
    
    void InitializePaths() {
        wchar_t buffer[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, buffer);
        m_workspaceRoot = buffer;
    }
    
    std::wstring SimulateCompletion(const CompletionRequest& request) {
        // Context-aware code completion using pattern analysis and language heuristics
        std::wstring completion;
        const std::wstring& prefix = request.prefix;
        const std::wstring& context = request.context;
        const std::wstring& lang = request.language;

        // ── Detect last significant token in prefix ───────────────────────
        std::wstring trimmed = prefix;
        while (!trimmed.empty() && (trimmed.back() == L' ' || trimmed.back() == L'\t'))
            trimmed.pop_back();

        // ── Structural completions (braces, parens, brackets) ─────────────
        if (trimmed.ends_with(L"{")) {
            // Detect what precedes the brace to generate appropriate body
            if (context.find(L"class") != std::wstring::npos && 
                context.rfind(L"class") > context.rfind(L";")) {
                completion = L"\npublic:\n    \n\nprivate:\n    \n};";
            } else if (context.find(L"switch") != std::wstring::npos) {
                completion = L"\n    case 0:\n        break;\n    default:\n        break;\n}";
            } else if (context.find(L"enum") != std::wstring::npos) {
                completion = L"\n    \n};";
            } else {
                completion = L"\n    \n}";
            }
        }
        // ── Loop completions ──────────────────────────────────────────────
        else if (trimmed.ends_with(L"for")) {
            if (lang == L"python") {
                completion = L" item in collection:\n    pass";
            } else {
                // Detect iterable variable in context
                completion = L" (int i = 0; i < count; ++i) {\n    \n}";
            }
        }
        else if (trimmed.ends_with(L"while")) {
            completion = L" (condition) {\n    \n}";
        }
        // ── Conditional completions ───────────────────────────────────────
        else if (trimmed.ends_with(L"if")) {
            completion = L" (condition) {\n    \n}";
        }
        else if (trimmed.ends_with(L"else")) {
            completion = L" {\n    \n}";
        }
        // ── Function/method completions ───────────────────────────────────
        else if (trimmed.ends_with(L"void")) {
            completion = L" execute() {\n    \n}";
        }
        else if (trimmed.ends_with(L"int") || trimmed.ends_with(L"int32_t")) {
            completion = L" getValue() const {\n    return 0;\n}";
        }
        else if (trimmed.ends_with(L"bool")) {
            completion = L" isValid() const {\n    return true;\n}";
        }
        else if (trimmed.ends_with(L"std::string") || trimmed.ends_with(L"QString")) {
            completion = L" toString() const {\n    return \"\";\n}";
        }
        // ── Return statement completions ──────────────────────────────────
        else if (trimmed.ends_with(L"return")) {
            // Infer return type from context
            if (context.find(L"bool ") != std::wstring::npos) {
                completion = L" true;";
            } else if (context.find(L"std::string") != std::wstring::npos) {
                completion = L" \"\";";
            } else if (context.find(L"int ") != std::wstring::npos) {
                completion = L" 0;";
            } else if (context.find(L"void*") != std::wstring::npos) {
                completion = L" nullptr;";
            } else {
                completion = L" result;";
            }
        }
        // ── Include completions ───────────────────────────────────────────
        else if (trimmed.ends_with(L"#include")) {
            if (context.find(L"vector") != std::wstring::npos) {
                completion = L" <algorithm>";
            } else if (context.find(L"string") != std::wstring::npos) {
                completion = L" <string>";
            } else if (context.find(L"cout") != std::wstring::npos) {
                completion = L" <iostream>";
            } else {
                completion = L" <>";
            }
        }
        // ── Namespace/class member completions ────────────────────────────
        else if (trimmed.ends_with(L"std::")) {
            // Suggest common std types based on context
            if (context.find(L"vector") != std::wstring::npos) {
                completion = L"vector<>";
            } else if (context.find(L"map") != std::wstring::npos) {
                completion = L"unordered_map<std::string, >";
            } else {
                completion = L"string";
            }
        }
        // ── Try/catch completions ─────────────────────────────────────────
        else if (trimmed.ends_with(L"try")) {
            completion = L" {\n    \n} catch (const std::exception& e) {\n    \n}";
        }
        else if (trimmed.ends_with(L"catch")) {
            completion = L" (const std::exception& e) {\n    \n}";
        }
        // ── Smart semicolon line completion ───────────────────────────────
        else if (trimmed.ends_with(L"(")) {
            // Close the paren
            completion = L")";
        }
        // ── Fallback: context-aware completion ────────────────────────────
        else {
            // Look at suffix to infer what's expected
            if (!request.suffix.empty()) {
                wchar_t nextChar = request.suffix.front();
                if (nextChar == L')' || nextChar == L']' || nextChar == L'}') {
                    completion = L"";  // Cursor is before a closing delimiter
                } else {
                    completion = L";";
                }
            } else {
                completion = L"";
            }
        }

        return completion;
    }
    
    std::wstring HotpatchCompletion(const std::wstring& completion, const std::wstring& context) {
        std::wstring result = completion;
        
        // Remove markdown code fences if present
        if (result.find(L"```") != std::wstring::npos) {
            size_t start = result.find(L"```");
            size_t end = result.rfind(L"```");
            if (start != end && end != std::wstring::npos) {
                result = result.substr(start + 3, end - start - 3);
                
                // Remove language identifier line
                size_t newline = result.find(L'\n');
                if (newline != std::wstring::npos) {
                    result = result.substr(newline + 1);
                }
            }
        }
        
        // Trim whitespace
        result.erase(0, result.find_first_not_of(L" \t\n\r"));
        result.erase(result.find_last_not_of(L" \t\n\r") + 1);
        
        return result;
    }
    
    float CalculateConfidence(const std::wstring& completion, const std::wstring& context) {
        // Simple confidence calculation based on completion characteristics
        float confidence = 0.7f; // Base confidence
        
        if (!completion.empty()) confidence += 0.1f;
        if (completion.length() > 10) confidence += 0.1f;
        if (completion.find(L"TODO") == std::wstring::npos) confidence += 0.1f;
        
        return (confidence > 1.0f) ? 1.0f : confidence;
    }
    
    void PruneConversationHistory() {
        if (m_conversationHistory.size() > m_maxHistorySize) {
            // Keep system message and recent messages
            size_t toRemove = m_conversationHistory.size() - m_maxHistorySize;
            m_conversationHistory.erase(
                m_conversationHistory.begin() + 1,  // Skip system message
                m_conversationHistory.begin() + 1 + toRemove
            );
        }
    }
    
    void LogInfo(const std::wstring& message) {
        OutputDebugStringW((L"[CopilotBridge] INFO: " + message + L"\n").c_str());
    }
    
    void LogWarning(const std::wstring& message) {
        OutputDebugStringW((L"[CopilotBridge] WARNING: " + message + L"\n").c_str());
    }
    
    void LogError(const std::wstring& message) {
        OutputDebugStringW((L"[CopilotBridge] ERROR: " + message + L"\n").c_str());
    }
};

// ============================================================================
// C INTERFACE FOR DLL EXPORT
// ============================================================================

extern "C" {

__declspec(dllexport) CopilotBridge* CreateCopilotBridge() {
    return new CopilotBridge();
}

__declspec(dllexport) void DestroyCopilotBridge(CopilotBridge* bridge) {
    delete bridge;
}

__declspec(dllexport) BOOL CopilotBridge_Initialize(CopilotBridge* bridge) {
    return bridge ? (bridge->Initialize() ? TRUE : FALSE) : FALSE;
}

__declspec(dllexport) const wchar_t* CopilotBridge_GenerateCompletion(CopilotBridge* bridge,
    const wchar_t* context, const wchar_t* prefix) {
    if (!bridge || !context || !prefix) return L"";
    
    static std::wstring result;
    result = bridge->GenerateInlineCompletion(context, prefix);
    return result.c_str();
}

__declspec(dllexport) const wchar_t* CopilotBridge_AnalyzeCode(CopilotBridge* bridge,
    const wchar_t* code, const wchar_t* filePath) {
    if (!bridge || !code || !filePath) return L"";
    
    static std::wstring result;
    auto analysis = bridge->AnalyzeActiveFile(code, filePath);
    result = analysis.summary;
    return result.c_str();
}

__declspec(dllexport) const wchar_t* CopilotBridge_SuggestRefactoring(CopilotBridge* bridge,
    const wchar_t* code) {
    if (!bridge || !code) return L"";
    
    static std::wstring result;
    result = bridge->SuggestRefactoring(code);
    return result.c_str();
}

__declspec(dllexport) const wchar_t* CopilotBridge_GenerateTests(CopilotBridge* bridge,
    const wchar_t* code, const wchar_t* framework) {
    if (!bridge || !code) return L"";
    
    std::wstring frameworkStr = framework ? framework : L"Standard";
    static std::wstring result;
    result = bridge->GenerateTestsForCode(code, frameworkStr);
    return result.c_str();
}

__declspec(dllexport) void CopilotBridge_SetEnabled(CopilotBridge* bridge, BOOL enabled) {
    if (bridge) {
        bridge->SetEnabled(enabled != FALSE);
    }
}

__declspec(dllexport) BOOL CopilotBridge_IsEnabled(CopilotBridge* bridge) {
    return (bridge && bridge->IsEnabled()) ? TRUE : FALSE;
}

__declspec(dllexport) const wchar_t* CopilotBridge_GetStatus(CopilotBridge* bridge) {
    if (!bridge) return L"Bridge not available";
    
    static std::wstring status;
    status = bridge->GetStatusReport();
    return status.c_str();
}

} // extern "C"

// ============================================================================
// DLL ENTRY POINT
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringW(L"[RawrXD_CopilotBridge] DLL loaded\n");
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringW(L"[RawrXD_CopilotBridge] DLL unloaded\n");
        break;
    }
    return TRUE;
}
