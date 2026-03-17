#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace NativeIDE {

class FileManager {
public:
    FileManager() = default;

    bool CreateFile(const std::wstring& filePath);
    bool DeleteFile(const std::wstring& filePath);
    bool CopyFile(const std::wstring& source, const std::wstring& destination);
    bool FileExists(const std::wstring& filePath);
    std::vector<std::wstring> ListDirectory(const std::wstring& dirPath);
};

} // namespace NativeIDE
