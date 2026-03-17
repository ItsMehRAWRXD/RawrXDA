#pragma once

#include "universal_wrapper_masm.hpp"
#include <QDebug>
#include <QStringList>

//========================================================================================================
// EXAMPLE USAGE PATTERNS FOR UNIVERSAL WRAPPER MASM
// Demonstrates all major API usage scenarios
//========================================================================================================

namespace UniversalWrapperExamples {

//========================================================================================================
// EXAMPLE 1: Basic Format Detection
//========================================================================================================

void exampleBasicDetection() {
    qDebug() << "=== Example 1: Basic Format Detection ===";
    
    UniversalWrapperMASM wrapper;
    
    // Detect format from file
    auto format = wrapper.detectFormat("model.safetensors");
    
    if (format == UniversalWrapperMASM::Format::SAFETENSORS) {
        qDebug() << "Detected SafeTensors format";
    } else {
        qDebug() << "Format not recognized";
    }
    
    // Check compression
    auto compression = wrapper.detectCompression("model.gz");
    if (compression == UniversalWrapperMASM::Compression::GZIP) {
        qDebug() << "File is gzip compressed";
    }
}

//========================================================================================================
// EXAMPLE 2: Format-Agnostic Model Loading
//========================================================================================================

void exampleLoadAnyFormat() {
    qDebug() << "=== Example 2: Load Any Format ===";
    
    UniversalWrapperMASM wrapper;
    
    // All these work with the SAME method!
    QStringList models = {
        "model.safetensors",
        "model.pt",
        "model.onnx",
        "model.pb",
        "model.npy"
    };
    
    for (const auto& modelPath : models) {
        if (wrapper.loadUniversalFormat(modelPath)) {
            qDebug() << "Loaded:" << modelPath;
            qDebug() << "  Format:" << static_cast<int>(wrapper.getDetectedFormat());
            qDebug() << "  Duration:" << wrapper.getLastDurationMs() << "ms";
        } else {
            qWarning() << "Failed to load:" << modelPath;
            qWarning() << "  Error:" << wrapper.getLastError();
        }
    }
}

//========================================================================================================
// EXAMPLE 3: Format-Specific Loading (aliases)
//========================================================================================================

void exampleLoadSpecificFormats() {
    qDebug() << "=== Example 3: Format-Specific Loading ===";
    
    UniversalWrapperMASM wrapper;
    
    // These are aliases that route to loadUniversalFormat()
    wrapper.loadSafeTensors("model.safetensors");
    wrapper.loadPyTorch("model.pt");
    wrapper.loadTensorFlow("model.pb");
    wrapper.loadONNX("model.onnx");
    wrapper.loadNumPy("model.npy");
    
    qDebug() << "All formats loaded successfully";
}

//========================================================================================================
// EXAMPLE 4: Format Conversion to GGUF
//========================================================================================================

void exampleConvertToGGUF() {
    qDebug() << "=== Example 4: Convert to GGUF ===";
    
    UniversalWrapperMASM wrapper;
    
    // Load any format and convert to GGUF
    if (wrapper.loadUniversalFormat("model.safetensors")) {
        if (wrapper.convertToGGUF("output.gguf")) {
            qDebug() << "Converted to GGUF:" << wrapper.getTempOutputPath();
        } else {
            qWarning() << "Conversion failed:" << wrapper.getLastError();
        }
    }
    
    // Or direct conversion (auto-detect input)
    if (wrapper.convertToGGUFWithInput("model.pt", "output.gguf")) {
        qDebug() << "Direct conversion successful";
    }
}

//========================================================================================================
// EXAMPLE 5: Batch Model Loading
//========================================================================================================

void exampleBatchLoading() {
    qDebug() << "=== Example 5: Batch Model Loading ===";
    
    QStringList models = {
        "models/llama.pt",
        "models/mistral.safetensors",
        "models/qwen.onnx",
        "models/gpt.pb"
    };
    
    // Load multiple models with single call
    auto results = loadModelsUniversal(models, 
                                       UniversalWrapperMASM::WrapperMode::PURE_MASM);
    
    for (const auto& result : results) {
        if (result.success) {
            qDebug() << "✓" << result.filePath;
            qDebug() << "  Format:" << static_cast<int>(result.format);
            qDebug() << "  Time:" << result.duration_ms << "ms";
        } else {
            qWarning() << "✗" << result.filePath;
            qWarning() << "  Error:" << result.errorMessage;
        }
    }
}

//========================================================================================================
// EXAMPLE 6: Mode Toggling
//========================================================================================================

void exampleModeToggling() {
    qDebug() << "=== Example 6: Mode Toggling ===";
    
    // Global mode (affects AUTO_SELECT instances)
    UniversalWrapperMASM::SetGlobalMode(UniversalWrapperMASM::WrapperMode::PURE_MASM);
    qDebug() << "Global mode set to PURE_MASM";
    
    // Create instance with AUTO_SELECT (uses global mode)
    UniversalWrapperMASM wrapper1(UniversalWrapperMASM::WrapperMode::AUTO_SELECT);
    qDebug() << "Wrapper1 mode:" << static_cast<int>(wrapper1.getMode());  // PURE_MASM
    
    // Create instance with specific mode
    UniversalWrapperMASM wrapper2(UniversalWrapperMASM::WrapperMode::CPP_QT);
    qDebug() << "Wrapper2 mode:" << static_cast<int>(wrapper2.getMode());  // CPP_QT
    
    // Change mode at runtime
    wrapper2.setMode(UniversalWrapperMASM::WrapperMode::PURE_MASM);
    qDebug() << "Wrapper2 mode changed to:" << static_cast<int>(wrapper2.getMode());
}

//========================================================================================================
// EXAMPLE 7: Statistics and Monitoring
//========================================================================================================

void exampleStatistics() {
    qDebug() << "=== Example 7: Statistics and Monitoring ===";
    
    UniversalWrapperMASM wrapper;
    
    // Perform some operations
    wrapper.detectFormat("model1.pt");
    wrapper.detectFormat("model2.safetensors");
    wrapper.detectFormat("model1.pt");  // Cache hit
    wrapper.loadUniversalFormat("model3.onnx");
    
    // Get statistics
    auto stats = wrapper.getStatistics();
    
    qDebug() << "Statistics:";
    qDebug() << "  Total detections:" << stats.total_detections;
    qDebug() << "  Total conversions:" << stats.total_conversions;
    qDebug() << "  Total errors:" << stats.total_errors;
    qDebug() << "  Cache hits:" << stats.cache_hits;
    qDebug() << "  Cache misses:" << stats.cache_misses;
    qDebug() << "  Cache size:" << stats.cache_entries;
    qDebug() << "  Current mode:" << stats.current_mode;
    
    // Calculate cache hit rate
    uint64_t total = stats.cache_hits + stats.cache_misses;
    float hit_rate = total > 0 ? (stats.cache_hits * 100.0f) / total : 0.0f;
    qDebug() << "Cache hit rate:" << hit_rate << "%";
}

//========================================================================================================
// EXAMPLE 8: Error Handling
//========================================================================================================

void exampleErrorHandling() {
    qDebug() << "=== Example 8: Error Handling ===";
    
    UniversalWrapperMASM wrapper;
    
    // Try to load non-existent file
    if (!wrapper.loadUniversalFormat("non_existent.pt")) {
        auto error_code = wrapper.getLastErrorCode();
        auto error_msg = wrapper.getLastError();
        
        switch (error_code) {
            case UniversalWrapperMASM::ErrorCode::FILE_NOT_FOUND:
                qWarning() << "File not found:" << error_msg;
                break;
            case UniversalWrapperMASM::ErrorCode::FORMAT_UNKNOWN:
                qWarning() << "Unknown format:" << error_msg;
                break;
            case UniversalWrapperMASM::ErrorCode::LOAD_FAILED:
                qWarning() << "Load failed:" << error_msg;
                break;
            default:
                qWarning() << "Error" << static_cast<int>(error_code) << ":" << error_msg;
        }
    }
    
    // Validate path before loading
    if (wrapper.validateModelPath("model.pt")) {
        qDebug() << "Model path is valid";
        wrapper.loadUniversalFormat("model.pt");
    } else {
        qWarning() << "Model path invalid";
    }
}

//========================================================================================================
// EXAMPLE 9: File Operations
//========================================================================================================

void exampleFileOperations() {
    qDebug() << "=== Example 9: File Operations ===";
    
    UniversalWrapperMASM wrapper;
    
    // Read large file in chunks
    QByteArray buffer;
    if (wrapper.readFileChunked("large_model.bin", buffer)) {
        qDebug() << "Read file:" << buffer.size() << "bytes";
    } else {
        qWarning() << "Read failed:" << wrapper.getLastError();
    }
    
    // Get temp directory
    QString tempDir = wrapper.getTempDirectory();
    qDebug() << "Temp directory:" << tempDir;
    
    // Generate temp GGUF path
    QString tempPath;
    if (wrapper.generateTempGGUFPath("model.pt", tempPath)) {
        qDebug() << "Temp GGUF path:" << tempPath;
    }
    
    // Write buffer to file
    if (wrapper.writeBufferToFile("output.gguf", buffer)) {
        qDebug() << "Written to file successfully";
    }
    
    // Clean up temp files
    wrapper.cleanupTempFiles();
    qDebug() << "Cleaned up temp files";
}

//========================================================================================================
// EXAMPLE 10: RAII and Smart Pointers
//========================================================================================================

void exampleRAII() {
    qDebug() << "=== Example 10: RAII and Smart Pointers ===";
    
    // RAII scope - auto cleanup
    {
        UniversalWrapperMASM wrapper;
        wrapper.loadUniversalFormat("model.pt");
        // Cleanup happens here automatically
    }
    
    // Smart pointer RAII
    {
        auto wrapper = createUniversalWrapper(UniversalWrapperMASM::WrapperMode::PURE_MASM);
        if (wrapper) {
            wrapper->loadUniversalFormat("model.safetensors");
            // Cleanup happens automatically when wrapper goes out of scope
        }
    }
}

//========================================================================================================
// EXAMPLE 11: Integration with Qt MainWindow
//========================================================================================================

class ExampleMainWindow /* : public QMainWindow */ {
private:
    std::unique_ptr<UniversalWrapperMASM> m_wrapper;
    
public:
    ExampleMainWindow() : m_wrapper(std::make_unique<UniversalWrapperMASM>()) {
        // Initialize with global mode
        UniversalWrapperMASM::SetGlobalMode(UniversalWrapperMASM::WrapperMode::PURE_MASM);
    }
    
