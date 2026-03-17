#include "universal_wrapper_masm.hpp"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <chrono>

//========================================================================================================
// STATIC INITIALIZATION
//========================================================================================================

UniversalWrapperMASM::WrapperMode UniversalWrapperMASM::s_globalMode = 
    UniversalWrapperMASM::WrapperMode::PURE_MASM;

//========================================================================================================
// LIFECYCLE MANAGEMENT
//========================================================================================================

UniversalWrapperMASM::UniversalWrapperMASM(WrapperMode mode)
    : m_masmWrapper(nullptr)
    , m_currentMode(mode == WrapperMode::AUTO_SELECT ? s_globalMode : mode)
    , m_initialized(false)
    , m_detectedFormat(Format::UNKNOWN)
    , m_lastErrorCode(ErrorCode::OK)
    , m_lastDurationMs(0)
{
    // Initialize global wrapper once
    static bool g_global_init = false;
    if (!g_global_init) {
        wrapper_global_init(static_cast<uint32_t>(m_currentMode));
        g_global_init = true;
    }
    
    // Create this wrapper instance
    m_masmWrapper = wrapper_create(static_cast<uint32_t>(m_currentMode));
    if (m_masmWrapper) {
        m_initialized = true;
        m_lastErrorCode = ErrorCode::OK;
    } else {
        updateError(ErrorCode::ALLOC_FAILED, "Failed to allocate MASM wrapper");
    }
}

UniversalWrapperMASM::~UniversalWrapperMASM()
{
    if (m_masmWrapper) {
        wrapper_destroy(m_masmWrapper);
        m_masmWrapper = nullptr;
    }
}

UniversalWrapperMASM::UniversalWrapperMASM(UniversalWrapperMASM&& other) noexcept
    : m_masmWrapper(other.m_masmWrapper)
    , m_currentMode(other.m_currentMode)
    , m_initialized(other.m_initialized)
    , m_detectedFormat(other.m_detectedFormat)
    , m_lastError(std::move(other.m_lastError))
    , m_lastErrorCode(other.m_lastErrorCode)
    , m_tempOutputPath(std::move(other.m_tempOutputPath))
    , m_lastDurationMs(other.m_lastDurationMs)
{
    other.m_masmWrapper = nullptr;
    other.m_initialized = false;
}

UniversalWrapperMASM& UniversalWrapperMASM::operator=(UniversalWrapperMASM&& other) noexcept
{
    if (this != &other) {
        if (m_masmWrapper) {
            wrapper_destroy(m_masmWrapper);
        }
        
        m_masmWrapper = other.m_masmWrapper;
        m_currentMode = other.m_currentMode;
        m_initialized = other.m_initialized;
        m_detectedFormat = other.m_detectedFormat;
        m_lastError = std::move(other.m_lastError);
        m_lastErrorCode = other.m_lastErrorCode;
        m_tempOutputPath = std::move(other.m_tempOutputPath);
        m_lastDurationMs = other.m_lastDurationMs;
        
        other.m_masmWrapper = nullptr;
        other.m_initialized = false;
    }
    return *this;
}

//========================================================================================================
// FORMAT DETECTION
//========================================================================================================

