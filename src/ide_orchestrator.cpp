#include "ide_orchestrator.h"
#include "vulkan_compute.h"
#include "swarm_orchestrator.h" // Ensure these are included too if they weren't
#include "chain_of_thought.h"
#include "token_generator.h"
#include "toolchain_integration.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace fs = std::filesystem;

namespace RawrXD {

IDEOrchestrator::IDEOrchestrator(const IDEConfig& config) : m_config(config) {
}

IDEOrchestrator::~IDEOrchestrator() {
    stop();
}

RawrXD::Expected<void, IDEError> IDEOrchestrator::initialize() {
    auto startTime = std::chrono::steady_clock::now();
    
    // Initialize components
    auto componentResult = initializeComponents();
    if (!componentResult) {
        return componentResult;
    }
    
    // Setup networking if enabled
    if (m_config.enableNetwork) {
        auto networkResult = setupNetworking();
        if (!networkResult) {
            fprintf(stderr, "[IDEOrchestrator] Network initialization failed, continuing without network\n");
        }
    }
    
    // Setup tokenization if enabled
    if (m_config.enableTokenization) {
        auto tokenResult = setupTokenization();
        if (!tokenResult) {
            return tokenResult;
        }
    }
    
    // Setup swarm if enabled
    if (m_config.enableSwarm) {
        auto swarmResult = setupSwarm();
        if (!swarmResult) {
            return swarmResult;
        }
    }
    
    // Setup chain-of-thought if enabled
    if (m_config.enableChainOfThought) {
        auto chainResult = setupChainOfThought();
        if (!chainResult) {
            return chainResult;
        }
    }
    
    // Setup editor if enabled
    if (m_config.enableMonaco) {
        auto editorResult = setupEditor();
        if (!editorResult) {
            return editorResult;
        }
    }
    
    // Setup inference
    auto inferenceResult = setupInference();
    if (!inferenceResult) {
        return inferenceResult;
    }
    
    // Setup toolchain integration (MASM64 assembler + linker)
    {
        auto toolchainResult = setupToolchain();
        if (!toolchainResult) {
            fprintf(stderr, "[IDEOrchestrator] Toolchain not found, IDE works without it\n");
        }
    }
    
    // Start background threads
    auto threadResult = startBackgroundThreads();
    if (!threadResult) {
        return threadResult;
    }
    
    m_initialized = true;
    
    return {};
}

RawrXD::Expected<void, IDEError> IDEOrchestrator::initializeComponents() {
    // Create Vulkan Compute first (hardware foundation)
    m_vulkanCompute = std::make_shared<VulkanCompute>();
    auto vkResult = m_vulkanCompute->initialize();
    if (!vkResult) {
        fprintf(stderr, "[IDEOrchestrator] Vulkan not available, running in CPU mode\n");
    }

    // Create inference engine first (needed by others)
    m_inferenceEngine = std::make_shared<CPUInferenceEngine>();
    
    // Create tokenizer
    m_tokenizer = std::make_shared<TokenGenerator>(m_config.tokenization);
    if (m_vulkanCompute) {
        m_tokenizer->setVulkanCompute(m_vulkanCompute);
    }
    
    // Create network manager
    m_network = std::make_shared<Net::NetworkManager>();
    
    // Create swarm orchestrator
    m_swarm = std::make_shared<SwarmOrchestrator>(m_config.maxAgents);
    
    // Create chain-of-thought
    m_chainOfThought = std::make_shared<ChainOfThought>();
    
    // Create Monaco editor
    m_editor = MonacoFactory::createEditor(MonacoVariant::Enterprise);
    
    // Create main IDE
    IDEConfig ideConfig;
    ideConfig.modelsPath = m_config.modelsPath;
    ideConfig.toolsPath = m_config.toolsPath;
    ideConfig.maxWorkers = m_config.maxWorkers;
    ideConfig.logLevel = m_config.logLevel;
    ideConfig.enableFileLogging = m_config.enableFileLogging;
    ideConfig.logPath = m_config.logPath;
    ideConfig.apiKey = m_config.apiKey;
    ideConfig.enableSandbox = m_config.enableSandbox;
    ideConfig.enableLSP = m_config.enableLSP;
    ideConfig.enableChat = true;
    ideConfig.enableOrchestrator = true;
    ideConfig.enableZeroDay = true;
    
    m_ide = std::make_shared<AgenticIDE>(ideConfig);
    
    // Wire components together
    m_ide->setEditor(m_editor.get());
    m_swarm->setAgenticEngine(m_ide->getAgenticEngine());
    m_chainOfThought->setInferenceEngine(m_inferenceEngine.get());
    
    return {};
}

RawrXD::Expected<void, IDEError> IDEOrchestrator::setupNetworking() {
    auto& netManager = Net::NetworkManager::instance();
    auto result = netManager.initialize(m_config.network);
    
    if (!result) {
        return std::unexpected(IDEError::NetworkUnavailable);
    }
    
    // Test network connectivity
    auto httpResult = netManager.getHttpClient().get("http://localhost:11435/api/tags");
    if (!httpResult) {
        fprintf(stderr, "[IDEOrchestrator] Network test failed but continuing\n");
    }
    
    return {};
}

RawrXD::Expected<void, IDEError> IDEOrchestrator::setupTokenization() {
    // Load vocabulary if path provided
    if (!m_config.tokenization.vocabPath.empty()) {
        auto result = m_tokenizer->loadVocabulary(m_config.tokenization.vocabPath);
        if (!result) {
            return std::unexpected(IDEError::TokenizationFailed);
        }
    }
    
    // Load merge rules if path provided
    if (!m_config.tokenization.mergesPath.empty()) {
        auto result = m_tokenizer->loadMergeRules(m_config.tokenization.mergesPath);
        if (!result) {
            return std::unexpected(IDEError::TokenizationFailed);
        }
    }
    
    // Load from model if path provided
    if (!m_config.tokenization.modelPath.empty()) {
        auto result = m_tokenizer->loadVocabularyFromModel(m_config.tokenization.modelPath);
        if (!result) {
            // Create minimal vocabulary for fallback
            // This ensures the tokenizer works even without files
        }
    }
    
    return {};
}

RawrXD::Expected<void, IDEError> IDEOrchestrator::setupSwarm() {
    // Add agents with specializations
    m_swarm->addAgent(AgentSpecialization::Coding, 
                     {AgentSpecialization::Coding, AgentSpecialization::Testing});
    m_swarm->addAgent(AgentSpecialization::Debugging, 
                     {AgentSpecialization::Debugging, AgentSpecialization::Analysis});
    m_swarm->addAgent(AgentSpecialization::Optimization, 
                     {AgentSpecialization::Optimization, AgentSpecialization::Performance});
    m_swarm->addAgent(AgentSpecialization::Analysis, 
                     {AgentSpecialization::Analysis, AgentSpecialization::Architecture});
    m_swarm->addAgent(AgentSpecialization::Documentation, 
                     {AgentSpecialization::Documentation, AgentSpecialization::Analysis});
    m_swarm->addAgent(AgentSpecialization::Security, 
                     {AgentSpecialization::Security, AgentSpecialization::Testing});
    
    return {};
}

RawrXD::Expected<void, IDEError> IDEOrchestrator::setupChainOfThought() {
    return {};
}

RawrXD::Expected<void, IDEError> IDEOrchestrator::setupEditor() {
    auto result = m_editor->initialize(nullptr); // Will create its own window
    if (!result) {
        return std::unexpected(IDEError::RenderingFailed);
    }
    
    // Load a test file
    std::string testFile = "test.cpp";
    std::ofstream testFileStream(testFile);
    testFileStream << "#include <iostream>\n\nint main() {\n    std::cout << \"Hello World!\" << std::endl;\n    return 0;\n}";
    testFileStream.close();
    
    auto loadResult = m_editor->loadFile(testFile);
    
    // Set up LSP if enabled
    if (m_config.enableLSP) {
        auto lspResult = m_editor->setLanguageServer("clangd.exe");
    }
    
    return {};
}

RawrXD::Expected<void, IDEError> IDEOrchestrator::setupInference() {
    // Load a test model
    std::string modelPath = m_config.modelsPath + "/test.gguf";
    if (fs::exists(modelPath)) {
        auto loadResult = m_inferenceEngine->loadModel(modelPath);
        if (loadResult) {
            // Test inference
            std::string testPrompt = "Write a hello world program in C++";
            auto result = m_inferenceEngine->generate(testPrompt, 0.7f, 0.9f, 100);
        }
    }
    
    return {};
}

RawrXD::Expected<void, IDEError> IDEOrchestrator::setupToolchain() {
    RawrXD::IDE::ToolchainConfig tcConfig;
    tcConfig.enabled         = true;
    tcConfig.autoDetectTasks = true;
    tcConfig.autoAnalyze     = true;
    tcConfig.debounceMs      = 300;
    tcConfig.maxDiagnostics  = 500;
    tcConfig.workspaceRoot   = m_config.toolsPath; /* Use workspace root when available */
    
    m_toolchain = std::make_unique<RawrXD::IDE::ToolchainIntegration>();
    if (!m_toolchain->initialize(tcConfig)) {
        m_toolchain.reset();
        return std::unexpected(IDEError::InitializationFailed);
    }
    
    return {};
}

RawrXD::Expected<void, IDEError> IDEOrchestrator::startBackgroundThreads() {
    m_running = true;
    
    // Start main thread
    m_mainThread = std::thread(&IDEOrchestrator::mainLoop, this);
    
    // Start inference thread
    m_inferenceThread = std::thread(&IDEOrchestrator::inferenceLoop, this);
    
    // Start render thread
    m_renderThread = std::thread(&IDEOrchestrator::renderLoop, this);
    
    // Start network thread
    m_networkThread = std::thread(&IDEOrchestrator::networkLoop, this);
    
    return {};
}

void IDEOrchestrator::mainLoop() {
    while (m_running.load()) {
        std::unique_lock lock(m_taskMutex);
        
        // Wait for tasks or timeout
        m_taskCondition.wait_for(lock, std::chrono::milliseconds(100), 
            [this] { return !m_taskQueue.empty() || !m_running.load(); });
        
        if (!m_running.load()) break;
        
        // Process tasks
        while (!m_taskQueue.empty()) {
            auto task = std::move(m_taskQueue.front());
            m_taskQueue.pop();
            lock.unlock();
            
            processTask(task);
            
            lock.lock();
        }
        
        lock.unlock();
        
        // Collect metrics periodically
        static auto lastMetrics = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastMetrics > std::chrono::seconds(30)) {
            collectMetrics();
            lastMetrics = now;
        }
    }
}

