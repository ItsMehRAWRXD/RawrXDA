#include "model_loader_with_compression.hpp"
#include <QDateTime>
#include <QFileInfo>
#include <QStandardPaths>
#include <QElapsedTimer>

ModelLoaderWithCompression::ModelLoaderWithCompression(const QString& modelPath)
    : m_modelPath(modelPath), m_loaded(false)
{
    qDebug() << "[ModelLoaderWithCompression] Created for model:" << modelPath;
}

ModelLoaderWithCompression::~ModelLoaderWithCompression()
{
    qDebug() << "[ModelLoaderWithCompression] Destroyed";
}

bool ModelLoaderWithCompression::loadModel()
{
    if (m_loaded) {
        qWarning() << "[ModelLoaderWithCompression] Model already loaded";
        return true;
    }
    
    return loadModelInternal();
}

bool ModelLoaderWithCompression::loadModelInternal()
{
    try {
        qInfo() << "[ModelLoaderWithCompression] Loading model:" << m_modelPath;
        
        // Create GGUF loader
        m_loader = std::make_unique<GGUFLoaderQt>(m_modelPath);
        
        if (!m_loader->isOpen()) {
            qCritical() << "[ModelLoaderWithCompression] Failed to open GGUF file";
            m_loader.reset();
            return false;
        }
        
        // Validate model
        if (!validateModel()) {
            qCritical() << "[ModelLoaderWithCompression] Model validation failed";
            m_loader.reset();
            return false;
        }
        
        m_loaded = true;
        qInfo() << "[ModelLoaderWithCompression] Model loaded successfully";
        qInfo() << "[ModelLoaderWithCompression] Tensors available:" << getTensorNames().size();
        
        return true;
        
    } catch (const std::exception& e) {
        qCritical() << "[ModelLoaderWithCompression] Exception loading model:" << e.what();
        m_loader.reset();
        return false;
    } catch (...) {
        qCritical() << "[ModelLoaderWithCompression] Unknown exception loading model";
        m_loader.reset();
        return false;
    }
}

QByteArray ModelLoaderWithCompression::compressTensor(const QString& tensorName)
{
    if (!m_loaded || !m_loader) {
        qWarning() << "[ModelLoaderWithCompression] Model not loaded";
        return QByteArray();
    }
    
    QElapsedTimer timer;
    timer.start();
    
    // Load tensor data
    QByteArray tensorData = m_loader->inflateWeight(tensorName);
    if (tensorData.isEmpty()) {
        qWarning() << "[ModelLoaderWithCompression] Failed to load tensor:" << tensorName;
        return QByteArray();
    }
    
    // Compress using brutal MASM
    QByteArray compressed = brutal::compress(tensorData);
    qint64 compressTime = timer.restart();
    
    if (compressed.isEmpty()) {
        qWarning() << "[ModelLoaderWithCompression] Compression failed for:" << tensorName;
        return QByteArray();
    }
    
    // Update compression stats
    updateCompressionStats(tensorName, tensorData.size(), compressed.size(), 
                          compressTime, 0);
    
    qDebug() << "[ModelLoaderWithCompression] Compressed" << tensorName 
             << "from" << tensorData.size() << "to" << compressed.size() << "bytes"
             << "in" << compressTime << "ms";
    
    return compressed;
}

QByteArray ModelLoaderWithCompression::decompressTensor(const QByteArray& compressedData)
{
    if (compressedData.isEmpty()) {
        qWarning() << "[ModelLoaderWithCompression] Empty compressed data";
        return QByteArray();
    }
    
    QElapsedTimer timer;
    timer.start();
    
    // Decompress using brutal MASM
    QByteArray decompressed = brutal::decompress(compressedData);
    qint64 decompressTime = timer.elapsed();
    
    if (decompressed.isEmpty()) {
        qWarning() << "[ModelLoaderWithCompression] Decompression failed";
        return QByteArray();
    }
    
    qDebug() << "[ModelLoaderWithCompression] Decompressed" << compressedData.size() 
             << "to" << decompressed.size() << "bytes in" << decompressTime << "ms";
    
    return decompressed;
}

QHash<QString, QVariant> ModelLoaderWithCompression::getModelMetadata() const
{
    QHash<QString, QVariant> metadata;
    
    if (!m_loaded || !m_loader) {
        return metadata;
    }
    
    // Get basic model parameters
    metadata.insert("n_layer", m_loader->getParam("n_layer", -1));
    metadata.insert("n_embd", m_loader->getParam("n_embd", -1));
    metadata.insert("n_vocab", m_loader->getParam("n_vocab", -1));
    metadata.insert("n_ctx", m_loader->getParam("n_ctx", -1));
    
    // Get file info
    QFileInfo fileInfo(m_modelPath);
    metadata.insert("file_size", fileInfo.size());
    metadata.insert("modified", fileInfo.lastModified());
    
    // Get tensor count
    metadata.insert("tensor_count", getTensorNames().size());
    
    return metadata;
}

QStringList ModelLoaderWithCompression::getTensorNames() const
{
    if (!m_loaded || !m_loader) {
        return QStringList();
    }
    
    return m_loader->tensorNames();
}

qint64 ModelLoaderWithCompression::getModelSize() const
{
    if (!m_loaded) {
        return 0;
    }
    
    QFileInfo fileInfo(m_modelPath);
    return fileInfo.size();
}

