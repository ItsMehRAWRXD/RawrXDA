#include "gguf_loader.h"
#include "gguf_vocab_resolver.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <sstream>

#ifdef RAWRXD_EXPERIMENTAL_GGUF_GPU
#include <windows.h>
#endif

#include <vector>
#include <cstdint>

// SCAFFOLD_101: GGUF loader and tensor read


// Forward declarations for codec functions
namespace codec {
    extern std::vector<uint8_t> deflate(const std::vector<uint8_t>& in, bool* ok);
    extern std::vector<uint8_t> inflate(const std::vector<uint8_t>& in, bool* ok);
}

// Forward declaration for brutal compression
namespace brutal {
    extern std::vector<uint8_t> compress(const std::vector<uint8_t>& in);
    extern std::vector<uint8_t> compress(const void* data, std::size_t size);
}


namespace RawrXD {

// Forward declaration for unsupported type name lookup (defined later in file)
static std::string GetUnsupportedTypeNameByValue(uint32_t type_val);

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
        // Return false instead of throwing to allow graceful fallback
        return false;
    }
    
    is_open_ = true;
    unsupported_types_.clear();  // Clear any previous unsupported types
    
    // ParseHeader now throws exceptions on error (consistent error handling)
    try {
        if (!ParseHeader()) {
            Close();
            return false;
        }
    } catch (const std::exception& e) {
        Close();
        throw;  // Re-throw after cleanup
    }
    
    return true;
}


bool GGUFLoader::Close() {
    if (file_.is_open()) {
        file_.close();
    }
    is_open_ = false;
    tensors_.clear();
    unsupported_types_.clear();  // Clear unsupported types tracking
    return true;
}


bool GGUFLoader::ParseHeader() {
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot parse header: file not open");
    }
    
    file_.seekg(0);
    
    // Read magic
    if (!ReadValue(header_.magic)) {
        throw std::runtime_error("Failed to read GGUF magic bytes");
    }
    if (header_.magic != 0x46554747) {  // "GGUF"
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
    
    // Read metadata KV count
    if (!ReadValue(header_.metadata_kv_count)) {
        throw std::runtime_error("Failed to read metadata KV count");
    }
    
    // Calculate metadata offset
    header_.metadata_offset = file_.tellg();
    
    std::cout << "GGUF Header: Magic=0x" << std::hex << header_.magic << std::dec 
              << ", Version=" << header_.version
              << ", Tensors=" << header_.tensor_count
              << ", Metadata=" << header_.metadata_kv_count << std::endl;
    
    return true;
}

