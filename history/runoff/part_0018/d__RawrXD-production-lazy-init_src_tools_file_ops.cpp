#define NOMINMAX
#include "tools/file_ops.h"
#include <windows.h>
#include <shlwapi.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

// Link with shlwapi.lib for PathRemoveFileSpecW and other path functions
#pragma comment(lib, "shlwapi.lib")

namespace RawrXD {
namespace Tools {

static FileOpResult ok(const std::string& msg = "ok", const std::optional<std::string>& path = std::nullopt) {
    return {true, msg, path};
}

static FileOpResult fail(const std::string& msg, const std::optional<std::string>& path = std::nullopt) {
    return {false, msg, path};
}

FileOpResult FileOps::readText(const std::string& path, std::string& out) {
    try {
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        if (!ifs) return fail("Unable to open file for reading", path);
        std::ostringstream ss;
        ss << ifs.rdbuf();
        out = ss.str();
        return ok("Read text", path);
    } catch (const std::exception& e) {
        return fail(std::string("readText error: ") + e.what(), path);
    }
}

FileOpResult FileOps::writeText(const std::string& path, const std::string& content, bool create_dirs) {
    try {
        if (create_dirs) {
            std::wstring wpath(path.begin(), path.end());
            if (PathRemoveFileSpecW(&wpath[0])) {
                CreateDirectoryW(wpath.c_str(), NULL);
            }
        }
        std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!ofs) return fail("Unable to open file for writing", path);
        ofs << content;
        return ok("Wrote text", path);
    } catch (const std::exception& e) {
        return fail(std::string("writeText error: ") + e.what(), path);
    }
}

FileOpResult FileOps::appendText(const std::string& path, const std::string& content) {
    try {
        std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::app);
        if (!ofs) return fail("Unable to open file for appending", path);
        ofs << content;
        return ok("Appended text", path);
    } catch (const std::exception& e) {
        return fail(std::string("appendText error: ") + e.what(), path);
    }
}

FileOpResult FileOps::remove(const std::string& path) {
    try {
        std::wstring wpath(path.begin(), path.end());
        SHFILEOPSTRUCTW fileOp = {0};
        fileOp.wFunc = FO_DELETE;
        fileOp.pFrom = wpath.c_str();
        fileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
        int result = SHFileOperationW(&fileOp);
        if (result != 0) return fail("Remove failed", path);
        return ok("Removed", path);
    } catch (const std::exception& e) {
        return fail(std::string("remove error: ") + e.what(), path);
    }
}

FileOpResult FileOps::rename(const std::string& from, const std::string& to, bool create_dirs) {
    try {
        if (create_dirs) {
            std::wstring wto(to.begin(), to.end());
            if (PathRemoveFileSpecW(&wto[0])) {
                CreateDirectoryW(wto.c_str(), NULL);
            }
        }
        std::wstring wfrom(from.begin(), from.end());
        std::wstring wto(to.begin(), to.end());
        if (!MoveFileW(wfrom.c_str(), wto.c_str())) {
            return fail("Rename failed", to);
        }
        return ok("Renamed", to);
    } catch (const std::exception& e) {
        return fail(std::string("rename error: ") + e.what(), to);
    }
}

FileOpResult FileOps::copy(const std::string& from, const std::string& to, const CopyOptions& opts) {
    try {
        if (opts.create_dirs) {
            std::wstring wto(to.begin(), to.end());
            if (PathRemoveFileSpecW(&wto[0])) {
                CreateDirectoryW(wto.c_str(), NULL);
            }
        }
        std::wstring wfrom(from.begin(), from.end());
        std::wstring wto(to.begin(), to.end());
        
        DWORD flags = 0;
        if (!opts.overwrite) flags |= COPY_FILE_FAIL_IF_EXISTS;
        
        if (!CopyFileW(wfrom.c_str(), wto.c_str(), (opts.overwrite ? FALSE : TRUE))) {
            return fail("Copy failed", to);
        }
        return ok("Copied", to);
    } catch (const std::exception& e) {
        return fail(std::string("copy error: ") + e.what(), to);
    }
}

FileOpResult FileOps::move(const std::string& from, const std::string& to, bool overwrite) {
    try {
        if (overwrite) {
            std::wstring wto(to.begin(), to.end());
            DWORD attrs = GetFileAttributesW(wto.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES) {
                if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
                    // Remove directory
                    SHFILEOPSTRUCTW fileOp = {0};
                    fileOp.wFunc = FO_DELETE;
                    fileOp.pFrom = wto.c_str();
                    fileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
                    SHFileOperationW(&fileOp);
                } else {
                    DeleteFileW(wto.c_str());
                }
            }
        }
        return rename(from, to, true);
    } catch (const std::exception& e) {
        return fail(std::string("move error: ") + e.what(), to);
    }
}

FileOpResult FileOps::ensureDir(const std::string& path) {
    try {
        std::wstring wpath(path.begin(), path.end());
        if (!CreateDirectoryW(wpath.c_str(), NULL)) {
            DWORD error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) {
                return fail("Failed to create directory", path);
            }
        }
        return ok("Ensured directory", path);
    } catch (const std::exception& e) {
        return fail(std::string("ensureDir error: ") + e.what(), path);
    }
}

FileOpResult FileOps::list(const std::string& path, std::vector<std::string>& out, bool recursive) {
    try {
        std::wstring wpath(path.begin(), path.end());
        WIN32_FIND_DATAW findData;
        HANDLE hFind;
        
        std::wstring searchPath = wpath;
        if (searchPath.back() != L'\\') searchPath += L'\\';
        searchPath += L'*';
        
        hFind = FindFirstFileW(searchPath.c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE) {
            return fail("Path does not exist or cannot be accessed", path);
        }
        
        do {
            std::wstring filename = findData.cFileName;
            if (filename != L"." && filename != L"..") {
                std::string fullPath = path;
                if (fullPath.back() != '\\') fullPath += '\\';
                fullPath += std::string(filename.begin(), filename.end());
                out.push_back(fullPath);
                
                if (recursive && (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::vector<std::string> subOut;
                    list(fullPath, subOut, true);
                    out.insert(out.end(), subOut.begin(), subOut.end());
                }
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
        return ok("Listed", path);
    } catch (const std::exception& e) {
        return fail(std::string("list error: ") + e.what(), path);
    }
}

bool FileOps::exists(const std::string& path) {
    std::wstring wpath(path.begin(), path.end());
    DWORD attrs = GetFileAttributesW(wpath.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

} // namespace Tools
} // namespace RawrXD
