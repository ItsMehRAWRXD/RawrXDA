#include "gguf_loader.hpp"
#include "gguf_loader.h"
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <memory>
#include <stdexcept>

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
    try {
        if (!m_loader) {
            qWarning() << "[GGUFLoaderQt] No loader available for tensor:" << tensorName;
            return QByteArray();
        }

        std::string nativeName = tensorName.toStdString();
        std::vector<uint8_t> data;

        if (!m_loader->LoadTensorZone(nativeName, data)) {
            qDebug() << "[GGUFLoaderQt] Failed to load tensor:" << tensorName;
            return QByteArray();
        }

        // Convert std::vector<uint8_t> to QByteArray
        return QByteArray(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));

    } catch (const std::exception& e) {
        qWarning() << "[GGUFLoaderQt] Exception loading tensor" << tensorName << ":" << e.what();
        return QByteArray();
    } catch (...) {
        qWarning() << "[GGUFLoaderQt] Unknown exception loading tensor:" << tensorName;
        return QByteArray();
    }
}

QHash<QString, QByteArray> GGUFLoaderQt::getTokenizerMetadata() const
{
    QHash<QString, QByteArray> result;
    // Tokenizer metadata extraction would go here
    // For now, return empty - actual implementation depends on GGUF structure
    return result;
}

QStringList GGUFLoaderQt::tensorNames() const
{
    return m_cachedTensorNames;
}
