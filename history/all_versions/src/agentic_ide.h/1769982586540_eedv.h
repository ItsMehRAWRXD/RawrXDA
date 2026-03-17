#pragma once
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <functional>
#include <vector>
#include <string>
#include <optional>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <deque>
#include <condition_variable>

// Third-party
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

// Forward declarations
namespace RawrXD {
    class Editor;
    class UniversalModelRouter;
    class CPUInferenceEngine;
    class ToolRegistry;
    class PlanOrchestrator;
    class LSPClient;
    class MultiTabEditor;
    class ChatInterface;
    class TerminalPool; 
}
class TerminalPool; 
class AutonomousModelManager;
class AutonomousIntelligenceOrchestrator;
class ZeroDayAgenticEngine;

// Modern error handling
enum class IDEError {
    Success = 0,
    InitializationFailed,
    ComponentNotFound,
    InvalidConfiguration,
    ResourceExhausted,
    Timeout,
    Cancelled,
    AlreadyRunning
};

// Simple Result type shim
template<typename T>
struct Result {
    std::optional<T> value;
    IDEError error = IDEError::Success;
    
    Result() {}
    Result(T v) : value(v) {}
    Result(IDEError e) : error(e) {}
    
    Result(const Result&) = default;
    Result(Result&&) = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&) = default;

    operator bool() const { return error == IDEError::Success; }
    IDEError error_code() const { return error; }
};

// Specialization for void
template<>
struct Result<void> {
    IDEError error = IDEError::Success;
    Result() {}
    Result(IDEError e) : error(e) {}
    operator bool() const { return error == IDEError::Success; }
    static Result<void> unused() { return Result<void>(); }
};

namespace std {
    template<typename E>
    Result<void> unexpected(E e) { return Result<void>(e); }
}

// Configuration structure
struct IDEConfig {
    // Paths
    std::string modelsPath = "./models";
    std::string toolsPath = "./tools";
    std::string configPath = "./config.json";
    std::string logPath = "./logs/agentic_ide.log";
    
    // Performance
    size_t maxWorkers = 4;
    size_t maxMemoryMB = 8192;
    std::chrono::seconds requestTimeout{30};
    
    // Features
    bool enableLSP = true;
    bool enableTerminal = true;
    bool enableChat = true;
    bool enableOrchestrator = true;
    bool enableZeroDay = true;
    
    // Logging
    std::string logLevel = "info";
    bool enableFileLogging = true;
    bool enableConsoleLogging = true;
    
    // Security
    std::optional<std::string> apiKey;
    bool enableSandbox = true;
};

// RAII guard for components
class ComponentGuard {
public:
    using CleanupFunc = std::function<void()>;
    
    ComponentGuard(CleanupFunc cleanup) : m_cleanup(std::move(cleanup)) {}
    ~ComponentGuard() { if (m_cleanup) m_cleanup(); }
    
    ComponentGuard(const ComponentGuard&) = delete;
    ComponentGuard& operator=(const ComponentGuard&) = delete;
    ComponentGuard(ComponentGuard&&) noexcept = default;
    ComponentGuard& operator=(ComponentGuard&&) noexcept = default;
    
private:
    CleanupFunc m_cleanup;
};

using json = nlohmann::json;

// Main Agentic IDE Class

class AgenticIDE : public std::enable_shared_from_this<AgenticIDE> {
public:
    explicit AgenticIDE(const IDEConfig& config = IDEConfig{});
    ~AgenticIDE();
    
    // Non-copyable
    AgenticIDE(const AgenticIDE&) = delete;
    AgenticIDE& operator=(const AgenticIDE&) = delete;
    
    // Movable
    AgenticIDE(AgenticIDE&&) noexcept = default;
    AgenticIDE& operator=(AgenticIDE&&) noexcept = default;
    
    // Initialization
    Result<void> initialize();
    Result<void> start();
    void stop();
    
    // Compatibility with old integration
    void startOrchestrator(); 

    // Configuration
    void setConfig(const IDEConfig& config);
    const IDEConfig& getConfig() const { return m_config; }
    
    // Component access
    template<typename T>
    std::shared_ptr<T> getComponent() const;
    
    // Editor integration
    void setEditor(RawrXD::Editor* editor);
    RawrXD::Editor* getEditor() const { return m_guiEditor; }
    
    // Status
    json getStatus() const;
    bool isRunning() const { return m_running.load(std::memory_order_acquire); }
    
    // Console input (for testing)
    void processConsoleInput();
    
private:
    IDEConfig m_config;
    std::atomic<bool> m_running{false};
    mutable std::shared_mutex m_mutex;
    
    // Components
    std::unique_ptr<RawrXD::UniversalModelRouter> m_modelRouter;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_inferenceEngine;
    std::unique_ptr<TerminalPool> m_terminalPool;
    std::unique_ptr<AutonomousModelManager> m_modelManager;
    std::unique_ptr<RawrXD::ToolRegistry> m_toolRegistry;
    std::unique_ptr<RawrXD::PlanOrchestrator> m_planOrchestrator;
    std::unique_ptr<RawrXD::LSPClient> m_lspClient;
    std::unique_ptr<MultiTabEditor> m_multiTabEditor;
    std::unique_ptr<ChatInterface> m_chatInterface;
    std::unique_ptr<AutonomousIntelligenceOrchestrator> m_orchestrator;
    std::unique_ptr<ZeroDayAgenticEngine> m_zeroDayAgent;
    
    RawrXD::Editor* m_guiEditor = nullptr;
    
    // Component guards for cleanup
    std::vector<ComponentGuard> m_componentGuards;
    
    // Thread pool for background tasks
    std::vector<std::thread> m_workerThreads;
    std::atomic<size_t> m_activeWorkers{0};
    
    // Logging
    std::shared_ptr<spdlog::logger> m_logger;
    
    // Initialization helpers
    Result<void> setupLogging();
    Result<void> initializeComponents();
    Result<void> wireComponents();
    Result<void> startBackgroundServices();
    
    // Cleanup helpers
    void cleanupComponents();
    void stopBackgroundServices();
    
    // Utility functions
    std::string getTimestamp() const;
    void log(const std::string& message, spdlog::level::level_enum level = spdlog::level::info) const;
};
