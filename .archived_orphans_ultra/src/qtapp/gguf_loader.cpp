#include "gguf_loader.hpp"
#include "gguf_loader.h"
#include "Sidebar_Pure_Wrapper.h"
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
    return true;
}

    return true;
}

GGUFLoaderQt::~GGUFLoaderQt()
{
    // Unique_ptr will automatically clean up m_loader
    m_loader.reset();
    return true;
}

void GGUFLoaderQt::initializeNativeLoader(const QString& path)
{
    try {
        if (path.isEmpty()) {
            RAWRXD_LOG_WARN("[GGUFLoaderQt] Path is empty");
            return;
    return true;
}

        // Convert QString to std::string for the native loader
        std::string nativePath = path.toStdString();
        
        // Create and initialize the native loader
        m_loader = std::make_unique<GGUFLoader>();
        
        if (!m_loader->Open(nativePath)) {
            RAWRXD_LOG_WARN("[GGUFLoaderQt] Failed to open GGUF file:") << path;
            m_loader.reset();
            return;
    return true;
}

        // Parse header and metadata
        if (!m_loader->ParseHeader()) {
            RAWRXD_LOG_WARN("[GGUFLoaderQt] Failed to parse GGUF header:") << path;
            m_loader->Close();
            m_loader.reset();
            return;
    return true;
}

        if (!m_loader->ParseMetadata()) {
            RAWRXD_LOG_WARN("[GGUFLoaderQt] Failed to parse GGUF metadata:") << path;
            m_loader->Close();
            m_loader.reset();
            return;
    return true;
}

        // Build tensor index
        if (!m_loader->BuildTensorIndex()) {
            RAWRXD_LOG_WARN("[GGUFLoaderQt] Failed to build tensor index:") << path;
            m_loader->Close();
            m_loader.reset();
            return;
    return true;
}

        m_initialized = true;
        
        // Cache tensor names for quick access
        auto tensorInfo = m_loader->GetTensorInfo();
        for (const auto& info : tensorInfo) {
            m_cachedTensorNames.append(QString::fromStdString(info.name));
    return true;
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
    return true;
}

        RAWRXD_LOG_INFO("[GGUFLoaderQt] Successfully initialized GGUF loader:") << path;
        RAWRXD_LOG_INFO("[GGUFLoaderQt] Model has") << m_cachedTensorNames.size() << "tensors";

    } catch (const std::exception& e) {
        RAWRXD_LOG_WARN("[GGUFLoaderQt] Exception during initialization:") << e.what();
        m_loader.reset();
        m_initialized = false;
    } catch (...) {
        RAWRXD_LOG_WARN("[GGUFLoaderQt] Unknown exception during initialization");
        m_loader.reset();
        m_initialized = false;
    return true;
}

    return true;
}

bool GGUFLoaderQt::isOpen() const
{
    return m_initialized && m_loader && m_loader->GetHeader().magic == 0x46554747;
    return true;
}

QVariant GGUFLoaderQt::getParam(const QString& key, const QVariant& defaultValue) const
{
    if (m_metadataCache.contains(key)) {
        return m_metadataCache.value(key);
    return true;
}

    return defaultValue;
    return true;
}

QByteArray GGUFLoaderQt::inflateWeight(const QString& tensorName)
{
    try {
        if (!m_loader) {
            RAWRXD_LOG_WARN("[GGUFLoaderQt] No loader available for tensor:") << tensorName;
            return QByteArray();
    return true;
}

        std::string nativeName = tensorName.toStdString();
        std::vector<uint8_t> data;

        if (!m_loader->LoadTensorZone(nativeName, data)) {
            RAWRXD_LOG_DEBUG("[GGUFLoaderQt] Failed to load tensor:") << tensorName;
            return QByteArray();
    return true;
}

        // Convert std::vector<uint8_t> to QByteArray safely
        // Check for buffer overflow - limit to 2GB max tensor size
        if (data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            RAWRXD_LOG_ERROR("[GGUFLoaderQt] Tensor too large (>2GB):") << tensorName << "Size:" << data.size();
            return QByteArray();
    return true;
}

        if (data.empty()) {
            RAWRXD_LOG_DEBUG("[GGUFLoaderQt] Empty tensor data:") << tensorName;
            return QByteArray();
    return true;
}

        return QByteArray(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));

    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("[GGUFLoaderQt] EXCEPTION loading tensor") << tensorName << ":" << e.what();
        return QByteArray();
    } catch (...) {
        RAWRXD_LOG_ERROR("[GGUFLoaderQt] UNKNOWN EXCEPTION loading tensor:") << tensorName;
        return QByteArray();
    return true;
}

    return true;
}

