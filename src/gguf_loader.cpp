#include "gguf_loader.h"

#include <fstream>
#include <algorithm>
#include <cstring>

// GGUF format constants
static constexpr uint32_t GGUF_MAGIC = 0x46554747; // "GGUF" in little-endian
static constexpr uint32_t GGUF_VERSION = 3;

// GGUF value types
enum class GGUFValueType : uint32_t {
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

GGUFLoader::GGUFLoader() : is_open_(false), fileSize(0), current_memory_usage_(0) {}

GGUFLoader::~GGUFLoader() {
    Close();
}

bool GGUFLoader::Open(const std::string& filepath) {
    filepath_ = filepath;
    file_.open(filepath, std::ios::binary);
    is_open_ = file_.is_open();
    
    if (is_open_) {
        // Get file size
        file_.seekg(0, std::ios::end);
        fileSize = static_cast<uint64_t>(file_.tellg());
        file_.seekg(0, std::ios::beg);
    }
    
    return is_open_;
}

bool GGUFLoader::Close() {
    if (file_.is_open()) {
        file_.close();
    }
    is_open_ = false;
    fileSize = 0;
    tensors_.clear();
    metadata_.properties.clear();
    metadata_.kv_pairs.clear();
    return true;
}

bool GGUFLoader::ParseHeader() {
    if (!is_open_ || !file_.is_open()) {
        return false;
    }
    
    file_.seekg(0, std::ios::beg);
    
    // Read magic
    uint32_t magic = 0;
    if (!ReadValue(magic)) {
        return false;
    }
    
    if (magic != GGUF_MAGIC) {
        // Try big-endian swap
        uint32_t swapped = ((magic >> 24) & 0xFF) | ((magic >> 8) & 0xFF00) | 
                          ((magic << 8) & 0xFF0000) | ((magic << 24) & 0xFF000000);
        if (swapped != GGUF_MAGIC) {
            return false;
        }
        magic = swapped;
    }
    
    header_.magic = magic;
    
    // Read version
    uint32_t version = 0;
    if (!ReadValue(version)) {
        return false;
    }
    header_.version = version;
    
    // Read tensor count
    uint64_t tensor_count = 0;
    if (!ReadValue(tensor_count)) {
        return false;
    }
    header_.tensor_count = tensor_count;
    
    // Read metadata kv count
    uint64_t metadata_kv_count = 0;
    if (!ReadValue(metadata_kv_count)) {
        return false;
    }
    header_.metadata_kv_count = metadata_kv_count;
    
    // Record metadata offset (current position)
    header_.metadata_offset = static_cast<uint64_t>(file_.tellg());
    
    return true;
}

bool GGUFLoader::ParseMetadata() {
    if (!is_open_ || !file_.is_open()) {
        return false;
    }
    
    // Seek to metadata offset
    file_.seekg(static_cast<std::streamoff>(header_.metadata_offset), std::ios::beg);
    
    // Parse metadata key-value pairs
    for (uint64_t i = 0; i < header_.metadata_kv_count; ++i) {
        std::string key;
        if (!ReadString(key)) {
            return false;
        }
        
        // Read value type
        uint32_t value_type_raw = 0;
        if (!ReadValue(value_type_raw)) {
            return false;
        }
        GGUFValueType value_type = static_cast<GGUFValueType>(value_type_raw);
        
        // Parse value based on type
        std::string value_str;
        if (!ParseMetadataValue(value_type, value_str)) {
            return false;
        }
        
        metadata_.properties[key] = value_str;
        metadata_.kv_pairs[key] = value_str;
        
        // Extract known metadata fields
        if (key == "general.architecture") {
            metadata_.architecture = value_str;
            metadata_.architecture_type = value_str;
        } else if (key == "general.name") {
            metadata_.name = value_str;
        } else if (key == "general.parameter_count") {
            metadata_.parameterCount = std::stoull(value_str);
        } else if (key == "tokenizer.ggml.vocab_size" || key == "llama.vocab_size") {
            metadata_.vocabSize = static_cast<uint32_t>(std::stoull(value_str));
            metadata_.vocab_size = metadata_.vocabSize;
        } else if (key == "llama.context_length" || key == "qwen2.context_length" || key == "qwen35.context_length") {
            metadata_.contextLength = static_cast<uint32_t>(std::stoull(value_str));
            metadata_.context_length = metadata_.contextLength;
        } else if (key == "llama.block_count" || key == "qwen2.block_count" || key == "qwen35.block_count") {
            metadata_.layer_count = static_cast<uint32_t>(std::stoull(value_str));
        } else if (key == "llama.embedding_length" || key == "qwen2.embedding_length" || key == "qwen35.embedding_length") {
            metadata_.embedding_dim = static_cast<uint32_t>(std::stoull(value_str));
        } else if (key == "llama.attention.head_count" || key == "qwen2.attention.head_count" || key == "qwen35.attention.head_count") {
            metadata_.head_count = static_cast<uint32_t>(std::stoull(value_str));
        } else if (key == "llama.attention.head_count_kv" || key == "qwen2.attention.head_count_kv" || key == "qwen35.attention.head_count_kv") {
            metadata_.head_count_kv = static_cast<uint32_t>(std::stoull(value_str));
        } else if (key == "llama.feed_forward_length" || key == "qwen2.feed_forward_length" || key == "qwen35.feed_forward_length") {
            metadata_.feed_forward_length = static_cast<uint32_t>(std::stoull(value_str));
        }
    }
    
    // Calculate tensor info offset
    tensor_info_offset = static_cast<uint64_t>(file_.tellg());
    
    // Parse tensor infos
    tensors_.clear();
    for (uint64_t i = 0; i < header_.tensor_count; ++i) {
        RawrXD::TensorInfo info;
        
        // Read tensor name
        if (!ReadString(info.name)) {
            return false;
        }
        
        // Read number of dimensions
        uint32_t n_dims = 0;
        if (!ReadValue(n_dims)) {
            return false;
        }
        
        // Read dimensions
        info.shape.resize(n_dims);
        for (uint32_t d = 0; d < n_dims; ++d) {
            uint64_t dim = 0;
            if (!ReadValue(dim)) {
                return false;
            }
            info.shape[d] = dim;
        }
        
        // Read tensor type
        uint32_t type_raw = 0;
        if (!ReadValue(type_raw)) {
            return false;
        }
        info.type = static_cast<RawrXD::GGMLType>(type_raw);
        
        // Read tensor offset
        if (!ReadValue(info.offset)) {
            return false;
        }
        
        // Calculate tensor size
        info.size = CalculateTensorSize(info.shape, info.type);
        info.size_bytes = info.size;
        info.loaded = false;
        info.data = nullptr;
        
        tensors_.push_back(info);
    }
    
    // Calculate data base offset (aligned to 32 bytes)
    data_base_offset = static_cast<uint64_t>(file_.tellg());
    data_base_offset = AlignTo32Bytes(data_base_offset);
    
    return true;
}

bool GGUFLoader::ParseMetadataValue(GGUFValueType type, std::string& out) {
    out.clear();
    
    switch (type) {
        case GGUFValueType::UINT8: {
            uint8_t val = 0;
            if (!ReadValue(val)) return false;
            out = std::to_string(val);
            break;
        }
        case GGUFValueType::INT8: {
            int8_t val = 0;
            if (!ReadValue(val)) return false;
            out = std::to_string(val);
            break;
        }
        case GGUFValueType::UINT16: {
            uint16_t val = 0;
            if (!ReadValue(val)) return false;
            out = std::to_string(val);
            break;
        }
        case GGUFValueType::INT16: {
            int16_t val = 0;
            if (!ReadValue(val)) return false;
            out = std::to_string(val);
            break;
        }
        case GGUFValueType::UINT32: {
            uint32_t val = 0;
            if (!ReadValue(val)) return false;
            out = std::to_string(val);
            break;
        }
        case GGUFValueType::INT32: {
            int32_t val = 0;
            if (!ReadValue(val)) return false;
            out = std::to_string(val);
            break;
        }
        case GGUFValueType::FLOAT32: {
            float val = 0.0f;
            if (!ReadValue(val)) return false;
            out = std::to_string(val);
            break;
        }
        case GGUFValueType::BOOL: {
            uint8_t val = 0;
            if (!ReadValue(val)) return false;
            out = val ? "true" : "false";
            break;
        }
        case GGUFValueType::STRING: {
            if (!ReadString(out)) return false;
            break;
        }
        case GGUFValueType::ARRAY: {
            // Read array type and count
            uint32_t arr_type = 0;
            uint64_t arr_count = 0;
            if (!ReadValue(arr_type)) return false;
            if (!ReadValue(arr_count)) return false;
            
            // Skip array data (we'll store count as string)
            out = "[" + std::to_string(arr_count) + " items]";
            
            // Skip the actual array data
            size_t item_size = GetValueTypeSize(static_cast<GGUFValueType>(arr_type));
            if (item_size == 0 && static_cast<GGUFValueType>(arr_type) == GGUFValueType::STRING) {
                // String array - skip each string
                for (uint64_t i = 0; i < arr_count; ++i) {
                    std::string dummy;
                    if (!ReadString(dummy)) return false;
                }
            } else {
                // Fixed-size array - seek past it
                uint64_t skip_bytes = arr_count * item_size;
                file_.seekg(static_cast<std::streamoff>(skip_bytes), std::ios::cur);
                if (!file_.good()) return false;
            }
            break;
        }
        case GGUFValueType::UINT64: {
            uint64_t val = 0;
            if (!ReadValue(val)) return false;
            out = std::to_string(val);
            break;
        }
        case GGUFValueType::INT64: {
            int64_t val = 0;
            if (!ReadValue(val)) return false;
            out = std::to_string(val);
            break;
        }
        case GGUFValueType::FLOAT64: {
            double val = 0.0;
            if (!ReadValue(val)) return false;
            out = std::to_string(val);
            break;
        }
        default:
            return false;
    }
    
    return true;
}

size_t GGUFLoader::GetValueTypeSize(GGUFValueType type) {
    switch (type) {
        case GGUFValueType::UINT8:
        case GGUFValueType::INT8:
        case GGUFValueType::BOOL:
            return 1;
        case GGUFValueType::UINT16:
        case GGUFValueType::INT16:
            return 2;
        case GGUFValueType::UINT32:
        case GGUFValueType::INT32:
        case GGUFValueType::FLOAT32:
            return 4;
        case GGUFValueType::UINT64:
        case GGUFValueType::INT64:
        case GGUFValueType::FLOAT64:
            return 8;
        case GGUFValueType::STRING:
            return 0; // Variable size
        case GGUFValueType::ARRAY:
            return 0; // Variable size
        default:
            return 0;
    }
}

bool GGUFLoader::VerifyIntegrity(std::string* reason) {
    if (reason) {
        reason->clear();
    }
    
    if (!is_open_) {
        if (reason) *reason = "File not open";
        return false;
    }
    
    if (header_.magic != GGUF_MAGIC) {
        if (reason) *reason = "Invalid GGUF magic";
        return false;
    }
    
    if (header_.version != GGUF_VERSION) {
        if (reason) *reason = "Unsupported GGUF version: " + std::to_string(header_.version);
        return false;
    }
    
    // Verify tensor offsets are within file bounds
    for (const auto& tensor : tensors_) {
        uint64_t tensor_end = data_base_offset + tensor.offset + tensor.size;
        if (tensor_end > fileSize) {
            if (reason) {
                *reason = "Tensor '" + tensor.name + "' spans beyond file end (" +
                         std::to_string(tensor_end) + " > " + std::to_string(fileSize) + ")";
            }
            return false;
        }
        
        // Verify tensor offset alignment
        uint64_t aligned_offset = AlignTo32Bytes(data_base_offset + tensor.offset);
        if (aligned_offset != data_base_offset + tensor.offset) {
            if (reason) {
                *reason = "Tensor '" + tensor.name + "' has unaligned offset";
            }
            return false;
        }
    }
    
    return true;
}

bool GGUFLoader::RepairTrivialIssues(std::string* report) {
    if (report) {
        report->clear();
    }
    
    bool repaired = false;
    
    // Check for zero-sized tensors and fix
    for (auto& tensor : tensors_) {
        if (tensor.size == 0 && !tensor.shape.empty()) {
            tensor.size = CalculateTensorSize(tensor.shape, tensor.type);
            tensor.size_bytes = tensor.size;
            repaired = true;
            if (report) {
                *report += "Fixed zero-size tensor: " + tensor.name + "\n";
            }
        }
    }
    
    return repaired;
}

bool GGUFLoader::LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) {
    if (!is_open_ || !file_.is_open()) {
        return false;
    }
    
    if (start_idx >= tensors_.size()) {
        return false;
    }
    
    count = std::min(count, tensors_.size() - start_idx);
    data.clear();
    
    for (size_t i = start_idx; i < start_idx + count; ++i) {
        const auto& tensor = tensors_[i];
        
        // Validate tensor span
        uint64_t tensor_start = data_base_offset + tensor.offset;
        uint64_t tensor_end = tensor_start + tensor.size;
        
        if (tensor_end > fileSize) {
            return false;
        }
        
        // Read tensor data
        file_.seekg(static_cast<std::streamoff>(tensor_start), std::ios::beg);
        if (!file_.good()) {
            return false;
        }
        
        size_t old_size = data.size();
        data.resize(old_size + tensor.size);
        file_.read(reinterpret_cast<char*>(data.data() + old_size), static_cast<std::streamsize>(tensor.size));
        
        if (!file_.good()) {
            return false;
        }
        
        tensors_[i].loaded = true;
    }
    
    return true;
}

