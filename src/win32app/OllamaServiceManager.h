#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <vector>
#include <unordered_map>
#include <chrono>

class Win32IDE;

/**
 * @brief Embedded Ollama Service Manager for Win32IDE
 * 
 * Manages an embedded Ollama service instance that runs alongside the IDE,
 * eliminating the need for external Ollama installations. Provides:
 * - Automatic Ollama binary download/bundling
 * - Service lifecycle management (start/stop/restart)
 * - Terminal pane integration with live logs
 * - Health monitoring and auto-recovery
 * - UI controls for service management
 */
class OllamaServiceManager {
public:
    enum class ServiceState {
        Stopped,
        Starting,
        Running,
        Stopping,
        Error,
        Downloading,
        Installing
    };

    enum class LogLevel {
        Info,
        Warning,
        Error,
        Debug
    };

    struct ServiceConfig {
        std::string host = "127.0.0.1";
        int port = 11434;
        std::string modelsPath = "models";
        bool autoStart = true;
        bool enableLogging = true;
        std::string logLevel = "info";
        int maxRestartAttempts = 3;
        int healthCheckIntervalMs = 5000;
        bool downloadOllamaIfMissing = true;
        std::string ollamaDownloadUrl = "https://github.com/ollama/ollama/releases/latest/download/ollama-windows-amd64.exe";
    };

    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        LogLevel level;
        std::string message;
        std::string source;
    };

    using LogCallback = std::function<void(const LogEntry&)>;
    using StateCallback = std::function<void(ServiceState, const std::string&)>;

public:
    explicit OllamaServiceManager(Win32IDE* ide);
    ~OllamaServiceManager();

    // Component lifecycle (following Win32IDE patterns)
    bool initialize();
    void shutdown();

    // Service management
    bool startService();
    bool stopService();
    bool restartService();
    bool isServiceRunning() const;
    bool isServiceHealthy() const;
    ServiceState getServiceState() const;
    DWORD getServicePID() const;

    // Configuration
    void setConfig(const ServiceConfig& config);
    ServiceConfig getConfig() const;
    std::string getServiceEndpoint() const;
    
    // Binary management
    bool isOllamaBinaryAvailable() const;
    bool downloadOllamaBinary();
    bool installOllamaBinary();
    std::string getOllamaBinaryPath() const;
    std::string getOllamaVersion() const;

    // Logging and monitoring
    void setLogCallback(LogCallback callback);
    void setStateCallback(StateCallback callback);
    std::vector<LogEntry> getRecentLogs(size_t maxCount = 100) const;
    void clearLogs();
    
    // Health monitoring
    bool performHealthCheck();
    std::string getHealthStatus() const;
    std::chrono::milliseconds getLastResponseTime() const;

    // Model management integration
    bool isModelLoaded(const std::string& modelName) const;
    std::vector<std::string> getLoadedModels() const;
    bool preloadModel(const std::string& modelName);
    bool unloadModel(const std::string& modelName);
    
    // Advanced model management (production features)
    bool cacheModelLocally(const std::string& modelName);
    bool isModelCached(const std::string& modelName) const;
    std::vector<std::string> getRecommendedModels() const;
    std::string getOptimalModelForTask(const std::string& taskType) const;
    bool purgeModelCache();
    std::size_t getCacheSize() const;

    // Task-aware model picker.
    bool optimizeModelSelection(const std::string& taskType, std::string& recommendedModel);
    
    // Model cache management
    bool validateModelCache() const;
    std::vector<std::string> getCachedModels() const;
    bool removeFromCache(const std::string& modelName);
    std::string getModelCachePath(const std::string& modelName) const;

    // Production performance monitoring (public release features)
    struct ModelPerformanceMetrics {
        std::string modelName;
        double averageResponseTimeMs = 0.0;
        double tokensPerSecond = 0.0;
        size_t totalRequests = 0;
        size_t successfulRequests = 0;
        std::chrono::system_clock::time_point lastUsed;
    };

    std::vector<ModelPerformanceMetrics> getModelPerformanceMetrics() const;
    bool optimizeModelForPerformance(const std::string& modelName);
    std::string getBestPerformingModel(const std::string& taskType) const;
    void updateModelPerformance(const std::string& modelName, double responseTime, double tokensPerSec, bool success);
    void resetPerformanceMetrics(const std::string& modelName = "");
    std::pair<std::string, double> getModelWithBestLatency() const;
    std::pair<std::string, double> getModelWithBestThroughput() const;

    // Terminal UI integration
    void attachTerminalPane(HWND hwndTerminal);
    void detachTerminalPane();
    void sendToTerminalPane(const std::string& message);
    bool isTerminalPaneAttached() const;
    void enableRealTimeLogging(bool enable);
    void sendFormattedMessage(const std::string& prefix, const std::string& message, LogLevel level = LogLevel::Info);
    void clearTerminalPane();

private:
    // Internal service management
    bool launchOllamaProcess();
    bool terminateOllamaProcess();
    void watchOllamaProcess();
    void performHealthChecks();
    void handleProcessExit(DWORD exitCode);

    // Binary management internal
    bool downloadFile(const std::string& url, const std::string& localPath, 
                     std::function<void(int)> progressCallback = nullptr);
    bool verifyBinaryIntegrity(const std::string& path) const;
    bool createModelsDirectory();
    bool setupOllamaEnvironment();

    // Logging internal
    void addLogEntry(LogLevel level, const std::string& message, const std::string& source = "OllamaService");
    void processOllamaOutput(const std::string& output);
    void readOllamaStdout();
    void readOllamaStderr();

    // HTTP health check
    bool testHttpEndpoint() const;
    bool testOllamaAPI() const;

private:
    Win32IDE* m_ide;
    ServiceConfig m_config;
    std::atomic<ServiceState> m_state;
    std::atomic<bool> m_shutdownRequested;
    
    // Process management
    HANDLE m_processHandle;
    DWORD m_processId;
    HANDLE m_stdoutRead;
    HANDLE m_stdoutWrite;
    HANDLE m_stderrRead;
    HANDLE m_stderrWrite;
    std::atomic<int> m_restartAttempts;

    // Health monitoring
    std::thread m_healthCheckThread;
    std::thread m_processWatchThread;
    std::thread m_outputReaderThread;
    std::atomic<bool> m_isHealthy;
    std::chrono::milliseconds m_lastResponseTime;
    std::chrono::system_clock::time_point m_lastHealthCheck;

    // Logging
    mutable std::mutex m_logMutex;
    std::vector<LogEntry> m_logs;
    LogCallback m_logCallback;
    StateCallback m_stateCallback;
    
    // Performance monitoring (production feature)
    mutable std::mutex m_performanceMutex;
    std::unordered_map<std::string, ModelPerformanceMetrics> m_modelMetrics;
    
    // Terminal integration
    HWND m_hwndTerminalPane;
    mutable std::mutex m_terminalMutex;

    // Binary paths
    std::string m_ollamaBinaryPath;
    std::string m_modelsDirectory;
    std::string m_configDirectory;
};

// Utility functions for Ollama integration
namespace OllamaUtils {
    std::string formatLogEntry(const OllamaServiceManager::LogEntry& entry);
    std::string serviceStateToString(OllamaServiceManager::ServiceState state);
    std::string getCurrentTimestamp();
    bool isPortAvailable(int port);
    bool killProcessByPort(int port);
    std::vector<std::string> parseOllamaModels(const std::string& apiResponse);
}