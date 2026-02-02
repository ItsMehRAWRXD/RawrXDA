#pragma once
#include <string>
#include <optional>
#include <chrono>
#include <vector>
#include <spdlog/spdlog.h> 

namespace RawrXD {

enum class IDEError {
    Success = 0,
    InitializationFailed,
    ComponentNotFound,
    InvalidConfiguration,
    ResourceExhausted,
    Timeout,
    Cancelled,
    AlreadyRunning,
    NetworkError,
    AuthenticationFailed,
    ConfigurationInvalid, 
    NetworkUnavailable,
    InferenceFailed,
    TokenizationFailed,
    RenderingFailed,
    FileOperationFailed,
    LSPCommunicationFailed,
    SwarmCoordinationFailed,
    ChainOfThoughtFailed,
    CancellationRequested
};

template<typename T>
struct Result {
    std::optional<T> value;
    IDEError error = IDEError::Success;
    Result() {}
    Result(T v) : value(v) {}
    Result(IDEError e) : error(e) {}
    operator bool() const { return error == IDEError::Success; }
    IDEError error_code() const { return error; }
};

template<>
struct Result<void> {
    IDEError error = IDEError::Success;
    Result() {}
    Result(IDEError e) : error(e) {}
    operator bool() const { return error == IDEError::Success; }
    static Result<void> unused() { return Result<void>(); }
};

struct IDEConfig {
    std::string modelsPath = "./models";
    std::string toolsPath = "./tools";
    std::string configPath = "./config.json";
    std::string logPath = "./logs/agentic_ide.log";
    
    size_t maxWorkers = 4;
    size_t maxMemoryMB = 8192;
    std::chrono::seconds requestTimeout{30};
    std::chrono::seconds keepAliveTimeout{60}; // Used in orchestrator
    
    bool enableLSP = true;
    bool enableTerminal = true;
    bool enableChat = true;
    bool enableOrchestrator = true;
    bool enableZeroDay = true;
    bool enableSwarm = true;
    bool enableNetwork = true;
    bool enableVulkan = true;
    bool headless = false;
    
    // Added for compatibility with orchestrator
    bool enableLogging = true;
    bool enableFileLogging = true;
    bool enableTokenization = true;
    bool enableChainOfThought = true;
    bool enableMonaco = true;
    bool enableMetrics = true;
    
    int logLevel = 2; // spdlog::level::info
};

struct SwarmTask {
    std::string id;
    std::string description;
    int priority;
    std::vector<std::string> dependencies;
};

} // namespace RawrXD
