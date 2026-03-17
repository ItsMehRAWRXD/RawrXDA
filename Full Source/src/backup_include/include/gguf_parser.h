#pragma once
#include <string>
#include <vector>
#include <expected>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace RawrXD {

enum class GGUFError {
    Success = 0,
    FileNotFound,
    InvalidMagic,
    VersionMismatch,
    CorruptedData,
    UnsupportedFeature,
    MemoryAllocationFailed
};

struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensorCount;
    uint64_t metadataCount;
};

struct GGUFTensorInfo {
    std::string name;
    std::vector<uint64_t> dimensions;
    uint32_t type;
    uint64_t offset;
    size_t size;
};

struct GGUFMetadata {
    std::string key;
    uint32_t type;
    std::vector<uint8_t> value;
};

struct GGUFModelData {
    GGUFHeader header;
    std::vector<GGUFTensorInfo> tensors;
    std::unordered_map<std::string, GGUFMetadata> metadata;
    std::vector<float> weights;
    std::vector<int> vocab;
    size_t totalSize;
    std::string modelName;
    std::string modelArchitecture;
};

class GGUFParser {
public:
    GGUFParser();
    ~GGUFParser() = default;
    
    // Real parsing
    std::expected<GGUFModelData, GGUFError> parse(const std::string& filePath);
    
    // Real metadata extraction
    std::expected<std::string, GGUFError> getMetadataString(
        const GGUFModelData& model,
        const std::string& key
    );
    
    std::expected<int32_t, GGUFError> getMetadataInt(
        const GGUFModelData& model,
        const std::string& key
    );
    
    // Real weight extraction
    std::expected<std::vector<float>, GGUFError> extractWeights(
        const GGUFModelData& model,
        const std::string& tensorName
    );
    
    // Real vocab loading
    std::expected<std::vector<int>, GGUFError> loadVocabulary(
        const GGUFModelData& model
    );
    
    // Validation
    std::expected<void, GGUFError> validateModel(const GGUFModelData& model);
    
private:
    // Real binary reading
    std::expected<GGUFHeader, GGUFError> readHeader(std::ifstream& file);
    
    std::expected<std::vector<GGUFTensorInfo>, GGUFError> readTensorInfos(
        std::ifstream& file,
        uint64_t count
    );
    
    std::expected<std::unordered_map<std::string, GGUFMetadata>, GGUFError> readMetadata(
        std::ifstream& file,
        uint64_t count
    );
    
    // Real weight loading with memory mapping
    std::expected<std::vector<float>, GGUFError> loadWeights(
        const std::string& filePath,
        const std::vector<GGUFTensorInfo>& tensors
    );
    
    // Memory-mapped file support
    std::expected<void*, GGUFError> mapFile(const std::string& filePath);
    void unmapFile(void* mapping, size_t size);
    
    // Buffer management
    std::vector<uint8_t> m_readBuffer;
    static constexpr size_t READ_BUFFER_SIZE = 64 * 1024 * 1024; // 64MB
};

} // namespace RawrXD
