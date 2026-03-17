#pragma once

#include <QString>
#include <memory>
#include <cstdint>
#include <vector>
#include <functional>

//========================================================================================================
// MASM Extern C Declarations - Universal Wrapper
// These functions are implemented in pure x64 MASM assembly (universal_wrapper.asm)
// Purpose: Provide single unified C++ wrapper replacing three separate wrapper classes
//========================================================================================================

extern "C" {
    
    //====================================================================================================
    // STRUCTURE DEFINITIONS (matching MASM definitions exactly)
    //====================================================================================================
    
    // Detection cache entry (32 bytes)
    struct DetectionCacheEntryMASM {
        const wchar_t*  path;           // file path
        uint32_t        format;         // FORMAT_XXX
        uint32_t        compression;    // COMPRESSION_XXX
        uint64_t        timestamp;      // cache time
        uint32_t        valid;          // 1 if valid
        uint32_t        _padding[3];
    };
    
    // Format detection result (32 bytes)
    struct DetectionResultMASM {
        uint32_t        format;         // FORMAT_XXX constant
        uint32_t        compression;    // COMPRESSION_XXX constant
        const char*     reason;         // error/status message
        uint32_t        confidence;     // 0-100 confidence
        uint32_t        valid;          // 1 if valid
    };
    
    // Model load result (64 bytes)
    struct LoadResultMASM {
        uint32_t        success;        // 1 if succeeded
        uint32_t        format;         // detected format
        void*           output_path;    // output file path
        void*           error_msg;      // error message
        uint64_t        duration_ms;    // operation duration
        uint64_t        bytes_read;     // bytes read
        uint64_t        _padding[2];
    };
    
    // Statistics (64 bytes)
    struct WrapperStatisticsMASM {
        uint64_t        total_detections;
        uint64_t        total_conversions;
        uint64_t        total_errors;
        uint64_t        cache_hits;
        uint64_t        cache_misses;
        uint32_t        cache_entries;
        uint32_t        current_mode;
        uint64_t        _padding[3];
    };
    
    // Universal wrapper state (512 bytes)
    struct UniversalWrapperMASM {
        void*           mutex;              // Windows HANDLE
        void*           detection_cache;    // cache array ptr
        uint32_t        cache_entries;      // cache size
        uint64_t        cache_hits;         // stats
        uint64_t        cache_misses;       // stats
        uint32_t        mode;               // wrapper mode
        uint32_t        is_initialized;     // init flag
        uint64_t        last_detection;     // timestamp
        void*           format_router;      // cached router
        void*           format_loader;      // cached loader
        void*           model_loader;       // cached model loader
        void*           error_message;      // error buffer (512B)
        uint32_t        error_code;         // last error
        uint32_t        _error_pad;         // alignment
        void*           temp_path;          // temp path buffer (1024B)
        uint64_t        total_detections;   // stats
        uint64_t        total_conversions;  // stats
        uint64_t        total_errors;       // stats
        uint64_t        _reserved[28];      // future expansion
    };
    
    //====================================================================================================
    // EXPORTED FUNCTIONS FROM MASM (universal_wrapper.asm)
    //====================================================================================================
    
    // Initialize global wrapper state
    int wrapper_global_init(uint32_t mode);
    
    // Create wrapper instance
    UniversalWrapperMASM* wrapper_create(uint32_t mode);
    
    // Detect format (extension + magic bytes, cached)
    uint32_t wrapper_detect_format_unified(UniversalWrapperMASM* wrapper, const wchar_t* file_path);
    
    // Auto-detect and load model
    uint32_t wrapper_load_model_auto(UniversalWrapperMASM* wrapper, const wchar_t* model_path, 
                                     LoadResultMASM* result);
    
    // Convert to GGUF format
    uint32_t wrapper_convert_to_gguf(UniversalWrapperMASM* wrapper, const wchar_t* input_path, 
                                     const wchar_t* output_path);
    
    // Change mode at runtime
    uint32_t wrapper_set_mode(UniversalWrapperMASM* wrapper, uint32_t new_mode);
    
    // Get statistics
    uint32_t wrapper_get_statistics(UniversalWrapperMASM* wrapper, WrapperStatisticsMASM* stats);
    
    // Destroy wrapper
    void wrapper_destroy(UniversalWrapperMASM* wrapper);
    
    // Helper functions
    uint32_t detect_extension_unified(const wchar_t* file_path);
    uint32_t detect_magic_bytes_unified(const wchar_t* file_path, unsigned char* magic_buffer);
}

