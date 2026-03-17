#include "file_manager.h"
#include <filesystem>
#include <fstream>

// File Manager Implementation
// This provides basic file operations for the IDE

namespace NativeIDE {
    
bool FileManager::CreateFile(const std::wstring& filePath) {
    std::ofstream file(filePath);
    return file.good();
}

bool FileManager::DeleteFile(const std::wstring& filePath) {
    try {
        return std::filesystem::remove(filePath);
    } catch (...) {
        return false;
    }
}

bool FileManager::CopyFile(const std::wstring& source, const std::wstring& destination) {
    try {
        return std::filesystem::copy_file(source, destination);
    } catch (...) {
        return false;
    }
}
    
bool FileManager::FileExists(const std::wstring& filePath) {
    return std::filesystem::exists(filePath);
}

std::vector<std::wstring> FileManager::ListDirectory(const std::wstring& dirPath) {
    std::vector<std::wstring> files;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            files.push_back(entry.path().wstring());
        }
    } catch (...) {
        // Return empty vector on error
    }
    return files;
}
    
}