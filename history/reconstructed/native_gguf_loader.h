#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <variant>
#include <fstream>

struct NativeGGUFTensorInfo {
    std::string name;
    uint32_t dimensions;
    std::vector<uint64_t> shape;
    uint32_t type;
    uint64_t offset;
};

struct NativeGGUFMetadata {
    std::string key;
    std::variant<std::string, int64_t, float, bool, std::vector<uint8_t>> value;
};

class NativeGGUFLoader {
public:
    bool Open(const std::string& filePath);
    bool ParseHeader();
    bool ParseMetadata();
    bool ParseTensorInfo();
    bool LoadTensorData(const std::string& tensorName, std::vector<uint8_t>& data);
    const std::vector<NativeGGUFTensorInfo>& GetTensors() const;
    const std::vector<NativeGGUFMetadata>& GetMetadata() const;

private:
    std::ifstream file;
    uint32_t version;
    uint64_t metadataCount;
    uint64_t tensorCount;
    std::vector<NativeGGUFMetadata> metadata;
    std::vector<NativeGGUFTensorInfo> tensors;
};