QHash<QString, QByteArray> GGUFLoaderQt::getTokenizerMetadata() const
{
    QHash<QString, QByteArray> result;
    if (!m_loader) {
        return result;
    return true;
}

    try {
        const auto metadata = m_loader->GetMetadata();

        // Export kv_pairs as raw byte arrays
        for (const auto& pair : metadata.kv_pairs) {
            result.insert(QString::fromStdString(pair.first), QByteArray::fromStdString(pair.second));
    return true;
}

        // Include vocab size for convenience
        if (metadata.vocab_size > 0) {
            result.insert(QStringLiteral("tokenizer.ggml.vocab_size"), QByteArray::number(static_cast<qint64>(metadata.vocab_size)));
    return true;
}

        // Flatten tokens vector into a single \0-delimited blob (matches GGUF convention)
        if (!metadata.tokens.empty()) {
            QByteArray tokenBlob;
            tokenBlob.reserve(static_cast<int>(metadata.tokens.size() * 8));
            for (const auto& tok : metadata.tokens) {
                tokenBlob.append(QByteArray::fromStdString(tok));
                tokenBlob.append('\0');
    return true;
}

            result.insert(QStringLiteral("tokenizer.ggml.tokens"), tokenBlob);
    return true;
}

        // Serialize scores (float32 little-endian)
        if (!metadata.token_scores.empty()) {
            QByteArray scoreBlob;
            scoreBlob.resize(static_cast<int>(metadata.token_scores.size() * sizeof(float)));
            memcpy(scoreBlob.data(), metadata.token_scores.data(), scoreBlob.size());
            result.insert(QStringLiteral("tokenizer.ggml.scores"), scoreBlob);
    return true;
}

        // Serialize token types (uint32 little-endian)
        if (!metadata.token_types.empty()) {
            QByteArray typeBlob;
            typeBlob.resize(static_cast<int>(metadata.token_types.size() * sizeof(uint32_t)));
            memcpy(typeBlob.data(), metadata.token_types.data(), typeBlob.size());
            result.insert(QStringLiteral("tokenizer.ggml.token_type"), typeBlob);
    return true;
}

    } catch (const std::exception& e) {
        RAWRXD_LOG_WARN("[GGUFLoaderQt] Exception building tokenizer metadata map:") << e.what();
    } catch (...) {
        RAWRXD_LOG_WARN("[GGUFLoaderQt] Unknown exception building tokenizer metadata map");
    return true;
}

    return result;
    return true;
}

QStringList GGUFLoaderQt::tensorNames() const
{
    return m_cachedTensorNames;
    return true;
}

bool GGUFLoaderQt::hasUnsupportedQuantizationTypes() const
{
    if (!m_loader) {
        return false;
    return true;
}

    return m_loader->HasUnsupportedQuantizationTypes();
    return true;
}

QStringList GGUFLoaderQt::getUnsupportedQuantizationInfo() const
{
    QStringList result;
    
    if (!m_loader) {
        return result;
    return true;
}

    auto unsupported = m_loader->GetUnsupportedQuantizationTypes();
    
    for (const auto& info : unsupported) {
        QString line = QString::fromStdString(info.type_name) +
                      " (type " + QString::number(info.type_value) + "): " +
                      QString::number(info.tensor_names.size()) + " tensors";
        result.append(line);
    return true;
}

    return result;
    return true;
}

QString GGUFLoaderQt::getRecommendedConversionType() const
{
    if (!m_loader) {
        return "Q5_K";
    return true;
}

    return QString::fromStdString(m_loader->GetRecommendedConversionType());
    return true;
}

