#pragma once
#include <string>
#include <optional>
#include <chrono>
#include <vector>

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

// IDEConfig is defined in ide_engine.hpp - avoid redefinition
#ifndef RAWRXD_IDECONFIG_DEFINED
#define RAWRXD_IDECONFIG_DEFINED
struct IDEConfig {
    std::string modelsPath = "./models";
    std::string toolsPath = "./tools";
    std::string configPath = "./config.json";
    std::string logPath = "./logs/agentic_ide.log";
    
    size_t maxWorkers = 4;
    size_t maxMemoryMB = 8192;
    std::chrono::seconds requestTimeout{30};
    std::chrono::seconds keepAliveTimeout{60};
    
    bool enableLSP = false;
    bool enableTerminal = false;
    bool enableChat = false;
    bool enableOrchestrator = false;
    bool enableZeroDay = false;
    bool enableSwarm = false;
    bool enableNetwork = false;
    bool enableVulkan = false;
    bool headless = true;
    bool enableLogging = false;
    bool enableFileLogging = false;
    bool enableTokenization = false;
    bool enableChainOfThought = false;
    bool enableMonaco = false;
    bool enableMetrics = false;
    int logLevel = 0;
};
#endif

#ifndef RAWRXD_SWARMTASK_DEFINED
#define RAWRXD_SWARMTASK_DEFINED
struct SwarmTask {
    std::string id;
    std::string description;
    int priority;
    std::vector<std::string> dependencies;
};
#endif

} // namespace RawrXD
