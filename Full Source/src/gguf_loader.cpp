#include "gguf_loader.h"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <sstream>
#include <cmath>

// =====================================================================
// GGUFLoader — Real GGUF v3 Parser (Memory-Mapped, CPU-Only Path)
// =====================================================================

GGUFLoader::GGUFLoader()
    : is_open_(false), mappedView(nullptr), fileSize(0),
      hFile(INVALID_HANDLE_VALUE), hMapping(nullptr),
      compression_type_(CompressionType::NONE) {
    std::memset(&header_, 0, sizeof(header_));
}

GGUFLoader::~GGUFLoader() {
    Close();
}

// ---- ReadValue<T> template — binary read ----
template<typename T>
bool GGUFLoader::ReadValue(T& val) {
    if (!file_.is_open()) return false;
    file_.read(reinterpret_cast<char*>(&val), sizeof(T));
    return file_.good();
}

// Explicit instantiations to satisfy the linker
template bool GGUFLoader::ReadValue<uint8_t>(uint8_t&);
template bool GGUFLoader::ReadValue<int8_t>(int8_t&);
template bool GGUFLoader::ReadValue<uint16_t>(uint16_t&);
template bool GGUFLoader::ReadValue<int16_t>(int16_t&);
template bool GGUFLoader::ReadValue<uint32_t>(uint32_t&);
template bool GGUFLoader::ReadValue<int32_t>(int32_t&);
template bool GGUFLoader::ReadValue<uint64_t>(uint64_t&);
template bool GGUFLoader::ReadValue<int64_t>(int64_t&);
template bool GGUFLoader::ReadValue<float>(float&);
template bool GGUFLoader::ReadValue<double>(double&);

// ---- ReadString — length-prefixed UTF-8 ----
bool GGUFLoader::ReadString(std::string& value) {
    uint64_t len = 0;
    if (!ReadValue(len)) return false;
    const uint64_t MAX_STRING_SIZE = 100 * 1024 * 1024; // 100MB safety
    if (len > MAX_STRING_SIZE) return false;
    value.resize(static_cast<size_t>(len));
    file_.read(&value[0], len);
    return file_.good();
}

// ---- Open — ifstream + Win32 memory-map ----
bool GGUFLoader::Open(const std::string& filepath) {
    if (is_open_) Close();
    filepath_ = filepath;

    file_.open(filepath, std::ios::binary);
    if (!file_.is_open()) return false;

    // Win32 memory-mapped file for tensor data
    hFile = CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz;
        GetFileSizeEx(hFile, &sz);
        fileSize = static_cast<uint64_t>(sz.QuadPart);
        hMapping = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (hMapping) {
            mappedView = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        }
    }

    is_open_ = true;
    return true;
}

bool GGUFLoader::Close() {
    if (mappedView) { UnmapViewOfFile(mappedView); mappedView = nullptr; }
    if (hMapping) { CloseHandle(hMapping); hMapping = nullptr; }
    if (hFile != INVALID_HANDLE_VALUE) { CloseHandle(hFile); hFile = INVALID_HANDLE_VALUE; }
    if (file_.is_open()) file_.close();
    is_open_ = false;
    return true;
}

// ---- ParseHeader — GGUF v3 header ----
bool GGUFLoader::ParseHeader() {
    if (!file_.is_open()) return false;
    file_.seekg(0);

    if (!ReadValue(header_.magic)) return false;
    // GGUF magic: 0x46475547 = "GGUF"
    if (header_.magic != 0x46475547) return false;
    if (!ReadValue(header_.version)) return false;
    if (header_.version < 2 || header_.version > 3) return false;
    if (!ReadValue(header_.tensor_count)) return false;
    if (!ReadValue(header_.metadata_kv_count)) return false;
    header_.metadata_offset = static_cast<uint64_t>(file_.tellg());
    return true;
}

