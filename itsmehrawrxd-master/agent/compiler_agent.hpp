#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <queue>
#include <iostream>

// Forward declarations
namespace IDE_AI {
    class AICoreHybrid;
    class ProjectKnowledgeGraph;
}

namespace Agents {

// Compilation intent analysis
struct CompilationIntent {
    std::string primary_goal;
    std::string target_platform;
    std::string optimization_level;
    std::vector<std::string> specific_requirements;
    float confidence;
    std::chrono::steady_clock::time_point timestamp;
    
    CompilationIntent() : confidence(0.0f) {
        timestamp = std::chrono::steady_clock::now();
    }
};

// Compilation context
struct CompilationContext {
    std::string source_code;
    std::string language_hint;
    std::string target_language;
    CompilationIntent intent;
    std::map<std::string, std::string> environment_vars;
    std::vector<std::string> include_paths;
    std::vector<std::string> library_paths;
    std::map<std::string, std::string> compiler_flags;
    bool debug_mode;
    bool verbose_output;
    
    CompilationContext() : debug_mode(false), verbose_output(false) {}
};

// LLM-Powered Compiler Agent
class CompilerAgent {
public:
    CompilerAgent(const std::string& name, IDE_AI::AICoreHybrid* ai_core, IDE_AI::ProjectKnowledgeGraph* pkg);
    ~CompilerAgent();

    void handle_message(const std::string& command, const std::map<std::string, std::string>& payload);
    
    // Main compilation method with AI-powered intent interpretation
    void compile_with_intent(const std::string& source_code, const std::string& user_intent);
    
    // Get compilation statistics
    struct CompilationStats {
        int total_compilations;
        int successful_compilations;
        int failed_compilations;
        std::map<std::string, int> intent_counts;
        std::map<std::string, int> language_counts;
        float average_confidence;
        int average_compilation_time_ms;
        std::chrono::steady_clock::time_point last_compilation;
    };
    
    CompilationStats get_compilation_stats() const;

private:
    void handle_compile_request(const std::map<std::string, std::string>& payload);
    void handle_compile_with_intent(const std::map<std::string, std::string>& payload);
    void handle_optimize_code(const std::map<std::string, std::string>& payload);
    void handle_analyze_code(const std::map<std::string, std::string>& payload);
    void handle_explain_compilation(const std::map<std::string, std::string>& payload);
    void handle_get_stats(const std::map<std::string, std::string>& payload);
    
    CompilationIntent analyze_compilation_intent(const std::string& user_intent, const std::string& source_code);
    std::string detect_language(const std::string& source_code);
    std::string determine_target_language(const CompilationIntent& intent);
    void optimize_compilation_settings(CompilationContext& context);
    void compilation_loop();
    void process_compilation_queue();
    void update_compilation_stats(const std::string& result, int compilation_time_ms);
    void update_statistics();
    
    // Data structures
    struct CompilationTask {
        std::string source_code;
        std::string user_intent;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    // Member variables
    std::string name_;
    IDE_AI::AICoreHybrid* ai_core_;
    IDE_AI::ProjectKnowledgeGraph* pkg_;
    
    std::queue<CompilationTask> compilation_queue_;
    CompilationStats compilation_stats_;
    
    std::mutex compilation_queue_mutex_;
    std::mutex stats_mutex_;
    
    std::atomic<bool> compilation_active_;
    std::thread compilation_thread_;
};

} // namespace Agents
