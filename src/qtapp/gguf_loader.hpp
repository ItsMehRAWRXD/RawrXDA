#pragma once


#include <memory>

// Forward declaration of the real C++ loader
class GGUFLoader;

/**
 * @brief Qt-compatible wrapper for GGUF model file parsing and tensor loading
 * 
 * This class adapts the native C++ GGUFLoader to provide a Qt-friendly interface
 * with std::string, std::vector<uint8_t>, and Qt signal/slot support.
 */
class GGUFLoaderQt {
public:
    explicit GGUFLoaderQt(const std::string& path);
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
    std::any getParam(const std::string& key, const std::any& defaultValue) const;
    
    /**
     * @brief Load and decompress a tensor from the GGUF file
     * @param tensorName The tensor name (e.g., "blk.0.attn_q.weight")
     * @return The decompressed tensor data, or empty std::vector<uint8_t> on error
     */
    std::vector<uint8_t> inflateWeight(const std::string& tensorName);
    
    /**
     * @brief Get metadata for the tokenizer (if available)
     */
    std::unordered_map<std::string, std::vector<uint8_t>> getTokenizerMetadata() const;
    
    /**
     * @brief Get the list of all tensor names in the model
     */
    std::vector<std::string> tensorNames() const;

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
     * @return std::vector<std::string> with format: "IQ4_NL (type 39): 145 tensors", etc.
     */
    std::vector<std::string> getUnsupportedQuantizationInfo() const;

    /**
     * @brief Get the recommended conversion type
     * 
     * Returns the quantization type to convert to (e.g., "Q5_K")
     */
    std::string getRecommendedConversionType() const;

private:
    std::unique_ptr<GGUFLoader> m_loader;
    mutable std::unordered_map<std::string, std::any> m_metadataCache;
    mutable std::vector<std::string> m_cachedTensorNames;
    bool m_initialized{false};
    
    void initializeNativeLoader(const std::string& path);
};

