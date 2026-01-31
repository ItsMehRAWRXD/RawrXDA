#pragma once
/*
 * RawrZ-MemoryBridge K1.6 - Cross-platform Memory-Mapped File Bridge
 * Educational implementation for AI model memory management
 * 
 * Features:
 * - Cross-platform memory-mapped file access (Windows/Linux)
 * - Safe memory operations with bounds checking
 * - Hot-patch capability for in-memory model modifications
 * - Undo/rollback functionality
 * - Educational tensor manipulation examples
 */

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#  include <windows.h>
#  include <memoryapi.h>
#else
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace RawrZ {

// Memory-mapped file wrapper with RAII
class MemoryBridge {
public:
    explicit MemoryBridge(const std::string& name, size_t size, bool create = true) 
        : name_(name), size_(size) {
        
#ifdef _WIN32
        if (create) {
            handle_ = CreateFileMappingA(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE,
                static_cast<DWORD>(size >> 32),
                static_cast<DWORD>(size & 0xFFFFFFFF),
                name.c_str()
            );
        } else {
            handle_ = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
        }
        
        if (!handle_) {
            throw std::runtime_error("Failed to create/open memory mapping: " + name);
        }
        
        base_ptr_ = static_cast<uint8_t*>(MapViewOfFile(
            handle_, 
            FILE_MAP_ALL_ACCESS, 
            0, 0, size
        ));
        
        if (!base_ptr_) {
            CloseHandle(handle_);
            throw std::runtime_error("Failed to map view of file");
        }
#else
        int flags = create ? (O_CREAT | O_RDWR) : O_RDWR;
        fd_ = shm_open(name.c_str(), flags, 0666);
        
        if (fd_ == -1) {
            throw std::runtime_error("Failed to open shared memory: " + name);
        }
        
        if (create && ftruncate(fd_, size) == -1) {
            close(fd_);
            throw std::runtime_error("Failed to set shared memory size");
        }
        
        base_ptr_ = static_cast<uint8_t*>(mmap(
            nullptr, size, 
            PROT_READ | PROT_WRITE, 
            MAP_SHARED, fd_, 0
        ));
        
        if (base_ptr_ == MAP_FAILED) {
            close(fd_);
            throw std::runtime_error("Failed to map shared memory");
        }
#endif
        
        std::cout << "[MemoryBridge] Mapped " << size << " bytes at " << static_cast<void*>(base_ptr_) << std::endl;
    }
    
    ~MemoryBridge() {
#ifdef _WIN32
        if (base_ptr_) UnmapViewOfFile(base_ptr_);
        if (handle_) CloseHandle(handle_);
#else
        if (base_ptr_ && base_ptr_ != MAP_FAILED) munmap(base_ptr_, size_);
        if (fd_ != -1) close(fd_);
#endif
    }
    
    // Non-copyable, moveable
    MemoryBridge(const MemoryBridge&) = delete;
    MemoryBridge& operator=(const MemoryBridge&) = delete;
    MemoryBridge(MemoryBridge&& other) noexcept = default;
    MemoryBridge& operator=(MemoryBridge&& other) noexcept = default;
    
    // Safe memory access with bounds checking
    template<typename T>
    T* get_ptr(size_t offset = 0) {
        if (offset + sizeof(T) > size_) {
            throw std::out_of_range("Access beyond memory boundary");
        }
        return reinterpret_cast<T*>(base_ptr_ + offset);
    }
    
    template<typename T>
    const T* get_ptr(size_t offset = 0) const {
        if (offset + sizeof(T) > size_) {
            throw std::out_of_range("Access beyond memory boundary");
        }
        return reinterpret_cast<const T*>(base_ptr_ + offset);
    }
    
    // Write data with bounds checking
    void write(size_t offset, const void* data, size_t length) {
        if (offset + length > size_) {
            throw std::out_of_range("Write beyond memory boundary");
        }
        std::memcpy(base_ptr_ + offset, data, length);
    }
    
    // Read data with bounds checking
    void read(size_t offset, void* data, size_t length) const {
        if (offset + length > size_) {
            throw std::out_of_range("Read beyond memory boundary");
        }
        std::memcpy(data, base_ptr_ + offset, length);
    }
    
    uint8_t* base() { return base_ptr_; }
    const uint8_t* base() const { return base_ptr_; }
    size_t size() const { return size_; }
    const std::string& name() const { return name_; }
    
private:
    std::string name_;
    size_t size_;
    uint8_t* base_ptr_ = nullptr;
    
#ifdef _WIN32
    HANDLE handle_ = nullptr;
#else
    int fd_ = -1;
#endif
};

// Patch management with undo capability
struct PatchEntry {
    size_t offset;
    std::vector<uint8_t> original_data;
    std::vector<uint8_t> patch_data;
    std::string description;
};

class HotPatcher {
public:
    explicit HotPatcher(MemoryBridge& bridge) : bridge_(bridge) {}
    
    // Apply a patch with automatic backup for undo
    bool apply_patch(size_t offset, const std::vector<uint8_t>& patch_data, 
                     const std::string& description = "") {
        try {
            // Backup original data
            std::vector<uint8_t> original(patch_data.size());
            bridge_.read(offset, original.data(), original.size());
            
            // Apply patch
            bridge_.write(offset, patch_data.data(), patch_data.size());
            
            // Store undo information
            patches_.push_back({
                offset, 
                std::move(original), 
                patch_data, 
                description
            });
            
            std::cout << "[HotPatcher] Applied patch at 0x" << std::hex << offset 
                      << " (" << patch_data.size() << " bytes): " << description << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[HotPatcher] Failed to apply patch: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Load patch from file
    bool apply_patch_from_file(size_t offset, const std::string& filename,
                               const std::string& description = "") {
        try {
            std::ifstream file(filename, std::ios::binary);
            if (!file) {
                throw std::runtime_error("Cannot open patch file: " + filename);
            }
            
            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            std::vector<uint8_t> data(size);
            file.read(reinterpret_cast<char*>(data.data()), size);
            
            return apply_patch(offset, data, description.empty() ? filename : description);
            
        } catch (const std::exception& e) {
            std::cerr << "[HotPatcher] Error loading patch file: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Undo last patch
    bool undo_last() {
        if (patches_.empty()) {
            std::cout << "[HotPatcher] No patches to undo" << std::endl;
            return false;
        }
        
        auto& last = patches_.back();
        try {
            bridge_.write(last.offset, last.original_data.data(), last.original_data.size());
            std::cout << "[HotPatcher] Undid patch: " << last.description << std::endl;
            patches_.pop_back();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[HotPatcher] Failed to undo patch: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Undo all patches
    void undo_all() {
        while (!patches_.empty()) {
            undo_last();
        }
    }
    
    // List applied patches
    void list_patches() const {
        std::cout << "[HotPatcher] Applied patches (" << patches_.size() << "):" << std::endl;
        for (size_t i = 0; i < patches_.size(); ++i) {
            const auto& patch = patches_[i];
            std::cout << "  " << i << ": 0x" << std::hex << patch.offset 
                      << " (" << std::dec << patch.patch_data.size() << " bytes) - " 
                      << patch.description << std::endl;
        }
    }
    
private:
    MemoryBridge& bridge_;
    std::vector<PatchEntry> patches_;
};

// Educational example: Simple tensor operations
namespace TensorOps {
    
    // Simple tensor header for educational purposes
    struct TensorHeader {
        uint32_t magic = 0x52415752; // "RAWR"
        uint32_t version = 1;
        uint32_t data_type; // 0=float32, 1=float16, 2=int8
        uint32_t dimensions;
        uint32_t shape[4]; // Up to 4D tensors for simplicity
        uint64_t data_offset;
        uint64_t data_size;
    };
    
    // Write a tensor to memory bridge
    bool write_tensor(MemoryBridge& bridge, size_t offset, 
                      const std::vector<uint32_t>& shape,
                      uint32_t data_type, const void* data, size_t data_size) {
        try {
            TensorHeader header{};
            header.data_type = data_type;
            header.dimensions = static_cast<uint32_t>(shape.size());
            for (size_t i = 0; i < shape.size() && i < 4; ++i) {
                header.shape[i] = shape[i];
            }
            header.data_offset = offset + sizeof(TensorHeader);
            header.data_size = data_size;
            
            bridge.write(offset, &header, sizeof(header));
            bridge.write(header.data_offset, data, data_size);
            
            std::cout << "[TensorOps] Wrote tensor at 0x" << std::hex << offset 
                      << " (data: " << std::dec << data_size << " bytes)" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[TensorOps] Error writing tensor: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Read tensor header
    bool read_tensor_header(const MemoryBridge& bridge, size_t offset, TensorHeader& header) {
        try {
            bridge.read(offset, &header, sizeof(header));
            if (header.magic != 0x52415752) {
                std::cerr << "[TensorOps] Invalid tensor magic at 0x" << std::hex << offset << std::endl;
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[TensorOps] Error reading tensor header: " << e.what() << std::endl;
            return false;
        }
    }
}

} // namespace RawrZ