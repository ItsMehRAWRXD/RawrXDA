#pragma once
// gguf_loader.h - Minimal stub for Win32 IDE build
// The actual GGUF loading is done via the native model bridge DLL

#include <string>
#include <vector>
#include <cstdint>

namespace GGUF {

struct TensorInfo {
    std::string name;
    uint32_t type;
    std::vector<uint64_t> dimensions;
    uint64_t offset;
    uint64_t size;
};

struct ModelHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensorCount;
    uint64_t metadataCount;
};

class GGUFLoader {
public:
    GGUFLoader() = default;
    ~GGUFLoader() = default;
    
    bool load(const std::string& filepath) {
        m_filepath = filepath;
        m_loaded = true;
        return true;
    }
    
    void unload() {
        m_loaded = false;
        m_filepath.clear();
    }
    
    bool isLoaded() const { return m_loaded; }
    
    std::string getModelInfo() const {
        return m_loaded ? "Model: " + m_filepath : "No model loaded";
    }
    
    const std::vector<TensorInfo>& getTensors() const { return m_tensors; }
    
    bool loadTensorData(const std::string& tensorName, std::vector<uint8_t>& data) {
        // Stub - actual implementation via native bridge
        return false;
    }

private:
    std::string m_filepath;
    bool m_loaded = false;
    ModelHeader m_header{};
    std::vector<TensorInfo> m_tensors;
};

} // namespace GGUF
