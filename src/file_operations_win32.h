/*
 * file_operations_win32.h
 *
 * Replaces:
 *   - src/utils/qt_directory_manager.*
 *   - src/qtapp/utils/file_operations.*
 *
 * When RAWRXD_FILEOPS_INPROC is defined (e.g. RawrXD-Win32IDE): uses in-process
 * Win32 implementation (FileOpsInProcess.cpp). Otherwise uses
 * RawrXD_FileManager_Win32.dll if available.
 */

#pragma once

#include <string>
#include <vector>

#ifdef RAWRXD_FILEOPS_INPROC
// In-process implementation: methods defined in src/win32app/FileOpsInProcess.cpp
namespace RawrXD {
namespace FileOps {

class FileManager {
public:
    static FileManager& instance();
    bool initialize();
    std::vector<std::string> listFiles(const std::string& path, const std::string& pattern = "*");
    bool readFile(const std::string& path, std::string& content);
    bool writeFile(const std::string& path, const std::string& content);
    bool fileExists(const std::string& path);
    bool deleteFile(const std::string& path);
    bool createDirectory(const std::string& path);
    void shutdown();
};

} // namespace FileOps
} // namespace RawrXD

#else

#include <windows.h>
#include <memory>
#include <cstring>

#ifdef RAWRXD_WIN32_STATIC_BUILD
// Ship FileManager linked statically (from Ship/RawrXD_FileManager_Win32.cpp)
extern "C" {
bool __stdcall RawrXD_ListFiles(const char* path, const char* pattern, char** buffer, uint32_t* count);
bool __stdcall RawrXD_ReadFile(const char* path, char* buffer, uint32_t bufSize, uint32_t* bytesRead);
bool __stdcall RawrXD_WriteFileContent(const char* path, const char* content, uint32_t contentLen);
bool __stdcall RawrXD_FileExists(const char* path);
bool __stdcall RawrXD_DeleteFile(const char* path);
bool __stdcall RawrXD_CreateDirectory(const char* path);
}
#endif

namespace RawrXD {
namespace FileOps {

class FileManager {
public:
    static FileManager& instance() {
        static FileManager inst;
        return inst;
    }

    bool initialize() {
#ifdef RAWRXD_WIN32_STATIC_BUILD
        return true;
#else
        module_ = LoadLibraryW(L"RawrXD_FileManager_Win32.dll");
        return module_ != nullptr;
#endif
    }

    std::vector<std::string> listFiles(const std::string& path, const std::string& pattern = "*") {
        std::vector<std::string> results;
#ifdef RAWRXD_WIN32_STATIC_BUILD
        char buffer[32768] = {0};
        uint32_t count = 0;
        if (::RawrXD_ListFiles(path.c_str(), pattern.c_str(), &buffer, &count)) {
            char* current = buffer;
            for (uint32_t i = 0; i < count; i++) {
                results.push_back(current);
                current += std::strlen(current) + 1;
            }
        }
        return results;
#else
        if (!module_) return results;
        typedef bool(__stdcall *PFN_ListFiles)(const char*, const char*, char**, uint32_t*);
        PFN_ListFiles pfnList = (PFN_ListFiles)GetProcAddress(module_, "ListFiles");
        if (pfnList) {
            char buffer[32768] = {0};
            uint32_t count = 0;
            if (pfnList(path.c_str(), pattern.c_str(), &buffer, &count)) {
                char* current = buffer;
                for (uint32_t i = 0; i < count; i++) {
                    results.push_back(current);
                    current += std::strlen(current) + 1;
                }
            }
        }
        return results;
#endif
    }

    bool readFile(const std::string& path, std::string& content) {
#ifdef RAWRXD_WIN32_STATIC_BUILD
        char buffer[1048576] = {0};
        uint32_t bytesRead = 0;
        if (::RawrXD_ReadFile(path.c_str(), buffer, sizeof(buffer), &bytesRead)) {
            content.assign(buffer, bytesRead);
            return true;
        }
        return false;
#else
        if (!module_) return false;
        typedef bool(__stdcall *PFN_ReadFile)(const char*, char*, uint32_t, uint32_t*);
        PFN_ReadFile pfnRead = (PFN_ReadFile)GetProcAddress(module_, "ReadFile");
        if (!pfnRead) return false;
        char buffer[1048576] = {0};
        uint32_t bytesRead = 0;
        if (pfnRead(path.c_str(), buffer, sizeof(buffer), &bytesRead)) {
            content.assign(buffer, bytesRead);
            return true;
        }
        return false;
#endif
    }

    bool writeFile(const std::string& path, const std::string& content) {
#ifdef RAWRXD_WIN32_STATIC_BUILD
        return ::RawrXD_WriteFileContent(path.c_str(), content.c_str(), (uint32_t)content.length());
#else
        if (!module_) return false;
        typedef bool(__stdcall *PFN_WriteFile)(const char*, const char*, uint32_t);
        PFN_WriteFile pfnWrite = (PFN_WriteFile)GetProcAddress(module_, "WriteFile");
        return pfnWrite ? pfnWrite(path.c_str(), content.c_str(), (uint32_t)content.length()) : false;
#endif
    }

    bool fileExists(const std::string& path) {
#ifdef RAWRXD_WIN32_STATIC_BUILD
        return ::RawrXD_FileExists(path.c_str());
#else
        if (!module_) return false;
        typedef bool(__stdcall *PFN_Exists)(const char*);
        PFN_Exists pfnExists = (PFN_Exists)GetProcAddress(module_, "FileExists");
        return pfnExists ? pfnExists(path.c_str()) : false;
#endif
    }

    bool deleteFile(const std::string& path) {
#ifdef RAWRXD_WIN32_STATIC_BUILD
        return ::RawrXD_DeleteFile(path.c_str());
#else
        if (!module_) return false;
        typedef bool(__stdcall *PFN_Delete)(const char*);
        PFN_Delete pfnDelete = (PFN_Delete)GetProcAddress(module_, "DeleteFile");
        return pfnDelete ? pfnDelete(path.c_str()) : false;
#endif
    }

    bool createDirectory(const std::string& path) {
#ifdef RAWRXD_WIN32_STATIC_BUILD
        return ::RawrXD_CreateDirectory(path.c_str());
#else
        if (!module_) return false;
        typedef bool(__stdcall *PFN_CreateDir)(const char*);
        PFN_CreateDir pfnCreate = (PFN_CreateDir)GetProcAddress(module_, "CreateDirectory");
        return pfnCreate ? pfnCreate(path.c_str()) : false;
#endif
    }

    void shutdown() {
#ifndef RAWRXD_WIN32_STATIC_BUILD
        if (module_) { FreeLibrary(module_); module_ = nullptr; }
#endif
    }

private:
    FileManager() : module_(nullptr) {}
    HMODULE module_;
};

} // namespace FileOps
} // namespace RawrXD

#endif // !RAWRXD_FILEOPS_INPROC

// Helper functions (replaces standalone functions from Qt file utils)
namespace RawrXD {
namespace FileOps {

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