//========================================================================================================
// FORMAT TYPE ENUMERATIONS
//========================================================================================================

namespace UniversalWrapperFormat {
    
    enum class Format : uint32_t {
        UNKNOWN         = 0,
        GGUF_LOCAL      = 1,
        HF_REPO         = 2,
        HF_FILE         = 3,
        OLLAMA          = 4,
        MASM_COMP       = 5,
        UNIVERSAL       = 6,
        SAFETENSORS     = 7,
        PYTORCH         = 8,
        TENSORFLOW      = 9,
        ONNX            = 10,
        NUMPY           = 11
    };
    
    enum class Compression : uint32_t {
        NONE            = 0,
        GZIP            = 1,
        ZSTD            = 2,
        LZ4             = 3
    };
    
    enum class WrapperMode : uint32_t {
        PURE_MASM       = 0,
        CPP_QT          = 1,
        AUTO_SELECT     = 2
    };
    
    enum class ErrorCode : uint32_t {
        OK                  = 0,
        INVALID_PTR         = 1,
        NOT_INITIALIZED     = 2,
        ALLOC_FAILED        = 3,
        MUTEX_FAILED        = 4,
        FILE_NOT_FOUND      = 5,
        FORMAT_UNKNOWN      = 6,
        LOAD_FAILED         = 7,
        CACHE_FULL          = 8,
        MODE_INVALID        = 9
    };
}

//========================================================================================================
// C++ UNIFIED WRAPPER CLASS
// Purpose: Single class replacing UniversalFormatLoaderMASM, FormatRouterMASM, EnhancedModelLoaderMASM
// Features: Mode toggle, auto-detection, format routing, conversion, caching, statistics
//========================================================================================================

class UniversalWrapperMASM {
public:
    using Format = UniversalWrapperFormat::Format;
    using Compression = UniversalWrapperFormat::Compression;
    using WrapperMode = UniversalWrapperFormat::WrapperMode;
    using ErrorCode = UniversalWrapperFormat::ErrorCode;
    
    //====================================================================================================
    // LIFECYCLE MANAGEMENT
    //====================================================================================================
    
    explicit UniversalWrapperMASM(WrapperMode mode = WrapperMode::PURE_MASM);
    ~UniversalWrapperMASM();
    
    // Deleted copy operations (not copyable)
    UniversalWrapperMASM(const UniversalWrapperMASM&) = delete;
    UniversalWrapperMASM& operator=(const UniversalWrapperMASM&) = delete;
    
    // Allow move operations
    UniversalWrapperMASM(UniversalWrapperMASM&& other) noexcept;
    UniversalWrapperMASM& operator=(UniversalWrapperMASM&& other) noexcept;
    
    //====================================================================================================
    // FORMAT DETECTION (unified across all three loaders)
    //====================================================================================================
    
    // Detect format with caching (replaces FormatRouterMASM::detectFormat)
    Format detectFormat(const QString& filePath);
    Format detectFormatExtension(const QString& filePath);
    Format detectFormatMagic(const QString& filePath);
    
    // Detect compression (replaces FormatRouterMASM::detectCompression)
    Compression detectCompression(const QString& filePath);
    
    // Validate model path (replaces FormatRouterMASM::validateModelPath)
    bool validateModelPath(const QString& path);
    
    //====================================================================================================
    // MODEL LOADING (unified format-agnostic loading)
    //====================================================================================================
    
    // Auto-detect and load any format (replaces EnhancedModelLoaderMASM::loadUniversalFormat)
    bool loadUniversalFormat(const QString& modelPath);
    
    // Format-specific loaders (all route through unified MASM)
    bool loadSafeTensors(const QString& modelPath);
    bool loadPyTorch(const QString& modelPath);
    bool loadTensorFlow(const QString& modelPath);
    bool loadONNX(const QString& modelPath);
    bool loadNumPy(const QString& modelPath);
    
