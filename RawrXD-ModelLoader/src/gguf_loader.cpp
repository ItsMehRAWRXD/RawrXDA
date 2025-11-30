#include "gguf_loader.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <iostream>

GGUFLoader::GGUFLoader() 
    : is_open_(false) {
    std::memset(&header_, 0, sizeof(GGUFHeader));
}

GGUFLoader::~GGUFLoader() {
    Close();
}

bool GGUFLoader::Open(const std::string& filepath) {
    filepath_ = filepath;
    file_.open(filepath, std::ios::binary);
    if (!file_.is_open()) {
        // Throw exception instead of just cerr, allowing caller to catch it
        throw std::runtime_error("Failed to open GGUF file: " + filepath);
    }
    
    is_open_ = true;
    if (!ParseHeader()) {
        Close();
        return false; // ParseHeader handles its own errors
    }
    
    return true;
}

bool GGUFLoader::Close() {
    if (file_.is_open()) {
        file_.close();
    }
    is_open_ = false;
    tensors_.clear();
    return true;
}

bool GGUFLoader::ParseHeader() {
    if (!file_.is_open()) return false;
    
    file_.seekg(0);
    
    // Read magic
    if (!ReadValue(header_.magic)) return false;
    if (header_.magic != 0x46554747) {  // "GGUF"
        std::cerr << "Invalid GGUF magic: 0x" << std::hex << header_.magic << std::endl;
        return false;
    }
    
    // Read version
    if (!ReadValue(header_.version)) return false;
    if (header_.version != 3) {
        std::cerr << "Unsupported GGUF version: " << header_.version << std::endl;
        return false;
    }
    
    // Read tensor count
    if (!ReadValue(header_.tensor_count)) return false;
    
    // Read metadata KV count
    if (!ReadValue(header_.metadata_kv_count)) return false;
    
    // Calculate metadata offset
    header_.metadata_offset = file_.tellg();
    
    std::cout << "GGUF Header: Magic=0x" << std::hex << header_.magic << std::dec 
              << ", Version=" << header_.version
              << ", Tensors=" << header_.tensor_count
              << ", Metadata=" << header_.metadata_kv_count << std::endl;
    
    return true;
}

bool GGUFLoader::ParseMetadata() {
    if (!file_.is_open() || header_.metadata_kv_count == 0) {
        return false;
    }
    
    file_.seekg(header_.metadata_offset);
    
    for (uint64_t i = 0; i < header_.metadata_kv_count; ++i) {
        std::string key, value;
        
        if (!ReadString(key)) {
            std::cerr << "Failed to read metadata key at index " << i << std::endl;
            return false;
        }
        
        uint32_t value_type;
        if (!ReadValue(value_type)) {
            std::cerr << "Failed to read metadata value type for key: " << key << std::endl;
            return false;
        }
        
        // Value type 1 = UTF-8 string
        if (value_type == 1) {
            if (!ReadString(value)) {
                std::cerr << "Failed to read metadata string value for key: " << key << std::endl;
                return false;
            }
            metadata_.kv_pairs[key] = value;
            
            // Parse important metadata
            if (key == "general.architecture") {
                if (value == "llama") metadata_.architecture_type = 1;
            } else if (key == "llama.block_count") {
                metadata_.layer_count = std::stoul(value);
            } else if (key == "llama.context_length") {
                metadata_.context_length = std::stoul(value);
            } else if (key == "llama.embedding_length") {
                metadata_.embedding_dim = std::stoul(value);
            } else if (key == "llama.vocab_size") {
                metadata_.vocab_size = std::stoul(value);
            }
        } else if (value_type == 4) {  // uint32
            uint32_t uint_val;
            if (!ReadValue(uint_val)) return false;
            metadata_.kv_pairs[key] = std::to_string(uint_val);
        } else if (value_type == 5) {  // int32
            int32_t int_val;
            if (!ReadValue(int_val)) return false;
            metadata_.kv_pairs[key] = std::to_string(int_val);
        } else if (value_type == 6) {  // float32
            float float_val;
            if (!ReadValue(float_val)) return false;
            metadata_.kv_pairs[key] = std::to_string(float_val);
        }
    }
    
    // Read tensor info
    for (uint64_t i = 0; i < header_.tensor_count; ++i) {
        TensorInfo tensor;
        
        if (!ReadString(tensor.name)) {
            std::cerr << "Failed to read tensor name at index " << i << std::endl;
            return false;
        }
        
        uint32_t n_dims;
        if (!ReadValue(n_dims)) return false;
        
        tensor.shape.resize(n_dims);
        for (uint32_t d = 0; d < n_dims; ++d) {
            if (!ReadValue(tensor.shape[d])) return false;
        }
        
        uint32_t type_val;
        if (!ReadValue(type_val)) return false;
        tensor.type = static_cast<GGMLType>(type_val);
        
        if (!ReadValue(tensor.offset)) return false;
        
        tensor.size_bytes = CalculateTensorSize(tensor.shape, tensor.type);
        tensors_.push_back(tensor);
    }
    
    std::cout << "Metadata parsed successfully. Layers: " << metadata_.layer_count
              << ", Context: " << metadata_.context_length
              << ", Embedding: " << metadata_.embedding_dim
              << ", Vocab: " << metadata_.vocab_size << std::endl;
    
    return true;
}

