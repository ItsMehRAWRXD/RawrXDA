#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <queue>
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include "../config/settings.hpp"

// Forward declarations
namespace IDE_AI {
    class CompletionModel;
    class ProjectKnowledgeGraph;
    class LanguageCore;
    
    namespace LanguageCore {
        class ULR;
    }
}

namespace IDE_AI {

// Task types for AI provider selection
enum class TaskType {
    Completion,
    Reasoning,
    CodeGeneration,
    CodeAnalysis,
    LanguageLearning,
    Optimization,
    Security,
    MultiModal,
    ToolUse,
    Reflection
};

// AI Provider interface
class IAIProvider {
public:
    virtual ~IAIProvider() = default;
    virtual std::string get_name() const = 0;
    virtual bool is_available() const = 0;
    virtual bool supports_task(TaskType task_type) const = 0;
    virtual void generate_completion_async(const std::string& prompt, std::function<void(const std::string&)> callback) = 0;
    virtual void get_inline_suggestion_async(const std::string& context, std::function<void(const std::string&)> callback) = 0;
    virtual void chat_async(const std::string& message, std::function<void(const std::string&)> callback) = 0;
    virtual void optimize_ir_async(const LanguageCore::ULR& ulr, std::function<void(const LanguageCore::ULR&)> callback) = 0;
    virtual float get_confidence_score() const = 0;
    virtual int get_latency_ms() const = 0;
    virtual bool requires_network() const = 0;
};

// Hybrid AI Core - manages multiple AI providers
class AICoreHybrid {
public:
    static AICoreHybrid& get_instance() {
        static AICoreHybrid instance;
        return instance;
    }

    void initialize();
    void shutdown();
    void add_provider(std::unique_ptr<IAIProvider> provider);
    IAIProvider* get_provider_for_task(TaskType task_type);
    std::vector<IAIProvider*> get_all_providers();
    
    void generate_completion_async(const std::string& prompt, TaskType task_type, std::function<void(const std::string&)> callback);
    void get_inline_suggestion_async(const std::string& context, std::function<void(const std::string&)> callback);
    void chat_async(const std::string& message, std::function<void(const std::string&)> callback);
    void optimize_ir_async(const LanguageCore::ULR& ulr, std::function<void(const LanguageCore::ULR&)> callback);
    
    // Get system statistics
    struct AIStats {
        int total_providers;
        int available_providers;
        int local_providers;
        int external_providers;
        float average_confidence;
        int average_latency_ms;
        std::map<std::string, int> provider_usage;
    };
    
    AIStats get_stats() const;

private:
    AICoreHybrid() = default;
    ~AICoreHybrid() { shutdown(); }
    
    void load_configuration();
    void initialize_providers();
    void monitoring_loop();
    void monitor_provider_health();
    void update_statistics();
    void cleanup_resources();
    
    std::vector<std::unique_ptr<IAIProvider>> available_providers_;
    std::unique_ptr<CompletionModel> local_model_;
    std::mutex providers_mutex_;
    std::atomic<bool> monitoring_active_;
    std::thread monitoring_thread_;
};

} // namespace IDE_AI
