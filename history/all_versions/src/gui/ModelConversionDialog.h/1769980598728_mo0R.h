#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <filesystem>
#include <optional>
#include <chrono>
#include <functional>
#include <memory>
#include <atomic>

namespace fs = std::filesystem;

struct ConversionConfig {
    fs::path converterPath;
    fs::path modelsDirectory;
    bool showUI = true;
    std::optional<fs::path> logFile;
    std::chrono::seconds timeout = std::chrono::hours(1);
    std::optional<std::string> apiKey;
    std::vector<std::string> allowedExtensions = {".gguf", ".bin", ".pth"};
};

struct ConversionResult {
    bool success = false;
    std::optional<int> exitCode;
    std::chrono::milliseconds duration{0};
    fs::path convertedModelPath;
    std::string errorOutput;
    std::string output;
};

enum class ConversionError {
    None,
    ConverterNotFound,
    InvalidConverterPath,
    PathTraversalDetected,
    InvalidModelPath,
    CommandExecutionFailed
};

class ProcessHandle {
public:
    explicit ProcessHandle(HANDLE h = nullptr) : m_handle(h) {}
    ~ProcessHandle();
    ProcessHandle(ProcessHandle&& other) noexcept;
    ProcessHandle& operator=(ProcessHandle&& other) noexcept;
    
    // Disable copy
    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;
    
    bool isValid() const { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }
    HANDLE get() const { return m_handle; }
    void close();
private:
    HANDLE m_handle;
};

class ModelConversionDialog {
public:
    enum Result {
        Converted,
        Cancelled,
        Failed
    };

    ModelConversionDialog(const std::vector<std::string>& unsupportedTypes,
                          std::string_view recommendedType,
                          const fs::path& modelPath,
                          HWND parent,
                          const ConversionConfig& config = {});
    
    Result exec();
    void execAsync(std::function<void(Result, const ConversionResult&)> callback);

    const ConversionResult& getConversionResult() const { return m_conversionResult; }
    
    static bool needsConversion(const fs::path& modelPath);
    static std::pair<fs::path, ConversionError> findConverter(const fs::path& searchRoot);

private:
    std::vector<std::string> m_unsupportedTypes;
    std::string m_recommendedType;
    fs::path m_modelPath;
    HWND m_parent;
    ConversionConfig m_config;
    
    ConversionResult m_conversionResult;
    ProcessHandle m_processHandle;
    std::atomic<bool> m_cancelRequested{false};

    // Validation
    std::pair<bool, ConversionError> validatePaths() const;
    std::pair<bool, ConversionError> validateModelPath() const;
    
    // Logic
    std::string buildUserMessage() const;
    std::string buildConverterNotFoundMessage() const;
    std::pair<std::string, ConversionError> buildCommandLine() const;
    
    // Execution
    std::pair<ProcessHandle, ConversionError> executeConverter(const std::string& commandLine) const;
    void monitorProcess(ProcessHandle& handle, std::chrono::seconds timeout);
    Result handleConversionResult();
    
    // Helpers
    std::string readPipeOutput(HANDLE hPipe) const;
    void showInfoDialog(const std::string& message) const;
    void showErrorDialog(const std::string& message) const;
    bool askUserPermission(const std::string& message) const;
    
    void log(const std::string& message, bool isError = false) const;
};


