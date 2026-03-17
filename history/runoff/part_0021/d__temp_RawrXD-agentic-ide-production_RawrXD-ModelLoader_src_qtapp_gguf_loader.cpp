#include "gguf_loader.hpp"
#include "gguf_loader.h"
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <QByteArray>
#include <memory>
#include <stdexcept>
#include <cstring>

GGUFLoaderQt::GGUFLoaderQt(const QString& path)
    : m_loader(nullptr), m_initialized(false)
{
    if (!path.isEmpty()) {
        initializeNativeLoader(path);
    }
}

GGUFLoaderQt::~GGUFLoaderQt()
{
    // Unique_ptr will automatically clean up m_loader
    m_loader.reset();
}

void GGUFLoaderQt::initializeNativeLoader(const QString& path)
{
    try {
        if (path.isEmpty()) {
            qWarning() << "[GGUFLoaderQt] Path is empty";
            return;
        }

        // Convert QString to std::string for the native loader
        std::string nativePath = path.toStdString();
        
        // Create and initialize the native loader
        m_loader = std::make_unique<GGUFLoader>();
        
        if (!m_loader->Open(nativePath)) {
            qWarning() << "[GGUFLoaderQt] Failed to open GGUF file:" << path;
            m_loader.reset();
            return;
        }

        // Parse header and metadata
        if (!m_loader->ParseHeader()) {
            qWarning() << "[GGUFLoaderQt] Failed to parse GGUF header:" << path;
            m_loader->Close();
            m_loader.reset();
            return;
        }

        if (!m_loader->ParseMetadata()) {
            qWarning() << "[GGUFLoaderQt] Failed to parse GGUF metadata:" << path;
            m_loader->Close();
            m_loader.reset();
            return;
        }

        // Build tensor index
        if (!m_loader->BuildTensorIndex()) {
            qWarning() << "[GGUFLoaderQt] Failed to build tensor index:" << path;
            m_loader->Close();
            m_loader.reset();
            return;
        }

        m_initialized = true;
        
        // Cache tensor names for quick access
        auto tensorInfo = m_loader->GetTensorInfo();
        for (const auto& info : tensorInfo) {
            m_cachedTensorNames.append(QString::fromStdString(info.name));
        }
        
        // Cache key metadata parameters
        auto metadata = m_loader->GetMetadata();
        m_metadataCache.insert("n_layer", static_cast<int>(metadata.layer_count));
        m_metadataCache.insert("n_embd", static_cast<int>(metadata.embedding_dim));
        m_metadataCache.insert("n_vocab", static_cast<int>(metadata.vocab_size));
        m_metadataCache.insert("n_ctx", static_cast<int>(metadata.context_length));
        
        // Cache additional metadata from kv_pairs
        for (const auto& pair : metadata.kv_pairs) {
            QString key = QString::fromStdString(pair.first);
            QString value = QString::fromStdString(pair.second);
            m_metadataCache.insert(key, value);
        }

        qInfo() << "[GGUFLoaderQt] Successfully initialized GGUF loader:" << path;
        qInfo() << "[GGUFLoaderQt] Model has" << m_cachedTensorNames.size() << "tensors";

    } catch (const std::exception& e) {
        qWarning() << "[GGUFLoaderQt] Exception during initialization:" << e.what();
        m_loader.reset();
        m_initialized = false;
    } catch (...) {
        qWarning() << "[GGUFLoaderQt] Unknown exception during initialization";
        m_loader.reset();
        m_initialized = false;
    }
}

bool GGUFLoaderQt::isOpen() const
{
    return m_initialized && m_loader && m_loader->GetHeader().magic == 0x46554747;
}

QVariant GGUFLoaderQt::getParam(const QString& key, const QVariant& defaultValue) const
{
    if (m_metadataCache.contains(key)) {
        return m_metadataCache.value(key);
    }
    return defaultValue;
}

