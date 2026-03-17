#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <cstdint>
#include <variant>

// Basic types for GGUF
enum class GGMLType : uint32_t {
    F32  = 0,
    F16  = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    Q8_1 = 9,
    // Add others as needed
    COUNT 
};

struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t metadata_kv_count;
};

struct TensorInfo {
    std::string name;
    std::vector<uint64_t> shape;
    GGMLType type;
    uint64_t offset;
    size_t size; // Computed size in bytes
};

struct GGUFMetadata {
    std::map<std::string, std::string> kv_pairs; 
    // In a real implementation this would hold variant types
};

struct IGGUFLoader {
    virtual ~IGGUFLoader() = default;
    
    virtual bool Open(const std::string& filepath) = 0;
    virtual bool Close() = 0;
    
    virtual bool ParseHeader() = 0;
    virtual GGUFHeader GetHeader() const = 0;
    
    virtual bool ParseMetadata() = 0;
    virtual GGUFMetadata GetMetadata() const = 0;
    
    virtual std::vector<TensorInfo> GetTensorInfo() const = 0;
    virtual bool LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) = 0;
};

class GGUFLoader : public IGGUFLoader {
public:
    GGUFLoader();
    virtual ~GGUFLoader();
    
    bool Open(const std::string& filepath) override;
    bool Close() override;
    
    bool ParseHeader() override;
    GGUFHeader GetHeader() const override { return header_; }
    
    bool ParseMetadata() override; // Implemented in cpp
    GGUFMetadata GetMetadata() const override { return metadata_; }
    
    std::vector<TensorInfo> GetTensorInfo() const override { return tensors_; }
    bool LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) override;

    // Helper for subclasses or internal use
    template<typename T>
    bool ReadValue(T& val) {
        if (!file_.is_open()) return false;
        file_.read(reinterpret_cast<char*>(&val), sizeof(T));
        return file_.good();
    }

protected:
    std::string filepath_;
    std::ifstream file_;
    bool is_open_;
    
    GGUFHeader header_;
    GGUFMetadata metadata_;
    std::vector<TensorInfo> tensors_;
    std::vector<std::string> unsupported_types_;
};
