#pragma once
// Minimal GGUF writer for RawrXD "Model Lab" profiles.
// Supports: header + KV pairs + tensor metadata (optional) + 32-byte alignment.
// Intent: generate metadata-only GGUF overlays (no tensors) for behavior toggles.

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RawrXD::GGUFFactory
{

enum class GGUFValueType : uint32_t
{
    UINT8 = 0,
    INT8 = 1,
    UINT16 = 2,
    INT16 = 3,
    UINT32 = 4,
    INT32 = 5,
    FLOAT32 = 6,
    BOOL = 7,
    STRING = 8,
    ARRAY = 9,
    UINT64 = 10,
    INT64 = 11,
    FLOAT64 = 12,
};

struct GGUFTensorInfoMinimal
{
    std::string name;
    std::vector<int64_t> shape;
    uint32_t type = 0;    // ggml_rxd_type value
    uint64_t offset = 0;  // offset into data section (filled on write_complete)
};

class GGUFWriterMinimal
{
  public:
    GGUFWriterMinimal() = default;
    ~GGUFWriterMinimal() = default;

    void setVersion(uint32_t version) { m_version = version; }

    void addString(const std::string& key, const std::string& value);
    void addInt32(const std::string& key, int32_t value);
    void addUInt32(const std::string& key, uint32_t value);
    void addFloat32(const std::string& key, float value);
    void addBool(const std::string& key, bool value);

    void addTensorMeta(const std::string& name, const std::vector<int64_t>& shape, uint32_t type);

    bool writeMetadataOnly(const std::string& path);

    bool writeComplete(const std::string& path,
                       const std::vector<std::pair<std::string, std::vector<char>>>& tensorData);

    const std::string& lastError() const { return m_lastError; }

  private:
    bool writeHeader_(std::ofstream& file);
    bool writeKvPairs_(std::ofstream& file);
    bool writeTensorInfo_(std::ofstream& file);
    bool writeTensorData_(std::ofstream& file,
                          const std::vector<std::pair<std::string, std::vector<char>>>& tensorData);

    bool writeKvPair_(std::ofstream& file, const std::string& key, GGUFValueType type, const void* value);
    static void padToAlignment_(std::ofstream& file, size_t alignment);

  private:
    uint32_t m_version = 3;
    std::unordered_map<std::string, std::string> m_strings;
    std::unordered_map<std::string, int32_t> m_ints;
    std::unordered_map<std::string, uint32_t> m_uints;
    std::unordered_map<std::string, float> m_floats;
    std::unordered_map<std::string, bool> m_bools;
    std::vector<GGUFTensorInfoMinimal> m_tensors;
    std::string m_lastError;
};

}  // namespace RawrXD::GGUFFactory
