/*
 * RawrXD_FileManager_Win32.cpp
 * Pure Win32 replacement for Qt file operations
 * Uses: FindFileW, CreateFileW, ReadFile, Win32 directory APIs
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cstring>

#ifndef RAWRXD_WIN32_STATIC_BUILD
#define RAWRXD_SHIP_EXPORT __declspec(dllexport)
#else
#define RAWRXD_SHIP_EXPORT
#endif

struct FileInfo {
    wchar_t fileName[MAX_PATH];
    wchar_t filePath[MAX_PATH];
    uint64_t fileSize;
    FILETIME createdTime;
    FILETIME modifiedTime;
    bool isDirectory;
};

class RawrXDFileManager {
private:
    wchar_t m_currentPath[MAX_PATH];
    std::vector<FileInfo> m_currentFiles;
    mutable CRITICAL_SECTION m_criticalSection;
    
public:
    RawrXDFileManager() {
        InitializeCriticalSection(&m_criticalSection);
        GetCurrentDirectoryW(MAX_PATH, m_currentPath);
    }
    
    ~RawrXDFileManager() {
        DeleteCriticalSection(&m_criticalSection);
    }
    
    bool ChangeDirectory(const wchar_t* path) {
        EnterCriticalSection(&m_criticalSection);
        
        DWORD attrs = GetFileAttributesW(path);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            LeaveCriticalSection(&m_criticalSection);
            return false;
        }
        
        wcscpy_s(m_currentPath, MAX_PATH, path);
        m_currentFiles.clear();
        
        LeaveCriticalSection(&m_criticalSection);
        return ListFiles(path);
    }
    
    bool ListFiles(const wchar_t* path) {
        EnterCriticalSection(&m_criticalSection);
        m_currentFiles.clear();
        
        wchar_t searchPath[MAX_PATH];
        wcscpy_s(searchPath, MAX_PATH, path);
        wcscat_s(searchPath, MAX_PATH, L"\\*");
        
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(searchPath, &findData);
        
        if (hFind == INVALID_HANDLE_VALUE) {
            LeaveCriticalSection(&m_criticalSection);
            return false;
        }
        
        do {
            if (wcscmp(findData.cFileName, L".") != 0 &&
                wcscmp(findData.cFileName, L"..") != 0) {
                
                FileInfo info;
                wcscpy_s(info.fileName, MAX_PATH, findData.cFileName);
                wcscpy_s(info.filePath, MAX_PATH, path);
                wcscat_s(info.filePath, MAX_PATH, L"\\");
                wcscat_s(info.filePath, MAX_PATH, findData.cFileName);
                
                info.isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                info.createdTime = findData.ftCreationTime;
                info.modifiedTime = findData.ftLastWriteTime;
                
                if (!info.isDirectory) {
                    uint64_t size = ((uint64_t)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
                    info.fileSize = size;
                }
                
                m_currentFiles.push_back(info);
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
        LeaveCriticalSection(&m_criticalSection);
        return true;
    }
    
    size_t GetFileCount() const {
        EnterCriticalSection(&m_criticalSection);
        size_t count = m_currentFiles.size();
        LeaveCriticalSection(&m_criticalSection);
        return count;
    }
    
    bool GetFileInfo(size_t index, wchar_t* nameBuffer, size_t nameSize,
        uint64_t* fileSize, bool* isDir) {
        
        EnterCriticalSection(&m_criticalSection);
        
        if (index >= m_currentFiles.size()) {
            LeaveCriticalSection(&m_criticalSection);
            return false;
        }
        
        const FileInfo& info = m_currentFiles[index];
        wcscpy_s(nameBuffer, nameSize, info.fileName);
        *fileSize = info.fileSize;
        *isDir = info.isDirectory;
        
        LeaveCriticalSection(&m_criticalSection);
        return true;
    }
    
    bool ReadFileContent(const wchar_t* filePath, char* buffer, size_t bufSize, size_t& bytesRead) {
        HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        
        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }
        
        DWORD dwRead;
        BOOL result = ReadFile(hFile, buffer, (DWORD)bufSize, &dwRead, nullptr);
        bytesRead = dwRead;
        
        CloseHandle(hFile);
        return result == TRUE;
    }
    
    bool WriteFileContent(const wchar_t* filePath, const char* buffer, size_t bufSize) {
        HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        
        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }
        
        DWORD dwWritten;
        BOOL result = WriteFile(hFile, buffer, (DWORD)bufSize, &dwWritten, nullptr);
        
        CloseHandle(hFile);
        return result == TRUE;
    }
    
    bool DeleteFile(const wchar_t* filePath) {
        return DeleteFileW(filePath) == TRUE;
    }
    
    bool CreateDirectory(const wchar_t* dirPath) {
        return CreateDirectoryW(dirPath, nullptr) == TRUE;
    }
    
    void GetCurrentPath(wchar_t* buffer, size_t bufSize) const {
        EnterCriticalSection(&m_criticalSection);
        wcscpy_s(buffer, bufSize, m_currentPath);
        LeaveCriticalSection(&m_criticalSection);
    }
    
    bool FileExists(const wchar_t* filePath) const {
        DWORD attrs = GetFileAttributesW(filePath);
        return attrs != INVALID_FILE_ATTRIBUTES;
    }
    
    uint64_t GetFileSize(const wchar_t* filePath) const {
        WIN32_FILE_ATTRIBUTE_DATA attrs;
        if (!GetFileAttributesExW(filePath, GetFileExInfoStandard, &attrs)) {
            return 0;
        }
        return ((uint64_t)attrs.nFileSizeHigh << 32) | attrs.nFileSizeLow;
    }
};

static RawrXDFileManager* g_fileManager = nullptr;

static RawrXDFileManager* GetFileManagerSingleton() {
    if (!g_fileManager)
        g_fileManager = new RawrXDFileManager();
    return g_fileManager;
}

// UTF-8 to wchar_t (for legacy C API used by file_operations_win32.h)
static int Utf8ToWide(const char* utf8, wchar_t* wide, int wideSize) {
    if (!utf8 || !wide || wideSize <= 0) return 0;
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wideSize);
    return len > 0 ? len - 1 : 0;
}

static int WideToUtf8(const wchar_t* wide, char* utf8, int utf8Size) {
    if (!wide || !utf8 || utf8Size <= 0) return 0;
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Size, nullptr, nullptr);
    return len > 0 ? len - 1 : 0;
}

// Legacy C API for file_operations_win32.h (char* = UTF-8). Used when linked statically.
extern "C" {

enum { kListFilesBufferSize = 32768 };

RAWRXD_SHIP_EXPORT bool __stdcall RawrXD_ListFiles(const char* path, const char* pattern, char** buffer, uint32_t* count) {
    if (!path || !buffer || !count) return false;
    wchar_t wpath[MAX_PATH];
    if (Utf8ToWide(path, wpath, MAX_PATH) < 0) return false;
    RawrXDFileManager* m = GetFileManagerSingleton();
    if (!m->ListFiles(wpath)) return false;
    size_t n = m->GetFileCount();
    if (n > 256) n = 256;
    *count = (uint32_t)n;
    char* out = *buffer;
    if (!out) { *count = (uint32_t)n; return true; }
    size_t used = 0;
    uint32_t writtenCount = 0;
    for (size_t i = 0; i < n && used + 260 < kListFilesBufferSize; i++) {
        wchar_t wname[MAX_PATH];
        uint64_t sz;
        bool isDir;
        if (!m->GetFileInfo(i, wname, MAX_PATH, &sz, &isDir)) break;
        int written = WideToUtf8(wname, out + used, (int)(kListFilesBufferSize - used));
        if (written <= 0) break;
        used += (size_t)written + 1;
        writtenCount++;
    }
    *count = writtenCount;
    return true;
}

RAWRXD_SHIP_EXPORT bool __stdcall RawrXD_ReadFile(const char* path, char* buffer, uint32_t bufSize, uint32_t* bytesRead) {
    if (!path || !buffer || !bytesRead) return false;
    wchar_t wpath[MAX_PATH];
    if (Utf8ToWide(path, wpath, MAX_PATH) < 0) return false;
    RawrXDFileManager* m = GetFileManagerSingleton();
    size_t read = 0;
    bool ok = m->ReadFileContent(wpath, buffer, bufSize, read);
    *bytesRead = (uint32_t)read;
    return ok;
}

RAWRXD_SHIP_EXPORT bool __stdcall RawrXD_WriteFileContent(const char* path, const char* content, uint32_t contentLen) {
    if (!path || !content) return false;
    wchar_t wpath[MAX_PATH];
    if (Utf8ToWide(path, wpath, MAX_PATH) < 0) return false;
    RawrXDFileManager* m = GetFileManagerSingleton();
    return m->WriteFileContent(wpath, content, contentLen);
}

RAWRXD_SHIP_EXPORT bool __stdcall RawrXD_FileExists(const char* path) {
    if (!path) return false;
    wchar_t wpath[MAX_PATH];
    if (Utf8ToWide(path, wpath, MAX_PATH) < 0) return false;
    RawrXDFileManager* m = GetFileManagerSingleton();
    return m->FileExists(wpath);
}

RAWRXD_SHIP_EXPORT bool __stdcall RawrXD_DeleteFile(const char* path) {
    if (!path) return false;
    wchar_t wpath[MAX_PATH];
    if (Utf8ToWide(path, wpath, MAX_PATH) < 0) return false;
    RawrXDFileManager* m = GetFileManagerSingleton();
    return m->DeleteFile(wpath);
}

RAWRXD_SHIP_EXPORT bool __stdcall RawrXD_CreateDirectory(const char* path) {
    if (!path) return false;
    wchar_t wpath[MAX_PATH];
    if (Utf8ToWide(path, wpath, MAX_PATH) < 0) return false;
    RawrXDFileManager* m = GetFileManagerSingleton();
    return m->CreateDirectory(wpath);
}

} // legacy API

extern "C" {
    RAWRXD_SHIP_EXPORT void* __stdcall CreateFileManager() {
        if (!g_fileManager) {
            g_fileManager = new RawrXDFileManager();
        }
        return g_fileManager;
    }
    
    RAWRXD_SHIP_EXPORT void __stdcall DestroyFileManager(void* mgr) {
        if (mgr && mgr == g_fileManager) {
            delete g_fileManager;
            g_fileManager = nullptr;
        }
    }
    
    RAWRXD_SHIP_EXPORT bool __stdcall FileManager_ChangeDirectory(void* mgr, const wchar_t* path) {
        RawrXDFileManager* m = static_cast<RawrXDFileManager*>(mgr);
        return m ? m->ChangeDirectory(path) : false;
    }
    
    RAWRXD_SHIP_EXPORT bool __stdcall FileManager_ListFiles(void* mgr, const wchar_t* path) {
        RawrXDFileManager* m = static_cast<RawrXDFileManager*>(mgr);
        return m ? m->ListFiles(path) : false;
    }
    
    RAWRXD_SHIP_EXPORT size_t __stdcall FileManager_GetFileCount(void* mgr) {
        RawrXDFileManager* m = static_cast<RawrXDFileManager*>(mgr);
        return m ? m->GetFileCount() : 0;
    }
    
    RAWRXD_SHIP_EXPORT bool __stdcall FileManager_GetFileInfo(void* mgr, size_t index,
        wchar_t* nameBuffer, size_t nameSize, uint64_t* fileSize, bool* isDir) {
        RawrXDFileManager* m = static_cast<RawrXDFileManager*>(mgr);
        return m ? m->GetFileInfo(index, nameBuffer, nameSize, fileSize, isDir) : false;
    }
    
    RAWRXD_SHIP_EXPORT bool __stdcall FileManager_ReadFile(void* mgr, const wchar_t* filePath,
        char* buffer, size_t bufSize, size_t* bytesRead) {
        RawrXDFileManager* m = static_cast<RawrXDFileManager*>(mgr);
        size_t read = 0;
        bool result = m ? m->ReadFileContent(filePath, buffer, bufSize, read) : false;
        if (bytesRead) *bytesRead = read;
        return result;
    }
    
    RAWRXD_SHIP_EXPORT bool __stdcall FileManager_WriteFile(void* mgr, const wchar_t* filePath,
        const char* buffer, size_t bufSize) {
        RawrXDFileManager* m = static_cast<RawrXDFileManager*>(mgr);
        return m ? m->WriteFileContent(filePath, buffer, bufSize) : false;
    }
}

#ifndef RAWRXD_WIN32_STATIC_BUILD
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        OutputDebugStringW(L"RawrXD_FileManager_Win32 loaded\n");
    } else if (fdwReason == DLL_PROCESS_DETACH && g_fileManager) {
        delete g_fileManager;
        g_fileManager = nullptr;
    }
    return TRUE;
}
#endif
