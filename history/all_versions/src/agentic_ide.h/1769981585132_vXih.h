@
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

class AgenticIDE {
public:
    AgenticIDE(const IDEConfig& config);
    ~AgenticIDE();

    Result<void> initialize();
    Result<void> start();
    void stop();

    void setEditor(RawrXD::Editor* editor);
    nlohmann::json getStatus() const;

private:
    IDEConfig m_config;
    std::atomic<bool> m_running{false};
    
    // Core Components
    std::shared_ptr<RawrXD::UniversalModelRouter> m_modelRouter;
    std::shared_ptr<RawrXD::TokenGenerator> m_tokenizer;
    
    // Agentic Components
    std::unique_ptr<RawrXD::SwarmOrchestrator> m_swarm;
    std::unique_ptr<RawrXD::ChainOfThought> m_chainOfThought;
    
    // Legacy/Shim Components (to maintain compatibility if needed)
    std::unique_ptr<RawrXD::PlanOrchestrator> m_planOrchestrator;
    std::unique_ptr<RawrXD::LSPClient> m_lspClient;
    
    // Editor Integration
    RawrXD::Editor* m_editor{nullptr};
    
    void setupLogging();
    Result<void> loadModels();
};
@
