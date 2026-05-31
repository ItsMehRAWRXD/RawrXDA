#pragma once
#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

struct NativeGGUFTensorInfo {
    std::string name;
    std::vector<uint64_t> shape;
    uint32_t type = 0;
    uint64_t offset = 0;
};

struct NativeGGUFMetadata {
    std::string key;
    std::string value;
};

class NativeGGUFLoader {
public:
    NativeGGUFLoader() = default;
    ~NativeGGUFLoader() { Close(); }

    bool Open(const std::string& path) {
        if (path.empty()) return false;
        file_.open(path, std::ios::binary);
        if (!file_.is_open()) return false;
        filepath_ = path;
        return ParseHeader() && ParseMetadata() && ParseTensorInfo();
    }

    void Close() {
        if (file_.is_open()) file_.close();
        m_tensors.clear();
        m_metadata.clear();
        filepath_.clear();
    }

    bool ParseHeader() {
        if (!file_.is_open()) return false;
        // Read GGUF magic (0x46554747 = 'GGUF')
        uint32_t magic = 0;
        file_.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic != 0x46554747) return false;
        // Read version
        uint32_t version = 0;
        file_.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version < 2 || version > 3) return false;
        // Read tensor count and metadata count
        uint64_t tensor_count = 0, metadata_count = 0;
        file_.read(reinterpret_cast<char*>(&tensor_count), sizeof(tensor_count));
        file_.read(reinterpret_cast<char*>(&metadata_count), sizeof(metadata_count));
        tensor_count_ = tensor_count;
        metadata_count_ = metadata_count;
        return true;
    }

    bool ParseMetadata() {
        if (!file_.is_open()) return false;
        m_metadata.clear();
        for (uint64_t i = 0; i < metadata_count_; ++i) {
            // Read key length
            uint64_t key_len = 0;
            file_.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
            if (!file_) return false;
            // Read key
            std::string key(key_len, '\0');
            file_.read(&key[0], static_cast<std::streamsize>(key_len));
            if (!file_) return false;
            // Read value type
            uint32_t val_type = 0;
            file_.read(reinterpret_cast<char*>(&val_type), sizeof(val_type));
            if (!file_) return false;
            // Skip value (simplified: just store key)
            NativeGGUFMetadata meta;
            meta.key = key;
            meta.value = "parsed";
            m_metadata.push_back(std::move(meta));
        }
        return true;
    }

    bool ParseTensorInfo() {
        if (!file_.is_open()) return false;
        m_tensors.clear();
        for (uint64_t i = 0; i < tensor_count_; ++i) {
            // Read name length
            uint64_t name_len = 0;
            file_.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
            if (!file_) return false;
            // Read name
            std::string name(name_len, '\0');
            file_.read(&name[0], static_cast<std::streamsize>(name_len));
            if (!file_) return false;
            // Read dimensions
            uint32_t n_dims = 0;
            file_.read(reinterpret_cast<char*>(&n_dims), sizeof(n_dims));
            if (!file_) return false;
            std::vector<uint64_t> shape(n_dims);
            for (uint32_t d = 0; d < n_dims; ++d) {
                file_.read(reinterpret_cast<char*>(&shape[d]), sizeof(shape[d]));
            }
            // Read type
            uint32_t type = 0;
            file_.read(reinterpret_cast<char*>(&type), sizeof(type));
            // Read offset
            uint64_t offset = 0;
            file_.read(reinterpret_cast<char*>(&offset), sizeof(offset));

            NativeGGUFTensorInfo info;
            info.name = name;
            info.shape = shape;
            info.type = type;
            info.offset = offset;
            m_tensors.push_back(std::move(info));
        }
        return true;
    }

    bool IsMemoryMapped() const { return false; }
    uint64_t GetMappedSize() const { return 0; }

    const std::vector<NativeGGUFTensorInfo>& GetTensors() const { return m_tensors; }
    const std::vector<NativeGGUFMetadata>& GetMetadata() const { return m_metadata; }

private:
    std::ifstream file_;
    std::string filepath_;
    uint64_t tensor_count_ = 0;
    uint64_t metadata_count_ = 0;
    std::vector<NativeGGUFTensorInfo> m_tensors;
    std::vector<NativeGGUFMetadata> m_metadata;
};