bool GGUFLoader::ParseMetadata() {
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot parse metadata: file not open");
    }
    if (header_.metadata_kv_count == 0) {
        // Valid case: GGUF file with no metadata
        return true;
    }
    
    file_.seekg(header_.metadata_offset);
    
    for (uint64_t i = 0; i < header_.metadata_kv_count; ++i) {
        std::string key, value;
        
        if (!ReadString(key)) {
            throw std::runtime_error("Failed to read metadata key at index " + std::to_string(i));
        }
        
        uint32_t value_type;
        if (!ReadValue(value_type)) {
            throw std::runtime_error("Failed to read metadata value type for key: " + key);
        }
        
        // GGUF Value Types (https://github.com/ggerganov/ggml/blob/master/docs/gguf.md)
        // 0=UInt8, 1=Int8, 2=UInt16, 3=Int16, 4=UInt32, 5=Int32, 6=Float32, 7=Bool, 8=String, 9=Array, 10=UInt64, 11=Int64, 12=Float64
        
        if (value_type == 8) {  // UTF-8 string
            if (!ReadString(value)) {
                throw std::runtime_error("Failed to read metadata string value for key: " + key);
            }
            metadata_.kv_pairs[key] = value;
            
            // Parse important metadata - support both llama and other model naming conventions
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
            if (!ReadValue(uint_val)) throw std::runtime_error("Failed to read uint32 for key: " + key);
            metadata_.kv_pairs[key] = std::to_string(uint_val);
            
            // Parse important numeric metadata
            if (key == "llama.block_count") {
                metadata_.layer_count = uint_val;
            } else if (key == "llama.context_length") {
                metadata_.context_length = uint_val;
            } else if (key == "llama.embedding_length") {
                metadata_.embedding_dim = uint_val;
            } else if (key == "llama.vocab_size") {
                metadata_.vocab_size = uint_val;
            } else if (key == "llama.feed_forward_length") {
                // Feed forward length is also useful metadata
            }
        } else if (value_type == 5) {  // int32
            int32_t int_val;
            if (!ReadValue(int_val)) throw std::runtime_error("Failed to read int32 for key: " + key);
            metadata_.kv_pairs[key] = std::to_string(int_val);
        } else if (value_type == 6) {  // float32
            float float_val;
            if (!ReadValue(float_val)) throw std::runtime_error("Failed to read float32 for key: " + key);
            metadata_.kv_pairs[key] = std::to_string(float_val);
        } else if (value_type == 10) {  // uint64
            uint64_t uint64_val;
            if (!ReadValue(uint64_val)) throw std::runtime_error("Failed to read uint64 for key: " + key);
            metadata_.kv_pairs[key] = std::to_string(uint64_val);
            
            // Parse important numeric metadata
            if (key == "llama.block_count") {
                metadata_.layer_count = uint64_val;
            } else if (key == "llama.context_length") {
                metadata_.context_length = uint64_val;
            } else if (key == "llama.embedding_length") {
                metadata_.embedding_dim = uint64_val;
            } else if (key == "llama.vocab_size") {
                metadata_.vocab_size = uint64_val;
            }
        } else if (value_type == 11) {  // int64
            int64_t int64_val;
            if (!ReadValue(int64_val)) throw std::runtime_error("Failed to read int64 for key: " + key);
            metadata_.kv_pairs[key] = std::to_string(int64_val);
        } else if (value_type == 12) {  // float64
            double float64_val;
            if (!ReadValue(float64_val)) throw std::runtime_error("Failed to read float64 for key: " + key);
            metadata_.kv_pairs[key] = std::to_string(float64_val);
        } else if (value_type == 7) {  // boolean
            uint8_t bool_val;
            if (!ReadValue(bool_val)) throw std::runtime_error("Failed to read bool for key: " + key);
            metadata_.kv_pairs[key] = bool_val ? "true" : "false";
        } else if (value_type == 0) {  // uint8
            uint8_t uint8_val;
            if (!ReadValue(uint8_val)) throw std::runtime_error("Failed to read uint8 for key: " + key);
            metadata_.kv_pairs[key] = std::to_string(uint8_val);
        } else if (value_type == 1) {  // int8
            int8_t int8_val;
            if (!ReadValue(int8_val)) throw std::runtime_error("Failed to read int8 for key: " + key);
            metadata_.kv_pairs[key] = std::to_string(int8_val);
        } else if (value_type == 2) {  // uint16
            uint16_t uint16_val;
            if (!ReadValue(uint16_val)) throw std::runtime_error("Failed to read uint16 for key: " + key);
            metadata_.kv_pairs[key] = std::to_string(uint16_val);
        } else if (value_type == 3) {  // int16
            int16_t int16_val;
            if (!ReadValue(int16_val)) throw std::runtime_error("Failed to read int16 for key: " + key);
            metadata_.kv_pairs[key] = std::to_string(int16_val);
        } else if (value_type == 9) {  // array
            // Array format: value_type (uint32) + array_length (uint64) + elements
            uint32_t array_type;
            uint64_t array_length;
            if (!ReadValue(array_type)) throw std::runtime_error("Failed to read array type for key: " + key);
            if (!ReadValue(array_length)) throw std::runtime_error("Failed to read array length for key: " + key);
            
            // Serialize for kv_pairs but also capture structured tokenizer data
            std::string array_str = "[";
            std::vector<std::string> string_elems;
            std::vector<float> float_elems;
            std::vector<uint32_t> uint32_elems;

            for (uint64_t j = 0; j < array_length; ++j) {
                if (array_type == 8) {  // string array
                    std::string elem;
                    if (!ReadString(elem)) throw std::runtime_error("Failed to read array string element");
                    array_str += "\"" + elem + "\"";
                    string_elems.push_back(std::move(elem));
                } else if (array_type == 4) {  // uint32 array
                    uint32_t elem;
                    if (!ReadValue(elem)) throw std::runtime_error("Failed to read array uint32 element");
                    array_str += std::to_string(elem);
                    uint32_elems.push_back(elem);
                } else if (array_type == 5) {  // int32 array
                    int32_t elem;
                    if (!ReadValue(elem)) throw std::runtime_error("Failed to read array int32 element");
                    array_str += std::to_string(elem);
                } else if (array_type == 6) {  // float32 array
                    float elem;
                    if (!ReadValue(elem)) throw std::runtime_error("Failed to read array float32 element");
                    array_str += std::to_string(elem);
                    float_elems.push_back(elem);
                } else {
                    throw std::runtime_error("Unsupported array element type: " + std::to_string(array_type));
                }
                if (j < array_length - 1) array_str += ", ";
            }
            array_str += "]";
            metadata_.kv_pairs[key] = array_str;

            // Persist structured tokenizer fields for downstream loaders
            if (key == "tokenizer.ggml.tokens" && !string_elems.empty()) {
                metadata_.tokens = string_elems;
            }
            if (key == "tokenizer.ggml.scores" && !float_elems.empty()) {
                metadata_.token_scores = float_elems;
            }
            if (key == "tokenizer.ggml.token_type" && !uint32_elems.empty()) {
                metadata_.token_types = uint32_elems;
            }
        } else {
            throw std::runtime_error("Unsupported metadata value type " + std::to_string(value_type) + " for key: " + key);
        }
    }
    
    // Read tensor info
    for (uint64_t i = 0; i < header_.tensor_count; ++i) {
        TensorInfo tensor;
        
        if (!ReadString(tensor.name)) {
            throw std::runtime_error("Failed to read tensor name at index " + std::to_string(i));
        }
        
        uint32_t n_dims;
        if (!ReadValue(n_dims)) {
            throw std::runtime_error("Failed to read dimension count for tensor: " + tensor.name);
        }
        
        // Safety check: tensors should have <= 4 dimensions (very rare to have more)
        if (n_dims > 8) {
            throw std::runtime_error("Tensor has too many dimensions (" + std::to_string(n_dims) + 
                                   ") for tensor: " + tensor.name);
        }
        
        tensor.shape.resize(n_dims);
        for (uint32_t d = 0; d < n_dims; ++d) {
            if (!ReadValue(tensor.shape[d])) {
                throw std::runtime_error("Failed to read dimension " + std::to_string(d) + 
                                       " for tensor: " + tensor.name);
            }
            
            // Safety check: individual dimensions shouldn't exceed 1 billion
            // (would be a sign of corrupted data)
            if (tensor.shape[d] > 1000000000ULL) {
                throw std::runtime_error("Tensor dimension too large (" + std::to_string(tensor.shape[d]) + 
                                       ") for tensor: " + tensor.name);
            }
        }
        
        uint32_t type_val;
        if (!ReadValue(type_val)) {
            throw std::runtime_error("Failed to read type for tensor: " + tensor.name);
        }
        
        // ========== UNSUPPORTED TYPE DETECTION ==========
        // Check if tensor type is outside the supported GGML enum range (0-24)
        // Types 39, 40, 41, etc. are newer quantization types (IQ4_NL, IQ4_XS, IQ3_S)
        // that require newer GGML versions. This triggers IDE conversion workflow.
        if (type_val > 24) {
            // Log the unsupported type for the user
            std::string type_name = GetUnsupportedTypeNameByValue(type_val);
            std::cerr << "WARNING: Tensor '" << tensor.name 
                     << "' uses unsupported type " << type_val 
                     << " (" << type_name << "). "
                     << "Model will need quantization conversion to proceed." << std::endl;
            
            // Track this unsupported type for later IDE prompting
            bool type_exists = false;
            for (auto& existing : unsupported_types_) {
                if (existing.type_value == type_val) {
                    existing.tensor_names.push_back(tensor.name);
                    type_exists = true;
                    break;
                }
            }
            if (!type_exists) {
                unsupported_types_.push_back({
                    type_val,
                    type_name,
                    {tensor.name}
                });
            }
        }
        
        tensor.type = static_cast<GGMLType>(type_val);
        
        if (!ReadValue(tensor.offset)) {
            throw std::runtime_error("Failed to read offset for tensor: " + tensor.name);
        }
        
        tensor.size_bytes = CalculateTensorSize(tensor.shape, tensor.type);
        tensors_.push_back(tensor);
    }
    
    // Build O(1) lookup index for tensor access (Bottleneck #14 fix)
    for (auto& tensor : tensors_) {
        tensor_index_[tensor.name] = &tensor;
    }
    
    // Fix vocab size using universal resolver
    GGUFVocabResolver vocabResolver;
    VocabSizeDetection vocabDetection = vocabResolver.detectVocabSize(metadata_.kv_pairs, filepath_);
    
    std::cout << "[GGUFLoader] Vocab detection: method=" << vocabDetection.detectionMethod
              << ", size=" << vocabDetection.detectedSize
              << ", confident=" << (vocabDetection.isConfident ? "yes" : "no") << std::endl;
    
    // Override metadata vocab size if detection was successful
    if (vocabDetection.isConfident && vocabDetection.detectedSize > 0) {
        if (metadata_.vocab_size != vocabDetection.detectedSize) {
            std::cout << "[GGUFLoader] Correcting vocab size: " << metadata_.vocab_size 
                      << " -> " << vocabDetection.detectedSize << std::endl;
            metadata_.vocab_size = vocabDetection.detectedSize;
        }
    } else if (!vocabDetection.isConfident && vocabDetection.detectedSize > 0) {
        // Use detection even if not confident, as it's better than potentially wrong metadata
        std::cout << "[GGUFLoader] Using low-confidence vocab size: " << vocabDetection.detectedSize << std::endl;
        metadata_.vocab_size = vocabDetection.detectedSize;
    }
    
    std::cout << "Metadata parsed successfully. Layers: " << metadata_.layer_count
              << ", Context: " << metadata_.context_length
              << ", Embedding: " << metadata_.embedding_dim
              << ", Vocab: " << metadata_.vocab_size << std::endl;
    
    return true;
}