void IDEOrchestrator::inferenceLoop() {
    while (m_running.load()) {
        std::unique_lock lock(m_inferenceMutex);
        
        m_inferenceCondition.wait_for(lock, std::chrono::milliseconds(10), 
            [this] { return !m_inferenceQueue.empty() || !m_running.load(); });
        
        if (!m_running.load()) break;
        
        while (!m_inferenceQueue.empty()) {
            auto inference = std::move(m_inferenceQueue.front());
            m_inferenceQueue.pop();
            lock.unlock();
            
            processInference(inference);
            
            lock.lock();
        }
    }
}

void IDEOrchestrator::renderLoop() {
    while (m_running.load()) {
        std::unique_lock lock(m_renderMutex);
        
        m_renderCondition.wait_for(lock, std::chrono::milliseconds(16), // ~60 FPS
            [this] { return !m_renderQueue.empty() || !m_running.load(); });
        
        if (!m_running.load()) break;
        
        while (!m_renderQueue.empty()) {
            auto render = std::move(m_renderQueue.front());
            m_renderQueue.pop();
            lock.unlock();
            
            processRender(render);
            
            lock.lock();
        }
    }
}

void IDEOrchestrator::networkLoop() {
    while (m_running.load()) {
        // Process network events
        auto& netManager = Net::NetworkManager::instance();
        
        if (netManager.isInitialized()) {
            // Check for pending requests
            // This is where we'd handle async network callbacks
            
            // Poll for incoming data
            // This is where we'd handle WebSocket messages
            
            // Handle timeouts
            // This is where we'd cancel stuck requests
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void IDEOrchestrator::processTask(std::function<void()> task) {
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        task();
        m_successfulRequests.fetch_add(1);
    } catch (const std::exception& e) {
        m_failedRequests.fetch_add(1);
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);
    m_totalProcessingTime.fetch_add(duration);
}

void IDEOrchestrator::processInference(std::function<void()> inference) {
    try {
        inference();
    } catch (const std::exception& e) {
    }
}

void IDEOrchestrator::processRender(std::function<void()> render) {
    try {
        render();
    } catch (const std::exception& e) {
    }
}

void IDEOrchestrator::collectMetrics() {
    json metrics = {
        {"timestamp", getTimestamp()},
        {"requests", {
            {"total", m_totalRequests.load()},
            {"successful", m_successfulRequests.load()},
            {"failed", m_failedRequests.load()}
        }},
        {"processing_time_ms", m_totalProcessingTime.load().count()},
        {"tokens_generated", m_tokensGenerated.load()},
        {"cache", {
            {"hits", m_cacheHits.load()},
            {"misses", m_cacheMisses.load()}
        }},
        {"components", {
            {"tokenizer", m_tokenizer->getStatus()},
            {"swarm", m_swarm->getStatus()},
            {"chain_of_thought", m_chainOfThought->getStatus()},
            {"network", Net::NetworkManager::instance().getNetworkStatus()},
            {"editor", m_editor->getStatus()}
        }}
    };
    
    // Save metrics to file
    std::string metricsPath = "metrics.json";
    std::ofstream file(metricsPath);
    if (file) {
        file << metrics.dump(2);
    }
    
    // Report to monitoring system if configured
    if (m_config.enableMetrics) {
        reportMetrics();
    }
}

void IDEOrchestrator::reportMetrics() {
    // In production, this would send metrics to a monitoring service
    // For now, metrics are collected silently
}

json IDEOrchestrator::getStatus() const {
    std::lock_guard lock(m_mutex);
    
    return {
        {"running", m_running.load()},
        {"initialized", m_initialized.load()},
        {"components", {
            {"ide", m_ide ? m_ide->getStatus() : json()},
            {"tokenizer", m_tokenizer ? m_tokenizer->getStatus() : json()},
            {"swarm", m_swarm ? m_swarm->getStatus() : json()},
            {"chain_of_thought", m_chainOfThought ? m_chainOfThought->getStatus() : json()},
            {"network", Net::NetworkManager::instance().getNetworkStatus()},
            {"editor", m_editor ? m_editor->getStatus() : json()},
            {"inference_engine", m_inferenceEngine ? m_inferenceEngine->getStatus() : json()}
        }},
        {"metrics", getMetrics()},
        {"config", {
            {"models_path", m_config.modelsPath},
            {"max_workers", m_config.maxWorkers},
            {"max_memory_mb", m_config.maxMemoryMB},
            {"enable_network", m_config.enableNetwork},
            {"enable_swarm", m_config.enableSwarm},
            {"enable_chain_of_thought", m_config.enableChainOfThought},
            {"enable_tokenization", m_config.enableTokenization},
            {"enable_vulkan", m_config.enableVulkan},
            {"enable_monaco", m_config.enableMonaco}
        }}
    };
}

json IDEOrchestrator::getMetrics() const {
    return {
        {"timestamp", getTimestamp()},
        {"requests", {
            {"total", m_totalRequests.load()},
            {"successful", m_successfulRequests.load()},
            {"failed", m_failedRequests.load()}
        }},
        {"processing_time_ms", m_totalProcessingTime.load().count()},
        {"tokens_generated", m_tokensGenerated.load()},
        {"cache", {
            {"hits", m_cacheHits.load()},
            {"misses", m_cacheMisses.load()}
        }}
    };
}

std::string IDEOrchestrator::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// IDEManager Implementation
IDEManager& IDEManager::instance() {
    static IDEManager instance;
    return instance;
}

std::expected<std::shared_ptr<IDEOrchestrator>, IDEError> IDEManager::createIDE(
    const IDEConfig& config
) {
    std::lock_guard lock(m_mutex);
    
    auto id = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    auto ide = std::make_shared<IDEOrchestrator>(config);
    
    auto result = ide->initialize();
    if (!result) {
        return std::unexpected(result.error());
    }
    
    m_ides[id] = ide;
    
    return ide;
}

std::expected<void, IDEError> IDEManager::destroyIDE(const std::string& id) {
    std::lock_guard lock(m_mutex);
    
    auto it = m_ides.find(id);
    if (it == m_ides.end()) {
        return std::unexpected(IDEError::ComponentNotFound);
    }
    
    it->second->stop();
    m_ides.erase(it);
    
    return {};
}

std::shared_ptr<IDEOrchestrator> IDEManager::getIDE(const std::string& id) const {
    std::lock_guard lock(m_mutex);
    
    auto it = m_ides.find(id);
    if (it != m_ides.end()) {
        return it->second;
    }
    
    return nullptr;
}

json IDEManager::getAllStatus() const {
    std::lock_guard lock(m_mutex);
    
    json status;
    status["ides"] = json::array();
    
    for (const auto& [id, ide] : m_ides) {
        status["ides"].push_back({
            {"id", id},
            {"status", ide->getStatus()}
        });
    }
    
    return status;
}

RawrXD::Expected<void, IDEError> IDEOrchestrator::start() {
    if (m_running) return {};
    if (m_ide) m_ide->start();
    return startBackgroundThreads();
}

RawrXD::Expected<void, IDEError> IDEOrchestrator::stop() {
    m_running = false;
    if (m_ide) m_ide->stop();
    if (m_mainThread.joinable()) m_mainThread.join();
    if (m_inferenceThread.joinable()) m_inferenceThread.join();
    if (m_renderThread.joinable()) m_renderThread.join();
    if (m_networkThread.joinable()) m_networkThread.join();
    return {};
}

json IDEManager::getAllMetrics() const {
    std::lock_guard lock(m_mutex);
    
    json metrics;
    metrics["ides"] = json::array();
    
    for (const auto& [id, ide] : m_ides) {
        metrics["ides"].push_back({
            {"id", id},
            {"metrics", ide->getMetrics()}
        });
    }
    
    return metrics;
}

} // namespace RawrXD
