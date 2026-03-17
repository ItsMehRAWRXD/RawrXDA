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

#include "token_generator.h"
#include "swarm_orchestrator.h"
#include "chain_of_thought.h"
#include "net_impl_win32.h"
#include "monaco_integration.h"
#include "agentic_ide.h"
#include "cpu_inference_engine.h"
#include "gguf_parser.h"
#include "vulkan_compute.h"

namespace RawrXD {

enum class IDEError {
    Success = 0,
    InitializationFailed,
    ComponentNotFound,
    ConfigurationInvalid,
    NetworkUnavailable,
    InferenceFailed,
    TokenizationFailed,
    RenderingFailed,
    FileOperationFailed,
    LSPCommunicationFailed,
    SwarmCoordinationFailed,
    ChainOfThoughtFailed,
    Timeout,
    CancellationRequested
};

struct IDEConfig {
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
    Net::NetworkConfig network;
    
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
    explicit IDEOrchestrator(const IDEConfig& config);
    ~IDEOrchestrator();
    
    // Non-copyable
    IDEOrchestrator(const IDEOrchestrator&) = delete;
    IDEOrchestrator& operator=(const IDEOrchestrator&) = delete;
    
    // Real initialization
    std::expected<void, IDEError> initialize();
    std::expected<void, IDEError> start();
    void stop();
    
    // Real IDE operations
    std::expected<std::string, IDEError> generateCode(
        const std::string& prompt,
        const std::unordered_map<std::string, std::string>& context = {}
    );
    
    std::expected<std::string, IDEError> debugCode(
        const std::string& code,
        const std::string& errorDescription
    );
    
    std::expected<std::string, IDEError> optimizeCode(
        const std::string& code
    );
    
    std::expected<std::string, IDEError> analyzeCodebase(
        const std::string& path
    );
    
    // Real file operations
    std::expected<void, IDEError> openFile(const std::string& path);
    std::expected<void, IDEError> saveFile(const std::string& path);
    std::expected<void, IDEError> closeFile(const std::string& path);
    
    // Real multi-file operations
    std::expected<std::vector<std::string>, IDEError> findReferences(
        const std::string& symbol
    );
    
    std::expected<std::vector<std::string>, IDEError> getCompletions(
        const std::string& context,
        size_t line,
        size_t column
    );
    
    // Real refactoring
    std::expected<void, IDEError> renameSymbol(
        const std::string& oldName,
        const std::string& newName
    );
    
    std::expected<void, IDEError> extractFunction(
        const std::string& codeBlock,
        const std::string& functionName
    );
    
    // Real testing
    std::expected<std::string, IDEError> generateTests(
        const std::string& code
    );
    
    std::expected<std::string, IDEError> runTests(
        const std::string& testCode
    );
    
    // Real documentation
    std::expected<std::string, IDEError> generateDocumentation(
        const std::string& code
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
    std::shared_ptr<Net::NetworkManager> getNetwork() const { return m_network; }
    std::shared_ptr<MonacoEditor> getEditor() const { return m_editor; }
    
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
    std::shared_ptr<Net::NetworkManager> m_network;
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
    std::atomic<std::chrono::milliseconds> m_totalProcessingTime{0};
    std::atomic<size_t> m_tokensGenerated{0};
    std::atomic<size_t> m_cacheHits{0};
    std::atomic<size_t> m_cacheMisses{0};
    
    // Real implementation methods
    std::expected<void, IDEError> initializeComponents();
    std::expected<void, IDEError> startBackgroundThreads();
    std::expected<void, IDEError> setupNetworking();
    std::expected<void, IDEError> setupTokenization();
    std::expected<void, IDEError> setupSwarm();
    std::expected<void, IDEError> setupChainOfThought();
    std::expected<void, IDEError> setupEditor();
    std::expected<void, IDEError> setupInference();
    
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
    static IDEManager& instance();
    
    std::expected<std::shared_ptr<IDEOrchestrator>, IDEError> createIDE(
        const IDEConfig& config
    );
    
    std::expected<void, IDEError> destroyIDE(const std::string& id);
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
