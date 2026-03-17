#pragma once
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <expected>
#include <functional>
#include <chrono>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "CommonTypes.h"
#include "token_generator.h"
#include "agentic/swarm_orchestrator.h"
#include "agentic/chain_of_thought.h"
#include "net_impl_win32.h"
#include "monaco_integration.h"
#include "agentic_ide.h"
#include "cpu_inference_engine.h"
#include "gguf_parser.h"
#include "vulkan_compute.h"
#include "utils/Expected.h"

namespace RawrXD {

// IDEError moved to CommonTypes.h

struct OrchestratorConfig {
    // Paths
    std::string modelsPath = "./models";
    std::string toolsPath = "./tools";
    std::string configPath = "./config.json";
    std::string logPath = "./logs/ide.log";
    std::string workspacePath = "./workspace";
    
    // Performance
    size_t maxWorkers = 8;
    size_t maxMemoryMB = 16384;
    std::chrono::seconds requestTimeout{30};
    std::chrono::seconds keepAliveTimeout{60};
    
    // Features
    bool enableNetwork = true;
    bool enableLSP = true;
    bool enableSwarm = true;
    bool enableChainOfThought = true;
    bool enableTokenization = true;
    bool enableVulkan = true;
    bool enableMonaco = true;
    bool enableLogging = true;
    bool enableMetrics = true;
    
    // Security
    std::optional<std::string> apiKey;
    bool enableSandbox = true;
    bool enableEncryption = true;
    
    // Tokenization
    TokenizationConfig tokenization;
    
    // Network
    RawrXD::Net::NetworkConfig network;
    
    // Swarm
    size_t maxAgents = 8;
    float requiredConfidence = 0.7f;
    std::chrono::milliseconds swarmTimeout{30000};
    
    // Logging
    std::string logLevel = "info";
    bool enableFileLogging = true;
    bool enableConsoleLogging = true;
};

class IDEOrchestrator : public std::enable_shared_from_this<IDEOrchestrator> {
public:
    explicit IDEOrchestrator(const OrchestratorConfig& config);
    ~IDEOrchestrator();
    
    // Non-copyable
    IDEOrchestrator(const IDEOrchestrator&) = delete;
    IDEOrchestrator& operator=(const IDEOrchestrator&) = delete;
    
    // Real initialization
    RawrXD::Expected<void, IDEError> initialize();
    RawrXD::Expected<void, IDEError> start();
    void stop();
    
    // Real IDE operations
    RawrXD::Expected<std::string, IDEError> generateCode(
        const std::string& prompt,
        const std::unordered_map<std::string, std::string>& context = {}
    );
    
    RawrXD::Expected<std::string, IDEError> debugCode(
        const std::string& code,
        const std::string& errorDescription
    );
    
    RawrXD::Expected<std::string, IDEError> optimizeCode(
        const std::string& code
    );
    
    RawrXD::Expected<std::string, IDEError> analyzeCodebase(
        const std::string& path
    );
    
    // Real file operations
    RawrXD::Expected<void, IDEError> openFile(const std::string& path);
    RawrXD::Expected<void, IDEError> saveFile(const std::string& path);
    RawrXD::Expected<void, IDEError> closeFile(const std::string& path);
    
    // Real multi-file operations
    RawrXD::Expected<std::vector<std::string>, IDEError> findReferences(
        const std::string& symbol
    );
    
    RawrXD::Expected<std::vector<std::string>, IDEError> getCompletions(
        const std::string& context,
        size_t line,
        size_t column
    );
    
    // Real refactoring
    RawrXD::Expected<void, IDEError> renameSymbol(
        const std::string& oldName,
        const std::string& newName
    );
    
    RawrXD::Expected<void, IDEError> extractFunction(
        const std::string& path,
        const std::string& functionName,
        const std::string& newPath
    );

    RawrXD::Expected<std::string, IDEError> generateTests(
        const std::string& path,
        const std::string& functionName
    );

    RawrXD::Expected<std::string, IDEError> runTests(
        const std::string& path,
        const std::string& testName
    );

    RawrXD::Expected<std::string, IDEError> generateDocumentation(
        const std::string& path,
        const std::string& symbol
    );
    
    // Status
    bool isRunning() const { return m_running.load(); }
    json getStatus() const;
    json getMetrics() const;
    
