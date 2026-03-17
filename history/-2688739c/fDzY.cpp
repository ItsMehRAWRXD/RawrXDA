#include "gguf_loader.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <QByteArray>

// Forward declarations for codec functions
namespace codec {
    extern QByteArray deflate(const QByteArray& in, bool* ok);
    extern QByteArray inflate(const QByteArray& in, bool* ok);
}

// Forward declaration for brutal compression
namespace brutal {
    extern QByteArray compress(const QByteArray& in);
    extern QByteArray compress(const void* data, std::size_t size);
}

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
            if (key == "llama.context_length") {
                metadata_.context_length = uint_val;
            } else if (key == "llama.embedding_length") {
                metadata_.embedding_dim = uint_val;
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
            
            // For now, serialize array as comma-separated string (improve later if needed)
            std::string array_str = "[";
            for (uint64_t j = 0; j < array_length; ++j) {
                if (array_type == 8) {  // string array
                    std::string elem;
                    if (!ReadString(elem)) throw std::runtime_error("Failed to read array string element");
                    array_str += "\"" + elem + "\"";
                } else if (array_type == 4) {  // uint32 array
                    uint32_t elem;
                    if (!ReadValue(elem)) throw std::runtime_error("Failed to read array uint32 element");
                    array_str += std::to_string(elem);
                } else if (array_type == 5) {  // int32 array
                    int32_t elem;
                    if (!ReadValue(elem)) throw std::runtime_error("Failed to read array int32 element");
                    array_str += std::to_string(elem);
                } else if (array_type == 6) {  // float32 array
                    float elem;
                    if (!ReadValue(elem)) throw std::runtime_error("Failed to read array float32 element");
                    array_str += std::to_string(elem);
                } else {
                    throw std::runtime_error("Unsupported array element type: " + std::to_string(array_type));
                }
                if (j < array_length - 1) array_str += ", ";
            }
            array_str += "]";
            metadata_.kv_pairs[key] = array_str;
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
        // Use an exception for a cleaner interface in a larger app
        throw std::runtime_error("Tensor not found: " + tensor_name);
    }
    
    const TensorInfo* tensor_info = it->second;
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
    
    // Safety check: strings longer than 100MB are invalid
    // This prevents bad allocation errors from corrupted metadata
    const uint64_t MAX_STRING_SIZE = 100 * 1024 * 1024;  // 100MB limit
    if (len > MAX_STRING_SIZE) {
        std::cerr << "[GGUFLoader] String length " << len << " exceeds maximum allowed size" << std::endl;
        return false;
    }
    
    value.resize(len);
    file_.read(&value[0], len);
    return file_.good();
}

uint64_t GGUFLoader::CalculateTensorSize(const std::vector<uint64_t>& shape, GGMLType type) const {
    uint64_t num_elements = 1;
    for (uint64_t dim : shape) {
        num_elements *= dim;
    }
    
    // Helper lambda for block-aligned size calculation
    // Formula: block_size × ceil(num_elements / block_elements)
    auto block_aligned_size = [num_elements](uint64_t block_elements, uint64_t block_size) -> uint64_t {
        uint64_t num_blocks = (num_elements + block_elements - 1) / block_elements;  // Ceiling division
        return num_blocks * block_size;
    };
    
    // Calculate the size in bytes based on the GGML type
    // Reference: https://github.com/ggerganov/ggml/blob/master/include/ggml.h
    switch (type) {
        case GGMLType::F32: 
            return num_elements * 4;
        case GGMLType::F16: 
            return num_elements * 2;
        
        // Q4_0: 32 elements per block, 18 bytes per block (2 bytes FP16 scale + 16 bytes data)
        case GGMLType::Q4_0: 
            return block_aligned_size(32, 18);
        
        // Q4_1: 32 elements per block, 20 bytes per block (2 bytes FP16 scale + 2 bytes FP16 min + 16 bytes data)
        case GGMLType::Q4_1: 
            return block_aligned_size(32, 20);
        
        // Q8_0: 32 elements per block, 34 bytes per block (2 bytes FP16 scale + 32 bytes data)
        case GGMLType::Q8_0: 
            return block_aligned_size(32, 34);
        
        // Q5_1: 32 elements per block, 24 bytes per block
        case GGMLType::Q5_1:
            return block_aligned_size(32, 24);
        
        // K-quantization (256 elements per super-block)
        // Q2_K: 256 elements, 84 bytes per super-block
        case GGMLType::Q2_K: 
            return block_aligned_size(256, 84);
        
        // Q3_K: 256 elements, 110 bytes per super-block
        case GGMLType::Q3_K: 
            return block_aligned_size(256, 110);
        
        // Q4_K: 256 elements, 144 bytes per super-block
        case GGMLType::Q4_K: 
            return block_aligned_size(256, 144);
        
        // Q5_K: 256 elements, 176 bytes per super-block
        case GGMLType::Q5_K: 
            return block_aligned_size(256, 176);
        
        // Q6_K: 256 elements, 210 bytes per super-block
        case GGMLType::Q6_K: 
            return block_aligned_size(256, 210);
        
        // F16_HALF
        case GGMLType::F16_HALF:
            return num_elements * 2;
        
        default:
            // Type 14 is Q8_K (256 elements, 128 bytes per super-block)
            // We handle it here as a fallback case since it's not in the enum
            if (static_cast<uint32_t>(type) == 14) {
                return block_aligned_size(256, 128);  // Q8_K: 256 elements, 128 bytes per block
            }
            // In production, log the unsupported type and use a safe default
            std::cerr << "[GGUFLoader] WARNING: Unsupported GGMLType " << static_cast<uint32_t>(type) 
                     << " encountered, assuming F32 size" << std::endl;
            return num_elements * 4;  // Default to F32 size
    }
}