QByteArray ModelLoaderWithCompression::loadTensor(const QString& tensorName, bool useCache)
{
    if (!m_loaded || !m_loader) {
        qWarning() << "[ModelLoaderWithCompression] Model not loaded";
        return QByteArray();
    }
    
    // Try cache first if enabled
    if (useCache) {
        QByteArray cached = loadCachedTensor(tensorName);
        if (!cached.isEmpty()) {
            qDebug() << "[ModelLoaderWithCompression] Loaded from cache:" << tensorName;
            return cached;
        }
    }
    
    // Load directly from GGUF
    QByteArray tensorData = m_loader->inflateWeight(tensorName);
    
    if (tensorData.isEmpty()) {
        qWarning() << "[ModelLoaderWithCompression] Failed to load tensor:" << tensorName;
        return QByteArray();
    }
    
    // Cache if enabled
    if (useCache) {
        cacheTensor(tensorName, tensorData);
    }
    
    return tensorData;
}

bool ModelLoaderWithCompression::cacheTensor(const QString& tensorName, const QByteArray& data)
{
    if (data.isEmpty()) {
        qWarning() << "[ModelLoaderWithCompression] Empty data for caching";
        return false;
    }
    
    // Ensure cache directory exists
    if (!ensureCacheDir()) {
        qWarning() << "[ModelLoaderWithCompression] Failed to create cache directory";
        return false;
    }

    // Compress before caching
    QByteArray compressed;
    // Prefer direct compression of provided data
    compressed = brutal::compress(data);
    if (compressed.isEmpty()) {
        qWarning() << "[ModelLoaderWithCompression] Compression failed for caching";
        return false;
    }
    
    // Save to cache file
    QString cacheFile = getCacheFilePath(tensorName);
    QFile file(cacheFile);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[ModelLoaderWithCompression] Failed to open cache file:" << cacheFile;
        return false;
    }
    
    if (file.write(compressed) != compressed.size()) {
        qWarning() << "[ModelLoaderWithCompression] Failed to write cache file";
        file.close();
        return false;
    }
    
    file.close();
    qDebug() << "[ModelLoaderWithCompression] Cached tensor:" << tensorName << "to" << cacheFile;
    
    return true;
}

QByteArray ModelLoaderWithCompression::loadCachedTensor(const QString& tensorName)
{
    QString cacheFile = getCacheFilePath(tensorName);
    QFile file(cacheFile);
    
    if (!file.exists()) {
        return QByteArray();
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[ModelLoaderWithCompression] Failed to open cache file:" << cacheFile;
        return QByteArray();
    }
    
    QByteArray compressed = file.readAll();
    file.close();
    
    if (compressed.isEmpty()) {
        qWarning() << "[ModelLoaderWithCompression] Empty cache file:" << cacheFile;
        return QByteArray();
    }
    
    // Decompress
    QByteArray decompressed = decompressTensor(compressed);
    
    if (decompressed.isEmpty()) {
        qWarning() << "[ModelLoaderWithCompression] Decompression failed for cache:" << cacheFile;
        return QByteArray();
    }
    
    return decompressed;
}

bool ModelLoaderWithCompression::validateModel() const
{
    if (!m_loader) {
        return false;
    }
    
    // Check for unsupported quantization
    if (m_loader->hasUnsupportedQuantizationTypes()) {
        qWarning() << "[ModelLoaderWithCompression] Model has unsupported quantization types";
        // Continue loading but warn user
    }
    
    // Check tensor count
    QStringList tensors = getTensorNames();
    if (tensors.isEmpty()) {
        qCritical() << "[ModelLoaderWithCompression] No tensors found in model";
        return false;
    }
    
    qInfo() << "[ModelLoaderWithCompression] Model validation passed";
    return true;
}

QStringList ModelLoaderWithCompression::getValidationErrors() const
{
    QStringList errors;
    
    if (!m_loader) {
        errors.append("Model loader not initialized");
        return errors;
    }
    
    if (m_loader->hasUnsupportedQuantizationTypes()) {
        QStringList unsupported = m_loader->getUnsupportedQuantizationInfo();
        errors.append("Unsupported quantization types:");
        errors.append(unsupported);
    }
    
    QStringList tensors = getTensorNames();
    if (tensors.isEmpty()) {
        errors.append("No tensors found in model");
    }
    
    return errors;
}

ModelLoaderWithCompression::CompressionStats ModelLoaderWithCompression::getCompressionStats(const QString& tensorName) const
{
    if (m_compressionStats.contains(tensorName)) {
        return m_compressionStats.value(tensorName);
    }
    
    return CompressionStats();
}

QString ModelLoaderWithCompression::getCacheDir() const
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString cacheDir = baseDir + "/model_cache/" + QFileInfo(m_modelPath).fileName();
    return cacheDir;
}

QString ModelLoaderWithCompression::getCacheFilePath(const QString& tensorName) const
{
    QString cacheDir = getCacheDir();
    QString safeName = tensorName;
    safeName.replace(QChar('/'), QChar('_'));
    safeName.replace(QChar('.'), QChar('_'));
    return cacheDir + "/" + safeName + ".compressed";
}

bool ModelLoaderWithCompression::ensureCacheDir() const
{
    QString cacheDir = getCacheDir();
    QDir dir;
    return dir.mkpath(cacheDir);
}

void ModelLoaderWithCompression::updateCompressionStats(const QString& tensorName, 
                                                       qint64 originalSize, qint64 compressedSize,
                                                       qint64 compressTime, qint64 decompressTime)
{
    CompressionStats stats;
    stats.originalSize = originalSize;
    stats.compressedSize = compressedSize;
    stats.compressionRatio = originalSize > 0 ? (1.0 - (double)compressedSize / originalSize) : 0.0;
    stats.compressionTimeMs = compressTime;
    stats.decompressionTimeMs = decompressTime;
    
    m_compressionStats.insert(tensorName, stats);
}
