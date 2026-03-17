#include "universal_format_loader.hpp"
#include <QFile>
#include <QDebug>
#include <QDir>
#include <cstring>

// External MASM functions (declared in universal_format_loader.hpp, linked from MASM modules)
extern "C" {
    int DetectFormatFromFile(const wchar_t* filePath);
    int DetectFormatFromBuffer(const uint8_t* buffer, size_t size);
    uint8_t* ParseSafeTensorsFile(const wchar_t* filePath, size_t* outSize);
    uint8_t* ParsePyTorchFile(const wchar_t* filePath, size_t* outSize);
}

UniversalFormatLoader::UniversalFormatLoader()
    : m_lastError("")
{
}

UniversalFormatLoader::~UniversalFormatLoader()
{
    m_tensors.clear();
}

UniversalFormat UniversalFormatLoader::detectFormat(const QString& filePath)
{
    if (filePath.isEmpty()) {
        m_lastError = "File path is empty";
        return UniversalFormat::Unknown;
    }
    
    QFile file(filePath);
    if (!file.exists()) {
        m_lastError = QString("File not found: %1").arg(filePath);
        return UniversalFormat::Unknown;
    }
    
    // Call MASM function
    int formatCode = DetectFormatFromFile(reinterpret_cast<const wchar_t*>(filePath.utf16()));
    
    return static_cast<UniversalFormat>(formatCode);
}

UniversalFormat UniversalFormatLoader::detectFormatFromBuffer(const uint8_t* buffer, size_t size)
{
    if (!buffer || size == 0) {
        m_lastError = "Buffer is empty";
        return UniversalFormat::Unknown;
    }
    
    int formatCode = DetectFormatFromBuffer(buffer, size);
    return static_cast<UniversalFormat>(formatCode);
}

QByteArray UniversalFormatLoader::loadSafeTensors(const QString& filePath)
{
    qDebug() << "[UniversalFormatLoader] Loading SafeTensors:" << filePath;
    
    if (filePath.isEmpty()) {
        m_lastError = "File path is empty";
        return QByteArray();
    }
    
    // Verify file exists
    QFile file(filePath);
    if (!file.exists()) {
        m_lastError = QString("SafeTensors file not found: %1").arg(filePath);
        qWarning() << m_lastError;
        return QByteArray();
    }
    
    size_t outSize = 0;
    uint8_t* ggufData = ParseSafeTensorsFile(
        reinterpret_cast<const wchar_t*>(filePath.utf16()),
        &outSize
    );
    
    if (!ggufData || outSize == 0) {
        m_lastError = "Failed to parse SafeTensors file";
        qWarning() << m_lastError;
        return QByteArray();
    }
    
    // Copy to QByteArray (will be freed by MASM malloc)
    QByteArray result(reinterpret_cast<const char*>(ggufData), static_cast<int>(outSize));
    
    // Free MASM-allocated memory
    free(ggufData);
    
    qDebug() << "[UniversalFormatLoader] SafeTensors converted successfully, size:" << outSize << "bytes";
    return result;
}

QByteArray UniversalFormatLoader::loadPyTorch(const QString& filePath)
{
    qDebug() << "[UniversalFormatLoader] Loading PyTorch:" << filePath;
    
    if (filePath.isEmpty()) {
        m_lastError = "File path is empty";
        return QByteArray();
    }
    
    QFile file(filePath);
    if (!file.exists()) {
        m_lastError = QString("PyTorch file not found: %1").arg(filePath);
        qWarning() << m_lastError;
        return QByteArray();
    }
    
    size_t outSize = 0;
    uint8_t* ggufData = ParsePyTorchFile(
        reinterpret_cast<const wchar_t*>(filePath.utf16()),
        &outSize
    );
    
    if (!ggufData || outSize == 0) {
        m_lastError = "Failed to parse PyTorch file";
        qWarning() << m_lastError;
        return QByteArray();
    }
    
    QByteArray result(reinterpret_cast<const char*>(ggufData), static_cast<int>(outSize));
    free(ggufData);
    
    qDebug() << "[UniversalFormatLoader] PyTorch converted successfully, size:" << outSize << "bytes";
    return result;
}

QByteArray UniversalFormatLoader::loadTensorFlow(const QString& filePath)
{
    m_lastError = "TensorFlow support coming in Phase 2";
    qWarning() << m_lastError;
    return QByteArray();
}

QByteArray UniversalFormatLoader::loadONNX(const QString& filePath)
{
    m_lastError = "ONNX support coming in Phase 2";
    qWarning() << m_lastError;
    return QByteArray();
}

QByteArray UniversalFormatLoader::load(const QString& filePath)
{
    if (filePath.isEmpty()) {
        m_lastError = "File path is empty";
        return QByteArray();
    }
    
    // Auto-detect format
    UniversalFormat format = detectFormat(filePath);
    
    qDebug() << "[UniversalFormatLoader] Auto-detected format:" << static_cast<int>(format);
    
    switch (format) {
        case UniversalFormat::SafeTensors:
            return loadSafeTensors(filePath);
        case UniversalFormat::PyTorch:
            return loadPyTorch(filePath);
        case UniversalFormat::TensorFlow:
            return loadTensorFlow(filePath);
        case UniversalFormat::ONNX:
            return loadONNX(filePath);
        case UniversalFormat::GGUF:
            m_lastError = "File is already GGUF format";
            return QByteArray();  // Let existing loader handle it
        default:
            m_lastError = "Unsupported or unrecognized format";
            return QByteArray();
    }
}
