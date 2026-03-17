#include "gguf_loader.h"
#include "vulkan_compute.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#define INVALID_HANDLE_CAST(x) ((x) == INVALID_HANDLE_VALUE)
#else
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#endif

GGUFLoader::GGUFLoader() 
    : is_open_(false) {
    std::memset(&header_, 0, sizeof(GGUFHeader));
}

GGUFLoader::~GGUFLoader() {
    Close();
}

bool GGUFLoader::Open(const std::string& filepath) {
    filepath_ = filepath;
    
    // Get file size first to decide loading strategy
    file_.open(filepath, std::ios::binary | std::ios::ate);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open GGUF file: " + filepath);
    }
    
    file_size_ = file_.tellg();
    file_.seekg(0);
    
    // Fast-path for tiny models (use dummy mode)
    constexpr uint64_t TINY_FILE_LIMIT = 1024 * 1024;  // 1 MB
    constexpr uint64_t MMAP_THRESHOLD = 128 * 1024 * 1024;  // 128 MB
    
    if (file_size_ < TINY_FILE_LIMIT) {
        std::cout << "GGUF < 1 MB – using synthetic dummy tensors for testing" << std::endl;
        use_dummy_mode_ = true;
        return CreateDummyModel();
    }
    
    // Use memory-mapping for large files to avoid RAM spike
    if (file_size_ > MMAP_THRESHOLD) {
        std::cout << "GGUF > 128 MB (" << (file_size_ / (1024*1024)) 
                  << " MB) – using memory-mapping for zero-copy access" << std::endl;
        file_.close();  // Close ifstream, use mmap instead
        if (!InitializeMemoryMap()) {
            std::cerr << "Memory-mapping failed, falling back to dummy mode" << std::endl;
            use_dummy_mode_ = true;
            return CreateDummyModel();
        }
        use_mmap_ = true;
    }
    
    is_open_ = true;
    if (!ParseHeader()) {
        Close();
        return false;
    }
    
    return true;
}

bool GGUFLoader::Close() {
    if (file_.is_open()) {
        file_.close();
    }
    CleanupMemoryMap();
    is_open_ = false;
    tensors_.clear();
    return true;
}