// =====================================================================
// MASM Compression Integration (Brutal GZIP + Deflate)
// =====================================================================

bool GGUFLoader::SetCompressionType(CompressionType type) {
    compression_type_ = type;
    // No initialization needed - brutal_gzip and deflate_brutal_qt are header-only/static
    return true;
}

bool GGUFLoader::DecompressData(const std::vector<uint8_t>& compressed, 
                                 std::vector<uint8_t>& decompressed) {
    if (compression_type_ == CompressionType::NONE) {
        decompressed = compressed;
        return true;
    }
    
    try {
        switch (compression_type_) {
            case CompressionType::BRUTAL_GZIP:
            case CompressionType::ZLIB: {
                // For GZIP/ZLIB, data is typically stored as-is or needs standard decompression
                // brutal_gzip is primarily a compression library, not decompression
                // For now, pass through (actual decompression would use zlib/gzip library)
                decompressed = compressed;
                return true;
            }
            
            case CompressionType::DEFLATE: {
                // Use codec::inflate from inflate_deflate_cpp.cpp
                // This handles DEFLATE decompression
                QByteArray input(reinterpret_cast<const char*>(compressed.data()), 
                                static_cast<int>(compressed.size()));
                bool ok = false;
                QByteArray output = codec::inflate(input, &ok);
                if (!ok) return false;
                
                decompressed.resize(output.size());
                std::memcpy(decompressed.data(), output.constData(), output.size());
                return true;
            }
            
            case CompressionType::NONE:
                decompressed = compressed;
                return true;
                
            default:
                throw std::runtime_error("Unsupported compression type");
        }
    } catch (const std::exception& e) {
        std::cerr << "Decompression error: " << e.what() << std::endl;
        return false;
    }
}

bool GGUFLoader::CompressData(const std::vector<uint8_t>& raw_data, 
                               std::vector<uint8_t>& compressed) {
    if (compression_type_ == CompressionType::NONE) {
        compressed = raw_data;
        return true;
    }
    
    try {
        switch (compression_type_) {
            case CompressionType::BRUTAL_GZIP:
            case CompressionType::ZLIB: {
                // Compress using brutal::compress (MASM-optimized gzip)
                QByteArray input(reinterpret_cast<const char*>(raw_data.data()), 
                                static_cast<int>(raw_data.size()));
                QByteArray output = brutal::compress(input);
                if (output.isEmpty()) return false;
                
                compressed.resize(output.size());
                std::memcpy(compressed.data(), output.constData(), output.size());
                return true;
            }
            
            case CompressionType::DEFLATE: {
                // Compress using codec::deflate (MASM-optimized deflate)
                QByteArray input(reinterpret_cast<const char*>(raw_data.data()), 
                                static_cast<int>(raw_data.size()));
                bool ok = false;
                QByteArray output = codec::deflate(input, &ok);
                if (!ok || output.isEmpty()) return false;
                
                compressed.resize(output.size());
                std::memcpy(compressed.data(), output.constData(), output.size());
                return true;
            }
            
            case CompressionType::NONE:
                compressed = raw_data;
                return true;
                
            default:
                throw std::runtime_error("Unsupported compression type");
        }
    } catch (const std::exception& e) {
        std::cerr << "Compression error: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// Quantization Type Validation (IDE Conversion Workflow)
// ============================================================================

bool GGUFLoader::HasUnsupportedQuantizationTypes() const {
    return !unsupported_types_.empty();
}

std::vector<GGUFLoader::UnsupportedTypeInfo> GGUFLoader::GetUnsupportedQuantizationTypes() const {
    return unsupported_types_;
}

std::string GGUFLoader::GetRecommendedConversionType() const {
    // If we found unsupported types, recommend conversion to Q5_K (type 13)
    // which provides excellent quality/size tradeoff and is widely supported
    return "Q5_K";
}

// Helper function to map unsupported type values to human-readable names
static std::string GetUnsupportedTypeNameByValue(uint32_t type_val) {
    switch (type_val) {
        case 39:  return "IQ4_NL";
        case 40:  return "IQ4_XS";
        case 41:  return "IQ3_S";
        case 42:  return "IQ2_S";
        case 43:  return "IQ2_M";
        default:  return "IQ" + std::to_string(type_val);  // Generic fallback
    }
}

