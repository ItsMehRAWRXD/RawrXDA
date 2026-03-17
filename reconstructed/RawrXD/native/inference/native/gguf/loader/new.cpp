#include "native_gguf_loader.h"
#include <iostream>
#include <cstring>
#include <algorithm>

NativeGGUFLoader::NativeGGUFLoader()
    : filepath_(""), is_open_(false), tensor_data_offset_(0)
{
    std::memset(&header_, 0, sizeof(NativeGGUFHeader));
}

NativeGGUFLoader::~NativeGGUFLoader() {
    Close();
}

bool NativeGGUFLoader::Open(const std::string& filepath) {
    filepath_ = filepath;
    file_.open(filepath.c_str(), std::ios::binary);
    if (!file_.is_open()) {
        return false;
    }

    is_open_ = true;
    tensors_.clear();
    tensor_index_.clear();
    metadata_.clear();

    try {
        if (!ParseHeader()) {
            Close();
            return false;
        }
        if (!ParseMetadata()) {
            Close();
            return false;
        }
        if (!ParseTensorInfo()) {
            Close();
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "GGUF parsing error: " << e.what() << std::endl;
        Close();
        return false;
    }

    return true;
}

bool NativeGGUFLoader::Close() {
    if (file_.is_open()) {
        file_.close();
    }
    is_open_ = false;
    tensors_.clear();
    tensor_index_.clear();
    metadata_.clear();
    return true;
}

bool NativeGGUFLoader::ParseHeader() {
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot parse header: file not open");
    }

    file_.seekg(0);

    // Read magic (GGUF)
    if (!ReadValue(header_.magic)) {
        throw std::runtime_error("Failed to read GGUF magic bytes");
    }
    if (header_.magic != 0x46554747) { // "GGUF" in little-endian
        throw std::runtime_error("Invalid GGUF magic: 0x" + std::to_string(header_.magic) +
                                 " (expected 0x46554747)");
    }

    // Read version
    if (!ReadValue(header_.version)) {
        throw std::runtime_error("Failed to read GGUF version");
    }
    if (header_.version != 3) {
        throw std::runtime_error("Unsupported GGUF version: " + std::to_string(header_.version) +
                                 " (only version 3 supported)");
    }

    // Read tensor count
    if (!ReadValue(header_.tensor_count)) {
        throw std::runtime_error("Failed to read tensor count");
    }

    // Read metadata count
    if (!ReadValue(header_.metadata_count)) {
        throw std::runtime_error("Failed to read metadata count");
    }

    std::cout << "GGUF Header: Magic=0x" << std::hex << header_.magic << std::dec
              << ", Version=" << header_.version
              << ", Tensors=" << header_.tensor_count
              << ", Metadata=" << header_.metadata_count << std::endl;

    return true;
}

bool NativeGGUFLoader::ParseMetadata() {
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot parse metadata: file not open");
    }
    if (header_.metadata_count == 0) {
        return true;
    }

    for (uint64_t i = 0; i < header_.metadata_count; ++i) {
        std::string key;
        if (!ReadString(key)) {
            throw std::runtime_error("Failed to read metadata key at index " + std::to_string(i));
        }

        uint32_t value_type;
        if (!ReadValue(value_type)) {
            throw std::runtime_error("Failed to read metadata value type for key: " + key);
        }

        NativeGGUFMetadata meta;
        meta.key = key;
        meta.value.type = value_type;

        switch (value_type) {
            case static_cast<uint32_t>(GGUFUtils::MetadataType::UINT8):
                if (!ReadValue(meta.value.primitive.u8)) throw std::runtime_error("Failed to read uint8 for key: " + key);
                break;
            case static_cast<uint32_t>(GGUFUtils::MetadataType::INT8):
                if (!ReadValue(meta.value.primitive.i8)) throw std::runtime_error("Failed to read int8 for key: " + key);
                break;
            case static_cast<uint32_t>(GGUFUtils::MetadataType::UINT16):
                if (!ReadValue(meta.value.primitive.u16)) throw std::runtime_error("Failed to read uint16 for key: " + key);
                break;
            case static_cast<uint32_t>(GGUFUtils::MetadataType::INT16):
                if (!ReadValue(meta.value.primitive.i16)) throw std::runtime_error("Failed to read int16 for key: " + key);
                break;
            case static_cast<uint32_t>(GGUFUtils::MetadataType::UINT32):
                if (!ReadValue(meta.value.primitive.u32)) throw std::runtime_error("Failed to read uint32 for key: " + key);
                break;
            case static_cast<uint32_t>(GGUFUtils::MetadataType::INT32):
                if (!ReadValue(meta.value.primitive.i32)) throw std::runtime_error("Failed to read int32 for key: " + key);
                break;
            case static_cast<uint32_t>(GGUFUtils::MetadataType::FLOAT32):
                if (!ReadValue(meta.value.primitive.f32)) throw std::runtime_error("Failed to read float32 for key: " + key);
                break;
            case static_cast<uint32_t>(GGUFUtils::MetadataType::UINT64):
                if (!ReadValue(meta.value.primitive.u64)) throw std::runtime_error("Failed to read uint64 for key: " + key);
                break;
            case static_cast<uint32_t>(GGUFUtils::MetadataType::INT64):
                if (!ReadValue(meta.value.primitive.i64)) throw std::runtime_error("Failed to read int64 for key: " + key);
                break;
            case static_cast<uint32_t>(GGUFUtils::MetadataType::FLOAT64):
                if (!ReadValue(meta.value.primitive.f64)) throw std::runtime_error("Failed to read float64 for key: " + key);
                break;
            case static_cast<uint32_t>(GGUFUtils::MetadataType::BOOL):
                if (!ReadValue(meta.value.primitive.b)) throw std::runtime_error("Failed to read bool for key: " + key);
                break;
            case static_cast<uint32_t>(GGUFUtils::MetadataType::STRING):
                if (!ReadString(meta.value.str)) throw std::runtime_error("Failed to read string for key: " + key);
                break;
            case static_cast<uint32_t>(GGUFUtils::MetadataType::ARRAY): {
                uint32_t arr_type;
                uint64_t arr_len;
                if (!ReadValue(arr_type) || !ReadValue(arr_len)) {
                    throw std::runtime_error("Failed to read array header for key: " + key);
                }
                if (!ReadArray(meta.value.arr, arr_type, arr_len)) {
                    throw std::runtime_error("Failed to read array data for key: " + key);
                }
                break;
            }
            default:
                throw std::runtime_error("Unsupported metadata type: " + std::to_string(value_type) + " for key: " + key);
        }

        metadata_[key] = std::move(meta);
    }

    return true;
}