// ---- ParseMetadata — reads all KV pairs from the GGUF metadata section ----
bool GGUFLoader::ParseMetadata() {
    if (!file_.is_open()) return false;
    if (header_.metadata_kv_count == 0) return true;

    file_.seekg(header_.metadata_offset);

    for (uint64_t i = 0; i < header_.metadata_kv_count; ++i) {
        std::string key;
        if (!ReadString(key)) return false;

        uint32_t value_type = 0;
        if (!ReadValue(value_type)) return false;

        // GGUF value types: 0=U8 1=I8 2=U16 3=I16 4=U32 5=I32 6=F32 7=Bool 8=String 9=Array 10=U64 11=I64 12=F64
        switch (value_type) {
            case 0: { uint8_t v;  ReadValue(v); metadata_.kv_pairs[key] = std::to_string(v); break; }
            case 1: { int8_t v;   ReadValue(v); metadata_.kv_pairs[key] = std::to_string(v); break; }
            case 2: { uint16_t v; ReadValue(v); metadata_.kv_pairs[key] = std::to_string(v); break; }
            case 3: { int16_t v;  ReadValue(v); metadata_.kv_pairs[key] = std::to_string(v); break; }
            case 4: { uint32_t v; ReadValue(v); metadata_.kv_pairs[key] = std::to_string(v);
                // Capture well-known uint32 metadata
                if (key.find("vocab_size") != std::string::npos) { metadata_.vocabSize = v; metadata_.vocab_size = v; }
                if (key.find("context_length") != std::string::npos) { metadata_.contextLength = v; metadata_.context_length = v; }
                if (key.find("block_count") != std::string::npos) metadata_.layer_count = v;
                if (key.find("embedding_length") != std::string::npos) metadata_.embedding_dim = v;
                if (key.find("head_count") != std::string::npos && key.find("head_count_kv") == std::string::npos) metadata_.head_count = v;
                break;
            }
            case 5: { int32_t v;  ReadValue(v); metadata_.kv_pairs[key] = std::to_string(v); break; }
            case 6: { float v;    ReadValue(v); metadata_.kv_pairs[key] = std::to_string(v); break; }
            case 7: { uint8_t v;  ReadValue(v); metadata_.kv_pairs[key] = (v ? "true" : "false"); break; }
            case 8: { std::string v; ReadString(v); metadata_.kv_pairs[key] = v;
                if (key == "general.name") metadata_.name = v;
                if (key == "general.architecture") { metadata_.architecture = v; metadata_.architecture_type = v; }
                break;
            }
            case 9: { // Array
                uint32_t arr_type = 0; ReadValue(arr_type);
                uint64_t arr_len = 0; ReadValue(arr_len);
                const uint64_t MAX_ARRAY = 10 * 1024 * 1024;
                if (arr_len > MAX_ARRAY) return false;

                bool is_token_list = (key == "tokenizer.ggml.tokens");
                bool is_score_list = (key == "tokenizer.ggml.scores");
                bool is_type_list  = (key == "tokenizer.ggml.token_type");

                for (uint64_t j = 0; j < arr_len; ++j) {
                    switch (arr_type) {
                        case 0: { uint8_t v; ReadValue(v); break; }
                        case 1: { int8_t v;  ReadValue(v); break; }
                        case 2: { uint16_t v; ReadValue(v); break; }
                        case 3: { int16_t v;  ReadValue(v); break; }
                        case 4: { uint32_t v; ReadValue(v);
                            if (is_type_list) { metadata_.token_types_u32.push_back(v); metadata_.token_types.push_back(static_cast<int32_t>(v)); }
                            break;
                        }
                        case 5: { int32_t v;  ReadValue(v);
                            if (is_type_list) { metadata_.token_types.push_back(v); metadata_.token_types_u32.push_back(static_cast<uint32_t>(v)); }
                            break;
                        }
                        case 6: { float v;    ReadValue(v);
                            if (is_score_list) metadata_.token_scores.push_back(v);
                            break;
                        }
                        case 7: { uint8_t v;  ReadValue(v); break; }
                        case 8: { std::string v; ReadString(v);
                            if (is_token_list) metadata_.tokens.push_back(v);
                            break;
                        }
                        case 10: { uint64_t v; ReadValue(v); break; }
                        case 11: { int64_t v;  ReadValue(v); break; }
                        case 12: { double v;   ReadValue(v); break; }
                        default: return false;
                    }
                }
                metadata_.kv_pairs[key] = "[array:" + std::to_string(arr_len) + "]";
                break;
            }
            case 10: { uint64_t v; ReadValue(v); metadata_.kv_pairs[key] = std::to_string(v); break; }
            case 11: { int64_t v;  ReadValue(v); metadata_.kv_pairs[key] = std::to_string(v); break; }
            case 12: { double v;   ReadValue(v); metadata_.kv_pairs[key] = std::to_string(v); break; }
            default: return false;
        }
        metadata_.properties = metadata_.kv_pairs;
    }

    // Populate vocabSize from token list if not set via metadata keys
    if (metadata_.vocab_size == 0 && !metadata_.tokens.empty()) {
        metadata_.vocab_size = static_cast<uint32_t>(metadata_.tokens.size());
        metadata_.vocabSize = metadata_.vocab_size;
    }

    // ---- Read tensor info descriptors ----
    tensors_.clear();
    tensors_.reserve(static_cast<size_t>(header_.tensor_count));
    for (uint64_t t = 0; t < header_.tensor_count; ++t) {
        RawrXD::TensorInfo ti;
        if (!ReadString(ti.name)) return false;
        uint32_t n_dims = 0;
        if (!ReadValue(n_dims)) return false;
        ti.shape.resize(n_dims);
        for (uint32_t d = 0; d < n_dims; ++d) {
            uint64_t dim = 0;
            if (!ReadValue(dim)) return false;
            ti.shape[d] = dim;
        }
        uint32_t type_val = 0;
        if (!ReadValue(type_val)) return false;
        ti.type = static_cast<RawrXD::GGMLType>(type_val);
        if (!ReadValue(ti.offset)) return false;
        ti.size = CalculateTensorSize(ti.shape, ti.type);
        ti.size_bytes = ti.size;
        tensors_.push_back(std::move(ti));
    }

    return true;
}

