#pragma once
#include <QString>
#include <QVariant>
#include <QByteArray>
#include <QHash>
#include <QStringList>
#include <memory>

// Forward declaration of the real C++ loader
class GGUFLoader;

/**
 * @brief Qt-compatible wrapper for GGUF model file parsing and tensor loading
 * 
 * This class adapts the native C++ GGUFLoader to provide a Qt-friendly interface
 * with QString, QByteArray, and Qt signal/slot support.
 */
class GGUFLoaderQt {
public:
    explicit GGUFLoaderQt(const QString& path);
    ~GGUFLoaderQt();

    /**
     * @brief Check if the GGUF file was successfully opened and parsed
     */
    bool isOpen() const;
    
    /**
     * @brief Get a metadata parameter from the GGUF file
     * @param key The parameter name (e.g., "n_layer", "n_embd")
     * @param defaultValue Default value if key not found
     */
    QVariant getParam(const QString& key, const QVariant& defaultValue) const;
    
    /**
     * @brief Load and decompress a tensor from the GGUF file
     * @param tensorName The tensor name (e.g., "blk.0.attn_q.weight")
     * @return The decompressed tensor data, or empty QByteArray on error
     */
    QByteArray inflateWeight(const QString& tensorName);
    
    /**
     * @brief Get metadata for the tokenizer (if available)
     */
    QHash<QString, QByteArray> getTokenizerMetadata() const;
    
    /**
     * @brief Get the list of all tensor names in the model
     */
    QStringList tensorNames() const;

    /**
     * @brief Check if model uses unsupported quantization types
     * 
     * This is used by the IDE's conversion workflow to detect when a model
     * needs quantization conversion (e.g., IQ4_NL → Q5_K)
     */
    bool hasUnsupportedQuantizationTypes() const;

    /**
     * @brief Get information about unsupported quantization types
     * 
     * Returns a list of type values that are not supported by the current GGML.
     * Used to prompt user for conversion in the IDE.
     * 
     * @return QStringList with format: "IQ4_NL (type 39): 145 tensors", etc.
     */
    QStringList getUnsupportedQuantizationInfo() const;

    /**
     * @brief Get the recommended conversion type
     * 
     * Returns the quantization type to convert to (e.g., "Q5_K")
     */
    QString getRecommendedConversionType() const;

private:
    std::unique_ptr<GGUFLoader> m_loader;
    mutable QHash<QString, QVariant> m_metadataCache;
    mutable QStringList m_cachedTensorNames;
    bool m_initialized{false};
    
    void initializeNativeLoader(const QString& path);
};


