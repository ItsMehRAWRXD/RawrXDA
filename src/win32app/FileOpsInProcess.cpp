/*
 * FileOpsInProcess.cpp
 * In-process Win32 implementation for RawrXD::FileOps (file_operations_win32.h).
 * Used when RAWRXD_FILEOPS_INPROC is defined (e.g. RawrXD-Win32IDE build).
 * When RAWRXD_WIN32_STATIC_BUILD is also defined, delegates to the converted
 * RawrXD_FileManager_Win32 (Qt→Win32) C API instead of local Win32 calls.
 */

#define RAWRXD_FILEOPS_INPROC
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <cstring>

// Include after defining RAWRXD_FILEOPS_INPROC so we get the declaration-only FileManager
#include "file_operations_win32.h"

#ifdef RAWRXD_WIN32_STATIC_BUILD
// Converted Qt→Win32 FileManager (RawrXD_FileManager_Win32.cpp) C API
extern "C" {
    void* __stdcall CreateFileManager();
    void __stdcall DestroyFileManager(void* mgr);
    bool __stdcall FileManager_ListFiles(void* mgr, const wchar_t* path);
    size_t __stdcall FileManager_GetFileCount(void* mgr);
    bool __stdcall FileManager_GetFileInfo(void* mgr, size_t index, wchar_t* nameBuffer,
        size_t nameSize, uint64_t* fileSize, bool* isDir);
    bool __stdcall FileManager_ReadFile(void* mgr, const wchar_t* filePath,
        char* buffer, size_t bufSize, size_t* bytesRead);
    bool __stdcall FileManager_WriteFile(void* mgr, const wchar_t* filePath,
        const char* buffer, size_t bufSize);
    bool __stdcall FileManager_FileExists(void* mgr, const wchar_t* filePath);
    bool __stdcall FileManager_DeleteFile(void* mgr, const wchar_t* filePath);
    bool __stdcall FileManager_CreateDirectory(void* mgr, const wchar_t* dirPath);
}
static void* s_convertedFileManager = nullptr;
static void* getConvertedFileManager() {
    if (!s_convertedFileManager) s_convertedFileManager = CreateFileManager();
    return s_convertedFileManager;
}
static std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string out(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &out[0], n, nullptr, nullptr);
    return out;
}
#endif

