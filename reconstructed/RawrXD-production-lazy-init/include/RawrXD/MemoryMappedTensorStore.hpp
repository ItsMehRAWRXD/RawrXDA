#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>

namespace RawrXD::Tools {

class MemoryMappedTensorStore {
public:
    struct TensorMapping {
        std::string name;
        uint32_t type;
        std::vector<uint64_t> dims;
        uint64_t offset;
        uint64_t size_bytes;
        
        // File mapping handle (owned)
        HANDLE h_mmap = nullptr;
        
        // View pointer (owned)
        void* p_view = nullptr;
        
        ~TensorMapping() {
            if (p_view) UnmapViewOfFile(p_view);
            if (h_mmap) CloseHandle(h_mmap);
        }
        
        // Delete copy, allow move
        TensorMapping(const TensorMapping&) = delete;
        TensorMapping& operator=(const TensorMapping&) = delete;
        
        TensorMapping(TensorMapping&& o) noexcept
            : name(std::move(o.name)), type(o.type), dims(std::move(o.dims)),
              offset(o.offset), size_bytes(o.size_bytes),
              h_mmap(o.h_mmap), p_view(o.p_view)
        {
            o.h_mmap = nullptr;
            o.p_view = nullptr;
        }
    };

    explicit MemoryMappedTensorStore(const std::string& filepath) 
        : filepath_(filepath) {
        
        h_file_ = CreateFileA(
            filepath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);
        
        if (h_file_ == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to open file: " + filepath);
        }

        LARGE_INTEGER liSize;
        if (!GetFileSizeEx(h_file_, &liSize)) {
            CloseHandle(h_file_);
            throw std::runtime_error("Failed to get file size");
        }
        file_size_ = liSize.QuadPart;
    }

    ~MemoryMappedTensorStore() {
        tensors_.clear();
        if (h_file_ != INVALID_HANDLE_VALUE) {
            CloseHandle(h_file_);
        }
    }

    /**
     * RegisterTensor - Map a tensor range from the file
     * Use this after parsing GGUF headers to lazy-load tensor data
     */
    TensorMapping* RegisterTensor(
        const std::string& name,
        uint32_t type,
        const std::vector<uint64_t>& dims,
        uint64_t offset,
        uint64_t size_bytes)
    {
        if (offset + size_bytes > static_cast<uint64_t>(file_size_)) {
            throw std::runtime_error("Tensor extent exceeds file bounds");
        }

        TensorMapping mapping;
        mapping.name = name;
        mapping.type = type;
        mapping.dims = dims;
        mapping.offset = offset;
        mapping.size_bytes = size_bytes;

        // Create file mapping for this range
        // Note: For simplicity, map the entire file once and reuse
        if (!global_mmap_) {
            global_mmap_ = CreateFileMappingA(
                h_file_,
                nullptr,
                PAGE_READONLY,
                static_cast<DWORD>(file_size_ >> 32),
                static_cast<DWORD>(file_size_ & 0xFFFFFFFF),
                nullptr);
            
            if (!global_mmap_) {
                throw std::runtime_error("Failed to create file mapping");
            }
        }

        // Map view for this specific tensor
        // Windows allows mapping subranges efficiently
        ULARGE_INTEGER liOffset;
        liOffset.QuadPart = offset;
        
        void* pView = MapViewOfFile(
            global_mmap_,
            FILE_MAP_READ,
            liOffset.HighPart,
            liOffset.LowPart,
            size_bytes);
        
        if (!pView) {
            throw std::runtime_error("Failed to map tensor view for " + name);
        }

        mapping.p_view = pView;
        mapping.h_mmap = nullptr; // Use global mapping
        
        tensors_.push_back(std::move(mapping));
        return &tensors_.back();
    }

    /**
     * GetTensorData - Get pointer to already-mapped tensor data (zero-copy)
     */
    const void* GetTensorData(const std::string& name) const {
        for (const auto& t : tensors_) {
            if (t.name == name && t.p_view) {
                return t.p_view;
            }
        }
        return nullptr;
    }

    /**
     * GetTensorInfo - Get metadata for registered tensor
     */
    const TensorMapping* GetTensorInfo(const std::string& name) const {
        for (const auto& t : tensors_) {
            if (t.name == name) return &t;
        }
        return nullptr;
    }

    size_t TensorCount() const { return tensors_.size(); }
    
    uint64_t FileSize() const { return file_size_; }

private:
    std::string filepath_;
    HANDLE h_file_ = INVALID_HANDLE_VALUE;
    HANDLE global_mmap_ = nullptr;
    uint64_t file_size_ = 0;
    std::vector<TensorMapping> tensors_;
};

} // namespace RawrXD::Tools
