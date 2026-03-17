#pragma once

#include <QString>
#include <memory>
#include <cstdint>
#include <map>

// ========================================================================================================
// MASM Extern C Declarations - Format Router
// These functions are implemented in pure x64 MASM assembly
// Purpose: Provide C++ wrapper around MASM format router
// ========================================================================================================

extern "C" {
    
    // Structure declarations (matching MASM definitions)
    struct CacheEntryMAVM {
        void*       path;
        uint32_t    format;
        uint32_t    compression;
        uint64_t    timestamp;
        uint32_t    valid;
        uint32_t    _padding[27];
    };
    
    struct FormatRouterMAVM {
        void*       mutex;
        void*       cache;
        uint32_t    cache_size;
        uint64_t    cache_hits;
        uint64_t    cache_misses;
        uint64_t    last_detection;
        uint32_t    is_initialized;
        uint32_t    _padding[115];
    };
    
    // EXPORTED FUNCTIONS FROM MASM
    FormatRouterMAVM* format_router_init();
    
    int detect_extension(const wchar_t* file_path, wchar_t* ext_output);
    
    int detect_magic_bytes(const wchar_t* file_path, unsigned char* magic_buffer);
    
    int format_router_detect_all(const wchar_t* file_path, void* result_struct);
    
    void format_router_shutdown(FormatRouterMAVM* router);
}

// ========================================================================================================
// C++ WRAPPER CLASS - Can be toggled to use MASM or original C++ implementation
// ========================================================================================================

class FormatRouterMASM {
public:
    enum class Mode {
        PURE_MASM,          // Use pure MASM implementation
        CPP_QT              // Use original C++/Qt implementation
    };
    
    enum class ModelFormat {
        UNKNOWN = 0,
        GGUF_LOCAL = 1,
        HF_REPO = 2,
        HF_FILE = 3,
        OLLAMA_REMOTE = 4,
        MASM_COMPRESSED = 5,
        UNIVERSAL_FORMAT = 6
    };
    
    enum class CompressionType {
        NONE = 0,
        GZIP = 1,
        ZSTD = 2,
        LZ4 = 3
    };
    
    explicit FormatRouterMASM(Mode mode = Mode::PURE_MASM);
    ~FormatRouterMASM();
    
    // Format detection - routes to MASM or C++ based on mode
    ModelFormat detectFormat(const QString& input);
    CompressionType detectCompression(const QString& filePath);
    QString detectExtension(const QString& filePath);
    
    // Magic byte detection
    bool hasMagic(const QString& filePath, ModelFormat& outFormat);
    
    // Route to correct loader
    bool validateModelPath(const QString& path);
    
    // Cache statistics
    uint64_t getCacheHits() const;
    uint64_t getCacheMisses() const;
    void clearCache();
    
    // Status
    QString getLastError() const { return m_lastError; }
    bool isInitialized() const { return m_initialized; }
    
    // Mode control
    static void SetGlobalMode(Mode mode) { s_globalMode = mode; }
    static Mode GetGlobalMode() { return s_globalMode; }
    
private:
    Mode m_currentMode;
    FormatRouterMAVM* m_masmRouter;
    
    QString m_lastError;
    bool m_initialized;
    
    // Local cache for C++ mode
    std::map<QString, ModelFormat> m_localCache;
    
    static Mode s_globalMode;
    
    // MASM implementation wrappers
    ModelFormat detectFormat_MASM(const QString& input);
    CompressionType detectCompression_MASM(const QString& filePath);
    bool validateModelPath_MASM(const QString& path);
    
    // Original C++/Qt implementation methods (forward declaration)
    ModelFormat detectFormat_CppQt(const QString& input);
    CompressionType detectCompression_CppQt(const QString& filePath);
    bool validateModelPath_CppQt(const QString& path);
};