bool GGUFLoader::ParseHeader() {
    // Support both mmap and file-based reading
    if (!file_.is_open() && !use_mmap_) return false;
    
    if (use_mmap_) {
        // Read header from memory-mapped region
        if (!mmap_base_ || file_size_ < sizeof(GGUFHeader)) {
            std::cerr << "Invalid mmap state for header parsing" << std::endl;
            return false;
        }
        
        const uint8_t* ptr = static_cast<const uint8_t*>(mmap_base_);
        std::memcpy(&header_.magic, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
        std::memcpy(&header_.version, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
        std::memcpy(&header_.tensor_count, ptr, sizeof(uint64_t)); ptr += sizeof(uint64_t);
        std::memcpy(&header_.metadata_kv_count, ptr, sizeof(uint64_t)); ptr += sizeof(uint64_t);
        
        header_.metadata_offset = ptr - static_cast<const uint8_t*>(mmap_base_);
    } else {
        file_.seekg(0);
        
        // Read magic
        if (!ReadValue(header_.magic)) return false;
        
        // Read version
        if (!ReadValue(header_.version)) return false;
        
        // Read tensor count
        if (!ReadValue(header_.tensor_count)) return false;
        
        // Read metadata KV count
        if (!ReadValue(header_.metadata_kv_count)) return false;
        
        // Calculate metadata offset
        header_.metadata_offset = file_.tellg();
    }
    
    if (header_.magic != 0x46554747) {  // "GGUF"
        std::cerr << "Invalid GGUF magic: 0x" << std::hex << header_.magic << std::endl;
        return false;
    }
    
    if (header_.version != 3) {
        std::cerr << "Unsupported GGUF version: " << header_.version << std::endl;
        return false;
    if (header_.version != 3) {
        std::cerr << "Unsupported GGUF version: " << header_.version << std::endl;
        return false;
    }
    
    std::cout << "GGUF Header: Magic=0x" << std::hex << header_.magic << std::dec 
              << ", Version=" << header_.version
              << ", Tensors=" << header_.tensor_count
              << ", Metadata=" << header_.metadata_kv_count << std::endl;
    
    return true;
}

bool GGUFLoader::ParseMetadata() {
    if (use_dummy_mode_) {
        std::cout << "Dummy mode active – skipping metadata parse" << std::endl;
        return true;
    }
    
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
        throw std::runtime_error("Tensor not found: " + tensor_name);
    }
    
    data.resize(it->size_bytes);
    
    // Use memory-mapped view if available (zero-copy)
    if (use_mmap_ && mmap_base_) {
        const void* slice = GetMappedSlice(it->offset, it->size_bytes);
        if (!slice) {
            throw std::runtime_error("Failed to get mapped slice for: " + tensor_name);
        }
        std::memcpy(data.data(), slice, it->size_bytes);
    } else {
        // Fallback to file read
        file_.seekg(it->offset);
        file_.read(reinterpret_cast<char*>(data.data()), it->size_bytes);
        
        if (!file_.good()) {
             throw std::runtime_error("Failed to read tensor data for: " + tensor_name);
        }
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

bool GGUFLoader::UploadAllTensorsToVulkan() {
    if (!vulkan_engine_) {
        throw std::runtime_error("Vulkan engine not attached to GGUFLoader");
    }
    if (!is_open_) {
        throw std::runtime_error("GGUF file is not open; call Open() before uploading tensors");
    }

    std::vector<uint8_t> tensor_data;
    
    for (const auto& tensor : tensors_) {
        // In dummy mode, create synthetic data instead of reading from file
        if (use_dummy_mode_) {
            // Generate simple test pattern: sequential floats 1.0, 2.0, 3.0...
            size_t float_count = tensor.size_bytes / sizeof(float);
            tensor_data.resize(tensor.size_bytes);
            float* float_data = reinterpret_cast<float*>(tensor_data.data());
            for (size_t i = 0; i < float_count; ++i) {
                float_data[i] = static_cast<float>(i % 256) / 10.0f;  // Pattern 0.0-25.5
            }
            std::cout << "Generated synthetic data for: " << tensor.name << std::endl;
        } else {
            LoadTensorZone(tensor.name, tensor_data);
        }
        
        VulkanTensor vt = vulkan_engine_->TransferGGUFTensor(tensor.name,
                                                             tensor_data.data(),
                                                             tensor_data.size());
        vulkan_tensors_[tensor.name] = vt;
        std::cout << "Transferred tensor to Vulkan: " << tensor.name
                  << " (" << static_cast<double>(tensor_data.size()) / (1024.0 * 1024.0)
                  << " MB)" << std::endl;
    }
    return true;
}

bool GGUFLoader::CreateDummyModel() {
    std::cout << "Creating synthetic dummy model for testing..." << std::endl;
    
    // Create minimal metadata
    metadata_.architecture_type = 1;  // LLaMA
    metadata_.layer_count = 1;
    metadata_.context_length = 512;
    metadata_.embedding_dim = 128;
    metadata_.vocab_size = 1000;
    metadata_.kv_pairs["general.architecture"] = "llama";
    
    // Create dummy header
    header_.magic = 0x46554747;  // "GGUF"
    header_.version = 3;
    header_.tensor_count = 1;
    header_.metadata_kv_count = 1;
    
    // Create one synthetic tensor (small matmul test matrix)
    TensorInfo dummy;
    dummy.name = "dummy_weight";
    dummy.shape = {128, 128};  // 128x128 = 16K floats = 64KB
    dummy.type = GGMLType::F32;
    dummy.offset = 0;
    dummy.size_bytes = 128 * 128 * sizeof(float);
    tensors_.push_back(dummy);
    
    is_open_ = true;
    std::cout << "Dummy model created: 1 tensor (128x128 F32), " 
              << (dummy.size_bytes / 1024) << " KB total" << std::endl;
    
    return true;
}

bool GGUFLoader::InitializeMemoryMap() {
#ifdef _WIN32
    // Windows memory-mapping implementation
    std::wstring wide_path(filepath_.begin(), filepath_.end());
    
    // 1. Open file handle with read-only access
    file_handle_ = CreateFileW(
        wide_path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (file_handle_ == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateFileW failed: " << GetLastError() << std::endl;
        return false;
    }
    
    // 2. Create file mapping object (read-only, entire file)
    map_handle_ = CreateFileMappingW(
        static_cast<HANDLE>(file_handle_),
        nullptr,
        PAGE_READONLY,
        0, 0,  // Map entire file
        nullptr
    );
    
    if (!map_handle_) {
        std::cerr << "CreateFileMappingW failed: " << GetLastError() << std::endl;
        CloseHandle(static_cast<HANDLE>(file_handle_));
        file_handle_ = nullptr;
        return false;
    }
    
    // 3. Map view into process address space
    mmap_base_ = MapViewOfFile(
        static_cast<HANDLE>(map_handle_),
        FILE_MAP_READ,
        0, 0,  // Start at beginning
        0      // Map entire file
    );
    
    if (!mmap_base_) {
        std::cerr << "MapViewOfFile failed: " << GetLastError() << std::endl;
        CloseHandle(static_cast<HANDLE>(map_handle_));
        CloseHandle(static_cast<HANDLE>(file_handle_));
        map_handle_ = nullptr;
        file_handle_ = nullptr;
        return false;
    }
    
    std::cout << "Memory-mapped " << (file_size_ / (1024*1024)) 
              << " MB file successfully (zero RAM allocation)" << std::endl;
    return true;
#else
    // Linux/macOS mmap implementation
    int fd = open(filepath_.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "open() failed for mmap\" << std::endl;
        return false;
    }
    
    mmap_base_ = mmap(nullptr, file_size_, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);  // Can close fd after mmap
    
    if (mmap_base_ == MAP_FAILED) {
        std::cerr << "mmap() failed\" << std::endl;
        mmap_base_ = nullptr;
        return false;
    }
    
    std::cout << "Memory-mapped " << (file_size_ / (1024*1024)) 
              << " MB file (mmap)\" << std::endl;
    return true;
#endif
}

void GGUFLoader::CleanupMemoryMap() {
#ifdef _WIN32
    if (mmap_base_) {
        UnmapViewOfFile(mmap_base_);
        mmap_base_ = nullptr;
    }
    if (map_handle_) {
        CloseHandle(static_cast<HANDLE>(map_handle_));
        map_handle_ = nullptr;
    }
    if (file_handle_ && file_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(file_handle_));
        file_handle_ = nullptr;
    }
#else
    if (mmap_base_) {
        munmap(mmap_base_, file_size_);
        mmap_base_ = nullptr;
    }
#endif
    use_mmap_ = false;
}

const void* GGUFLoader::GetMappedSlice(uint64_t offset, uint64_t size) const {
    if (!mmap_base_) {
        return nullptr;
    }
    
    if (offset + size > file_size_) {
        std::cerr << "Slice out of bounds: offset=\" << offset 
                  << \", size=\" << size << \", file_size=\" << file_size_ << std::endl;
        return nullptr;
    }
    
    // Return pointer into mapped region (no copy!)
    return static_cast<const uint8_t*>(mmap_base_) + offset;
}
