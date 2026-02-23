// RawrXD_FileOperations.hpp - Production-grade file and directory operations
// Pure C++20 / Win32 - No Qt Dependencies

#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>

namespace fs = std::filesystem;

namespace RawrXD {

enum class Encoding {
    UTF8,
    UTF16_LE,
    UTF16_BE,
    ASCII,
    Unknown
};

struct FileOperationResult {
    bool success = false;
    std::wstring errorMessage;
    std::wstring backupPath;

    FileOperationResult() = default;
    explicit FileOperationResult(bool ok) : success(ok) {}
    FileOperationResult(bool ok, const std::wstring& msg) : success(ok), errorMessage(msg) {}
};

class FileManager {
public:
    FileManager();
    ~FileManager();

    // Reading
    bool readFile(const std::wstring& path, std::wstring& content, Encoding* detectedEncoding = nullptr);
    bool readFileRaw(const std::wstring& path, std::vector<uint8_t>& data);
    static Encoding detectEncoding(const std::vector<uint8_t>& data);

    // Writing
    FileOperationResult writeFile(const std::wstring& path, const std::wstring& content, bool createBackupFlag = true);
    FileOperationResult writeFileRaw(const std::wstring& path, const std::vector<uint8_t>& data, bool createBackupFlag = true);

    // File CRUD
    FileOperationResult createFile(const std::wstring& path);
    FileOperationResult deleteFile(const std::wstring& path, bool moveToTrash = true);
    FileOperationResult renameFile(const std::wstring& oldPath, const std::wstring& newPath);
    FileOperationResult moveFile(const std::wstring& sourcePath, const std::wstring& destPath);
    FileOperationResult copyFile(const std::wstring& sourcePath, const std::wstring& destPath, bool overwrite = false);

    // Directory operations
    FileOperationResult createDirectory(const std::wstring& path);
    FileOperationResult deleteDirectory(const std::wstring& path, bool moveToTrash = true);
    FileOperationResult copyDirectory(const std::wstring& sourcePath, const std::wstring& destPath);

    // Path operations
    static std::wstring toAbsolutePath(const std::wstring& relativePath, const std::wstring& basePath = L"");
    static std::wstring toRelativePath(const std::wstring& absolutePath, const std::wstring& basePath);

    // Info helpers
    static bool exists(const std::wstring& path);
    static bool isFile(const std::wstring& path);
    static bool isDirectory(const std::wstring& path);
    static bool isSymlink(const std::wstring& path);
    static bool isReadable(const std::wstring& path);
    static bool isWritable(const std::wstring& path);
    static int64_t fileSize(const std::wstring& path);
    static uint64_t lastModified(const std::wstring& path);

    // Backup
    std::wstring createBackup(const std::wstring& path);

    void setAutoBackup(bool enable);
    bool isAutoBackupEnabled() const;

    static std::wstring createTempPath(const std::wstring& originalPath);

private:
    bool m_autoBackup = true;

    static std::wstring formatWinError(DWORD code);
    static bool moveToTrash(const std::wstring& path);
    static std::wstring utf8ToWide(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> wideToUtf8(const std::wstring& text);
};

} // namespace RawrXD
