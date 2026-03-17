#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <memory>
#include <functional>

// GGUF file format structures
struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t kv_count;
};

struct GGUFKV {
    std::string key;
    uint32_t type;
    std::vector<char> value;
};

struct GGUFModelInfo {
    std::string name;
    std::string file_path;
    size_t file_size;
    std::string architecture;
    uint64_t parameter_count;
    std::string quantization;
    std::string description;
    bool compressed;
    bool masm_compressed;
};

class GGUFModelLoader {
public:
    GGUFModelLoader();
    ~GGUFModelLoader();

    bool loadModel(const std::string& file_path);
    bool loadModelWithStreaming(const std::string& file_path, std::function<void(int progress)> progress_callback = nullptr);
    
    const GGUFModelInfo& getModelInfo() const { return m_model_info; }
    const std::vector<GGUFKV>& getMetadata() const { return m_metadata; }
    
    bool isCompressed() const { return m_model_info.compressed; }
    bool isMasmCompressed() const { return m_model_info.masm_compressed; }
    
    // Decompression methods
    std::vector<char> decompressZlib(const std::vector<char>& compressed_data);
    std::vector<char> decompressMasm(const std::vector<char>& compressed_data);

private:
    bool parseGGUFHeader(std::ifstream& file);
    bool parseGGUFMetadata(std::ifstream& file);
    bool parseGGUFTensors(std::ifstream& file);
    
    GGUFModelInfo m_model_info;
    std::vector<GGUFKV> m_metadata;
    std::vector<std::vector<char>> m_tensor_data;
};

class ModelBrowser {
public:
    ModelBrowser();
    
    void scanDirectory(const std::string& directory_path);
    std::vector<GGUFModelInfo> getModels() const { return m_models; }
    
    bool loadModel(const std::string& model_name);
    GGUFModelLoader& getCurrentLoader() { return m_current_loader; }
    
private:
    std::vector<GGUFModelInfo> m_models;
    GGUFModelLoader m_current_loader;
};

class InferenceEngine {
public:
    InferenceEngine();
    
    bool initialize(const GGUFModelLoader& loader);
    std::string generate(const std::string& prompt, int max_tokens = 256);
    
private:
    std::unique_ptr<GGUFModelLoader> m_loader;
    // Inference state would go here
};