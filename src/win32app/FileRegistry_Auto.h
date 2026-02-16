#pragma once

#include <windows.h>
#include <string>
#include <vector>

struct FileEntry;

class FileRegistry {
public:
    // Register individual file
    static void registerFile(const std::string& filepath);
    
    // Register all files (auto-scan or CMake-generated)
    static void registerAllFiles();
    
    // Create Win32 menu structure
    static HMENU createFileMenu();
    
    // Get file path from menu command ID
    static std::string getFilePath(UINT commandId);
    
    // Query registered files
    static std::vector<FileEntry> getAllFiles();
    static std::vector<FileEntry> getFilesByCategory(const std::string& category);
    static int getFileCount();
    static std::vector<std::string> getCategories();
};
