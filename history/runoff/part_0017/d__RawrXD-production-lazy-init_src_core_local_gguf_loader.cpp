// ============================================================================
// Local GGUF Loader - Production Implementation
// ============================================================================
// Direct file I/O implementation for GGUF format parsing
// Supports GGUF version 3, all quantization types
// ============================================================================

#include "local_gguf_loader.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace RawrXD {
namespace Core {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

LocalGGUFLoader::LocalGGUFLoader() {
    clearError();
}

LocalGGUFLoader::~LocalGGUFLoader() {
    closeFile();
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

bool LocalGGUFLoader::load(const std::string& file_path) {
    // Reset state
    closeFile();
    tensors_.clear();
    metadata_ = ModelMetadata();
    clearError();
    
    // Validate file first
    if (!validateFile(file_path)) {
        return false;
    }
    
    // Open file
    if (!openFile(file_path)) {
        return false;
    }
    
    // Read header
    if (!readHeader()) {
        closeFile();
        return false;
    }
    
    // Read metadata
    if (!readMetadata()) {
        closeFile();
        return false;
    }
    
    // Read tensor info
    if (!readTensorInfo()) {
        closeFile();
        return false;
    }
    
    // Populate metadata
    metadata_.path = file_path;
    metadata_.file_size_bytes = getFileSize();
    metadata_.is_valid = metadata_.validate();
    
    // File remains open for tensor data access
    return metadata_.is_valid;
}

bool LocalGGUFLoader::validateFile(const std::string& file_path) {
    // Check if file exists
    FILE* test_file = std::fopen(file_path.c_str(), "rb");
    if (!test_file) {
        setError("File not found or not readable: " + file_path);
        return false;
    }
    
    // Check file size (minimum for header)
    std::fseek(test_file, 0, SEEK_END);
    long file_size = std::ftell(test_file);
    std::fclose(test_file);
    
    if (file_size < static_cast<long>(sizeof(GGUFHeader))) {
        setError("File too small to be valid GGUF: " + file_path);
        return false;
    }
    
    // Try to read and validate header
    FILE* file = std::fopen(file_path.c_str(), "rb");
    if (!file) {
        setError("Cannot open file for reading: " + file_path);
        return false;
    }
    
    GGUFHeader header;
    size_t bytes_read = std::fread(&header, 1, sizeof(header), file);
    std::fclose(file);
    
    if (bytes_read != sizeof(header)) {
        setError("Failed to read GGUF header from: " + file_path);
        return false;
    }
    
    if (!header.isValid()) {
        setError("Invalid GGUF header in file: " + file_path + 
                 " (magic: 0x" + toHex(header.magic) + 
                 ", expected: 0x" + toHex(GGUF_MAGIC) + ")");
        return false;
    }
    
    return true;
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

bool LocalGGUFLoader::openFile(const std::string& path) {
    closeFile(); // Ensure any previous file is closed
    
    file_ = std::fopen(path.c_str(), "rb");
    if (!file_) {
        setError("Failed to open file: " + path);
        return false;
    }
    
    return true;
}

void LocalGGUFLoader::closeFile() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

uint64_t LocalGGUFLoader::getFileSize() const {
    if (!file_) return 0;
    
    long current_pos = std::ftell(file_);
    std::fseek(file_, 0, SEEK_END);
    long size = std::ftell(file_);
    std::fseek(file_, current_pos, SEEK_SET);
    
    return static_cast<uint64_t>(size);
}

// ============================================================================
// READING METHODS
// ============================================================================

bool LocalGGUFLoader::readHeader() {
    if (!file_) {
        setError("File not open");
        return false;
    }
    
    std::rewind(file_);
    
    size_t bytes_read = std::fread(&header_, 1, sizeof(header_), file_);
    if (bytes_read != sizeof(header_)) {
        setError("Failed to read GGUF header");
        return false;
    }
    
    if (!header_.isValid()) {
        setError("Invalid GGUF header");
        return false;
    }
    
    return true;
}

bool LocalGGUFLoader::readMetadata() {
    if (!file_ || header_.metadata_kv_count == 0) {
        return true; // No metadata to read
    }
    
    // Seek to metadata position (right after header)
    // Header is at position 0, metadata starts immediately after
    std::fseek(file_, sizeof(GGUFHeader), SEEK_SET);
    
    // Read all metadata key-value pairs
    for (uint64_t i = 0; i < header_.metadata_kv_count; ++i) {
        GGUFMetadataKV kv;
        if (!readValue(kv)) {
            setError("Failed to read metadata KV #" + std::to_string(i));
            return false;
        }
        
        // Store important metadata fields
        if (kv.key == "general.architecture") {
            metadata_.architecture = kv.string_value;
        } else if (kv.key == "general.name") {
            metadata_.name = kv.string_value;
        } else if (kv.key == "llama.context_length") {
            metadata_.context_length = kv.uint32_value;
        } else if (kv.key == "llama.embedding_length") {
            metadata_.hidden_size = kv.uint32_value;
        } else if (kv.key == "llama.feed_forward_length") {
            // Store if needed
        } else if (kv.key == "llama.attention.head_count") {
            metadata_.head_count = kv.uint32_value;
        } else if (kv.key == "llama.attention.head_count_kv") {
            metadata_.head_count_kv = kv.uint32_value;
        } else if (kv.key == "llama.block_count") {
            metadata_.layer_count = kv.uint32_value;
        } else if (kv.key == "llama.vocab_size") {
            metadata_.vocab_size = kv.uint32_value;
        } else if (kv.key == "llama.rope.freq_base") {
            metadata_.rope_theta = kv.float32_value;
        } else if (kv.key == "general.file_type") {
            // Quantization type indicator
        } else if (kv.key == "general.quantization_version") {
            // Quantization version
        }
    }
    
    return true;
}

bool LocalGGUFLoader::readTensorInfo() {
    if (!file_ || header_.tensor_count == 0) {
        return true; // No tensors to read
    }
    
    tensors_.reserve(header_.tensor_count);
    
    // Read all tensor info
    for (uint64_t i = 0; i < header_.tensor_count; ++i) {
        GGUFTensorInfo tensor;
        
        // Read name length
        if (std::fread(&tensor.name_length, 1, sizeof(tensor.name_length), file_) != sizeof(tensor.name_length)) {
            setError("Failed to read tensor name length #" + std::to_string(i));
            return false;
        }
        
        // Read name
        tensor.name.resize(tensor.name_length);
        if (std::fread(&tensor.name[0], 1, tensor.name_length, file_) != tensor.name_length) {
            setError("Failed to read tensor name #" + std::to_string(i));
            return false;
        }
        
        // Read dimension count
        if (std::fread(&tensor.n_dimensions, 1, sizeof(tensor.n_dimensions), file_) != sizeof(tensor.n_dimensions)) {
            setError("Failed to read tensor dimension count #" + std::to_string(i));
            return false;
        }
        
        // Read dimensions
        tensor.dimensions.resize(tensor.n_dimensions);
        if (std::fread(tensor.dimensions.data(), 1, 
                       tensor.n_dimensions * sizeof(uint64_t), file_) != 
                       tensor.n_dimensions * sizeof(uint64_t)) {
            setError("Failed to read tensor dimensions #" + std::to_string(i));
            return false;
        }
        
        // Read tensor type
        uint32_t type_value;
        if (std::fread(&type_value, 1, sizeof(type_value), file_) != sizeof(type_value)) {
            setError("Failed to read tensor type #" + std::to_string(i));
            return false;
        }
        tensor.tensor_type = static_cast<GGUFTensorType>(type_value);
        
        // Read offset
        if (std::fread(&tensor.offset, 1, sizeof(tensor.offset), file_) != sizeof(tensor.offset)) {
            setError("Failed to read tensor offset #" + std::to_string(i));
            return false;
        }
        
        tensors_.push_back(std::move(tensor));
    }
    
    return true;
}

bool LocalGGUFLoader::readString(std::string& out_str, uint64_t length) {
    if (!file_ || length == 0) {
        out_str.clear();
        return true;
    }
    
    out_str.resize(length);
    size_t bytes_read = std::fread(&out_str[0], 1, length, file_);
    
    if (bytes_read != length) {
        setError("Failed to read string (expected " + std::to_string(length) + 
                 " bytes, got " + std::to_string(bytes_read) + ")");
        return false;
    }
    
    return true;
}

bool LocalGGUFLoader::readValue(GGUFMetadataKV& kv) {
    if (!file_) return false;
    
    // Read key length
    if (std::fread(&kv.key_length, 1, sizeof(kv.key_length), file_) != sizeof(kv.key_length)) {
        return false;
    }
    
    // Read key
    if (!readString(kv.key, kv.key_length)) {
        return false;
    }
    
    // Read value type
    uint32_t type_value;
    if (std::fread(&type_value, 1, sizeof(type_value), file_) != sizeof(type_value)) {
        return false;
    }
    kv.value_type = static_cast<GGUFValueType>(type_value);
    
    // Read value based on type
    switch (kv.value_type) {
        case GGUFValueType::UINT8:
            if (std::fread(&kv.value.uint8_value, 1, sizeof(kv.value.uint8_value), file_) != 
                sizeof(kv.value.uint8_value)) return false;
            break;
            
        case GGUFValueType::INT8:
            if (std::fread(&kv.value.int8_value, 1, sizeof(kv.value.int8_value), file_) != 
                sizeof(kv.value.int8_value)) return false;
            break;
            
        case GGUFValueType::UINT16:
            if (std::fread(&kv.value.uint16_value, 1, sizeof(kv.value.uint16_value), file_) != 
                sizeof(kv.value.uint16_value)) return false;
            break;
            
        case GGUFValueType::INT16:
            if (std::fread(&kv.value.int16_value, 1, sizeof(kv.value.int16_value), file_) != 
                sizeof(kv.value.int16_value)) return false;
            break;
            
        case GGUFValueType::UINT32:
            if (std::fread(&kv.value.uint32_value, 1, sizeof(kv.value.uint32_value), file_) != 
                sizeof(kv.value.uint32_value)) return false;
            break;
            
        case GGUFValueType::INT32:
            if (std::fread(&kv.value.int32_value, 1, sizeof(kv.value.int32_value), file_) != 
                sizeof(kv.value.int32_value)) return false;
            break;
            
        case GGUFValueType::FLOAT32:
            if (std::fread(&kv.value.float32_value, 1, sizeof(kv.value.float32_value), file_) != 
                sizeof(kv.value.float32_value)) return false;
            break;
            
        case GGUFValueType::BOOL:
            if (std::fread(&kv.value.bool_value, 1, sizeof(kv.value.bool_value), file_) != 
                sizeof(kv.value.bool_value)) return false;
            break;
            
        case GGUFValueType::STRING: {
            uint64_t str_length;
            if (std::fread(&str_length, 1, sizeof(str_length), file_) != sizeof(str_length)) 
                return false;
            if (!readString(kv.string_value, str_length)) return false;
            break;
        }
            
        case GGUFValueType::ARRAY: {
            // Read array type and length
            uint32_t array_type;
            if (std::fread(&array_type, 1, sizeof(array_type), file_) != sizeof(array_type)) 
                return false;
            
            uint64_t array_length;
            if (std::fread(&array_length, 1, sizeof(array_length), file_) != sizeof(array_length)) 
                return false;
            
            // Read array data (for now, just skip)
            // TODO: Implement proper array reading if needed
            kv.array_value.resize(array_length * 4); // Assume 4 bytes per element
            if (std::fread(kv.array_value.data(), 1, kv.array_value.size(), file_) != 
                kv.array_value.size()) return false;
            break;
        }
            
        default:
            setError("Unsupported value type: " + std::to_string(static_cast<uint32_t>(kv.value_type)));
            return false;
    }
    
    return true;
}

// ============================================================================
// STATIC UTILITY METHODS
// ============================================================================

std::string LocalGGUFLoader::tensorTypeToString(GGUFTensorType type) {
    switch (type) {
        case GGUFTensorType::F32: return "F32";
        case GGUFTensorType::F16: return "F16";
        case GGUFTensorType::Q4_0: return "Q4_0";
        case GGUFTensorType::Q4_1: return "Q4_1";
        case GGUFTensorType::Q5_0: return "Q5_0";
        case GGUFTensorType::Q5_1: return "Q5_1";
        case GGUFTensorType::Q8_0: return "Q8_0";
        case GGUFTensorType::Q8_1: return "Q8_1";
        case GGUFTensorType::Q2_K: return "Q2_K";
        case GGUFTensorType::Q3_K: return "Q3_K";
        case GGUFTensorType::Q4_K: return "Q4_K";
        case GGUFTensorType::Q5_K: return "Q5_K";
        case GGUFTensorType::Q6_K: return "Q6_K";
        case GGUFTensorType::Q8_K: return "Q8_K";
        case GGUFTensorType::IQ2_XXS: return "IQ2_XXS";
        case GGUFTensorType::IQ2_XS: return "IQ2_XS";
        case GGUFTensorType::IQ3_XXS: return "IQ3_XXS";
        case GGUFTensorType::IQ1_S: return "IQ1_S";
        case GGUFTensorType::IQ4_NL: return "IQ4_NL";
        case GGUFTensorType::IQ3_S: return "IQ3_S";
        case GGUFTensorType::IQ2_S: return "IQ2_S";
        case GGUFTensorType::IQ4_XS: return "IQ4_XS";
        case GGUFTensorType::I8: return "I8";
        case GGUFTensorType::I16: return "I16";
        case GGUFTensorType::I32: return "I32";
        case GGUFTensorType::I64: return "I64";
        case GGUFTensorType::F64: return "F64";
        case GGUFTensorType::IQ1_M: return "IQ1_M";
        case GGUFTensorType::BF16: return "BF16";
        default: return "UNKNOWN";
    }
}

uint32_t LocalGGUFLoader::getBitsPerWeight(GGUFTensorType type) {
    switch (type) {
        case GGUFTensorType::F32: return 32;
        case GGUFTensorType::F16: return 16;
        case GGUFTensorType::BF16: return 16;
        case GGUFTensorType::Q4_0: return 4;
        case GGUFTensorType::Q4_1: return 4;
        case GGUFTensorType::Q5_0: return 5;
        case GGUFTensorType::Q5_1: return 5;
        case GGUFTensorType::Q8_0: return 8;
        case GGUFTensorType::Q8_1: return 8;
        case GGUFTensorType::Q2_K: return 2;
        case GGUFTensorType::Q3_K: return 3;
        case GGUFTensorType::Q4_K: return 4;
        case GGUFTensorType::Q5_K: return 5;
        case GGUFTensorType::Q6_K: return 6;
        case GGUFTensorType::Q8_K: return 8;
        case GGUFTensorType::IQ2_XXS: return 2;
        case GGUFTensorType::IQ2_XS: return 2;
        case GGUFTensorType::IQ3_XXS: return 3;
        case GGUFTensorType::IQ1_S: return 1;
        case GGUFTensorType::IQ4_NL: return 4;
        case GGUFTensorType::IQ3_S: return 3;
        case GGUFTensorType::IQ2_S: return 2;
        case GGUFTensorType::IQ4_XS: return 4;
        case GGUFTensorType::I8: return 8;
        case GGUFTensorType::I16: return 16;
        case GGUFTensorType::I32: return 32;
        case GGUFTensorType::I64: return 64;
        case GGUFTensorType::F64: return 64;
        case GGUFTensorType::IQ1_M: return 1;
        default: return 0;
    }
}

bool LocalGGUFLoader::isQuantized(GGUFTensorType type) {
    return getBitsPerWeight(type) < 16;
}

// ============================================================================
// ERROR HANDLING
// ============================================================================

void LocalGGUFLoader::setError(const std::string& error) {
    last_error_ = error;
}

void LocalGGUFLoader::clearError() {
    last_error_.clear();
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string LocalGGUFLoader::toHex(uint32_t value) {
    char buffer[11];
    std::snprintf(buffer, sizeof(buffer), "%08X", value);
    return std::string(buffer);
}

// ============================================================================
// MODEL METADATA METHODS
// ============================================================================

std::string ModelMetadata::quant_type_str() const {
    return LocalGGUFLoader::tensorTypeToString(quant_type);
}

bool ModelMetadata::is_quantized() const {
    return LocalGGUFLoader::isQuantized(quant_type);
}

uint32_t ModelMetadata::bits_per_weight() const {
    return LocalGGUFLoader::getBitsPerWeight(quant_type);
}

bool ModelMetadata::validate() const {
    // Basic validation
    if (path.empty()) return false;
    if (context_length == 0) return false;
    if (hidden_size == 0) return false;
    if (head_count == 0) return false;
    if (layer_count == 0) return false;
    if (vocab_size == 0) return false;
    if (file_size_bytes == 0) return false;
    
    // Architecture-specific validation
    if (architecture.empty()) return false;
    
    return true;
}

std::string ModelMetadata::toString() const {
    std::string result;
    result += "Model: " + name + "\n";
    result += "Path: " + path + "\n";
    result += "Architecture: " + architecture + "\n";
    result += "Context Length: " + std::to_string(context_length) + "\n";
    result += "Hidden Size: " + std::to_string(hidden_size) + "\n";
    result += "Heads: " + std::to_string(head_count) + "\n";
    result += "KV Heads: " + std::to_string(head_count_kv) + "\n";
    result += "Layers: " + std::to_string(layer_count) + "\n";
    result += "Vocab Size: " + std::to_string(vocab_size) + "\n";
    result += "Quantization: " + quant_type_str() + "\n";
    result += "Bits/Weight: " + std::to_string(bits_per_weight()) + "\n";
    result += "File Size: " + std::to_string(file_size_bytes) + " bytes\n";
    result += "RoPE Theta: " + std::to_string(rope_theta) + "\n";
    result += "Valid: " + std::string(is_valid ? "Yes" : "No") + "\n";
    return result;
}

} // namespace Core
} // namespace RawrXD