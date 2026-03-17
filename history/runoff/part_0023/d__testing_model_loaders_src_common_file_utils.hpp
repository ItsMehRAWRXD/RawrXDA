// ============================================================================
// File: src/common/file_utils.hpp
// Purpose: File/directory utilities replacing QFile/QDir/QFileInfo
// No Qt dependency - pure C++17 with std::filesystem
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#endif

namespace fs = std::filesystem;

namespace FileUtils {

inline bool exists(const std::string& path) {
    return fs::exists(path);
}

inline bool isDirectory(const std::string& path) {
    return fs::is_directory(path);
}

inline bool isFile(const std::string& path) {
    return fs::is_regular_file(path);
}

inline std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

inline bool writeFile(const std::string& path, const std::string& content) {
    // Ensure directory exists
    auto parent = fs::path(path).parent_path();
    if (!parent.empty() && !fs::exists(parent)) {
        fs::create_directories(parent);
    }
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file << content;
    return file.good();
}

inline bool createDirectories(const std::string& path) {
    std::error_code ec;
    return fs::create_directories(path, ec);
}

inline std::string absolutePath(const std::string& path) {
    return fs::absolute(path).string();
}

inline std::string fileName(const std::string& path) {
    return fs::path(path).filename().string();
}

inline std::string parentPath(const std::string& path) {
    return fs::path(path).parent_path().string();
}

inline std::string extension(const std::string& path) {
    return fs::path(path).extension().string();
}

inline uintmax_t fileSize(const std::string& path) {
    if (!fs::exists(path)) return 0;
    return fs::file_size(path);
}

inline std::vector<std::string> listDirectory(const std::string& path) {
    std::vector<std::string> entries;
    if (!fs::exists(path)) return entries;
    for (const auto& entry : fs::directory_iterator(path)) {
        entries.push_back(entry.path().string());
    }
    return entries;
}

inline std::vector<std::string> listDirectoryRecursive(const std::string& path) {
    std::vector<std::string> entries;
    if (!fs::exists(path)) return entries;
    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        entries.push_back(entry.path().string());
    }
    return entries;
}

inline std::string getAppDataPath() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path) == S_OK) {
        return std::string(path);
    }
    return ".";
#else
    const char* home = getenv("HOME");
    if (home) return std::string(home) + "/.local/share";
    return ".";
#endif
}

inline std::string getTempPath() {
    return fs::temp_directory_path().string();
}

/**
 * @brief Memory-mapped file helper (replacing QFile::map)
 */
struct MappedFile {
    void* data = nullptr;
    size_t size = 0;
#ifdef _WIN32
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = NULL;
#else
    int fd = -1;
#endif
    bool valid = false;

    bool open(const std::string& path) {
#ifdef _WIN32
        hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        
        LARGE_INTEGER fileSize;
        GetFileSizeEx(hFile, &fileSize);
        size = static_cast<size_t>(fileSize.QuadPart);
        
        hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!hMapping) { CloseHandle(hFile); return false; }
        
        data = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        if (!data) { CloseHandle(hMapping); CloseHandle(hFile); return false; }
#else
        fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0) return false;
        
        struct stat st;
        fstat(fd, &st);
        size = static_cast<size_t>(st.st_size);
        
        data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) { data = nullptr; ::close(fd); return false; }
#endif
        valid = true;
        return true;
    }

    void close() {
        if (!valid) return;
#ifdef _WIN32
        if (data) UnmapViewOfFile(data);
        if (hMapping) CloseHandle(hMapping);
        if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
#else
        if (data) munmap(data, size);
        if (fd >= 0) ::close(fd);
#endif
        data = nullptr;
        size = 0;
        valid = false;
    }

    ~MappedFile() { close(); }

    // Non-copyable
    MappedFile() = default;
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&& other) noexcept 
        : data(other.data), size(other.size), valid(other.valid) {
#ifdef _WIN32
        hFile = other.hFile;
        hMapping = other.hMapping;
        other.hFile = INVALID_HANDLE_VALUE;
        other.hMapping = NULL;
#else
        fd = other.fd;
        other.fd = -1;
#endif
        other.data = nullptr;
        other.valid = false;
    }
};

} // namespace FileUtils
