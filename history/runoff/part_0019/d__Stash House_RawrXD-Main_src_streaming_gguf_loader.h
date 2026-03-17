#pragma once

#include "gguf_loader.h"

class StreamingGGUFLoader : public IGGUFLoader {
public:
    StreamingGGUFLoader();
    ~StreamingGGUFLoader() override;

    bool Open(const std::string& filepath) override;
    bool Close() override;

    bool ParseHeader() override;
    GGUFHeader GetHeader() const override;

    bool ParseMetadata() override;
    GGUFMetadata GetMetadata() const override;

    std::vector<TensorInfo> GetTensorInfo() const override;
    bool LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) override;

    bool LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) override;
    size_t GetTensorByteSize(const TensorInfo& tensor) const override;
    std::string GetTypeString(GGMLType type) const override;
    uint64_t GetFileSize() const override;
    bool BuildTensorIndex() override;
    bool LoadZone(const std::string& zone_name, uint64_t max_memory_mb = 512) override;
    bool UnloadZone(const std::string& zone_name) override;
    std::vector<std::string> GetLoadedZones() const override;
    std::vector<std::string> GetAllZones() const override;
    std::vector<TensorInfo> GetAllTensorInfo() const override;
    uint64_t GetCurrentMemoryUsage() const override;
};

namespace RawrXD {
using ::StreamingGGUFLoader;
}