bool GGUFLoader::GetTensorData(const std::string& tensor_name, std::vector<uint8_t>& data) {
    data.clear();
    
    if (!is_open_ || !file_.is_open()) {
        return false;
    }
    
    // Find tensor by name
    auto it = tensor_index_map_.find(tensor_name);
    if (it == tensor_index_map_.end()) {
        return false;
    }
    
    RawrXD::TensorInfo* info = it->second;
    if (!info) {
        return false;
    }
    
    // Validate tensor span
    uint64_t tensor_start = data_base_offset + info->offset;
    uint64_t tensor_end = tensor_start + info->size;
    
    if (tensor_end > fileSize) {
        return false;
    }
    
    if (tensor_start < data_base_offset) {
        return false;
    }
    
    // Read tensor data
    file_.seekg(static_cast<std::streamoff>(tensor_start), std::ios::beg);
    if (!file_.good()) {
        return false;
    }
    
    data.resize(info->size);
    file_.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(info->size));
    
    if (!file_.good()) {
        return false;
    }
    
    info->loaded = true;
    info->hostData = data;
    
    return true;
}

std::string GGUFLoader::GetTypeString(RawrXD::GGMLType type) const {
    switch (type) {
        case RawrXD::GGMLType::F32: return "F32";
        case RawrXD::GGMLType::F16: return "F16";
        case RawrXD::GGMLType::Q4_0: return "Q4_0";
        case RawrXD::GGMLType::Q4_1: return "Q4_1";
        case RawrXD::GGMLType::Q5_0: return "Q5_0";
        case RawrXD::GGMLType::Q5_1: return "Q5_1";
        case RawrXD::GGMLType::Q8_0: return "Q8_0";
        case RawrXD::GGMLType::Q2_K: return "Q2_K";
        case RawrXD::GGMLType::Q3_K: return "Q3_K";
        case RawrXD::GGMLType::Q4_K: return "Q4_K";
        case RawrXD::GGMLType::Q5_K: return "Q5_K";
        case RawrXD::GGMLType::Q6_K: return "Q6_K";
        default: return "UNKNOWN";
    }
}

