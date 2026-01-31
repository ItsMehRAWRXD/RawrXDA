#pragma once


#include <memory>

// Qt wrapper for basic GGUF file parsing and tensor discovery
class GGUFLoader {
public:
    explicit GGUFLoader(const std::string& path);
    ~GGUFLoader();

    bool isOpen() const;
    std::any getParam(const std::string& key, const std::any& defaultValue) const;
    std::vector<uint8_t> inflateWeight(const std::string& tensorName);
    std::unordered_map<std::string, std::vector<uint8_t>> getTokenizerMetadata() const;
    std::vector<std::string> tensorNames() const;

private:
    std::string m_path;
    mutable std::unordered_map<std::string, std::any> m_metadataCache;
    mutable std::vector<std::string> m_cachedTensorNames;
    bool m_initialized{false};
    
    void initializeNativeLoader();
};