    // Component access
    std::shared_ptr<AgenticIDE> getIDE() const { return m_ide; }
    std::shared_ptr<TokenGenerator> getTokenizer() const { return m_tokenizer; }
    std::shared_ptr<SwarmOrchestrator> getSwarm() const { return m_swarm; }
    std::shared_ptr<ChainOfThought> getChainOfThought() const { return m_chainOfThought; }
    std::shared_ptr<RawrXD::Net::NetworkManager> getNetwork() const { return m_network; }
    std::shared_ptr<MonacoEditor> getEditor() const { return m_editor; }
    std::shared_ptr<CPUInferenceEngine> getInferenceEngine() const { return m_inferenceEngine; }
    std::shared_ptr<VulkanCompute> getVulkanCompute() const { return m_vulkanCompute; }
    std::shared_ptr<GGUFParser> getGGUFParser() const { return m_ggufParser; }
    
    // Configuration
    const OrchestratorConfig& getConfig() const { return m_config; }
    void updateConfig(const OrchestratorConfig& config);
    
private:
    IDEConfig m_config;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};
    mutable std::mutex m_mutex;
    
    // Real components
    std::shared_ptr<AgenticIDE> m_ide;
    std::shared_ptr<TokenGenerator> m_tokenizer;
    std::shared_ptr<SwarmOrchestrator> m_swarm;
    std::shared_ptr<ChainOfThought> m_chainOfThought;
    std::shared_ptr<RawrXD::Net::NetworkManager> m_network;
    std::shared_ptr<MonacoEditor> m_editor;
    std::shared_ptr<CPUInferenceEngine> m_inferenceEngine;
    std::shared_ptr<VulkanCompute> m_vulkanCompute;
    std::shared_ptr<GGUFParser> m_ggufParser;
    
    // Real orchestration threads
    std::thread m_mainThread;
    std::thread m_inferenceThread;
    std::thread m_renderThread;
    std::thread m_networkThread;
    
    // Real task queues
    std::queue<std::function<void()>> m_taskQueue;
    std::queue<std::function<void()>> m_inferenceQueue;
    std::queue<std::function<void()>> m_renderQueue;
    std::mutex m_taskMutex;
    std::mutex m_inferenceMutex;
    std::mutex m_renderMutex;
    std::condition_variable m_taskCondition;
    std::condition_variable m_inferenceCondition;
    std::condition_variable m_renderCondition;
    
    // Real metrics
    std::atomic<size_t> m_totalRequests{0};
    std::atomic<size_t> m_successfulRequests{0};
    std::atomic<size_t> m_failedRequests{0};
    // Use long long ms count to avoid atomic constructor issues
    std::atomic<long long> m_totalProcessingTimeMs{0};
    std::atomic<size_t> m_tokensGenerated{0};
    std::atomic<size_t> m_cacheHits{0};
    std::atomic<size_t> m_cacheMisses{0};
    
    // Real implementation methods
    RawrXD::Expected<void, IDEError> initializeComponents();
    RawrXD::Expected<void, IDEError> startBackgroundThreads();
    RawrXD::Expected<void, IDEError> setupNetworking();
    RawrXD::Expected<void, IDEError> setupTokenization();
    RawrXD::Expected<void, IDEError> setupSwarm();
    RawrXD::Expected<void, IDEError> setupChainOfThought();
    RawrXD::Expected<void, IDEError> setupEditor();
    RawrXD::Expected<void, IDEError> setupInference();
    
    void mainLoop();
    void inferenceLoop();
    void renderLoop();
    void networkLoop();
    
    // Real task processing
    void processTask(std::function<void()> task);
    void processInference(std::function<void()> inference);
    void processRender(std::function<void()> render);
    
    // Real error handling
    void handleError(IDEError error, const std::string& context);
    void logOperation(const std::string& operation, const std::string& details);
    
    // Real metrics collection
    void collectMetrics();
    void reportMetrics();
    
    // Helpers
    std::string getTimestamp() const;
    json createErrorReport(IDEError error, const std::string& context) const;
};

// Global singleton
class IDEManager {
public:
    static IDEManager& getInstance();

    RawrXD::Expected<std::shared_ptr<IDEOrchestrator>, IDEError> createIDE(
        const IDEConfig& config
    );

    RawrXD::Expected<void, IDEError> destroyIDE(const std::string& id);
    std::shared_ptr<IDEOrchestrator> getIDE(const std::string& id) const;
    
    json getAllStatus() const;
    json getAllMetrics() const;
    
private:
    IDEManager() = default;
    ~IDEManager() = default;
    
    IDEManager(const IDEManager&) = delete;
    IDEManager& operator=(const IDEManager&) = delete;
    
    std::unordered_map<std::string, std::shared_ptr<IDEOrchestrator>> m_ides;
    mutable std::mutex m_mutex;
};

} // namespace RawrXD