bool GGUFLoader::BuildTensorIndex() {
    tensor_index_map_.clear();
    
    for (auto& tensor : tensors_) {
        tensor_index_map_[tensor.name] = &tensor;
    }
    
    return true;
}

bool GGUFLoader::LoadZone(const std::string& zone_name, uint64_t max_memory_mb) {
    (void)zone_name;
    (void)max_memory_mb;
    return true;
}

bool GGUFLoader::UnloadZone(const std::string& zone_name) {
    (void)zone_name;
    return true;
}

bool GGUFLoader::LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) {
    return GetTensorData(tensor_name, data);
}

uint64_t GGUFLoader::GetFileSize() const {
    return fileSize;
}

uint64_t GGUFLoader::GetCurrentMemoryUsage() const {
    return current_memory_usage_;
}

std::vector<std::string> GGUFLoader::GetLoadedZones() const {
    return loaded_zones_;
}

bool GGUFLoader::Load(VkDevice vkDevice, VkPhysicalDevice vkPhysDevice) {
    device = vkDevice;
    physDevice = vkPhysDevice;
    
    // Parse the full file
    if (!ParseHeader()) {
        return false;
    }
    
    if (!ParseMetadata()) {
        return false;
    }
    
    if (!BuildTensorIndex()) {
        return false;
    }
    
    std::string integrity_reason;
    if (!VerifyIntegrity(&integrity_reason)) {
        return false;
    }
    
    return true;
}