bool NativeGGUFLoader::ParseTensorInfo() {
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot parse tensor info: file not open");
    }
    if (header_.tensor_count == 0) {
        return true;
    }

    for (uint64_t i = 0; i < header_.tensor_count; ++i) {
        NativeGGUFTensorInfo tensor;

        if (!ReadString(tensor.name)) {
            throw std::runtime_error("Failed to read tensor name at index " + std::to_string(i));
        }

        if (!ReadValue(tensor.n_dims)) {
            throw std::runtime_error("Failed to read dimension count for tensor: " + tensor.name);
        }

        if (tensor.n_dims > 8) {
            throw std::runtime_error("Tensor has too many dimensions (" + std::to_string(tensor.n_dims) +
                                   ") for tensor: " + tensor.name);
        }

        tensor.dims.resize(tensor.n_dims);
        for (uint32_t d = 0; d < tensor.n_dims; ++d) {
            if (!ReadValue(tensor.dims[d])) {
                throw std::runtime_error("Failed to read dimension " + std::to_string(d) +
                                       " for tensor: " + tensor.name);
            }
        }

        uint32_t type_int;
        if (!ReadValue(type_int)) {
            throw std::runtime_error("Failed to read type for tensor: " + tensor.name);
        }
        tensor.type = static_cast<QuantType>(type_int);

        if (!ReadValue(tensor.offset)) {
            throw std::runtime_error("Failed to read offset for tensor: " + tensor.name);
        }

        // Calculate size
        tensor.size_bytes = CalculateTensorSize(tensor);

        tensors_.push_back(std::move(tensor));
        tensor_index_[tensors_.back().name] = tensors_.size() - 1;
    }

    // Calculate tensor data offset (after tensor info, aligned to 32 bytes)
    uint64_t current_pos = file_.tellg();
    uint64_t padding = (alignment_ - (current_pos % alignment_)) % alignment_;
    tensor_data_offset_ = current_pos + padding;

    return true;
}

size_t NativeGGUFLoader::CalculateTensorSize(const NativeGGUFTensorInfo& tensor) const {
    uint64_t total_elements = 1;
    for (auto dim : tensor.dims) {
        total_elements *= dim;
    }

    size_t element_size = GGUFUtils::GetDataTypeSize(static_cast<GGUFUtils::DataType>(tensor.type));
    return total_elements * element_size;
}

const NativeGGUFTensorInfo* NativeGGUFLoader::GetTensor(const std::string& name) const {
    auto it = tensor_index_.find(name);
    if (it == tensor_index_.end()) {
        return nullptr;
    }
    return &tensors_[it->second];
}

bool NativeGGUFLoader::LoadTensorData(const std::string& name, void* buffer, size_t buffer_size) {
    const auto* tensor = GetTensor(name);
    if (!tensor) return false;
    return LoadTensorData(*tensor, buffer, buffer_size);
}

bool NativeGGUFLoader::LoadTensorData(const NativeGGUFTensorInfo& tensor, void* buffer, size_t buffer_size) {
    if (!file_.is_open() || buffer_size < tensor.size_bytes) {
        return false;
    }

    file_.seekg(tensor_data_offset_ + tensor.offset);
    return file_.read(static_cast<char*>(buffer), tensor.size_bytes).good();
}

uint64_t NativeGGUFLoader::GetFileSize() const {
    if (!file_.is_open()) return 0;
    auto pos = file_.tellg();
    file_.seekg(0, std::ios::end);
    uint64_t size = file_.tellg();
    file_.seekg(pos);
    return size;
}

