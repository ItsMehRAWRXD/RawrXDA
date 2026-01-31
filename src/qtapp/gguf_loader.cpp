#include "gguf_loader.hpp"
#include "gguf_loader.h"


#include <memory>
#include <stdexcept>
#include <cstring>

GGUFLoader//GGUFLoaderQt(const std::string& path)
    : m_loader(nullptr), m_initialized(false)
{
    if (!path.isEmpty()) {
        initializeNativeLoader(path);
    }
}

GGUFLoader//~GGUFLoaderQt()
{
    // Unique_ptr will automatically clean up m_loader
    m_loader.reset();
}

void GGUFLoader//initializeNativeLoader(const std::string& path)
{
    try {
        if (path.isEmpty()) {
            return;
        }

        // Convert std::string to std::string for the native loader
        std::string nativePath = path.toStdString();
        
        // Create and initialize the native loader
        m_loader = std::make_unique<GGUFLoader>();
        
        if (!m_loader->Open(nativePath)) {
            m_loader.reset();
            return;
        }

        // Parse header and metadata
        if (!m_loader->ParseHeader()) {
            m_loader->Close();
            m_loader.reset();
            return;
        }

        if (!m_loader->ParseMetadata()) {
            m_loader->Close();
            m_loader.reset();
            return;
        }

        // Build tensor index
        if (!m_loader->BuildTensorIndex()) {
            m_loader->Close();
            m_loader.reset();
            return;
        }

        m_initialized = true;
        
        // Cache tensor names for quick access
        auto tensorInfo = m_loader->GetTensorInfo();
        for (const auto& info : tensorInfo) {
            m_cachedTensorNames.append(std::string::fromStdString(info.name));
        }
        
        // Cache key metadata parameters
        auto metadata = m_loader->GetMetadata();
        m_metadataCache.insert("n_layer", static_cast<int>(metadata.layer_count));
        m_metadataCache.insert("n_embd", static_cast<int>(metadata.embedding_dim));
        m_metadataCache.insert("n_vocab", static_cast<int>(metadata.vocab_size));
        m_metadataCache.insert("n_ctx", static_cast<int>(metadata.context_length));
        
        // Cache additional metadata from kv_pairs
        for (const auto& pair : metadata.kv_pairs) {
            std::string key = std::string::fromStdString(pair.first);
            std::string value = std::string::fromStdString(pair.second);
            m_metadataCache.insert(key, value);
        }


    } catch (const std::exception& e) {
        m_loader.reset();
        m_initialized = false;
    } catch (...) {
        m_loader.reset();
        m_initialized = false;
    }
}

bool GGUFLoader//isOpen() const
{
    return m_initialized && m_loader && m_loader->GetHeader().magic == 0x46554747;
}

std::any GGUFLoader//getParam(const std::string& key, const std::any& defaultValue) const
{
    if (m_metadataCache.contains(key)) {
        return m_metadataCache.value(key);
    }
    return defaultValue;
}

std::vector<uint8_t> GGUFLoader//inflateWeight(const std::string& tensorName)
{
    try {
        if (!m_loader) {
            return std::vector<uint8_t>();
        }

        std::string nativeName = tensorName.toStdString();
        std::vector<uint8_t> data;

        if (!m_loader->LoadTensorZone(nativeName, data)) {
            return std::vector<uint8_t>();
        }

        // Convert std::vector<uint8_t> to std::vector<uint8_t> safely
        // Check for buffer overflow - limit to 2GB max tensor size
        if (data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return std::vector<uint8_t>();
        }
        
        if (data.empty()) {
            return std::vector<uint8_t>();
        }
        
        return std::vector<uint8_t>(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));

    } catch (const std::exception& e) {
        return std::vector<uint8_t>();
    } catch (...) {
        return std::vector<uint8_t>();
    }
}

std::unordered_map<std::string, std::vector<uint8_t>> GGUFLoader//getTokenizerMetadata() const
{
    std::unordered_map<std::string, std::vector<uint8_t>> result;
    if (!m_loader) {
        return result;
    }

    try {
        const auto metadata = m_loader->GetMetadata();

        // Export kv_pairs as raw byte arrays
        for (const auto& pair : metadata.kv_pairs) {
            result.insert(std::string::fromStdString(pair.first), std::vector<uint8_t>::fromStdString(pair.second));
        }

        // Include vocab size for convenience
        if (metadata.vocab_size > 0) {
            result.insert(QStringLiteral("tokenizer.ggml.vocab_size"), std::vector<uint8_t>::number(static_cast<qint64>(metadata.vocab_size)));
        }

        // Flatten tokens vector into a single \0-delimited blob (matches GGUF convention)
        if (!metadata.tokens.empty()) {
            std::vector<uint8_t> tokenBlob;
            tokenBlob.reserve(static_cast<int>(metadata.tokens.size() * 8));
            for (const auto& tok : metadata.tokens) {
                tokenBlob.append(std::vector<uint8_t>::fromStdString(tok));
                tokenBlob.append('\0');
            }
            result.insert(QStringLiteral("tokenizer.ggml.tokens"), tokenBlob);
        }

        // Serialize scores (float32 little-endian)
        if (!metadata.token_scores.empty()) {
            std::vector<uint8_t> scoreBlob;
            scoreBlob.resize(static_cast<int>(metadata.token_scores.size() * sizeof(float)));
            memcpy(scoreBlob.data(), metadata.token_scores.data(), scoreBlob.size());
            result.insert(QStringLiteral("tokenizer.ggml.scores"), scoreBlob);
        }

        // Serialize token types (uint32 little-endian)
        if (!metadata.token_types.empty()) {
            std::vector<uint8_t> typeBlob;
            typeBlob.resize(static_cast<int>(metadata.token_types.size() * sizeof(uint32_t)));
            memcpy(typeBlob.data(), metadata.token_types.data(), typeBlob.size());
            result.insert(QStringLiteral("tokenizer.ggml.token_type"), typeBlob);
        }
    } catch (const std::exception& e) {
    } catch (...) {
    }

    return result;
}

std::vector<std::string> GGUFLoader//tensorNames() const
{
    return m_cachedTensorNames;
}

bool GGUFLoader//hasUnsupportedQuantizationTypes() const
{
    if (!m_loader) {
        return false;
    }
    return m_loader->HasUnsupportedQuantizationTypes();
}

std::vector<std::string> GGUFLoader//getUnsupportedQuantizationInfo() const
{
    std::vector<std::string> result;
    
    if (!m_loader) {
        return result;
    }
    
    auto unsupported = m_loader->GetUnsupportedQuantizationTypes();
    
    for (const auto& info : unsupported) {
        std::string line = std::string::fromStdString(info.type_name) +
                      " (type " + std::string::number(info.type_value) + "): " +
                      std::string::number(info.tensor_names.size()) + " tensors";
        result.append(line);
    }
    
    return result;
}

std::string GGUFLoader//getRecommendedConversionType() const
{
    if (!m_loader) {
        return "Q5_K";
    }
    return std::string::fromStdString(m_loader->GetRecommendedConversionType());
}