void GGUFLoader::CreateVulkanResources() {
    vulkanResourcesCreated_ = true;
}

bool GGUFLoader::SetCompressionType(CompressionType type) {
    compression_type_ = type;
    return true;
}

bool GGUFLoader::DecompressData(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    out = in;
    return true;
}

bool GGUFLoader::CompressData(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    out = in;
    return true;
}

bool GGUFLoader::HasUnsupportedQuantizationTypes() const {
    return !unsupported_types_structs_.empty();
}

std::vector<GGUFLoader::UnsupportedTypeInfo> GGUFLoader::GetUnsupportedQuantizationTypes() const {
    return unsupported_types_structs_;
}

std::string GGUFLoader::GetRecommendedConversionType() const {
    return "q4_k_m";
}

template <typename T>
bool GGUFLoader::ReadValue(T& val) {
    if (!file_.is_open()) {
        return false;
    }
    file_.read(reinterpret_cast<char*>(&val), sizeof(T));
    return file_.good();
}

bool GGUFLoader::ReadString(std::string& str) {
    str.clear();
    
    if (!file_.is_open()) {
        return false;
    }
    
    // Read string length (uint64_t in GGUF)
    uint64_t len = 0;
    if (!ReadValue(len)) {
        return false;
    }
    
    if (len == 0) {
        return true;
    }
    
    // Sanity check: string length shouldn't exceed remaining file size
    uint64_t current_pos = static_cast<uint64_t>(file_.tellg());
    if (current_pos + len > fileSize) {
        return false;
    }
    
    // Sanity check: string length shouldn't be unreasonably large
    if (len > 1024 * 1024) { // 1MB max string
        return false;
    }
    
    str.resize(len);
    file_.read(str.data(), static_cast<std::streamsize>(len));
    
    return file_.good();
}

