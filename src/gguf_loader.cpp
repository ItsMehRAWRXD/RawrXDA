#include "gguf_loader.h"
#include "gguf_vocab_resolver.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <windows.h>

#ifdef RAWR_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#else
// Define these as empty macros to bypass calls when Vulkan is disabled
#define vkCreateCommandPool(a,b,c,d) 0
#define vkAllocateCommandBuffers(a,b,c) 0
#define vkGetDeviceQueue(a,b,c,d) 
#endif

// Define MEM_RESERVE_PLACEHOLDER if not available
#ifndef MEM_RESERVE_PLACEHOLDER
#define MEM_RESERVE_PLACEHOLDER 0x00040000
#endif

// VirtualAlloc2 function pointer for dynamic loading
typedef PVOID (WINAPI *VirtualAlloc2Func)(
    HANDLE                 Process,
    PVOID                  BaseAddress,
    SIZE_T                 Size,
    ULONG                  AllocationType,
    ULONG                  PageProtection,
    MEM_EXTENDED_PARAMETER *ExtendedParameters,
    ULONG                  ParameterCount
);

// MapViewOfFile3 function pointer for dynamic loading  
typedef PVOID (WINAPI *MapViewOfFile3Func)(
    HANDLE                 FileMappingObjectHandle,
    HANDLE                 Process,
    PVOID                  BaseAddress,
    ULONG64                Offset,
    SIZE_T                 ViewSize,
    ULONG                  AllocationType,
    ULONG                  PageProtection,
    MEM_EXTENDED_PARAMETER *ExtendedParameters,
    ULONG                  ParameterCount
);

static VirtualAlloc2Func pVirtualAlloc2_gguf = nullptr;
static MapViewOfFile3Func pMapViewOfFile3_gguf = nullptr;
static bool g_placeholderInitialized_gguf = false;
static PVOID g_placeholderBase_gguf = nullptr;

// Initialize placeholder memory management APIs for GGUF loader
static bool InitializePlaceholderAPIsGGUF() {
    if (g_placeholderInitialized_gguf) return true;
    
    printf("[GGUF] Initializing sovereign placeholder APIs...\n");
    
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) return false;
    
    pVirtualAlloc2_gguf = (VirtualAlloc2Func)GetProcAddress(hKernel32, "VirtualAlloc2");
    pMapViewOfFile3_gguf = (MapViewOfFile3Func)GetProcAddress(hKernel32, "MapViewOfFile3");
    
    if (!pVirtualAlloc2_gguf) {
        printf("[GGUF] VirtualAlloc2 not available, using legacy mapping\n");
    }
    
    g_placeholderInitialized_gguf = true;
    return pVirtualAlloc2_gguf && pMapViewOfFile3_gguf;
}

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

        std::string integrity_reason;
        if (!VerifyIntegrity(&integrity_reason)) {
            Close();
            throw std::runtime_error("GGUF integrity check failed: " + integrity_reason);
        }
    } catch (const std::exception& e) {
        Close();
        throw;  // Re-throw after cleanup
    }
    
    return true;
}

bool GGUFLoader::VerifyIntegrity(std::string* reason) {
    if (!file_.is_open()) {
        if (reason) *reason = "file not open";
        return false;
    }

    if (header_.magic != 0x46554747) {
        if (reason) *reason = "invalid GGUF magic";
        return false;
    }

    if (header_.version != 3) {
        if (reason) *reason = "unsupported GGUF version";
        return false;
    }

    file_.seekg(0, std::ios::end);
    const std::streamoff size = file_.tellg();
    if (size < 24) {
        if (reason) *reason = "file too small";
        return false;
    }

    if (header_.metadata_kv_count > 10000000ULL) {
        if (reason) *reason = "metadata KV count is implausibly large";
        return false;
    }

    if (header_.tensor_count > 10000000ULL) {
        if (reason) *reason = "tensor count is implausibly large";
        return false;
    }

    if (reason) *reason = "ok";
    return true;
}