bool GGUFLoader::LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) {
    auto it = std::find_if(tensors_.begin(), tensors_.end(),
                          [&tensor_name](const TensorInfo& t) { return t.name == tensor_name; });
    
    if (it == tensors_.end()) {
        // Use an exception for a cleaner interface in a larger app
        throw std::runtime_error("Tensor not found: " + tensor_name);
    }
    
    data.resize(it->size_bytes);
    file_.seekg(it->offset);
    file_.read(reinterpret_cast<char*>(data.data()), it->size_bytes);
    
    if (!file_.good()) {
         throw std::runtime_error("Failed to read tensor data for: " + tensor_name);
    }
    
    return true;
}

bool GGUFLoader::LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) {
    if (start_idx + count > tensors_.size()) {
        throw std::out_of_range("Tensor range out of bounds");
    }
    
    size_t total_size = 0;
    for (size_t i = start_idx; i < start_idx + count; ++i) {
        total_size += tensors_[i].size_bytes;
    }
    
    data.resize(total_size);
    size_t offset = 0;
    
    for (size_t i = start_idx; i < start_idx + count; ++i) {
        file_.seekg(tensors_[i].offset);
        file_.read(reinterpret_cast<char*>(data.data() + offset), tensors_[i].size_bytes);
        if (!file_.good()) {
            throw std::runtime_error("Failed to read tensor range during bulk load.");
        }
        offset += tensors_[i].size_bytes;
    }
    
    return true;
}

size_t GGUFLoader::GetTensorByteSize(const TensorInfo& tensor) const {
    return CalculateTensorSize(tensor.shape, tensor.type);
}

std::string GGUFLoader::GetTypeString(GGMLType type) const {
    switch (type) {
        case GGMLType::F32: return "F32";
        case GGMLType::F16: return "F16";
        case GGMLType::Q4_0: return "Q4_0";
        case GGMLType::Q4_1: return "Q4_1";
        case GGMLType::Q4_K: return "Q4_K";
        case GGMLType::Q5_K: return "Q5_K";
        case GGMLType::Q3_K: return "Q3_K";
        case GGMLType::Q2_K: return "Q2_K";
        case GGMLType::Q6_K: return "Q6_K";
        case GGMLType::Q8_0: return "Q8_0";
        default: return "UNKNOWN";
    }
}

uint64_t GGUFLoader::GetFileSize() const {
    if (!file_.is_open()) return 0;
    
    std::streampos current = file_.tellg();
    file_.seekg(0, std::ios::end);
    uint64_t size = file_.tellg();
    file_.seekg(current);
    return size;
}

// Note: BuildTensorIndex, LoadZone, UnloadZone, GetLoadedZones, GetAllZones, 
//       GetAllTensorInfo, GetCurrentMemoryUsage are implemented inline in gguf_loader.h

template<typename T>
bool GGUFLoader::ReadValue(T& value) {
    file_.read(reinterpret_cast<char*>(&value), sizeof(T));
    return file_.good();
}

bool GGUFLoader::ReadString(std::string& value) {
    uint64_t len;
    if (!ReadValue(len)) return false;
    
    value.resize(len);
    file_.read(&value[0], len);
    return file_.good();
}

uint64_t GGUFLoader::CalculateTensorSize(const std::vector<uint64_t>& shape, GGMLType type) const {
    uint64_t num_elements = 1;
    for (uint64_t dim : shape) {
        num_elements *= dim;
    }
    
    // Calculate the size in bytes based on the GGML type
    switch (type) {
        case GGMLType::F32: return num_elements * 4;
        case GGMLType::F16: return num_elements * 2;
        // K-quantization sizes are complex but generally follow these patterns:
        case GGMLType::Q4_0: return num_elements / 2 + (num_elements / 16) * 2;
        case GGMLType::Q4_1: return num_elements / 2 + (num_elements / 16) * 2 + 4;
        case GGMLType::Q8_0: return num_elements + num_elements / 32;
        case GGMLType::Q2_K: return (num_elements / 16) * (2 + 256/8);
        case GGMLType::Q3_K: return (num_elements / 16) * (2 + 3 + 16/8);
        case GGMLType::Q4_K: return (num_elements / 16) * (2 + 4 + 16/8);
        case GGMLType::Q5_K: return (num_elements / 16) * (2 + 5 + 16/8);
        case GGMLType::Q6_K: return (num_elements / 16) * (2 + 6 + 16/8);
        default:
            // In an enterprise setting, failing hard on an unknown type is safer
            throw std::runtime_error("Unsupported GGMLType encountered for size calculation.");
    }
}
