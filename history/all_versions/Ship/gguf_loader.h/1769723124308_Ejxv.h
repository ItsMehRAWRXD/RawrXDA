#pragma once
// gguf_loader.h - Minimal stub for Win32 IDE build
// The actual GGUF loading is done via the native model bridge DLL

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace GGUF {

struct TensorInfo {
    std::string name;
    uint32_t type;
    std::vector<uint64_t> dimensions;
    uint64_t offset;
    uint64_t size;
};

struct ModelMetadata {
    std::string name;
    std::string architecture;
    uint64_t parameterCount;
    uint32_t vocabSize;
    uint32_t contextLength;
    std::map<std::string, std::string> properties;
};

struct ModelHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensorCount;
    uint64_t metadataCount;
};

// Interface for GGUF loader
class IGGUFLoader {
public:
    virtual ~IGGUFLoader() = default;
    virtual bool load(const std::string& filepath) = 0;
    virtual void unload() = 0;
    virtual bool isLoaded() const = 0;
    virtual std::string getModelInfo() const = 0;
    virtual const std::vector<TensorInfo>& getTensors() const = 0;
    virtual const ModelMetadata& getMetadata() const = 0;
};

class GGUFLoader : public IGGUFLoader {
public:
    GGUFLoader() = default;
    ~GGUFLoader() override = default;
    
    bool load(const std::string& filepath) override {
        m_filepath = filepath;
        m_loaded = true;
        return true;
    }
    
    void unload() override {
        m_loaded = false;
        m_filepath.clear();
    }
    
    
    bool isLoaded() const override { return m_loaded; }
    
    std::string getModelInfo() const override {
        return m_loaded ? "Model: " + m_filepath : "No model loaded";
    }
    
    const std::vector<TensorInfo>& getTensors() const override { return m_tensors; }
    const ModelMetadata& getMetadata() const override { return m_metadata; }
    
    bool loadTensorData(const std::string& tensorName, std::vector<uint8_t>& data) {
        // Stub - actual implementation via native bridge
        return false;
    }

private:
    std::string m_filepath;
    bool m_loaded = false;
    ModelHeader m_header{};
    ModelMetadata m_metadata{};
    std::vector<TensorInfo> m_tensors;
};

} // namespace GGUF

// Aliases for compatibility
using IGGUFLoader = GGUF::IGGUFLoader;
using TensorInfo = GGUF::TensorInfo;
using ModelMetadata = GGUF::ModelMetadata;