    void loadModelSlot(const QString& modelPath) {
        // Detect format
        auto format = m_wrapper->detectFormat(modelPath);
        qDebug() << "Detected format:" << static_cast<int>(format);
        
        // Load model
        if (m_wrapper->loadUniversalFormat(modelPath)) {
            onModelLoaded();
        } else {
            onModelLoadFailed(m_wrapper->getLastError());
        }
    }
    
    void convertModelSlot(const QString& inputPath, const QString& outputPath) {
        if (m_wrapper->convertToGGUFWithInput(inputPath, outputPath)) {
            onConversionComplete(outputPath);
        } else {
            onConversionFailed(m_wrapper->getLastError());
        }
    }
    
private:
    void onModelLoaded() { qDebug() << "Model loaded"; }
    void onModelLoadFailed(const QString& error) { qWarning() << "Load failed:" << error; }
    void onConversionComplete(const QString& path) { qDebug() << "Converted to:" << path; }
    void onConversionFailed(const QString& error) { qWarning() << "Conversion failed:" << error; }
};

//========================================================================================================
// EXAMPLE 12: Integration with HotpatchManager
//========================================================================================================

void exampleHotpatchIntegration() {
    qDebug() << "=== Example 12: HotpatchManager Integration ===";
    
    // Load model using universal wrapper
    UniversalWrapperMASM wrapper;
    if (wrapper.loadUniversalFormat("model.safetensors")) {
        qDebug() << "Model loaded, ready for hotpatching";
        
        // Now apply hotpatches (example pseudocode)
        // UnifiedHotpatchManager manager;
        // manager.applyMemoryPatch(patch);
        // manager.applyBytePatch(patch);
        // etc.
    }
}

//========================================================================================================
// EXAMPLE 13: Cache Validation
//========================================================================================================

void exampleCacheValidation() {
    qDebug() << "=== Example 13: Cache Validation ===";
    
    UniversalWrapperMASM wrapper;
    
    // First detection - cache miss
    auto format1 = wrapper.detectFormat("model.pt");
    auto stats1 = wrapper.getStatistics();
    qDebug() << "After first detection - cache misses:" << stats1.cache_misses;
    
    // Second detection - cache hit
    auto format2 = wrapper.detectFormat("model.pt");
    auto stats2 = wrapper.getStatistics();
    qDebug() << "After second detection - cache hits:" << stats2.cache_hits;
    
    // Clear cache and try again
    wrapper.clearCache();
    auto format3 = wrapper.detectFormat("model.pt");
    auto stats3 = wrapper.getStatistics();
    qDebug() << "After cache clear - cache misses:" << stats3.cache_misses;
}

//========================================================================================================
// EXAMPLE 14: Performance Benchmarking
//========================================================================================================

void examplePerformanceBenchmark() {
    qDebug() << "=== Example 14: Performance Benchmarking ===";
    
    UniversalWrapperMASM wrapper;
    
    QStringList models = {
        "model1.pt",
        "model2.safetensors",
        "model3.onnx",
        "model4.pb",
        "model5.npy"
    };
    
    uint64_t totalTime = 0;
    
    for (const auto& model : models) {
        auto format = wrapper.detectFormat(model);
        totalTime += wrapper.getLastDurationMs();
        
        qDebug() << model << "- Format:" << static_cast<int>(format)
                 << "Time:" << wrapper.getLastDurationMs() << "ms";
    }
    
    qDebug() << "Total time:" << totalTime << "ms";
    qDebug() << "Average time:" << (totalTime / models.size()) << "ms";
    
    auto stats = wrapper.getStatistics();
    qDebug() << "\nCache Performance:";
    qDebug() << "  Hits:" << stats.cache_hits;
    qDebug() << "  Misses:" << stats.cache_misses;
    qDebug() << "  Ratio:" << (stats.cache_hits * 100.0) / (stats.cache_hits + stats.cache_misses) << "%";
}

//========================================================================================================
// EXAMPLE 15: Initialization Pattern
//========================================================================================================

void exampleInitialization() {
    qDebug() << "=== Example 15: Initialization Pattern ===";
    
    // Initialize global mode (call once at application startup)
    UniversalWrapperMASM::SetGlobalMode(UniversalWrapperMASM::WrapperMode::PURE_MASM);
    
    // Create wrapper instance
    auto wrapper = createUniversalWrapper(UniversalWrapperMASM::WrapperMode::AUTO_SELECT);
    
    if (!wrapper || !wrapper->isInitialized()) {
        qWarning() << "Failed to initialize wrapper";
        return;
    }
    
    qDebug() << "Wrapper initialized successfully";
    qDebug() << "Mode:" << static_cast<int>(wrapper->getMode());
}

} // namespace UniversalWrapperExamples

#endif // UNIVERSAL_WRAPPER_EXAMPLES_HPP
