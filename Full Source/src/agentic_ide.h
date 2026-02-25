#pragma once
#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <functional>
#include <atomic>
#include <thread>
//#include <shared_mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include "CommonTypes.h"
#include "utils/Expected.h"

// Forward decs
namespace RawrXD {
    class ZeroDayAgenticEngine;
    class AutonomousIntelligenceOrchestrator;
    class ChatInterface;
    class MultiTabEditor;
    class ToolRegistry;
    class AutonomousModelManager;
    class TerminalPool;
    class CPUInferenceEngine;
    class UniversalModelRouter;
    class LSPClient;
    class PlanOrchestrator;
    class Editor;
}

namespace RawrXD {

class AgenticIDE : public std::enable_shared_from_this<AgenticIDE> {
public:
    explicit AgenticIDE(const IDEConfig& config = IDEConfig{});
    ~AgenticIDE();

    Result<void> initialize();
    Result<void> start();
    Result<void> stop();

    void setConfig(const IDEConfig& config);
    const IDEConfig& getConfig() const { return m_config; }
    
    nlohmann::json getStatus() const;
    void setEditor(Editor* editor);
    void processConsoleInput();

private:
    IDEConfig m_config;
    std::atomic<bool> m_running{false};
    mutable std::mutex m_mutex;
    
    // Task Queue
    std::mutex m_queueMutex;
    std::deque<std::function<void()>> m_taskQueue;
    std::vector<std::function<void()>> m_componentGuards;

    // Worker Threads
    std::condition_variable_any m_queueCv;
    std::vector<std::thread> m_workerThreads;
    std::atomic<int> m_activeWorkers{0};

    // Components
    std::shared_ptr<ZeroDayAgenticEngine> m_zeroDayAgent;
    std::shared_ptr<AutonomousIntelligenceOrchestrator> m_orchestrator;
    std::shared_ptr<ChatInterface> m_chatInterface;
    std::shared_ptr<MultiTabEditor> m_multiTabEditor;
    std::shared_ptr<ToolRegistry> m_toolRegistry;
    std::shared_ptr<AutonomousModelManager> m_modelManager;
    std::shared_ptr<TerminalPool> m_terminalPool;
    std::shared_ptr<CPUInferenceEngine> m_inferenceEngine;
    std::shared_ptr<UniversalModelRouter> m_modelRouter;
    std::shared_ptr<LSPClient> m_lspClient;
    std::shared_ptr<PlanOrchestrator> m_planOrchestrator;
    
    Editor* m_guiEditor = nullptr;

    Result<void> setupLogging();
    Result<void> initializeComponents();
    void cleanupComponents();
    
    // Missing methods added
    Result<void> wireComponents();
    Result<void> startBackgroundServices();
    void stopBackgroundServices();
    void startOrchestrator();
    void submitTask(std::function<void()> task);
    std::string getTimestamp() const;

    template<typename T>
    std::shared_ptr<T> getComponent() const;

    void log(const std::string& msg, spdlog::level::level_enum lvl);

private:
    std::shared_ptr<spdlog::logger> m_logger;
};

// Template implementation
template<typename T>
std::shared_ptr<T> AgenticIDE::getComponent() const {
    if constexpr (std::is_same_v<T, ZeroDayAgenticEngine>) return m_zeroDayAgent;
    if constexpr (std::is_same_v<T, AutonomousIntelligenceOrchestrator>) return m_orchestrator;
    if constexpr (std::is_same_v<T, ChatInterface>) return m_chatInterface;
    if constexpr (std::is_same_v<T, MultiTabEditor>) return m_multiTabEditor;
    if constexpr (std::is_same_v<T, ToolRegistry>) return m_toolRegistry;
    if constexpr (std::is_same_v<T, AutonomousModelManager>) return m_modelManager;
    if constexpr (std::is_same_v<T, TerminalPool>) return m_terminalPool;
    if constexpr (std::is_same_v<T, CPUInferenceEngine>) return m_inferenceEngine;
    if constexpr (std::is_same_v<T, UniversalModelRouter>) return m_modelRouter;
    if constexpr (std::is_same_v<T, LSPClient>) return m_lspClient;
    if constexpr (std::is_same_v<T, PlanOrchestrator>) return m_planOrchestrator;
    return nullptr;
}

} // namespace RawrXD