// ---- CalculateTensorSize — bytes for a tensor given shape + quant type ----
size_t GGUFLoader::CalculateTensorSize(const std::vector<uint64_t>& shape, RawrXD::GGMLType type) const {
    uint64_t num_elements = 1;
    for (auto d : shape) num_elements *= d;

    auto block_aligned_size = [&](uint64_t block_size, uint64_t bytes_per_block) -> size_t {
        uint64_t num_blocks = (num_elements + block_size - 1) / block_size;
        return static_cast<size_t>(num_blocks * bytes_per_block);
    };

    switch (type) {
        case GGMLType::F32:  return static_cast<size_t>(num_elements * 4);
        case GGMLType::F16:  return static_cast<size_t>(num_elements * 2);
        case GGMLType::Q4_0: return block_aligned_size(32, 18);  // 32 elements, 2+16 bytes
        case GGMLType::Q4_1: return block_aligned_size(32, 20);
        case GGMLType::Q5_0: return block_aligned_size(32, 22);
        case GGMLType::Q5_1: return block_aligned_size(32, 24);
        case GGMLType::Q8_0: return block_aligned_size(32, 34);
        case GGMLType::Q8_1: return block_aligned_size(32, 40);
        case GGMLType::Q2_K: return block_aligned_size(256, 84);
        case GGMLType::Q3_K: return block_aligned_size(256, 110);
        case GGMLType::Q4_K: return block_aligned_size(256, 144);
        case GGMLType::Q5_K: return block_aligned_size(256, 176);
        case GGMLType::Q6_K: return block_aligned_size(256, 210);
        case GGMLType::Q8_K: return block_aligned_size(256, 292);
        default: return static_cast<size_t>(num_elements * 4);
    }
}

