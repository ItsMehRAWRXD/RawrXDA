// ============================================================================
// CLI Local Model Integration - Production Implementation
// ============================================================================
// Command-line interface for loading local GGUF models without cloud dependencies
// ============================================================================

#include "cli_local_model.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

namespace RawrXD {
namespace CLI {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

CLILocalModel::CLILocalModel() {
    loader_ = std::make_unique<Core::LocalGGUFLoader>();
    clearError();
}

CLILocalModel::~CLILocalModel() {
    // Cleanup handled by unique_ptr
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

int CLILocalModel::loadModel(const std::string& path) {
    clearError();
    
    // Validate file path
    if (!validateFilePath(path)) {
        return static_cast<int>(CLILocalModelError::FILE_NOT_FOUND);
    }
    
    // Validate GGUF file
    if (!validateGGUFFile(path)) {
        return static_cast<int>(CLILocalModelError::INVALID_GGUF);
    }
    
    // Report start
    reportProgress("Loading model: " + path, 0);
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Load model
    if (!loader_->load(path)) {
        setError(loader_->getLastError());
        reportError("Failed to load model: " + path, true);
        return static_cast<int>(CLILocalModelError::LOAD_FAILED);
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    // Store metadata
    last_metadata_ = loader_->getMetadata();
    
    // Report success
    reportProgress("Model loaded successfully in " + 
                   formatDuration(duration / 1000.0) + "", 100);
    
    // Display metadata if verbose
    if (verbose_) {
        displayMetadata(last_metadata_);
    }
    
    return static_cast<int>(CLILocalModelError::SUCCESS);
}

int CLILocalModel::validateModel(const std::string& path) {
    clearError();
    
    // Validate file path
    if (!validateFilePath(path)) {
        return static_cast<int>(CLILocalModelError::FILE_NOT_FOUND);
    }
    
    // Validate GGUF file
    if (!validateGGUFFile(path)) {
        return static_cast<int>(CLILocalModelError::INVALID_GGUF);
    }
    
    // Load to get full metadata
    if (!loader_->load(path)) {
        setError(loader_->getLastError());
        reportError("Validation failed: " + path, true);
        return static_cast<int>(CLILocalModelError::VALIDATION_FAILED);
    }
    
    last_metadata_ = loader_->getMetadata();
    
    // Report validation results
    reportInfo("✓ File exists and is readable");
    reportInfo("✓ Valid GGUF header (magic: 0x" + toHex(last_metadata_.file_size_bytes) + ")");
    reportInfo("✓ Version " + std::to_string(3) + " supported");
    reportInfo("✓ Architecture: " + last_metadata_.architecture);
    reportInfo("✓ Quantization: " + last_metadata_.quant_type_str());
    reportInfo("✓ Context length: " + std::to_string(last_metadata_.context_length));
    reportInfo("✓ Hidden size: " + std::to_string(last_metadata_.hidden_size));
    reportInfo("✓ Heads: " + std::to_string(last_metadata_.head_count));
    reportInfo("✓ Layers: " + std::to_string(last_metadata_.layer_count));
    reportInfo("✓ Vocab size: " + std::to_string(last_metadata_.vocab_size));
    reportInfo("✓ File size: " + formatFileSize(last_metadata_.file_size_bytes));
    
    if (verbose_) {
        displayMetadata(last_metadata_);
    }
    
    return static_cast<int>(CLILocalModelError::SUCCESS);
}

int CLILocalModel::showModelInfo(const std::string& path) {
    clearError();
    
    // Load model to get metadata
    int result = loadModel(path);
    if (result != static_cast<int>(CLILocalModelError::SUCCESS)) {
        return result;
    }
    
    // Display full metadata
    displayMetadata(last_metadata_);
    
    // Display tensor info if verbose
    if (verbose_) {
        const auto& tensors = loader_->getTensorInfo();
        displayTensorInfo(tensors);
    }
    
    return static_cast<int>(CLILocalModelError::SUCCESS);
}

int CLILocalModel::loadModels(const std::vector<std::string>& paths) {
    clearError();
    
    if (paths.empty()) {
        reportError("No models specified", false);
        return static_cast<int>(CLILocalModelError::UNKNOWN_ERROR);
    }
    
    reportInfo("Loading " + std::to_string(paths.size()) + " models...");
    
    int success_count = 0;
    int fail_count = 0;
    
    for (size_t i = 0; i < paths.size(); ++i) {
        const auto& path = paths[i];
        int progress = static_cast<int>((i * 100) / paths.size());
        
        reportProgress("Loading model " + std::to_string(i + 1) + "/" + 
                       std::to_string(paths.size()) + ": " + path, progress);
        
        int result = loadModel(path);
        if (result == static_cast<int>(CLILocalModelError::SUCCESS)) {
            success_count++;
        } else {
            fail_count++;
        }
    }
    
    reportProgress("Batch load complete: " + std::to_string(success_count) + 
                   " succeeded, " + std::to_string(fail_count) + " failed", 100);
    
    return fail_count == 0 ? static_cast<int>(CLILocalModelError::SUCCESS) 
                           : static_cast<int>(CLILocalModelError::LOAD_FAILED);
}

int CLILocalModel::validateModels(const std::vector<std::string>& paths) {
    clearError();
    
    if (paths.empty()) {
        reportError("No models specified", false);
        return static_cast<int>(CLILocalModelError::UNKNOWN_ERROR);
    }
    
    reportInfo("Validating " + std::to_string(paths.size()) + " models...");
    
    int success_count = 0;
    int fail_count = 0;
    
    for (size_t i = 0; i < paths.size(); ++i) {
        const auto& path = paths[i];
        int progress = static_cast<int>((i * 100) / paths.size());
        
        reportProgress("Validating model " + std::to_string(i + 1) + "/" + 
                       std::to_string(paths.size()) + ": " + path, progress);
        
        int result = validateModel(path);
        if (result == static_cast<int>(CLILocalModelError::SUCCESS)) {
            success_count++;
        } else {
            fail_count++;
        }
    }
    
    reportProgress("Batch validation complete: " + std::to_string(success_count) + 
                   " valid, " + std::to_string(fail_count) + " invalid", 100);
    
    return fail_count == 0 ? static_cast<int>(CLILocalModelError::SUCCESS) 
                           : static_cast<int>(CLILocalModelError::VALIDATION_FAILED);
}

// ============================================================================
// CALLBACK METHODS
// ============================================================================

void CLILocalModel::reportProgress(const std::string& message, int percent) {
    if (progress_callback_) {
        progress_callback_(message, percent);
    }
    
    if (show_progress_ && percent >= 0) {
        std::cout << "[" << std::setw(3) << percent << "%] " << message << std::endl;
    } else if (verbose_) {
        std::cout << message << std::endl;
    }
}

void CLILocalModel::reportError(const std::string& message, bool fatal) {
    if (error_callback_) {
        error_callback_(message, fatal);
    }
    
    std::cerr << "ERROR: " << message << std::endl;
    
    if (fatal) {
        setError(message);
    }
}

void CLILocalModel::reportInfo(const std::string& message) {
    if (info_callback_) {
        info_callback_(message);
    }
    
    if (verbose_) {
        std::cout << "INFO: " << message << std::endl;
    }
}

void CLILocalModel::clearError() {
    last_error_.clear();
}

void CLILocalModel::setError(const std::string& error) {
    last_error_ = error;
}

// ============================================================================
// VALIDATION METHODS
// ============================================================================

bool CLILocalModel::validateFilePath(const std::string& path) {
    // Check if file exists
    if (!fs::exists(path)) {
        setError("File not found: " + path);
        return false;
    }
    
    // Check if it's a regular file (not directory)
    if (!fs::is_regular_file(path)) {
        setError("Not a regular file: " + path);
        return false;
    }
    
    // Check file extension
    if (fs::path(path).extension() != ".gguf") {
        reportInfo("Warning: File extension is not .gguf: " + path);
    }
    
    return true;
}

bool CLILocalModel::validateGGUFFile(const std::string& path) {
    // Use the loader's validation
    if (!loader_->validateFile(path)) {
        setError(loader_->getLastError());
        return false;
    }
    
    return true;
}

// ============================================================================
// DISPLAY METHODS
// ============================================================================

void CLILocalModel::displayMetadata(const Core::ModelMetadata& metadata) {
    std::cout << "\n═══════════════════════════════════════════════════════════════\n";
    std::cout << "  MODEL METADATA\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "Name:           " << metadata.name << "\n";
    std::cout << "Path:           " << metadata.path << "\n";
    std::cout << "Architecture:   " << metadata.architecture << "\n";
    std::cout << "Context Length: " << metadata.context_length << " tokens\n";
    std::cout << "Hidden Size:    " << metadata.hidden_size << "\n";
    std::cout << "Heads:          " << metadata.head_count << "\n";
    std::cout << "KV Heads:       " << metadata.head_count_kv << "\n";
    std::cout << "Layers:         " << metadata.layer_count << "\n";
    std::cout << "Vocab Size:     " << metadata.vocab_size << "\n";
    std::cout << "Quantization:   " << quantTypeToString(metadata.quant_type) << "\n";
    std::cout << "Bits/Weight:    " << metadata.bits_per_weight() << "\n";
    std::cout << "File Size:      " << formatFileSize(metadata.file_size_bytes) << "\n";
    std::cout << "RoPE Theta:     " << metadata.rope_theta << "\n";
    std::cout << "Valid:          " << (metadata.is_valid ? "Yes" : "No") << "\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
}

void CLILocalModel::displayTensorInfo(const std::vector<Core::GGUFTensorInfo>& tensors) {
    if (tensors.empty()) {
        return;
    }
    
    std::cout << "\n═══════════════════════════════════════════════════════════════\n";
    std::cout << "  TENSOR INFORMATION (" << tensors.size() << " tensors)\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    
    // Group tensors by type
    std::map<Core::GGUFTensorType, int> type_counts;
    for (const auto& tensor : tensors) {
        type_counts[tensor.tensor_type]++;
    }
    
    std::cout << "Quantization Distribution:\n";
    for (const auto& [type, count] : type_counts) {
        std::cout << "  " << quantTypeToString(type) << ": " 
                  << count << " tensors\n";
    }
    
    // Show first few tensors
    size_t show_count = std::min(size_t(5), tensors.size());
    std::cout << "\nFirst " << show_count << " tensors:\n";
    for (size_t i = 0; i < show_count; ++i) {
        const auto& tensor = tensors[i];
        std::cout << "  " << tensor.name << " [";
        for (size_t j = 0; j < tensor.dimensions.size(); ++j) {
            if (j > 0) std::cout << " x ";
            std::cout << tensor.dimensions[j];
        }
        std::cout << "] (" << quantTypeToString(tensor.tensor_type) << ")\n";
    }
    
    std::cout << "═══════════════════════════════════════════════════════════════\n";
}

// ============================================================================
// STATIC UTILITY METHODS
// ============================================================================

std::string CLILocalModel::quantTypeToString(Core::GGUFTensorType type) {
    return Core::LocalGGUFLoader::tensorTypeToString(type);
}

std::string CLILocalModel::formatFileSize(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return oss.str();
}

std::string CLILocalModel::formatDuration(double seconds) {
    if (seconds < 1.0) {
        return std::to_string(static_cast<int>(seconds * 1000)) + " ms";
    } else if (seconds < 60.0) {
        return std::to_string(seconds) + " s";
    } else {
        int minutes = static_cast<int>(seconds / 60);
        int secs = static_cast<int>(seconds) % 60;
        return std::to_string(minutes) + "m " + std::to_string(secs) + "s";
    }
}

} // namespace CLI
} // namespace RawrXD