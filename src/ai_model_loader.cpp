// AI Model Loader - GGUF Model Loading Implementation
// Parses GGUF file headers and loads model metadata

#include "ai_model_loader.h"
#include <fstream>
#include <iostream>
#include <cstring>

namespace AIModelLoader {

static const uint32_t GGUF_MAGIC = 0x46554747U; // "GGUF" little-endian (bytes 47 47 55 46)

class GGUFModelLoader : public IModelLoader {
public:
    GGUFModelLoader() = default;
    ~GGUFModelLoader() override { unloadModel(); }
    
    LoadResult loadModel(const std::string& path) override {
        LoadResult result;
        
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            result.errorMessage = "Cannot open file: " + path;
            return result;
        }
        
        // Read GGUF magic number
        uint32_t magic = 0;
        file.read(reinterpret_cast<char*>(&magic), 4);
        if (magic != GGUF_MAGIC) {
            result.errorMessage = "Not a valid GGUF file (bad magic)";
            return result;
        }
        
        // Read version
        uint32_t version = 0;
        file.read(reinterpret_cast<char*>(&version), 4);
        if (version < 2 || version > 3) {
            result.errorMessage = "Unsupported GGUF version: " + std::to_string(version);
            return result;
        }
        
        // Read tensor count and metadata KV count
        uint64_t tensorCount = 0, kvCount = 0;
        file.read(reinterpret_cast<char*>(&tensorCount), 8);
        file.read(reinterpret_cast<char*>(&kvCount), 8);
        
        // Parse metadata KV pairs for model info
        m_info = ModelInfo{};
        m_info.name = path.substr(path.find_last_of("/\\") + 1);
        
        for (uint64_t i = 0; i < kvCount && file.good(); ++i) {
            // Read key string
            uint64_t keyLen = 0;
            file.read(reinterpret_cast<char*>(&keyLen), 8);
            if (keyLen > 4096) break; // Sanity check
            
            std::string key(keyLen, '\0');
            file.read(&key[0], keyLen);
            
            // Read value type
            uint32_t valueType = 0;
            file.read(reinterpret_cast<char*>(&valueType), 4);
            
            // Parse value based on type
            if (valueType == 4) { // UINT32
                uint32_t val = 0;
                file.read(reinterpret_cast<char*>(&val), 4);
                if (key.find("context_length") != std::string::npos) m_info.contextLength = val;
                else if (key.find("embedding_length") != std::string::npos) m_info.embeddingDim = val;
                else if (key.find("block_count") != std::string::npos) m_info.layerCount = val;
                else if (key.find("head_count") != std::string::npos) m_info.headCount = val;
                else if (key.find("vocab_size") != std::string::npos) m_info.vocabSize = val;
            } else if (valueType == 8) { // STRING
                uint64_t strLen = 0;
                file.read(reinterpret_cast<char*>(&strLen), 8);
                std::string val(strLen, '\0');
                file.read(&val[0], strLen);
                if (key.find("general.architecture") != std::string::npos) m_info.architecture = val;
                else if (key.find("general.name") != std::string::npos) m_info.name = val;
                else if (key.find("general.quantization") != std::string::npos) m_info.quantName = val;
            } else {
                // Skip unknown value types (simplified: seek past based on type size)
                if (valueType <= 1) file.seekg(1, std::ios::cur);       // UINT8/INT8
                else if (valueType <= 3) file.seekg(2, std::ios::cur);  // UINT16/INT16
                else if (valueType <= 5) file.seekg(4, std::ios::cur);  // UINT32/INT32/FLOAT32
                else if (valueType <= 7) file.seekg(8, std::ios::cur);  // UINT64/INT64/FLOAT64
                else if (valueType == 9) file.seekg(1, std::ios::cur);  // BOOL
                else {
                    // Array or complex type - skip to next KV (best effort)
                    break;
                }
            }
        }
        
        // Get file size
        file.seekg(0, std::ios::end);
        m_info.fileSizeBytes = file.tellg();
        file.close();
        
        // Estimate parameter count from file size and quantization
        if (m_info.fileSizeBytes > 0) {
            // Rough estimate: file_size / bytes_per_param
            double bytesPerParam = 2.0; // Default FP16
            if (m_info.quantName.find("Q4") != std::string::npos) bytesPerParam = 0.5;
            else if (m_info.quantName.find("Q5") != std::string::npos) bytesPerParam = 0.625;
            else if (m_info.quantName.find("Q8") != std::string::npos) bytesPerParam = 1.0;
            m_info.parameterCount = static_cast<uint64_t>(m_info.fileSizeBytes / bytesPerParam);
        }
        
        m_loaded = true;
        result.success = true;
        result.info = m_info;
        
        std::cout << "[ModelLoader] Loaded: " << m_info.name 
                  << " (" << m_info.architecture << ", " 
                  << m_info.layerCount << " layers, "
                  << (m_info.fileSizeBytes / (1024*1024)) << " MB)" << std::endl;
        
        return result;
    }
    
    void unloadModel() override {
        if (m_loaded) {
            m_loaded = false;
            m_info = ModelInfo{};
            std::cout << "[ModelLoader] Model unloaded" << std::endl;
        }
    }
    
    bool isLoaded() const override { return m_loaded; }
    ModelInfo getModelInfo() const override { return m_info; }
    
private:
    bool m_loaded = false;
    ModelInfo m_info;
};

std::unique_ptr<IModelLoader> createModelLoader() {
    return std::make_unique<GGUFModelLoader>();
}

} // namespace AIModelLoader

