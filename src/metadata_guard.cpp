#include "metadata_guard.hpp"

#include <algorithm>

namespace rawrxd::gguf {

namespace {
constexpr char kSkippedString[] = "[skipped]";
}

std::wstring FileView::ToWide(const std::string& path) {
    if (path.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring wide;
    wide.resize(len);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide.data(), len);
    if (!wide.empty()) {
        wide.resize(len - 1);
    }
    return wide;
}

FileView::FileView(const std::string& path) {
    const std::wstring wide_path = ToWide(path);
    file_ = CreateFileW(wide_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file_ == INVALID_HANDLE_VALUE) {
        return;
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file_, &size)) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
        return;
    }

    if (size.QuadPart == 0) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
        return;
    }

    size_ = static_cast<size_t>(size.QuadPart);
    mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping_) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
        return;
    }

    base_ = MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0);
    if (!base_) {
        CloseHandle(mapping_);
        CloseHandle(file_);
        mapping_ = nullptr;
        file_ = INVALID_HANDLE_VALUE;
    }
}

FileView::~FileView() {
    if (base_) {
        UnmapViewOfFile(base_);
        base_ = nullptr;
    }
    if (mapping_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
    }
    if (file_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
    }
}

bool GStringView::equals(const char* str) const noexcept {
    if (!str || !data_) return false;
    size_t idx = 0;
    while (idx < len_ && str[idx]) {
        if (data_[idx] != str[idx]) return false;
        ++idx;
    }
    return idx == len_ && str[idx] == '\0';
}

bool GStringView::starts_with(const char* prefix) const noexcept {
    if (!prefix || !data_) return false;
    size_t idx = 0;
    while (prefix[idx] && idx < len_) {
        if (data_[idx] != prefix[idx]) return false;
        ++idx;
    }
    return prefix[idx] == '\0';
}

MetadataScanner::MetadataScanner(FileView& view) noexcept
    : file_(view), offset_(0), strict_mode_(true) {}

bool MetadataScanner::seek(size_t position) noexcept {
    if (position > file_.size()) return false;
    offset_ = position;
    return true;
}

bool MetadataScanner::skip_entry() noexcept {
    GStringView key;
    if (!read_key(key)) return false;
    ValueType type;
    if (!read_value_type(type)) return false;
    return skip_value(type);
}

bool MetadataScanner::read_key(GStringView& out) noexcept {
    if (!check_bounds(sizeof(uint32_t))) return false;
    const uint32_t* len_ptr = file_.ptr_at<uint32_t>(offset_);
    if (!len_ptr) return false;
    const uint32_t key_len = *len_ptr;
    offset_ += sizeof(uint32_t);
    if (!check_bounds(key_len)) return false;
    const char* data = file_.ptr_at<char>(offset_);
    if (!data) return false;
    out = GStringView(data, key_len);
    offset_ += key_len;
    return true;
}

bool MetadataScanner::read_value_type(ValueType& out) noexcept {
    uint32_t base = 0;
    if (!read_uint32(base)) return false;
    out = static_cast<ValueType>(base);
    return true;
}

bool MetadataScanner::read_string(GStringView& out, uint64_t max_len) noexcept {
    uint64_t len = 0;
    if (!read_uint64(len)) return false;
    if (!check_bounds(len)) return false;
    if (len > max_len) {
        skip_bytes(len);
        out = GStringView(kSkippedString, sizeof(kSkippedString) - 1);
        return true;
    }
    const char* data = file_.ptr_at<char>(offset_);
    if (!data) return false;
    out = GStringView(data, len);
    offset_ += len;
    return true;
}

bool MetadataScanner::skip_value(ValueType type) noexcept {
    switch (type) {
        case ValueType::STRING: {
            uint64_t len = 0;
            if (!read_uint64(len)) return false;
            return skip_bytes(len);
        }
        case ValueType::ARRAY: {
            uint32_t elem = 0;
            if (!read_uint32(elem)) return false;
            uint64_t arr_len = 0;
            if (!read_uint64(arr_len)) return false;
            const ValueType elem_type = static_cast<ValueType>(elem);
            uint32_t elem_size = ElementSize(elem_type);
            if (elem_size == 0) {
                if (strict_mode_) return false;
                elem_size = 8;
            }
            uint64_t total = SafeMultiply(arr_len, elem_size);
            if (total == 0 && arr_len != 0) return false;
            return skip_bytes(total);
        }
        default: {
            uint32_t scalar_size = ElementSize(type);
            if (scalar_size == 0) return false;
            return skip_bytes(scalar_size);
        }
    }
}

bool MetadataScanner::skip_large_block(uint64_t bytes) noexcept {
    return skip_bytes(bytes);
}

bool MetadataScanner::check_bounds(uint64_t advance) const noexcept {
    if (offset_ > file_.size()) return false;
    return advance <= (file_.size() - offset_);
}

bool MetadataScanner::read_uint32(uint32_t& out) noexcept {
    if (!check_bounds(sizeof(uint32_t))) return false;
    const uint32_t* val = file_.ptr_at<uint32_t>(offset_);
    if (!val) return false;
    out = *val;
    offset_ += sizeof(uint32_t);
    return true;
}

bool MetadataScanner::read_uint64(uint64_t& out) noexcept {
    if (!check_bounds(sizeof(uint64_t))) return false;
    const uint64_t* val = file_.ptr_at<uint64_t>(offset_);
    if (!val) return false;
    out = *val;
    offset_ += sizeof(uint64_t);
    return true;
}

bool MetadataScanner::skip_bytes(uint64_t bytes) noexcept {
    if (!check_bounds(bytes)) return false;
    offset_ += bytes;
    return true;
}

uint64_t MetadataScanner::SafeMultiply(uint64_t a, uint64_t b) noexcept {
    if (b == 0 || a == 0) return 0;
    if (a > UINT64_MAX / b) return 0;
    return a * b;
}

uint32_t MetadataScanner::ElementSize(ValueType type) noexcept {
    switch (type) {
        case ValueType::UINT8: case ValueType::INT8: return 1;
        case ValueType::UINT16: case ValueType::INT16: return 2;
        case ValueType::UINT32: case ValueType::INT32: case ValueType::FLOAT32: return 4;
        case ValueType::UINT64: case ValueType::INT64: case ValueType::FLOAT64: return 8;
        case ValueType::BOOL: return 1;
        default: return 0;
    }
}

} // namespace rawrxd::gguf
