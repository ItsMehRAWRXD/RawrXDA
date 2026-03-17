#pragma once
#include <string>
#include <optional>
#include <chrono>
#include <vector>
#include <spdlog/spdlog.h> // For error checking inside Result if needed, but Result is simple.

namespace RawrXD {

// Modern error handling
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
    AuthenticationFailed
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
    bool enableSwarm = true;
    bool enableNetwork = true; // Added missing field
    bool headless = false; 
};

struct SwarmTask {
    std::string id;
    std::string description;
    int priority;
    std::vector<std::string> dependencies;
};

// Common shim for Expected if not using the separate header everywhere
// But for now, let's keep Expected in its own utils header and just use Result here.

} // namespace RawrXD