namespace RawrXD {
namespace FileOps {

static std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    if (wlen <= 0) return L"";
    std::wstring wide(size_t(wlen), 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wide[0], wlen);
    return wide;
}

static std::vector<std::string> listFilesImpl(const std::string& path, const std::string& pattern) {
    std::vector<std::string> results;
    std::wstring wpath = utf8ToWide(path);
    if (wpath.empty()) return results;

#ifdef RAWRXD_WIN32_STATIC_BUILD
    void* mgr = getConvertedFileManager();
    if (!mgr || !FileManager_ListFiles(mgr, wpath.c_str())) return results;
    size_t count = FileManager_GetFileCount(mgr);
    wchar_t wname[MAX_PATH];
    for (size_t i = 0; i < count; i++) {
        uint64_t sz = 0;
        bool isDir = false;
        if (FileManager_GetFileInfo(mgr, i, wname, MAX_PATH, &sz, &isDir))
            results.push_back(wideToUtf8(wname));
    }
    return results;
#else
    std::wstring wpattern = utf8ToWide(pattern.empty() ? "*" : pattern);
    std::wstring searchPath = wpath;
    if (searchPath.back() != L'\\') searchPath += L'\\';
    searchPath += wpattern;

    WIN32_FIND_DATAW fd = {};
    HANDLE h = FindFirstFileW(searchPath.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return results;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        char buf[MAX_PATH * 4] = {};
        int n = WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, buf, (int)sizeof(buf), nullptr, nullptr);
        if (n > 0) results.push_back(buf);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return results;
#endif
}

static bool readFileImpl(const std::string& path, std::string& content) {
    std::wstring wpath = utf8ToWide(path);
    if (wpath.empty()) return false;
#ifdef RAWRXD_WIN32_STATIC_BUILD
    void* mgr = getConvertedFileManager();
    if (!mgr) return false;
    char buffer[16 * 1024 * 1024];
    size_t bytesRead = 0;
    if (!FileManager_ReadFile(mgr, wpath.c_str(), buffer, sizeof(buffer), &bytesRead))
        return false;
    content.assign(buffer, bytesRead);
    return true;
#else
    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER li = {};
    if (!GetFileSizeEx(h, &li) || li.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(h);
        return false;
    }
    content.resize((size_t)li.QuadPart);
    DWORD read = 0;
    BOOL ok = ::ReadFile(h, &content[0], (DWORD)content.size(), &read, nullptr);
    CloseHandle(h);
    if (ok && read <= content.size()) content.resize(read);
    return ok == TRUE;
#endif
}

static bool writeFileImpl(const std::string& path, const std::string& content) {
    std::wstring wpath = utf8ToWide(path);
    if (wpath.empty()) return false;
#ifdef RAWRXD_WIN32_STATIC_BUILD
    void* mgr = getConvertedFileManager();
    if (!mgr) return false;
    return FileManager_WriteFile(mgr, wpath.c_str(),
        content.empty() ? "" : content.data(), content.size());
#else
    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_WRITE, 0, nullptr,
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = content.empty() ? TRUE : ::WriteFile(h, content.data(), (DWORD)content.size(), &written, nullptr);
    CloseHandle(h);
    return ok == TRUE;
#endif
}

static bool fileExistsImpl(const std::string& path) {
    std::wstring wpath = utf8ToWide(path);
    if (wpath.empty()) return false;
#ifdef RAWRXD_WIN32_STATIC_BUILD
    void* mgr = getConvertedFileManager();
    return mgr && FileManager_FileExists(mgr, wpath.c_str());
#else
    DWORD a = GetFileAttributesW(wpath.c_str());
    return a != INVALID_FILE_ATTRIBUTES;
#endif
}

static bool deleteFileImpl(const std::string& path) {
    std::wstring wpath = utf8ToWide(path);
    if (wpath.empty()) return false;
#ifdef RAWRXD_WIN32_STATIC_BUILD
    void* mgr = getConvertedFileManager();
    return mgr && FileManager_DeleteFile(mgr, wpath.c_str());
#else
    return ::DeleteFileW(wpath.c_str()) == TRUE;
#endif
}

static bool createDirectoryImpl(const std::string& path) {
    std::wstring wpath = utf8ToWide(path);
    if (wpath.empty()) return false;
#ifdef RAWRXD_WIN32_STATIC_BUILD
    void* mgr = getConvertedFileManager();
    return mgr && FileManager_CreateDirectory(mgr, wpath.c_str());
#else
    return CreateDirectoryW(wpath.c_str(), nullptr) == TRUE;
#endif
}

// FileManager singleton and method definitions (used when RAWRXD_FILEOPS_INPROC)

FileManager& FileManager::instance() {
    static FileManager inst;
    return inst;
}

bool FileManager::initialize() {
    return true;
}

std::vector<std::string> FileManager::listFiles(const std::string& path, const std::string& pattern) {
    return listFilesImpl(path, pattern);
}

bool FileManager::readFile(const std::string& path, std::string& content) {
    return readFileImpl(path, content);
}

bool FileManager::writeFile(const std::string& path, const std::string& content) {
    return writeFileImpl(path, content);
}

bool FileManager::fileExists(const std::string& path) {
    return fileExistsImpl(path);
}

bool FileManager::deleteFile(const std::string& path) {
    return deleteFileImpl(path);
}

bool FileManager::createDirectory(const std::string& path) {
    return createDirectoryImpl(path);
}

void FileManager::shutdown() {
#ifdef RAWRXD_WIN32_STATIC_BUILD
    if (s_convertedFileManager) {
        DestroyFileManager(s_convertedFileManager);
        s_convertedFileManager = nullptr;
    }
#endif
}

} // namespace FileOps
} // namespace RawrXD
