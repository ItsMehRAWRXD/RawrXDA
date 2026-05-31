#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace weights
{

struct TensorMeta
{
    std::string name;
    std::vector<int64_t> shape;
    std::string dtype;
    size_t offset = 0;
    size_t size = 0;
};

class SafetensorsReader
{
  public:
    SafetensorsReader() = default;

    bool load(const std::string& path);
    bool load_from_memory(const uint8_t* data, size_t size);

    const TensorMeta* get_tensor_meta(const std::string& name) const;
    std::vector<std::string> get_tensor_names() const;

    const uint8_t* get_tensor_data_ptr(const std::string& name) const;
    bool copy_tensor_data(const std::string& name, void* dst, size_t dst_size) const;

    const std::string& get_metadata_json() const { return m_metadata_json; }
    const std::string& last_error() const { return m_last_error; }

  private:
    bool parse_metadata_json_(const std::string& json);
    static size_t dtype_size_(const std::string& dtype);

    std::string m_metadata_json;
    std::unordered_map<std::string, TensorMeta> m_tensors;

    std::vector<uint8_t> m_owned_data;
    const uint8_t* m_data_ptr = nullptr;
    size_t m_data_size = 0;

    std::string m_last_error;
};

}  // namespace weights