size_t GGUFLoader::CalculateTensorSize(const std::vector<uint64_t>& shape, RawrXD::GGMLType type) const {
    if (shape.empty()) {
        return 0;
    }
    
    uint64_t elements = 1;
    for (uint64_t d : shape) {
        elements *= d;
    }
    
    // Calculate bytes per element based on type
    size_t bytes_per_element = 4; // Default F32
    switch (type) {
        case RawrXD::GGMLType::F32:
            bytes_per_element = 4;
            break;
        case RawrXD::GGMLType::F16:
            bytes_per_element = 2;
            break;
        case RawrXD::GGMLType::Q4_0:
            bytes_per_element = 18; // 32 elements per block, 18 bytes per block
            elements = (elements + 31) / 32; // Number of blocks
            break;
        case RawrXD::GGMLType::Q4_1:
            bytes_per_element = 20;
            elements = (elements + 31) / 32;
            break;
        case RawrXD::GGMLType::Q5_0:
            bytes_per_element = 22;
            elements = (elements + 31) / 32;
            break;
        case RawrXD::GGMLType::Q5_1:
            bytes_per_element = 24;
            elements = (elements + 31) / 32;
            break;
        case RawrXD::GGMLType::Q8_0:
            bytes_per_element = 34;
            elements = (elements + 31) / 32;
            break;
        case RawrXD::GGMLType::Q2_K:
            bytes_per_element = 84;
            elements = (elements + 255) / 256;
            break;
        case RawrXD::GGMLType::Q3_K:
            bytes_per_element = 110;
            elements = (elements + 255) / 256;
            break;
        case RawrXD::GGMLType::Q4_K:
            bytes_per_element = 144;
            elements = (elements + 255) / 256;
            break;
        case RawrXD::GGMLType::Q5_K:
            bytes_per_element = 176;
            elements = (elements + 255) / 256;
            break;
        case RawrXD::GGMLType::Q6_K:
            bytes_per_element = 210;
            elements = (elements + 255) / 256;
            break;
        default:
            bytes_per_element = 4;
            break;
    }
    
    return static_cast<size_t>(elements * bytes_per_element);
}

void GGUFLoader::LoadTensorAsync(RawrXD::TensorInfo& info) {
    (void)info;
}

void GGUFLoader::UploadF32(RawrXD::TensorInfo& info, void* src, size_t count) {
    (void)info;
    (void)src;
    (void)count;
}

void GGUFLoader::DequantAndUploadQ4_0(RawrXD::TensorInfo& info, void* src, size_t count) {
    (void)info;
    (void)src;
    (void)count;
}

void GGUFLoader::BeginCommandBuffer() {
    commandBufferActive_ = true;
}

void GGUFLoader::EndCommandBuffer() {
    commandBufferActive_ = false;
}

uint32_t GGUFLoader::FindMemoryType(uint32_t typeFilter, uint32_t props) {
    (void)typeFilter;
    (void)props;
    return 0;
}

uint32_t GGUFLoader::FindQueueFamilyIndex(VkPhysicalDevice device_, uint32_t queueFlags) {
    (void)device_;
    (void)queueFlags;
    return 0;
}
