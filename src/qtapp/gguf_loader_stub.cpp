#include "gguf_loader.hpp"
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <cstring>

GGUFLoader::GGUFLoader(const QString& path)
    : m_path(path), m_initialized(false)
{
    try {
        initializeNativeLoader();
    } catch (const std::exception& e) {
        qWarning() << "[GGUFLoader] Failed to initialize:" << e.what();
    } catch (...) {
        qWarning() << "[GGUFLoader] Unknown error during initialization";
    }
}

GGUFLoader::~GGUFLoader() = default;

void GGUFLoader::initializeNativeLoader()
{
    if (m_path.isEmpty()) {
        throw std::runtime_error("Model path is empty");
    }
    
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open GGUF file");
    }
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // Read and verify magic bytes "GGUF"
    uint32_t magic;
    stream >> magic;
    if (magic != 0x46554747) {  // "GGUF"
        throw std::runtime_error("Invalid GGUF magic bytes");
    }
    
    // Read version
    uint32_t version;
    stream >> version;
    qInfo() << "[GGUFLoader] GGUF version:" << version;
    
    // Read tensor count
    uint64_t tensorCount;
    stream >> tensorCount;
    
    // Read metadata KV count
    uint64_t kvCount;
    stream >> kvCount;
    
    qInfo() << "[GGUFLoader] Found" << tensorCount << "tensors and" << kvCount << "metadata entries";
    
    // For now, just count the tensors - full parsing would be complex
    // The real loader in src/gguf_loader.cpp handles the full parsing
    m_cachedTensorNames.clear();
    for (uint64_t i = 0; i < tensorCount && i < 1000; i++) {
        m_cachedTensorNames.append(QString("tensor_%1").arg(i));
    }
    
    m_initialized = true;
    file.close();
}

bool GGUFLoader::isOpen() const
{
    return m_initialized;
}

QVariant GGUFLoader::getParam(const QString& key, const QVariant& defaultValue) const
{
    // Check cache first
    if (m_metadataCache.contains(key)) {
        return m_metadataCache.value(key);
    }
    return defaultValue;
}

QByteArray GGUFLoader::inflateWeight(const QString& tensorName)
{
    // Return empty for now - real implementation would load tensor data
    // This is a placeholder that at least allows the app not to crash
    return QByteArray();
}

QHash<QString, QByteArray> GGUFLoader::getTokenizerMetadata() const
{
    QHash<QString, QByteArray> meta;
    // Return empty - tokenizer metadata loading is complex
    return meta;
}

QStringList GGUFLoader::tensorNames() const
{
    return m_cachedTensorNames;
}