QByteArray GGUFLoaderQt::inflateWeight(const QString& tensorName)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    
    try {
        if (!m_loader) {
            qWarning() << "[GGUFLoaderQt] No loader available for tensor:" << tensorName;
            return QByteArray();
        }

        std::string nativeName = tensorName.toStdString();
        std::vector<uint8_t> data;

        QElapsedTimer loadTimer;
        loadTimer.start();
        if (!m_loader->LoadTensorZone(nativeName, data)) {
            qDebug() << "[GGUFLoaderQt] Failed to load tensor:" << tensorName;
            return QByteArray();
        }
        qint64 loadElapsed = loadTimer.elapsed();

        // Convert std::vector<uint8_t> to QByteArray safely
        // Check for buffer overflow - limit to 2GB max tensor size
        if (data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            qCritical() << "[GGUFLoaderQt] Tensor too large (>2GB):" << tensorName << "Size:" << data.size();
            return QByteArray();
        }
        
        if (data.empty()) {
            qDebug() << "[GGUFLoaderQt] Empty tensor data:" << tensorName;
            return QByteArray();
        }
        
        QByteArray rawData(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));
        
        // === MASM DECOMPRESSION INTEGRATION ===
        // Check if tensor is compressed and apply MASM decompression if needed
        QByteArray decompressedData;
        bool isCompressed = false;
        
        // Heuristic: Check for gzip magic (0x1f 0x8b) or deflate-like patterns
        if (rawData.size() >= 2 && 
            static_cast<uint8_t>(rawData[0]) == 0x1f && 
            static_cast<uint8_t>(rawData[1]) == 0x8b) {
            // GZIP-compressed tensor detected
            isCompressed = true;
            qInfo() << "[GGUFLoaderQt] Tensor" << tensorName << "is GZIP-compressed, invoking MASM decompression";
            
            BrutalGzipWrapper gzipDecompressor;
            QElapsedTimer decompTimer;
            decompTimer.start();
            
            bool success = gzipDecompressor.decompress(rawData, decompressedData);
            qint64 decompElapsed = decompTimer.elapsed();
            
            if (!success) {
                qCritical() << "[GGUFLoaderQt] GZIP decompression FAILED for tensor:" << tensorName;
                qCritical() << "[GGUFLoaderQt] Falling back to raw (compressed) data - inference may fail";
                decompressedData = rawData;  // Fallback
            } else {
                double throughputMBps = (decompressedData.size() / 1024.0 / 1024.0) / (decompElapsed / 1000.0);
                qInfo() << QString("[GGUFLoaderQt] ✓ Decompressed %1: %2 → %3 bytes (%.2f MB/s)")
                    .arg(tensorName).arg(rawData.size()).arg(decompressedData.size()).arg(throughputMBps);
            }
        } else if (rawData.size() >= 10) {
            // Heuristic: Check if data might be deflate-compressed
            // Try decompression; if it fails, assume uncompressed
            DeflateWrapper deflateDecompressor;
            QByteArray testDecomp;
            
            if (deflateDecompressor.decompress(rawData, testDecomp) && testDecomp.size() > rawData.size()) {
                // Successful decompression with expansion - likely was compressed
                isCompressed = true;
                decompressedData = testDecomp;
                qInfo() << QString("[GGUFLoaderQt] ✓ DEFLATE decompressed %1: %2 → %3 bytes")
                    .arg(tensorName).arg(rawData.size()).arg(decompressedData.size());
            } else {
                // Not compressed or decompression failed - use raw data
                decompressedData = rawData;
            }
        } else {
            // Too small to be compressed or no compression markers
            decompressedData = rawData;
        }
        
        qint64 totalElapsed = totalTimer.elapsed();
        qDebug() << QString("[GGUFLoaderQt] Tensor %1 loaded in %2ms (disk: %3ms, decomp: %4ms, compressed: %5)")
            .arg(tensorName).arg(totalElapsed).arg(loadElapsed)
            .arg(isCompressed ? (totalElapsed - loadElapsed) : 0)
            .arg(isCompressed ? "yes" : "no");
        
        return decompressedData;

    } catch (const std::exception& e) {
        qCritical() << "[GGUFLoaderQt] EXCEPTION loading tensor" << tensorName << ":" << e.what();
        return QByteArray();
    } catch (...) {
        qCritical() << "[GGUFLoaderQt] UNKNOWN EXCEPTION loading tensor:" << tensorName;
        return QByteArray();
    }
}

