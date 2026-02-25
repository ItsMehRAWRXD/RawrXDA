/*
 * file_operations_win32.h
 * 
 * Replaces:
 *   - src/utils/qt_directory_manager.*
 *   - src/qtapp/utils/file_operations.*
 *   - // , // , // Info from Qt
 * 
 * Uses: RawrXD_FileManager_Win32.dll (FindFirstFileW, CreateFileW)
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace FileOps {

class FileManager {
public:
    static FileManager& instance() {
        static FileManager inst;
        return inst;
    }
    
    bool initialize() {
        module_ = LoadLibraryW(L"RawrXD_FileManager_Win32.dll");
        return module_ != nullptr;
    }
    
    // List files in directory (replaces // Dir::entryList)
    std::vector<std::string> listFiles(const std::string& path, const std::string& pattern = "*") {
        std::vector<std::string> results;
        
        if (!module_) return results;
        
        typedef bool(__stdcall *PFN_ListFiles)(const char*, const char*, char**, uint32_t*);
        PFN_ListFiles pfnList = (PFN_ListFiles)GetProcAddress(module_, "ListFiles");
        
        if (pfnList) {
            char buffer[32768] = {0};  // Max 256 files * 128 chars each
            uint32_t count = 0;
            
            if (pfnList(path.c_str(), pattern.c_str(), &buffer, &count)) {
                char* current = buffer;
                for (uint32_t i = 0; i < count; i++) {
                    results.push_back(current);
                    current += strlen(current) + 1;
                }
            }
        }
        
        return results;
    }
    
    // Read file content (replaces // File::readAll)
    bool readFile(const std::string& path, std::string& content) {
        if (!module_) return false;
        
        typedef bool(__stdcall *PFN_ReadFile)(const char*, char*, uint32_t, uint32_t*);
        PFN_ReadFile pfnRead = (PFN_ReadFile)GetProcAddress(module_, "ReadFile");
        
        if (!pfnRead) return false;
        
        char buffer[1048576] = {0};  // 1 MB max
        uint32_t bytesRead = 0;
        
        if (pfnRead(path.c_str(), buffer, sizeof(buffer), &bytesRead)) {
            content.assign(buffer, bytesRead);
            return true;
        }
        
        return false;
    }
    
    // Write file content (replaces // File::write)
    bool writeFile(const std::string& path, const std::string& content) {
        if (!module_) return false;
        
        typedef bool(__stdcall *PFN_WriteFile)(const char*, const char*, uint32_t);
        PFN_WriteFile pfnWrite = (PFN_WriteFile)GetProcAddress(module_, "WriteFile");
        
        return pfnWrite ? pfnWrite(path.c_str(), content.c_str(), content.length()) : false;
    }
    
    // Check if file exists (replaces // File::exists)
    bool fileExists(const std::string& path) {
        if (!module_) return false;
        
        typedef bool(__stdcall *PFN_Exists)(const char*);
        PFN_Exists pfnExists = (PFN_Exists)GetProcAddress(module_, "FileExists");
        
        return pfnExists ? pfnExists(path.c_str()) : false;
    }
    
    // Delete file (replaces // File::remove)
    bool deleteFile(const std::string& path) {
        if (!module_) return false;
        
        typedef bool(__stdcall *PFN_Delete)(const char*);
        PFN_Delete pfnDelete = (PFN_Delete)GetProcAddress(module_, "DeleteFile");
        
        return pfnDelete ? pfnDelete(path.c_str()) : false;
    }
    
    // Create directory (replaces // Dir::mkpath)
    bool createDirectory(const std::string& path) {
        if (!module_) return false;
        
        typedef bool(__stdcall *PFN_CreateDir)(const char*);
        PFN_CreateDir pfnCreate = (PFN_CreateDir)GetProcAddress(module_, "CreateDirectory");
        
        return pfnCreate ? pfnCreate(path.c_str()) : false;
    }
    
    void shutdown() {
        if (module_) FreeLibrary(module_);
        module_ = nullptr;
    }
    
private:
    FileManager() : module_(nullptr) {}
    HMODULE module_;
};

// Helper functions (replaces standalone functions from // /// )

inline std::vector<std::string> listFilesInDirectory(const std::string& path) {
    return FileManager::instance().listFiles(path, "*");
}

inline bool readFileContent(const std::string& path, std::string& content) {
    return FileManager::instance().readFile(path, content);
}

inline bool writeFileContent(const std::string& path, const std::string& content) {
    return FileManager::instance().writeFile(path, content);
}

inline bool fileExists(const std::string& path) {
    return FileManager::instance().fileExists(path);
}

} // namespace FileOps
} // namespace RawrXD