bool GGUFLoader::LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) {
    // O(1) lookup using tensor index (Bottleneck #14 fix - eliminates O(n) std::find_if)
    auto it = tensor_index_.find(tensor_name);
    
    if (it == tensor_index_.end()) {
        throw std::runtime_error("Tensor not found: " + tensor_name);
    }
    
    const TensorInfo* tensor_info = it->second;

    // Use memory mapping for direct tensor access if available
    if (use_mmap_) {
        const void* mapped_data = GetMappedSlice(tensor_info->offset, tensor_info->size_bytes);
        if (mapped_data) {
            data.assign(static_cast<const uint8_t*>(mapped_data), static_cast<const uint8_t*>(mapped_data) + tensor_info->size_bytes);
            return true;
        }
    }

    // Fallback to file I/O if mmap is not used or fails
    data.resize(tensor_info->size_bytes);
    file_.seekg(tensor_info->offset);
    file_.read(reinterpret_cast<char*>(data.data()), tensor_info->size_bytes);
    
    if (!file_.good()) {
         throw std::runtime_error("Failed to read tensor data for: " + tensor_name);
    }
    
    // Apply MASM-optimized decompression if enabled (brutal_gzip or deflate)
    if (IsCompressed()) {
        std::vector<uint8_t> decompressed;
        if (!DecompressData(data, decompressed)) {
            throw std::runtime_error("Failed to decompress tensor: " + tensor_name);
        }
        data = std::move(decompressed);
    }
    
    return true;
}