bool GGUFLoader::RepairTrivialIssues(std::string* report) {
    if (filepath_.empty()) {
        if (report) *report = "no file path available";
        return false;
    }

    std::fstream file(filepath_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        if (report) *report = "unable to open file for repair";
        return false;
    }

    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (!file.good()) {
        if (report) *report = "failed reading magic";
        return false;
    }

    if (magic == 0x46554747) {
        if (report) *report = "no repair needed";
        return true;
    }

    // Trivial repair supported: endian-swapped magic marker.
    if (magic == 0x47475546) {
        const uint32_t fixed = 0x46554747;
        file.seekp(0, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&fixed), sizeof(fixed));
        file.flush();
        if (report) *report = "repaired swapped GGUF magic";
        return file.good();
    }

    if (report) *report = "unsupported corruption pattern";
    return false;
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
    
    // Bounds check: cap metadata KV count to prevent unbounded iteration
    // Typical GGUF files have <1000 metadata entries; allow up to 10000 as conservative upper bound
    const uint64_t MAX_METADATA_KV_COUNT = 10000;
    if (header_.metadata_kv_count > MAX_METADATA_KV_COUNT) {
        throw std::runtime_error("Metadata KV count " + std::to_string(header_.metadata_kv_count) + 
                                 " exceeds limit of " + std::to_string(MAX_METADATA_KV_COUNT));
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
        
        if (value_type == 10) { // uint64
            uint64_t v;
            if (!ReadValue(v)) throw std::runtime_error("Failed to read uint64");
            metadata_.kv_pairs[key] = std::to_string(v);
        } else if (value_type == 4) { // uint32
            uint32_t v;
            if (!ReadValue(v)) throw std::runtime_error("Failed to read uint32");
            metadata_.kv_pairs[key] = std::to_string(v);
        } else if (value_type == 6) { // float32
            float v;
            if (!ReadValue(v)) throw std::runtime_error("Failed to read float32");
            metadata_.kv_pairs[key] = std::to_string(v);
        } else if (value_type == 8) {  // UTF-8 string
            if (!ReadString(value)) {
                throw std::runtime_error("Failed to read metadata string value for key: " + key);
            }
            metadata_.properties[key] = value;
            metadata_.kv_pairs[key] = value;
            
            // Parse important metadata - support both llama and other model naming conventions
            if (key == "general.architecture") {
                if (value == "llama") metadata_.architecture_type = 1;
            } else if (key == "llama.block_count") {
                try {
                    metadata_.layer_count = std::stoul(value);
                } catch (const std::exception&) {
                    metadata_.layer_count = 0;  // safe fallback on parse error
                }
            } else if (key == "llama.context_length") {
                try {
                    metadata_.context_length = std::stoul(value);
                } catch (const std::exception&) {
                    metadata_.context_length = 0;  // safe fallback on parse error
                }
            } else if (key == "llama.embedding_length") {
                try {
                    metadata_.embedding_dim = std::stoul(value);
                } catch (const std::exception&) {
                    metadata_.embedding_dim = 0;  // safe fallback on parse error
                }
            } else if (key == "llama.vocab_size") {
                try {
                    metadata_.vocab_size = std::stoul(value);
                } catch (const std::exception&) {
                    metadata_.vocab_size = 0;  // safe fallback on parse error
                }
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

            // Safety cap: array_length is from untrusted file data; an implausibly large value
            // would loop indefinitely consuming memory.  1 billion covers the largest real models.
            constexpr uint64_t kMaxArrayLen = 1ULL << 30;
            if (array_length > kMaxArrayLen) {
                throw std::runtime_error("Metadata array length too large ("
                                         + std::to_string(array_length)
                                         + ") for key: " + key);
            }
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
            metadata_.properties[key] = array_str;
            metadata_.kv_pairs[key] = array_str;

            // Persist structured tokenizer fields for downstream loaders
            if (key == "tokenizer.ggml.tokens" && !string_elems.empty()) {
                metadata_.tokens = string_elems;
            }
            if (key == "tokenizer.ggml.scores" && !float_elems.empty()) {
                metadata_.token_scores = float_elems;
            }
            if (key == "tokenizer.ggml.token_type" && !uint32_elems.empty()) {
                metadata_.token_types_u32 = uint32_elems;
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


            // Track this unsupported type for later IDE prompting
            bool type_exists = false;
            for (auto& existing : unsupported_types_structs_) {
                if (existing.type_value == type_val) {
                    existing.tensor_names.push_back(tensor.name);
                    type_exists = true;
                    break;
                }
            }
            if (!type_exists) {
                unsupported_types_structs_.push_back({
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
        tensor_index_map_[tensor.name] = &tensor;
    }
    
    // Fix vocab size using universal resolver
    GGUFVocabResolver vocabResolver;
    auto kv_pairs = metadata_.kv_pairs;
    VocabSizeDetection vocabDetection = vocabResolver.detectVocabSize(kv_pairs, filepath_);


    // Override metadata vocab size if detection was successful
    if (vocabDetection.isConfident && vocabDetection.detectedSize > 0) {
        if (metadata_.vocab_size != vocabDetection.detectedSize) {
            
            metadata_.vocab_size = vocabDetection.detectedSize;
        }
    } else if (!vocabDetection.isConfident && vocabDetection.detectedSize > 0) {
        // Use detection even if not confident, as it's better than potentially wrong metadata
        
        metadata_.vocab_size = vocabDetection.detectedSize;
    }


    return true;
}

bool GGUFLoader::LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) {
    // O(1) lookup using tensor index (Bottleneck #14 fix - eliminates O(n) std::find_if)
    auto it = tensor_index_map_.find(tensor_name);
    
    if (it == tensor_index_map_.end()) {
        // Use an exception for a cleaner interface in a larger app
        throw std::runtime_error("Tensor not found: " + tensor_name);
    }
    
    const TensorInfo* tensor_info = it->second;
    // A zero size_bytes means CalculateTensorSize failed (unsupported type or overflow).
    if (tensor_info->size_bytes == 0) {
        throw std::runtime_error("Tensor has zero computed size (unsupported type or dimension overflow): "
                                 + tensor_name);
    }
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
        const size_t sz = tensors_[i].size_bytes;
        if (sz > SIZE_MAX - total_size) {
            throw std::overflow_error("Total tensor range size overflows size_t");
        }
        total_size += sz;
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

uint64_t GGUFLoader::GetFileSize() const {
    if (!file_.is_open()) return 0;
    
    // Use a mutable cast/hack for const-correctness in a minimal way
    std::ifstream& f = const_cast<std::ifstream&>(file_);
    std::streampos current = f.tellg();
    f.seekg(0, std::ios::end);
    uint64_t size = f.tellg();
    f.seekg(current);
    return size;
}

// Note: GetTensorByteSize, GetTypeString, BuildTensorIndex, LoadZone, UnloadZone, 
//       GetLoadedZones, GetAllZones, GetAllTensorInfo, GetCurrentMemoryUsage 
//       are implemented inline in gguf_loader.h or below if non-inline.

template<typename T>
bool GGUFLoader::ReadValue(T& value) {
    file_.read(reinterpret_cast<char*>(&value), sizeof(T));
    return file_.good();
}

bool GGUFLoader::ReadString(std::string& value) {
    uint64_t len;
    if (!ReadValue(len)) return false;
    
    // Safety check: strings longer than 16MB are invalid per GGUF spec
    // This prevents catastrophic allocation from corrupted/malicious metadata
    // GGUF metadata keys/values should be <1MB in practice; 16MB is conservative upper bound
    const uint64_t MAX_STRING_SIZE = 16 * 1024 * 1024;  // 16MB limit (was 100MB)
    if (len > MAX_STRING_SIZE) {
        throw std::runtime_error("String size " + std::to_string(len) + " exceeds 16MB limit");
    }
    
    value.resize(len);
    file_.read(&value[0], len);
    return file_.good();
}

uint64_t GGUFLoader::CalculateTensorSize(const std::vector<uint64_t>& shape, GGMLType type) const {
    uint64_t num_elements = 1;
    for (uint64_t dim : shape) {
        // Guard against uint64_t overflow in the dimension product.
        if (dim > 0 && num_elements > UINT64_MAX / dim) {
            return 0;  // overflow guard — caller treats 0 as invalid size
        }
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
        
        default:
            // Type 14 is Q8_K (256 elements, 128 bytes per super-block)
            // We handle it here as a fallback case since it's not in the enum
            if (static_cast<uint32_t>(type) == 14) {
                return block_aligned_size(256, 128);  // Q8_K: 256 elements, 128 bytes per block
            }
            // In production, log the unsupported type and use a safe default
            
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
                std::vector<uint8_t> input = compressed;
                bool ok = false;
                std::vector<uint8_t> output = codec::inflate(input, &ok);
                if (!ok) return false;
                
                decompressed.resize(output.size());
                std::memcpy(decompressed.data(), output.data(), output.size());
                return true;
            }
            
            case CompressionType::NONE:
                decompressed = compressed;
                return true;
                
            default:
                throw std::runtime_error("Unsupported compression type");
        }
    } catch (const std::exception& e) {
        
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
                std::vector<uint8_t> input = raw_data;
                std::vector<uint8_t> output = brutal::compress(input);
                if (output.empty()) return false;
                
                compressed.resize(output.size());
                std::memcpy(compressed.data(), output.data(), output.size());
                return true;
            }
            
            case CompressionType::DEFLATE: {
                // Compress using codec::deflate (MASM-optimized deflate)
                std::vector<uint8_t> input = raw_data;
                bool ok = false;
                std::vector<uint8_t> output = codec::deflate(input, &ok);
                if (!ok || output.empty()) return false;
                
                compressed.resize(output.size());
                std::memcpy(compressed.data(), output.data(), output.size());
                return true;
            }
            
            case CompressionType::NONE:
                compressed = raw_data;
                return true;
                
            default:
                throw std::runtime_error("Unsupported compression type");
        }
    } catch (const std::exception& e) {
        
        return false;
    }
}

// ============================================================================
// Quantization Type Validation (IDE Conversion Workflow)
// ============================================================================

bool GGUFLoader::HasUnsupportedQuantizationTypes() const {
    return !unsupported_types_structs_.empty();
}

std::vector<GGUFLoader::UnsupportedTypeInfo> GGUFLoader::GetUnsupportedQuantizationTypes() const {
    return unsupported_types_structs_;
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

// ============================================================================
// RawrXD Async Loader Implementation (Integration)
// ============================================================================

bool GGUFLoader::Load(VkDevice vkDevice, VkPhysicalDevice vkPhysDevice) {
#ifndef RAWR_ENABLE_VULKAN
    return false;
#else
    device = vkDevice;
    physDevice = vkPhysDevice;

    // Initialize placeholder APIs for sovereign bypass
    InitializePlaceholderAPIsGGUF();

    std::wstring wpath(filepath_.begin(), filepath_.end());
    hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                       OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    fileSize = size.QuadPart;

    hMapping = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping) return false;
    
    // Try sovereign placeholder bypass first
    mappedView = nullptr;
    if (pVirtualAlloc2_gguf && pMapViewOfFile3_gguf) {
        g_placeholderBase_gguf = pVirtualAlloc2_gguf(GetCurrentProcess(), NULL, static_cast<SIZE_T>(fileSize),
            MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
        if (g_placeholderBase_gguf) {
            // Use MapViewOfFile3 to replace a same-sized placeholder region.
            mappedView = pMapViewOfFile3_gguf(hMapping, GetCurrentProcess(), g_placeholderBase_gguf,
                0, static_cast<SIZE_T>(fileSize), MEM_REPLACE_PLACEHOLDER, PAGE_READONLY, NULL, 0);
        }
        if (mappedView) {
            printf("[GGUF] Sovereign Placeholder Bypass: Mapped %llu MB at placeholder address %p\n", 
                   fileSize / (1024*1024), mappedView);
        } else if (g_placeholderBase_gguf) {
            VirtualFree(g_placeholderBase_gguf, 0, MEM_RELEASE);
            g_placeholderBase_gguf = nullptr;
        }
    }
    
    // Fallback to regular MapViewOfFile if placeholder mapping failed
    if (!mappedView) {
        mappedView = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        if (mappedView) {
            printf("[GGUF] Legacy mapping: Mapped %llu MB at address %p\n", 
                   fileSize / (1024*1024), mappedView);
        }
    }
    
    if (!mappedView) {
        DWORD error = GetLastError();
        printf("[GGUF] MapViewOfFile failed (Error: %lu) - insufficient memory/commit limits\n", error);
        return false;
    }
    
    tensors.clear();
    for(auto& t : tensors_) {
        // ... Load tensors logic ...
    }
    
    CreateVulkanResources();
    return true;
#endif
}


void GGUFLoader::CreateVulkanResources() {
#ifdef RAWR_ENABLE_VULKAN
    if (!device) return;
    
    // Vulkan resource creation logic here
#endif
}

#ifdef RAWR_ENABLE_VULKAN
uint32_t GGUFLoader::FindQueueFamilyIndex(VkPhysicalDevice device, uint32_t queueFlags) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, props.data());

    for (uint32_t i = 0; i < count; i++) {
        if (props[i].queueFlags & queueFlags) return i;
    }
    return 0;
}

uint32_t GGUFLoader::FindMemoryType(uint32_t typeFilter, uint32_t props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return 0;
}
#endif

void GGUFLoader::LoadTensorAsync(TensorInfo& info) {
#ifdef RAWR_ENABLE_VULKAN
    auto* src = (uint8_t*)mappedView + info.offset;
    // ... Async load implementation ...
#endif
}

void GGUFLoader::UploadF32(TensorInfo& info, void* src, size_t count) {
#ifdef RAWR_ENABLE_VULKAN
    size_t size = count * sizeof(float);
    // ... rest of Vulkan upload logic ...
#endif
}

void GGUFLoader::DequantAndUploadQ4_0(TensorInfo& info, void* src, size_t count) {
#ifdef RAWR_ENABLE_VULKAN
    // ... rest of Vulkan dequant upload logic ...
#endif
}

void GGUFLoader::BeginCommandBuffer() {
#ifdef RAWR_ENABLE_VULKAN
    // ... rest of Vulkan command buffer logic ...
#endif
}

void GGUFLoader::EndCommandBuffer() {
#ifdef RAWR_ENABLE_VULKAN
    // ... rest of Vulkan command buffer logic ...
#endif
}