UniversalWrapperMASM::Format UniversalWrapperMASM::detectFormat(const QString& filePath)
{
    if (!m_initialized || !m_masmWrapper) {
        updateError(ErrorCode::NOT_INITIALIZED, "Wrapper not initialized");
        return Format::UNKNOWN;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    uint32_t result = wrapper_detect_format_unified(
        m_masmWrapper, 
        reinterpret_cast<const wchar_t*>(filePath.utf16())
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    m_lastDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    m_detectedFormat = static_cast<Format>(result);
    m_lastErrorCode = ErrorCode::OK;
    
    return m_detectedFormat;
}

UniversalWrapperMASM::Format UniversalWrapperMASM::detectFormatExtension(const QString& filePath)
{
    auto result = detect_extension_unified(reinterpret_cast<const wchar_t*>(filePath.utf16()));
    return static_cast<Format>(result);
}

UniversalWrapperMASM::Format UniversalWrapperMASM::detectFormatMagic(const QString& filePath)
{
    unsigned char magic[16] = {0};
    auto result = detect_magic_bytes_unified(
        reinterpret_cast<const wchar_t*>(filePath.utf16()),
        magic
    );
    return static_cast<Format>(result);
}

UniversalWrapperMASM::Compression UniversalWrapperMASM::detectCompression(const QString& filePath)
{
    auto format = detectFormat(filePath);
    
    // Check if format indicates compression
    if (format == Format::MASM_COMP) {
        unsigned char magic[16] = {0};
        detect_magic_bytes_unified(
            reinterpret_cast<const wchar_t*>(filePath.utf16()),
            magic
        );
        
        // Analyze magic bytes for compression type
        uint32_t magic32 = *reinterpret_cast<uint32_t*>(magic);
        
        if ((magic32 & 0xFFFF) == 0x8B1F) {
            return Compression::GZIP;
        } else if (magic32 == 0xFD2FB528) {
            return Compression::ZSTD;
        } else if (magic32 == 0x184D2204) {
            return Compression::LZ4;
        }
    }
    
    return Compression::NONE;
}

bool UniversalWrapperMASM::validateModelPath(const QString& path)
{
    QFile file(path);
    if (!file.exists()) {
        updateError(ErrorCode::FILE_NOT_FOUND, QString("File not found: %1").arg(path));
        return false;
    }
    
    // Try to detect format
    auto format = detectFormat(path);
    return format != Format::UNKNOWN;
}

//========================================================================================================
// MODEL LOADING (unified format-agnostic)
//========================================================================================================

bool UniversalWrapperMASM::loadUniversalFormat(const QString& modelPath)
{
    if (!m_initialized || !m_masmWrapper) {
        updateError(ErrorCode::NOT_INITIALIZED, "Wrapper not initialized");
        return false;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    LoadResultMASM result = {};
    uint32_t success = wrapper_load_model_auto(
        m_masmWrapper,
        reinterpret_cast<const wchar_t*>(modelPath.utf16()),
        &result
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    m_lastDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    if (success) {
        m_detectedFormat = static_cast<Format>(result.format);
        if (result.output_path) {
            m_tempOutputPath = QString::fromUtf16(
                reinterpret_cast<const ushort*>(result.output_path)
            );
        }
        m_lastErrorCode = ErrorCode::OK;
        return true;
    } else {
        if (result.error_msg) {
            m_lastError = QString::fromUtf8(reinterpret_cast<const char*>(result.error_msg));
        }
        m_lastErrorCode = ErrorCode::LOAD_FAILED;
        return false;
    }
}

bool UniversalWrapperMASM::loadSafeTensors(const QString& modelPath)
{
    return loadUniversalFormat(modelPath);  // Route through unified loader
}

bool UniversalWrapperMASM::loadPyTorch(const QString& modelPath)
{
    return loadUniversalFormat(modelPath);  // Route through unified loader
}

bool UniversalWrapperMASM::loadTensorFlow(const QString& modelPath)
{
    return loadUniversalFormat(modelPath);  // Route through unified loader
}

bool UniversalWrapperMASM::loadONNX(const QString& modelPath)
{
    return loadUniversalFormat(modelPath);  // Route through unified loader
}

bool UniversalWrapperMASM::loadNumPy(const QString& modelPath)
{
    return loadUniversalFormat(modelPath);  // Route through unified loader
}

//========================================================================================================
// CONVERSION TO GGUF
//========================================================================================================

bool UniversalWrapperMASM::convertToGGUF(const QString& outputPath)
{
    if (m_detectedFormat == Format::UNKNOWN || m_tempOutputPath.isEmpty()) {
        updateError(ErrorCode::FORMAT_UNKNOWN, "No format detected or loaded");
        return false;
    }
    
    return convertToGGUFWithInput(m_tempOutputPath, outputPath);
}

bool UniversalWrapperMASM::convertToGGUFWithInput(const QString& inputPath, const QString& outputPath)
{
    if (!m_initialized || !m_masmWrapper) {
        updateError(ErrorCode::NOT_INITIALIZED, "Wrapper not initialized");
        return false;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    uint32_t result = wrapper_convert_to_gguf(
        m_masmWrapper,
        reinterpret_cast<const wchar_t*>(inputPath.utf16()),
        reinterpret_cast<const wchar_t*>(outputPath.utf16())
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    m_lastDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    if (result) {
        m_lastErrorCode = ErrorCode::OK;
        m_tempOutputPath = outputPath;
        return true;
    } else {
        m_lastErrorCode = ErrorCode::LOAD_FAILED;
        return false;
    }
}

//========================================================================================================
// FILE I/O OPERATIONS
//========================================================================================================

bool UniversalWrapperMASM::readFileChunked(const QString& filePath, QByteArray& outBuffer)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        updateError(ErrorCode::FILE_NOT_FOUND, QString("Cannot open file: %1").arg(filePath));
        return false;
    }
    
    // Read entire file in chunks (default 64KB chunks)
    const size_t CHUNK_SIZE = 65536;
    while (!file.atEnd()) {
        QByteArray chunk = file.read(CHUNK_SIZE);
        if (chunk.isEmpty() && !file.atEnd()) {
            updateError(ErrorCode::LOAD_FAILED, "Error reading file");
            file.close();
            return false;
        }
        outBuffer.append(chunk);
    }
    
    file.close();
    m_lastErrorCode = ErrorCode::OK;
    return true;
}

bool UniversalWrapperMASM::writeBufferToFile(const QString& filePath, const QByteArray& buffer)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        updateError(ErrorCode::FILE_NOT_FOUND, QString("Cannot write to file: %1").arg(filePath));
        return false;
    }
    
    qint64 written = file.write(buffer);
    file.close();
    
    if (written != buffer.size()) {
        updateError(ErrorCode::LOAD_FAILED, "Failed to write complete buffer to file");
        return false;
    }
    
    m_lastErrorCode = ErrorCode::OK;
    return true;
}

QString UniversalWrapperMASM::getTempDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
}

bool UniversalWrapperMASM::generateTempGGUFPath(const QString& modelName, QString& outPath)
{
    QString tempDir = getTempDirectory();
    QString fileName = modelName;
    
    // Remove extension and add .gguf
    int lastDot = fileName.lastIndexOf('.');
    if (lastDot > 0) {
        fileName = fileName.left(lastDot);
    }
    fileName += ".gguf";
    
    outPath = QDir(tempDir).filePath(fileName);
    return true;
}

void UniversalWrapperMASM::cleanupTempFiles()
{
    if (!m_tempOutputPath.isEmpty()) {
        QFile::remove(m_tempOutputPath);
        m_tempOutputPath.clear();
    }
}

//========================================================================================================
// CACHE MANAGEMENT
//========================================================================================================

uint64_t UniversalWrapperMASM::getCacheHits() const
{
    if (!m_initialized || !m_masmWrapper) return 0;
    
    WrapperStatisticsMASM stats = {};
    if (wrapper_get_statistics(m_masmWrapper, &stats)) {
        return stats.cache_hits;
    }
    return 0;
}

uint64_t UniversalWrapperMASM::getCacheMisses() const
{
    if (!m_initialized || !m_masmWrapper) return 0;
    
    WrapperStatisticsMASM stats = {};
    if (wrapper_get_statistics(m_masmWrapper, &stats)) {
        return stats.cache_misses;
    }
    return 0;
}

uint32_t UniversalWrapperMASM::getCacheSize() const
{
    if (!m_initialized || !m_masmWrapper) return 0;
    
    WrapperStatisticsMASM stats = {};
    if (wrapper_get_statistics(m_masmWrapper, &stats)) {
        return stats.cache_entries;
    }
    return 0;
}

void UniversalWrapperMASM::clearCache()
{
    // Cache clearing would be implemented in MASM
    // For now, just log the operation
    qDebug() << "UniversalWrapperMASM: Cache clear requested";
}

//========================================================================================================
// MODE CONTROL
//========================================================================================================

void UniversalWrapperMASM::setMode(WrapperMode newMode)
{
    if (!m_initialized || !m_masmWrapper) {
        updateError(ErrorCode::NOT_INITIALIZED, "Wrapper not initialized");
        return;
    }
    
    m_currentMode = newMode;
    wrapper_set_mode(m_masmWrapper, static_cast<uint32_t>(newMode));
}

UniversalWrapperMASM::WrapperMode UniversalWrapperMASM::getMode() const
{
    return m_currentMode;
}

void UniversalWrapperMASM::SetGlobalMode(WrapperMode mode)
{
    s_globalMode = mode;
}

UniversalWrapperMASM::WrapperMode UniversalWrapperMASM::GetGlobalMode()
{
    return s_globalMode;
}

//========================================================================================================
// STATUS & STATISTICS
//========================================================================================================

UniversalWrapperMASM::Statistics UniversalWrapperMASM::getStatistics() const
{
    Statistics stats = {};
    
    if (m_initialized && m_masmWrapper) {
        WrapperStatisticsMASM masmStats = {};
        if (wrapper_get_statistics(m_masmWrapper, &masmStats)) {
            stats.total_detections = masmStats.total_detections;
            stats.total_conversions = masmStats.total_conversions;
            stats.total_errors = masmStats.total_errors;
            stats.cache_hits = masmStats.cache_hits;
            stats.cache_misses = masmStats.cache_misses;
            stats.cache_entries = masmStats.cache_entries;
            stats.current_mode = masmStats.current_mode;
        }
    }
    
    return stats;
}

void UniversalWrapperMASM::resetStatistics()
{
    // Reset would be implemented in MASM
    qDebug() << "UniversalWrapperMASM: Statistics reset requested";
}

//========================================================================================================
// PRIVATE HELPERS
//========================================================================================================

void UniversalWrapperMASM::updateError(ErrorCode code, const QString& message)
{
    m_lastErrorCode = code;
    m_lastError = message;
    qWarning() << "UniversalWrapperMASM Error:" << message;
}

QString UniversalWrapperMASM::masmErrorToString(uint32_t masmErrorCode) const
{
    switch (static_cast<ErrorCode>(masmErrorCode)) {
        case ErrorCode::OK:
            return "Success";
        case ErrorCode::INVALID_PTR:
            return "Invalid pointer";
        case ErrorCode::NOT_INITIALIZED:
            return "Not initialized";
        case ErrorCode::ALLOC_FAILED:
            return "Memory allocation failed";
        case ErrorCode::MUTEX_FAILED:
            return "Mutex operation failed";
        case ErrorCode::FILE_NOT_FOUND:
            return "File not found";
        case ErrorCode::FORMAT_UNKNOWN:
            return "Format unknown";
        case ErrorCode::LOAD_FAILED:
            return "Load failed";
        case ErrorCode::CACHE_FULL:
            return "Cache full";
        case ErrorCode::MODE_INVALID:
            return "Invalid mode";
        default:
            return "Unknown error";
    }
}

//========================================================================================================
// UTILITY FUNCTIONS
//========================================================================================================

std::unique_ptr<UniversalWrapperMASM> createUniversalWrapper(UniversalWrapperMASM::WrapperMode mode)
{
    auto wrapper = std::make_unique<UniversalWrapperMASM>(mode);
    if (wrapper->isInitialized()) {
        return wrapper;
    }
    return nullptr;
}

UniversalWrapperMASM::Format detectFormatQuick(const QString& filePath)
{
    UniversalWrapperMASM wrapper;
    return wrapper.detectFormat(filePath);
}

std::vector<BatchLoadResult> loadModelsUniversal(
    const QStringList& modelPaths,
    UniversalWrapperMASM::WrapperMode mode)
{
    std::vector<BatchLoadResult> results;
    auto wrapper = createUniversalWrapper(mode);
    
    if (!wrapper) {
        for (const auto& path : modelPaths) {
            results.push_back({
                path,
                UniversalWrapperMASM::Format::UNKNOWN,
                false,
                "Failed to create wrapper",
                0
            });
        }
        return results;
    }
    
    for (const auto& path : modelPaths) {
        auto start = std::chrono::high_resolution_clock::now();
        
        bool success = wrapper->loadUniversalFormat(path);
        auto format = wrapper->getDetectedFormat();
        
        auto end = std::chrono::high_resolution_clock::now();
        uint64_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        results.push_back({
            path,
            static_cast<UniversalWrapperMASM::Format>(format),
            success,
            wrapper->getLastError(),
            duration
        });
    }
    
    return results;
}
