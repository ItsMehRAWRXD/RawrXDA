#pragma once
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <shared_mutex>
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
    mutable std::shared_mutex m_mutex;
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
    
    void log(const std::string& msg, spdlog::level::level_enum lvl);
};

} // namespace RawrXD
