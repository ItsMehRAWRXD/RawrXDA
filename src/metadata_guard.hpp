#pragma once

#include <Windows.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <type_traits>

namespace rawrxd::gguf {

// Zero-copy file map view (read-only)
class FileView {
public:
    explicit FileView(const std::string& path);
    ~FileView();

    FileView(const FileView&) = delete;
    FileView& operator=(const FileView&) = delete;

    bool valid() const noexcept { return base_ != nullptr; }
    size_t size() const noexcept { return size_; }

    template<typename T>
    const T* ptr_at(size_t offset) const noexcept {
        if (offset > size_ || offset + sizeof(T) > size_) return nullptr;
        return reinterpret_cast<const T*>(static_cast<const uint8_t*>(base_) + offset);
    }

    template<typename T>
    T* ptr_at(size_t offset) noexcept {
        if (offset > size_ || offset + sizeof(T) > size_) return nullptr;
        return reinterpret_cast<T*>(static_cast<uint8_t*>(base_) + offset);
    }

private:
    HANDLE file_{INVALID_HANDLE_VALUE};
    HANDLE mapping_{nullptr};
    void* base_{nullptr};
    size_t size_{0};

    static std::wstring ToWide(const std::string& path);
};

// Simple string view into mapped file
class GStringView {
public:
    GStringView() = default;
    GStringView(const char* data, uint64_t len) noexcept
        : data_(data), len_(len) {}

    uint64_t size() const noexcept { return len_; }
    bool empty() const noexcept { return len_ == 0; }
    const char* data() const noexcept { return data_; }

    bool equals(const char* str) const noexcept;
    bool starts_with(const char* prefix) const noexcept;

private:
    const char* data_{nullptr};
    uint64_t len_{0};
};

enum class ValueType : uint32_t {
    UINT8 = 0, INT8, UINT16, INT16, UINT32, INT32,
    FLOAT32, BOOL, STRING, ARRAY, UINT64, INT64, FLOAT64
};

class MetadataScanner {
public:
    explicit MetadataScanner(FileView& view) noexcept;

    bool seek(size_t position) noexcept;
    bool skip_entry() noexcept;
    bool read_key(GStringView& out) noexcept;
    bool read_value_type(ValueType& out) noexcept;
    bool skip_value(ValueType type) noexcept;
    bool read_string(GStringView& out, uint64_t max_len = 0xA00000) noexcept;
    bool skip_large_block(uint64_t bytes) noexcept;
    bool check_bounds(uint64_t advance) const noexcept;
    size_t tell() const noexcept { return offset_; }

private:
    FileView& file_;
    size_t offset_{0};
    bool strict_mode_{true};

    bool read_uint32(uint32_t& out) noexcept;
    bool read_uint64(uint64_t& out) noexcept;
    bool skip_bytes(uint64_t bytes) noexcept;

    static uint64_t SafeMultiply(uint64_t a, uint64_t b) noexcept;
    static uint32_t ElementSize(ValueType type) noexcept;
};

} // namespace rawrxd::gguf