bool GGUFLoader::LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) {
    if (start_idx + count > tensors_.size()) {
        throw std::out_of_range("Tensor range out of bounds");
    }
    
    // Calculate total size needed for output buffer
    size_t total_size = 0;
    for (size_t i = start_idx; i < start_idx + count; ++i) {
        total_size += tensors_[i].size_bytes;
    }
    
    data.resize(total_size);
    size_t offset = 0;
    
    // Note: Each tensor.offset is already 32-byte aligned per GGUF spec
    // The GGUF writer ensures proper padding between tensors, so we just
    // seek to the stored offset directly without manual alignment calculation
    for (size_t i = start_idx; i < start_idx + count; ++i) {
        file_.seekg(tensors_[i].offset);
        file_.read(reinterpret_cast<char*>(data.data() + offset), tensors_[i].size_bytes);
        if (!file_.good()) {
            throw std::runtime_error("Failed to read tensor range during bulk load.");
        }
        offset += tensors_[i].size_bytes;
    }
    
    // Apply MASM-optimized decompression if enabled (brutal_gzip or deflate)
    if (IsCompressed()) {
        std::vector<uint8_t> decompressed;
        if (!DecompressData(data, decompressed)) {
            throw std::runtime_error("Failed to decompress tensor range");
        }
        data = std::move(decompressed);
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
        case GGMLType::Q5_1: return "Q5_1";
        case GGMLType::Q8_0: return "Q8_0";
        case GGMLType::Q2_K: return "Q2_K";
        case GGMLType::Q3_K: return "Q3_K";
        case GGMLType::Q4_K: return "Q4_K";
        case GGMLType::Q5_K: return "Q5_K";
        case GGMLType::Q6_K: return "Q6_K";
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

// ============================================================================
// Streaming Interface — Non-streaming loader minimal implementations
// (GGUFLoader reads entire file; zones are a streaming-only concept)
// ============================================================================

bool GGUFLoader::BuildTensorIndex() {
    tensor_index_.clear();
    tensor_index_.reserve(tensors_.size());
    for (const auto& t : tensors_) {
        tensor_index_[t.name] = &t;
    }
    return true;
}

bool GGUFLoader::LoadZone(const std::string& zone_name, uint64_t max_memory_mb) {
    if (!is_open_ || !file_.is_open()) return false;

    // Already loaded.
    if (loaded_zones_.find(zone_name) != loaded_zones_.end()) {
        return true;
    }

    const uint64_t max_bytes = max_memory_mb * 1024ULL * 1024ULL;
    std::vector<uint8_t> zone_data;
    zone_data.reserve(1024);

    bool found_any = false;

    // Prefix match: allow zone grouping by common prefix (e.g. "blk.0").
    for (const auto& tensor : tensors_) {
        if (tensor.name == zone_name || tensor.name.rfind(zone_name, 0) == 0) {
            std::vector<uint8_t> tensor_data;
            try {
                if (!LoadTensorZone(tensor.name, tensor_data)) {
                    continue;
                }
            } catch (...) {
                // Non-streaming loader: keep zone loading best-effort.
                continue;
            }

            if (!tensor_data.empty()) {
                if (!zone_data.empty() && (zone_data.size() + tensor_data.size() > max_bytes)) {
                    break;
                }
                zone_data.insert(zone_data.end(), tensor_data.begin(), tensor_data.end());
                found_any = true;
                if (zone_data.size() >= max_bytes) {
                    break;
                }
            }
        }
    }

    if (!found_any) {
        return false;
    }

    current_memory_usage_ += static_cast<uint64_t>(zone_data.size());
    loaded_zones_.emplace(zone_name, std::move(zone_data));
    return true;
}

bool GGUFLoader::UnloadZone(const std::string& zone_name) {
    auto it = loaded_zones_.find(zone_name);
    if (it == loaded_zones_.end()) return false;

    const uint64_t sz = static_cast<uint64_t>(it->second.size());
    if (current_memory_usage_ >= sz) {
        current_memory_usage_ -= sz;
    } else {
        current_memory_usage_ = 0;
    }
    loaded_zones_.erase(it);
    return true;
}

std::vector<std::string> GGUFLoader::GetLoadedZones() const {
    std::vector<std::string> zones;
    zones.reserve(loaded_zones_.size());
    for (const auto& kv : loaded_zones_) {
        zones.push_back(kv.first);
    }
    return zones;
}

std::vector<std::string> GGUFLoader::GetAllZones() const {
    // Derive zone names from tensor name prefixes (split on last '.').
    std::unordered_map<std::string, bool> zone_set;
    zone_set.reserve(tensors_.size());

    for (const auto& t : tensors_) {
        const std::string& name = t.name;
        const size_t dot = name.rfind('.');
        if (dot != std::string::npos && dot > 0) {
            zone_set[name.substr(0, dot)] = true;
        } else {
            zone_set[name] = true;
        }
    }

    std::vector<std::string> zones;
    zones.reserve(zone_set.size());
    for (const auto& kv : zone_set) {
        zones.push_back(kv.first);
    }
    std::sort(zones.begin(), zones.end());
    return zones;
}

std::vector<TensorInfo> GGUFLoader::GetAllTensorInfo() const {
    return tensors_;
}

uint64_t GGUFLoader::GetCurrentMemoryUsage() const {
    return current_memory_usage_;
}

bool GGUFLoader::SetCompressionType(CompressionType type) {
    compression_type_ = type;
    return true;
}

bool GGUFLoader::DecompressData(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& decompressed) {
    decompressed.clear();

    switch (compression_type_) {
        case CompressionType::NONE:
            decompressed = compressed;
            return true;
        case CompressionType::BRUTAL_GZIP: {
            decompressed = brutal::decompress(compressed);
            return !decompressed.empty() || compressed.empty();
        }
        case CompressionType::DEFLATE:
        case CompressionType::ZLIB: {
            bool ok = false;
            decompressed = codec::inflate(compressed, &ok);
            if (ok) return true;
            // Fallback: brutal::decompress returns passthrough if it can't inflate.
            decompressed = brutal::decompress(compressed);
            return true;
        }
        default:
            // Unknown compression: preserve original bytes.
            decompressed = compressed;
            return true;
    }
}

bool GGUFLoader::CompressData(const std::vector<uint8_t>& raw_data, std::vector<uint8_t>& compressed) {
    compressed.clear();

    switch (compression_type_) {
        case CompressionType::NONE:
            compressed = raw_data;
            return true;
        case CompressionType::BRUTAL_GZIP:
            compressed = brutal::compress(raw_data);
            return true;
        case CompressionType::DEFLATE:
        case CompressionType::ZLIB: {
            bool ok = false;
            compressed = codec::deflate(raw_data, &ok);
            if (!ok) compressed = raw_data;
            return true;
        }
        default:
            compressed = raw_data;
            return true;
    }
}

bool GGUFLoader::HasUnsupportedQuantizationTypes() const {
    return !unsupported_types_.empty();
}

std::vector<GGUFLoader::UnsupportedTypeInfo> GGUFLoader::GetUnsupportedQuantizationTypes() const {
    return unsupported_types_;
}

std::string GGUFLoader::GetRecommendedConversionType() const {
    // Conservative baseline supported widely by older ggml consumers.
    if (HasUnsupportedQuantizationTypes()) return "Q4_0";
    return GetTypeString(GGMLType::Q4_0);
}

template<typename T>
bool GGUFLoader::ReadValue(T& value) {
    file_.read(reinterpret_cast<char*>(&value), sizeof(T));
    return file_.good();
}

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

bool GGUFLoader::ReadString(std::string& value) {
    uint64_t len = 0;
    if (!ReadValue(len)) return false;
    if (len == 0) {
        value.clear();
        return true;
    }
    // Hard upper bound to avoid OOM on corrupt inputs.
    if (len > (1024ULL * 1024ULL * 64ULL)) {
        return false;
    }

    value.resize(static_cast<size_t>(len));
    file_.read(&value[0], static_cast<std::streamsize>(len));
    return file_.good();
}

uint64_t GGUFLoader::CalculateTensorSize(const std::vector<uint64_t>& shape, GGMLType type) const {
    uint64_t num_elements = 1;
    for (uint64_t dim : shape) {
        num_elements *= dim;
    }

    float bytes_per_element = 4.0f;  // Default F32
    switch (type) {
        case GGMLType::F32:       bytes_per_element = 4.0f; break;
        case GGMLType::F16:       bytes_per_element = 2.0f; break;
        case GGMLType::F16_HALF:  bytes_per_element = 2.0f; break;
        case GGMLType::Q4_0:      bytes_per_element = 0.5f + (2.0f / 32); break;  // 18 bytes per 32 elements
        case GGMLType::Q4_1:      bytes_per_element = 0.5f + (4.0f / 32); break;  // 20 bytes per 32 elements
        case GGMLType::Q5_0:      bytes_per_element = 0.625f + (4.0f / 32); break;
        case GGMLType::Q5_1:      bytes_per_element = 0.625f + (4.0f / 32); break;
        case GGMLType::Q8_0:      bytes_per_element = 1.0f + (2.0f / 32); break;
        case GGMLType::Q8_1:      bytes_per_element = 1.0f + (8.0f / 32); break;
        case GGMLType::Q2_K:      bytes_per_element = 0.3125f; break;
        case GGMLType::Q3_K:      bytes_per_element = 0.4375f; break;
        case GGMLType::Q4_K:      bytes_per_element = 0.5f; break;
        case GGMLType::Q5_K:      bytes_per_element = 0.625f; break;
        case GGMLType::Q6_K:      bytes_per_element = 0.75f; break;
        case GGMLType::Q8_K:      bytes_per_element = 1.0625f; break;
        case GGMLType::IQ2_XXS:   bytes_per_element = 0.25f; break;
        case GGMLType::IQ2_XS:    bytes_per_element = 0.28125f; break;
        case GGMLType::IQ3_XXS:   bytes_per_element = 0.375f; break;
        case GGMLType::IQ1_S:     bytes_per_element = 0.1875f; break;
        case GGMLType::IQ4_NL:    bytes_per_element = 0.5f; break;
        case GGMLType::IQ3_S:     bytes_per_element = 0.4375f; break;
        case GGMLType::IQ2_S:     bytes_per_element = 0.3125f; break;
        case GGMLType::IQ4_XS:    bytes_per_element = 0.5f; break;
        case GGMLType::I8:        bytes_per_element = 1.0f; break;
        case GGMLType::I16:       bytes_per_element = 2.0f; break;
        case GGMLType::I32:       bytes_per_element = 4.0f; break;
        case GGMLType::I64:       bytes_per_element = 8.0f; break;
        case GGMLType::F64:       bytes_per_element = 8.0f; break;
        case GGMLType::IQ1_M:     bytes_per_element = 0.21875f; break;
        default:                  bytes_per_element = 4.0f; break;
    }

    return static_cast<uint64_t>(num_elements * bytes_per_element);
}

bool GGUFLoader::CreateDummyModel() {
    use_dummy_mode_ = true;
    tensors_.clear();
    tensor_index_.clear();
    return true;
}

bool GGUFLoader::InitializeMemoryMap() {
    use_mmap_ = false;
    mmap_base_ = nullptr;
    file_handle_ = nullptr;
    map_handle_ = nullptr;
    return false;
}

void GGUFLoader::CleanupMemoryMap() {
    use_mmap_ = false;
    mmap_base_ = nullptr;
    file_handle_ = nullptr;
    map_handle_ = nullptr;
}

const void* GGUFLoader::GetMappedSlice(uint64_t offset, uint64_t size) const {
    if (!use_mmap_ || !mmap_base_) return nullptr;
    if (offset + size < offset) return nullptr;
    if (file_size_ && (offset + size > file_size_)) return nullptr;
    return static_cast<const uint8_t*>(mmap_base_) + offset;
}

static std::string GetUnsupportedTypeNameByValue(uint32_t type_val) {
    switch (type_val) {
        case 39: return "IQ4_NL";
        case 40: return "IQ4_XS";
        case 41: return "IQ3_S";
        default: break;
    }
    return "UNKNOWN";
}

} // namespace RawrXD
