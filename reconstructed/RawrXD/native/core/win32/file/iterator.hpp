// native_core/win32_file_iterator.hpp
#pragma once
#include <windows.h>
#include <string>
#include <functional>

namespace RawrXD::Native {

class FileIterator {
    WIN32_FIND_DATAW findData_{};
    HANDLE hFind_ = INVALID_HANDLE_VALUE;
    std::wstring pattern_;
    bool first_ = true;

public:
    explicit FileIterator(const wchar_t* directory) {
        pattern_ = directory;
        pattern_ += L"\\*";
    }

    ~FileIterator() { if (hFind_ != INVALID_HANDLE_VALUE) FindClose(hFind_); }

    // Delete copy/move—RAII handle
    FileIterator(const FileIterator&) = delete;
    FileIterator& operator=(const FileIterator&) = delete;

    bool ForEach(std::function<bool(const WIN32_FIND_DATAW&)> callback) {
        hFind_ = FindFirstFileW(pattern_.c_str(), &findData_);
        if (hFind_ == INVALID_HANDLE_VALUE) return false;

        do {
            if (findData_.cFileName[0] == L'.') continue; // Skip . and ..
            if (!callback(findData_)) break;
        } while (FindNextFileW(hFind_, &findData_));

        return true;
    }

    static bool Exists(const wchar_t* path) {
        DWORD attribs = GetFileAttributesW(path);
        return attribs != INVALID_FILE_ATTRIBUTES;
    }

    static uint64_t GetSize(const wchar_t* path) {
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data)) return 0;
        return (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    }
};

} // namespace RawrXD::Native