#pragma once

#include <QString>
#include <memory>
#include <cstdint>

// ========================================================================================================
// MASM Extern C Declarations - Universal Format Loader
// These functions are implemented in pure x64 MASM assembly
// Purpose: Provide C++ wrapper around MASM universal format loader
// ========================================================================================================

extern "C" {
    
    // Structure declarations (matching MASM definitions)
    struct UniversalFormatLoaderMAVM {
        void*       mutex;              // HANDLE to Windows mutex
        void*       file_buffer;        // malloc'd file contents
        uint64_t    buffer_size;        // size of file buffer
        uint32_t    format_type;        // detected format (enum value)
        uint32_t    error_code;         // last error code
        void*       temp_path;          // temp file path
        uint8_t     magic_bytes[16];    // file magic bytes
        uint32_t    is_initialized;     // initialization flag
        uint32_t    _padding[43];       // alignment padding
    };
    
    struct DetectionResultMAVM {
        uint32_t    format;             // ModelFormat enum
        uint32_t    compression;        // CompressionType enum
        const char* reason;             // error/status message
        uint32_t    valid;              // 0 or 1
        uint32_t    _padding;           // alignment
    };

    struct ParserContext {
        uint32_t error_code;
        const char* error_message;
        void (*progress_cb)(uint64_t current, uint64_t total, void* data);
        void* progress_data;
        uint32_t total_steps;
        uint32_t current_step;
    };
    
    // EXPORTED FUNCTIONS FROM MASM
    UniversalFormatLoaderMAVM* universal_loader_init();
    
    int detect_format_magic(const wchar_t* file_path, unsigned char* magic_buffer);
    
    int read_file_to_buffer(const wchar_t* file_path, void** buffer_ptr, size_t* size);
    
    int parse_safetensors_metadata(void* buffer, size_t size, void* output_array);
    
    int convert_to_gguf(UniversalFormatLoaderMAVM* loader, const wchar_t* temp_path);
    
    DetectionResultMAVM* get_detection_result(int format, int valid, const char* reason);
    
    void* ParseTensorFlowSavedModel(const wchar_t* path, ParserContext* ctx, size_t* out_size);
    void* ParseONNXFile(const wchar_t* path, ParserContext* ctx, size_t* out_size);
    
    void universal_loader_shutdown(UniversalFormatLoaderMAVM* loader);
}

// ========================================================================================================
// C++ WRAPPER CLASS - Can be toggled to use MASM or original C++ implementation
// ========================================================================================================

class UniversalFormatLoaderMASM {
public:
    enum class Mode {
        PURE_MASM,          // Use pure MASM implementation
        CPP_QT              // Use original C++/Qt implementation
    };
    
    explicit UniversalFormatLoaderMASM(Mode mode = Mode::PURE_MASM);
    ~UniversalFormatLoaderMASM();
    
    // Format detection - routes to MASM or C++ based on mode
    int detectFormatMagic(const QString& filePath);
    int detectFormatExtension(const QString& filePath);
    
    // File loading - routes to MASM or C++ based on mode
    bool loadSafeTensors(const QString& modelPath);
    bool loadPyTorch(const QString& modelPath);
    bool loadTensorFlow(const QString& modelPath);
    bool loadONNX(const QString& modelPath);
    bool loadNumPy(const QString& modelPath);
    
    // Conversion - routes to MASM or C++ based on mode
    bool convertToGGUF(const QString& outputPath);
    
    // Status queries
    QString getLastError() const { return m_lastError; }
    int getDetectedFormat() const { return m_detectedFormat; }
    QString getTempOutputPath() const { return m_tempOutputPath; }
    bool isInitialized() const { return m_initialized; }
    
    // Mode control
    static void SetGlobalMode(Mode mode) { s_globalMode = mode; }
    static Mode GetGlobalMode() { return s_globalMode; }
    
private:
    Mode m_currentMode;
    UniversalFormatLoaderMAVM* m_masmLoader;
    
    QString m_lastError;
    int m_detectedFormat;
    QString m_tempOutputPath;
    bool m_initialized;
    
    static Mode s_globalMode;
    
    // MASM implementation wrappers
    int detectFormatMagic_MASM(const QString& filePath);
    bool loadSafeTensors_MASM(const QString& modelPath);
    bool loadPyTorch_MASM(const QString& modelPath);
    bool convertToGGUF_MASM(const QString& outputPath);
    
    // Original C++/Qt implementation methods (forward declaration)
    int detectFormatMagic_CppQt(const QString& filePath);
    bool loadSafeTensors_CppQt(const QString& modelPath);
    bool loadPyTorch_CppQt(const QString& modelPath);
    bool convertToGGUF_CppQt(const QString& outputPath);
};