// ---- LoadTensorRange — read raw tensor bytes from mmap ----
bool GGUFLoader::LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) {
    if (!mappedView || start_idx >= tensors_.size()) return false;
    size_t end_idx = (std::min)(start_idx + count, tensors_.size());

    // Calculate data region start (after header+metadata+tensor_info)
    // In GGUF, tensor data is at the end of file, tensors_.offset is relative to data start
    // We need to find the alignment boundary
    uint64_t data_start = static_cast<uint64_t>(file_.tellg());
    // Align to 32 bytes
    data_start = (data_start + 31) & ~uint64_t(31);

    size_t total_size = 0;
    for (size_t i = start_idx; i < end_idx; ++i) {
        total_size += static_cast<size_t>(tensors_[i].size);
    }
    data.resize(total_size);

    size_t write_pos = 0;
    const uint8_t* base = static_cast<const uint8_t*>(mappedView);
    for (size_t i = start_idx; i < end_idx; ++i) {
        uint64_t abs_offset = data_start + tensors_[i].offset;
        if (abs_offset + tensors_[i].size > fileSize) return false;
        std::memcpy(data.data() + write_pos, base + abs_offset, static_cast<size_t>(tensors_[i].size));
        write_pos += static_cast<size_t>(tensors_[i].size);
    }
    return true;
}

bool GGUFLoader::LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) {
    for (size_t i = 0; i < tensors_.size(); ++i) {
        if (tensors_[i].name == tensor_name) {
            return LoadTensorRange(i, 1, data);
        }
    }
    return false;
}

uint64_t GGUFLoader::GetFileSize() const {
    return fileSize;
}

// ---- Vulkan Path (guarded) ----
bool GGUFLoader::Load(VkDevice vkDevice, VkPhysicalDevice vkPhysDevice) {
#ifdef RAWR_ENABLE_VULKAN
    device = vkDevice;
    physDevice = vkPhysDevice;
    CreateVulkanResources();
    return true;
#else
    (void)vkDevice; (void)vkPhysDevice;
    return false;
#endif
}

void GGUFLoader::CreateVulkanResources() {
#ifdef RAWR_ENABLE_VULKAN
    // Real Vulkan init would go here
#endif
}

void GGUFLoader::LoadTensorAsync(RawrXD::TensorInfo& info) {
    (void)info;
}

void GGUFLoader::UploadF32(RawrXD::TensorInfo& info, void* src, size_t count) {
    (void)info; (void)src; (void)count;
}

void GGUFLoader::DequantAndUploadQ4_0(RawrXD::TensorInfo& info, void* src, size_t count) {
    (void)info; (void)src; (void)count;
}

void GGUFLoader::BeginCommandBuffer() {}
void GGUFLoader::EndCommandBuffer() {}

uint32_t GGUFLoader::FindMemoryType(uint32_t typeFilter, uint32_t props) {
    (void)typeFilter; (void)props;
    return 0;
}

uint32_t GGUFLoader::FindQueueFamilyIndex(VkPhysicalDevice dev, uint32_t queueFlags) {
    (void)dev; (void)queueFlags;
    return 0;
}

// ---- Compression (stubs — MASM integration point) ----
bool GGUFLoader::SetCompressionType(CompressionType type) {
    compression_type_ = type;
    return true;
}

bool GGUFLoader::DecompressData(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& decompressed) {
    if (compression_type_ == CompressionType::NONE) {
        decompressed = compressed;
        return true;
    }
    return false; // Not yet implemented
}

bool GGUFLoader::CompressData(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    (void)in; (void)out;
    return false;
}

// ---- Unsupported quantization detection ----
bool GGUFLoader::HasUnsupportedQuantizationTypes() const {
    return !unsupported_types_.empty();
}

std::vector<GGUFLoader::UnsupportedTypeInfo> GGUFLoader::GetUnsupportedQuantizationTypes() const {
    return unsupported_types_structs_;
}

std::string GGUFLoader::GetRecommendedConversionType() const {
    return "Q4_K_M";
}