template<typename T>
bool NativeGGUFLoader::ReadValue(T& value) {
    return file_.read(reinterpret_cast<char*>(&value), sizeof(T)).good();
}

bool NativeGGUFLoader::ReadString(std::string& str) {
    uint64_t length;
    if (!ReadValue(length)) {
        return false;
    }

    str.resize(length);
    return file_.read(&str[0], length).good();
}

bool NativeGGUFLoader::ReadArray(std::vector<uint8_t>& arr, uint32_t element_type, uint64_t length) {
    size_t element_size = 1; // Default for bytes
    switch (element_type) {
        case static_cast<uint32_t>(GGUFUtils::MetadataType::UINT8):
        case static_cast<uint32_t>(GGUFUtils::MetadataType::INT8): element_size = 1; break;
        case static_cast<uint32_t>(GGUFUtils::MetadataType::UINT16):
        case static_cast<uint32_t>(GGUFUtils::MetadataType::INT16): element_size = 2; break;
        case static_cast<uint32_t>(GGUFUtils::MetadataType::UINT32):
        case static_cast<uint32_t>(GGUFUtils::MetadataType::INT32):
        case static_cast<uint32_t>(GGUFUtils::MetadataType::FLOAT32): element_size = 4; break;
        case static_cast<uint32_t>(GGUFUtils::MetadataType::UINT64):
        case static_cast<uint32_t>(GGUFUtils::MetadataType::INT64):
        case static_cast<uint32_t>(GGUFUtils::MetadataType::FLOAT64): element_size = 8; break;
        default: return false; // Unsupported array type
    }

    size_t total_size = length * element_size;
    arr.resize(total_size);
    return file_.read(reinterpret_cast<char*>(arr.data()), total_size).good();
}

std::string NativeGGUFLoader::GetMetadataString(const std::string& key) const {
    auto it = metadata_.find(key);
    if (it != metadata_.end() && it->second.value.type == static_cast<uint32_t>(GGUFUtils::MetadataType::STRING)) {
        return it->second.value.str;
    }
    return "";
}

uint64_t NativeGGUFLoader::GetMetadataUInt64(const std::string& key) const {
    auto it = metadata_.find(key);
    if (it != metadata_.end() && it->second.value.type == static_cast<uint32_t>(GGUFUtils::MetadataType::UINT64)) {
        return it->second.value.primitive.u64;
    }
    return 0;
}

bool NativeGGUFLoader::GetMetadataBool(const std::string& key) const {
    auto it = metadata_.find(key);
    if (it != metadata_.end() && it->second.value.type == static_cast<uint32_t>(GGUFUtils::MetadataType::BOOL)) {
        return it->second.value.primitive.b;
    }
    return false;
}

// GGUFUtils implementation
namespace GGUFUtils {
    size_t GetDataTypeSize(DataType type) {
        switch (type) {
            case DataType::F32: return 4;
            case DataType::F16: case DataType::F16_HALF: return 2;
            case DataType::Q4_0: case DataType::Q4_1: case DataType::Q4_K: return 1;
            case DataType::Q5_K: case DataType::Q3_K: case DataType::Q2_K: case DataType::Q6_K: case DataType::Q8_0: case DataType::Q5_1: return 1;
            default: return 4;
        }
    }

    size_t GetBlockSize(DataType type) {
        switch (type) {
            case DataType::Q4_0: case DataType::Q4_1: return 32;
            case DataType::Q5_0: case DataType::Q5_1: return 32;
            case DataType::Q8_0: case DataType::Q8_1: return 32;
            case DataType::Q2_K: return 256;
            case DataType::Q3_K: return 256;
            case DataType::Q4_K: return 256;
            case DataType::Q5_K: return 256;
            case DataType::Q6_K: return 256;
            case DataType::Q8_K: return 256;
            default: return 1;
        }
    }

    bool IsQuantized(DataType type) {
        return type >= DataType::Q4_0 && type <= DataType::F16_HALF;
    }
}

// TerraForm-compatible C interface implementation
extern "C" {

struct NativeGGUFLoaderHandle {
    NativeGGUFLoader* loader;
};

native_gguf_loader_t native_gguf_loader_create() {
    return new NativeGGUFLoaderHandle{new NativeGGUFLoader()};
}

void native_gguf_loader_destroy(native_gguf_loader_t loader) {
    if (loader) {
        delete loader->loader;
        delete loader;
    }
}

bool native_gguf_loader_open(native_gguf_loader_t loader, const char* filepath) {
    if (!loader || !loader->loader) return false;
    return loader->loader->Open(filepath);
}

bool native_gguf_loader_close(native_gguf_loader_t loader) {
    if (!loader || !loader->loader) return false;
    return loader->loader->Close();
}

size_t native_gguf_loader_get_tensor_count(native_gguf_loader_t loader) {
    if (!loader || !loader->loader) return 0;
    return loader->loader->GetTensorCount();
}

bool native_gguf_loader_load_tensor(native_gguf_loader_t loader, const char* name, void* buffer, size_t buffer_size) {
    if (!loader || !loader->loader) return false;
    return loader->loader->LoadTensorData(name, buffer, buffer_size);
}

}