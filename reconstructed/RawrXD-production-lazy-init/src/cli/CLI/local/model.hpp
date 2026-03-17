// ============================================================================
// CLI Local Model Integration - Production Implementation
// ============================================================================
// Command-line interface for loading local GGUF models without cloud dependencies
// Supports: load_model, validate_model, show_model_info
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "../core/local_gguf_loader.hpp"

namespace RawrXD {
namespace CLI {

// ============================================================================
// CLI LOCAL MODEL CLASS
// ============================================================================

/**
 * @class CLILocalModel
 * @brief Command-line interface for loading local GGUF models
 * 
 * Features:
 * - Direct GGUF file loading (no cloud dependencies)
 * - GGUF validation before loading
 * - Clear error messages for invalid files
 * - Support for Q2/Q3/Q4/Q5/Q6/Q8 quantizations
 * - Metadata extraction and display
 * - Integration with LocalGGUFLoader
 */
class CLILocalModel {
public:
    CLILocalModel();
    ~CLILocalModel();
    
    // Non-copyable (file handles)
    CLILocalModel(const CLILocalModel&) = delete;
    CLILocalModel& operator=(const CLILocalModel&) = delete;
    
    // Model loading
    int loadModel(const std::string& path);
    int validateModel(const std::string& path);
    int showModelInfo(const std::string& path);
    
    // Batch operations
    int loadModels(const std::vector<std::string>& paths);
    int validateModels(const std::vector<std::string>& paths);
    
    // Configuration
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setShowProgress(bool show) { show_progress_ = show; }
    
    // Callbacks
    void setProgressCallback(std::function<void(const std::string&, int)> callback) {
        progress_callback_ = callback;
    }
    
    void setErrorCallback(std::function<void(const std::string&, bool)> callback) {
        error_callback_ = callback;
    }
    
    void setInfoCallback(std::function<void(const std::string&)> callback) {
        info_callback_ = callback;
    }
    
    // Results
    const Core::ModelMetadata& getLastMetadata() const { return last_metadata_; }
    bool hasError() const { return !last_error_.empty(); }
    std::string getLastError() const { return last_error_; }
    
    // Static utilities
    static std::string quantTypeToString(Core::GGUFTensorType type);
    static std::string formatFileSize(uint64_t bytes);
    static std::string formatDuration(double seconds);
    
private:
    std::unique_ptr<Core::LocalGGUFLoader> loader_;
    Core::ModelMetadata last_metadata_;
    std::string last_error_;
    bool verbose_ = false;
    bool show_progress_ = true;
    bool local_only_ = false; // Bypass cloud checks
    
    // Callbacks
    std::function<void(const std::string&, int)> progress_callback_;
    std::function<void(const std::string&, bool)> error_callback_;
    std::function<void(const std::string&)> info_callback_;
    
    // Internal methods
    void reportProgress(const std::string& message, int percent);
    void reportError(const std::string& message, bool fatal = false);
    void reportInfo(const std::string& message);
    void clearError();
    void setError(const std::string& error);
    
    // Validation
    bool validateFilePath(const std::string& path);
    bool validateGGUFFile(const std::string& path);
    
    // Display
    void displayMetadata(const Core::ModelMetadata& metadata);
    void displayTensorInfo(const std::vector<Core::GGUFTensorInfo>& tensors);
};

// ============================================================================
// CLI COMMAND REGISTRATION
// ============================================================================

/**
 * @class CLILocalModelCommands
 * @brief Registers local model commands with CLI argument parser
 */
class CLILocalModelCommands {
public:
    static void registerCommands(CLIArgumentParser& parser);
    static int executeCommand(const std::string& command, 
                             const std::vector<std::string>& args,
                             CLILocalModel& local_model);
    
private:
    static int cmdLoadModel(CLILocalModel& local_model, 
                           const std::vector<std::string>& args);
    static int cmdValidateModel(CLILocalModel& local_model, 
                               const std::vector<std::string>& args);
    static int cmdShowModelInfo(CLILocalModel& local_model, 
                               const std::vector<std::string>& args);
    static int cmdLoadModels(CLILocalModel& local_model, 
                            const std::vector<std::string>& args);
};

// ============================================================================
// ERROR CODES
// ============================================================================

enum class CLILocalModelError {
    SUCCESS = 0,
    FILE_NOT_FOUND = 1,
    FILE_NOT_READABLE = 2,
    INVALID_GGUF = 3,
    UNSUPPORTED_QUANTIZATION = 4,
    LOAD_FAILED = 5,
    VALIDATION_FAILED = 6,
    UNKNOWN_ERROR = 99
};

} // namespace CLI
} // namespace RawrXD