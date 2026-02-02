#pragma once
#include <compare>
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <chrono>
#include <queue>
#include <condition_variable>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <unordered_map>

#include "CommonTypes.h"
#include "utils/Expected.h"

// Forward declarations
namespace RawrXD {
    class TokenGenerator;
    class SwarmOrchestrator;
    class ChainOfThought;
    // class Net::NetworkManager; // Net namespace issue
    class MonacoEditor;
    class AgenticIDE;
    class CPUInferenceEngine;
    class GGUFParser;
    class VulkanCompute;
    namespace Net { class NetworkManager; }
}

namespace RawrXD {

class IDEOrchestrator {
public:
    explicit IDEOrchestrator(const IDEConfig& config);
    ~IDEOrchestrator();

    // Core lifecycle
    RawrXD::Expected<void, IDEError> initialize();
    RawrXD::Expected<void, IDEError> start();
    RawrXD::Expected<void, IDEError> stop();
    
    // Configuration
    const IDEConfig& getConfig() const { return m_config; }
    void updateConfig(const IDEConfig& config);

    // Status
    bool isRunning() const { return m_running.load(); }
    nlohmann::json getStatus() const;
    nlohmann::json getMetrics() const;
    
    // Component access
    std::shared_ptr<AgenticIDE> getIDE() const { return m_ide; }
    std::shared_ptr<TokenGenerator> getTokenizer() const { return m_tokenizer; }
    
private:
    IDEConfig m_config;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};
    mutable std::mutex m_mutex;
    
    // Components
    std::shared_ptr<AgenticIDE> m_ide;
    std::shared_ptr<TokenGenerator> m_tokenizer;
    std::shared_ptr<SwarmOrchestrator> m_swarm;
    std::shared_ptr<ChainOfThought> m_chainOfThought;
    std::shared_ptr<RawrXD::Net::NetworkManager> m_network;
    std::shared_ptr<MonacoEditor> m_editor;
    std::shared_ptr<CPUInferenceEngine> m_inferenceEngine;
    std::shared_ptr<VulkanCompute> m_vulkanCompute;
    std::shared_ptr<GGUFParser> m_ggufParser;
    
    // Threads
    std::thread m_mainThread;
    std::thread m_inferenceThread;
    std::thread m_renderThread;
    std::thread m_networkThread;
    
    // Queues
    std::queue<std::function<void()>> m_taskQueue;
    std::mutex m_taskMutex;
    std::condition_variable m_taskCondition;
    
    // Private implementation methods
    RawrXD::Expected<void, IDEError> initializeComponents();
    RawrXD::Expected<void, IDEError> setupNetworking();
    RawrXD::Expected<void, IDEError> setupTokenization();
    RawrXD::Expected<void, IDEError> setupSwarm();
    RawrXD::Expected<void, IDEError> setupChainOfThought();
    RawrXD::Expected<void, IDEError> setupEditor();
    RawrXD::Expected<void, IDEError> setupInference();
    RawrXD::Expected<void, IDEError> startBackgroundThreads();
    
    void mainLoop();
    void inferenceLoop();
    void renderLoop();
    void networkLoop();
    void processTask(std::function<void()> task);
    void processInference(std::function<void()> task);
    void processRender(std::function<void()> task);
    
    void collectMetrics();
    void reportMetrics();
    std::string getTimestamp() const;
};

class IDEManager {
public:
    static IDEManager& getInstance();
    RawrXD::Expected<std::shared_ptr<IDEOrchestrator>, IDEError> createIDE(const IDEConfig& config);
    RawrXD::Expected<void, IDEError> destroyIDE(const std::string& id);
    std::shared_ptr<IDEOrchestrator> getIDE(const std::string& id) const;
    nlohmann::json getAllStatus() const;
    nlohmann::json getAllMetrics() const;
private:
    IDEManager() = default;
    
    std::unordered_map<std::string, std::shared_ptr<IDEOrchestrator>> m_ides;
    mutable std::mutex m_mutex;
};

} // namespace RawrXD