    //====================================================================================================
    // CONVERSION TO GGUF (unified format conversion)
    //====================================================================================================
    
    // Universal GGUF conversion (replaces UniversalFormatLoaderMASM::convertToGGUF)
    bool convertToGGUF(const QString& outputPath);
    bool convertToGGUFWithInput(const QString& inputPath, const QString& outputPath);
    
    //====================================================================================================
    // FILE I/O OPERATIONS (replaces EnhancedModelLoaderMASM file methods)
    //====================================================================================================
    
    // Read file chunked for large models
    bool readFileChunked(const QString& filePath, QByteArray& outBuffer);
    
    // Write buffer to file
    bool writeBufferToFile(const QString& filePath, const QByteArray& buffer);
    
    // Temp file management
    QString getTempDirectory() const;
    bool generateTempGGUFPath(const QString& modelName, QString& outPath);
    void cleanupTempFiles();
    
    //====================================================================================================
    // CACHE MANAGEMENT (replaces FormatRouterMASM cache methods)
    //====================================================================================================
    
    uint64_t getCacheHits() const;
    uint64_t getCacheMisses() const;
    uint32_t getCacheSize() const;
    void clearCache();
    
    //====================================================================================================
    // MODE CONTROL (unified across all components)
    //====================================================================================================
    
    // Set operation mode (PURE_MASM, CPP_QT, AUTO_SELECT)
    void setMode(WrapperMode newMode);
    WrapperMode getMode() const;
    
    // Global mode setter (affects all new instances with AUTO_SELECT)
    static void SetGlobalMode(WrapperMode mode);
    static WrapperMode GetGlobalMode();
    
    //====================================================================================================
    // STATUS & ERROR HANDLING
    //====================================================================================================
    
    QString getLastError() const { return m_lastError; }
    ErrorCode getLastErrorCode() const { return m_lastErrorCode; }
    
    // Statistics
    struct Statistics {
        uint64_t total_detections;
        uint64_t total_conversions;
        uint64_t total_errors;
        uint64_t cache_hits;
        uint64_t cache_misses;
        uint32_t cache_entries;
        uint32_t current_mode;
    };
    
    Statistics getStatistics() const;
    void resetStatistics();
    
    // Status queries
    bool isInitialized() const { return m_initialized; }
    QString getTempOutputPath() const { return m_tempOutputPath; }
    uint32_t getDetectedFormat() const { return static_cast<uint32_t>(m_detectedFormat); }
    uint64_t getLastDurationMs() const { return m_lastDurationMs; }
    
private:
    //====================================================================================================
    // PRIVATE MEMBERS
    //====================================================================================================
    
    UniversalWrapperMASM* m_masmWrapper;    // Native MASM wrapper instance
    WrapperMode m_currentMode;
    bool m_initialized;
    
    // Cached results
    Format m_detectedFormat;
    QString m_lastError;
    ErrorCode m_lastErrorCode;
    QString m_tempOutputPath;
    uint64_t m_lastDurationMs;
    
    // Global mode (affects AUTO_SELECT instances)
    static WrapperMode s_globalMode;
    
    //====================================================================================================
    // PRIVATE HELPER METHODS
    //====================================================================================================
    
    void updateError(ErrorCode code, const QString& message);
    QString masmErrorToString(uint32_t masmErrorCode) const;
    
};

//========================================================================================================
// UTILITY FUNCTIONS
//========================================================================================================

// Create a managed wrapper instance (RAII)
std::unique_ptr<UniversalWrapperMASM> createUniversalWrapper(
    UniversalWrapperMASM::WrapperMode mode = UniversalWrapperMASM::WrapperMode::PURE_MASM
);

// Helper to detect format from file without creating wrapper
UniversalWrapperMASM::Format detectFormatQuick(const QString& filePath);

// Batch operation helper
struct BatchLoadResult {
    QString filePath;
    UniversalWrapperMASM::Format format;
    bool success;
    QString errorMessage;
    uint64_t duration_ms;
};

std::vector<BatchLoadResult> loadModelsUniversal(
    const QStringList& modelPaths,
    UniversalWrapperMASM::WrapperMode mode = UniversalWrapperMASM::WrapperMode::PURE_MASM
);

#endif // UNIVERSAL_WRAPPER_MASM_HPP