QHash<QString, QByteArray> GGUFLoaderQt::getTokenizerMetadata() const
{
    QHash<QString, QByteArray> result;
    if (!m_loader) {
        return result;
    }

    try {
        const auto metadata = m_loader->GetMetadata();

        // Export kv_pairs as raw byte arrays
        for (const auto& pair : metadata.kv_pairs) {
            result.insert(QString::fromStdString(pair.first), QByteArray::fromStdString(pair.second));
        }

        // Include vocab size for convenience
        if (metadata.vocab_size > 0) {
            result.insert(QStringLiteral("tokenizer.ggml.vocab_size"), QByteArray::number(static_cast<qint64>(metadata.vocab_size)));
        }

        // Flatten tokens vector into a single \0-delimited blob (matches GGUF convention)
        if (!metadata.tokens.empty()) {
            QByteArray tokenBlob;
            tokenBlob.reserve(static_cast<int>(metadata.tokens.size() * 8));
            for (const auto& tok : metadata.tokens) {
                tokenBlob.append(QByteArray::fromStdString(tok));
                tokenBlob.append('\0');
            }
            result.insert(QStringLiteral("tokenizer.ggml.tokens"), tokenBlob);
        }

        // Serialize scores (float32 little-endian)
        if (!metadata.token_scores.empty()) {
            QByteArray scoreBlob;
            scoreBlob.resize(static_cast<int>(metadata.token_scores.size() * sizeof(float)));
            memcpy(scoreBlob.data(), metadata.token_scores.data(), scoreBlob.size());
            result.insert(QStringLiteral("tokenizer.ggml.scores"), scoreBlob);
        }

        // Serialize token types (uint32 little-endian)
        if (!metadata.token_types.empty()) {
            QByteArray typeBlob;
            typeBlob.resize(static_cast<int>(metadata.token_types.size() * sizeof(uint32_t)));
            memcpy(typeBlob.data(), metadata.token_types.data(), typeBlob.size());
            result.insert(QStringLiteral("tokenizer.ggml.token_type"), typeBlob);
        }
    } catch (const std::exception& e) {
        qWarning() << "[GGUFLoaderQt] Exception building tokenizer metadata map:" << e.what();
    } catch (...) {
        qWarning() << "[GGUFLoaderQt] Unknown exception building tokenizer metadata map";
    }

    return result;
}

QStringList GGUFLoaderQt::tensorNames() const
{
    return m_cachedTensorNames;
}

bool GGUFLoaderQt::hasUnsupportedQuantizationTypes() const
{
    if (!m_loader) {
        return false;
    }
    return m_loader->HasUnsupportedQuantizationTypes();
}

QStringList GGUFLoaderQt::getUnsupportedQuantizationInfo() const
{
    QStringList result;
    
    if (!m_loader) {
        return result;
    }
    
    auto unsupported = m_loader->GetUnsupportedQuantizationTypes();
    
    for (const auto& info : unsupported) {
        QString line = QString::fromStdString(info.type_name) +
                      " (type " + QString::number(info.type_value) + "): " +
                      QString::number(info.tensor_names.size()) + " tensors";
        result.append(line);
    }
    
    return result;
}

QString GGUFLoaderQt::getRecommendedConversionType() const
{
    if (!m_loader) {
        return "Q5_K";
    }
    return QString::fromStdString(m_loader->GetRecommendedConversionType());
}

