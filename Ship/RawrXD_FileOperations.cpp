// RawrXD_FileOperations.cpp - Production-grade file and directory operations
// Pure C++20 / Win32 - No Qt Dependencies

#include "RawrXD_FileOperations.hpp"

namespace RawrXD {

FileManager::FileManager() = default;
FileManager::~FileManager() = default;

bool FileManager::readFile(const std::wstring& path, std::wstring& content, Encoding* detectedEncoding) {
    std::vector<uint8_t> rawData;
    if (!readFileRaw(path, rawData)) {
        return false;
    }

    Encoding encoding = detectEncoding(rawData);
    if (detectedEncoding) {
        *detectedEncoding = encoding;
    }

    switch (encoding) {
        case Encoding::UTF8:
            content = utf8ToWide(rawData);
            break;
        case Encoding::UTF16_LE: {
            if (rawData.size() < 2) {
                content.clear();
                return true;
            }
            const wchar_t* wbuf = reinterpret_cast<const wchar_t*>(rawData.data());
            size_t wlen = rawData.size() / sizeof(wchar_t);
            content.assign(wbuf, wbuf + wlen);
            break;
        }
        case Encoding::UTF16_BE: {
            std::vector<uint8_t> swapped = rawData;
            for (size_t i = 0; i + 1 < swapped.size(); i += 2) {
                std::swap(swapped[i], swapped[i + 1]);
            }
            const wchar_t* wbuf = reinterpret_cast<const wchar_t*>(swapped.data());
            size_t wlen = swapped.size() / sizeof(wchar_t);
            content.assign(wbuf, wbuf + wlen);
            break;
        }
        case Encoding::ASCII:
        case Encoding::Unknown:
        default: {
            content = utf8ToWide(rawData);
            break;
        }
    }

    return true;
}

bool FileManager::readFileRaw(const std::wstring& path, std::vector<uint8_t>& data) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    if (size < 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    data.resize(static_cast<size_t>(size));
    if (size > 0) {
        file.read(reinterpret_cast<char*>(data.data()), size);
    }
    return file.good() || file.eof();
}

Encoding FileManager::detectEncoding(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return Encoding::UTF8;
    }

    if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        return Encoding::UTF8;
    }
    if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        return Encoding::UTF16_LE;
    }
    if (data.size() >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        return Encoding::UTF16_BE;
    }

    int utf8Sequences = 0;
    int asciiChars = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        unsigned char c = data[i];
        if (c < 0x80) {
            asciiChars++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < data.size() && (data[i + 1] & 0xC0) == 0x80) {
            utf8Sequences++;
            i += 1;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < data.size()) {
            if ((data[i + 1] & 0xC0) == 0x80 && (data[i + 2] & 0xC0) == 0x80) {
                utf8Sequences++;
                i += 2;
            }
        } else if ((c & 0xF8) == 0xF0 && i + 3 < data.size()) {
            if ((data[i + 1] & 0xC0) == 0x80 && (data[i + 2] & 0xC0) == 0x80 && (data[i + 3] & 0xC0) == 0x80) {
                utf8Sequences++;
                i += 3;
            }
        }
    }

    if (utf8Sequences > 0) {
        return Encoding::UTF8;
    }
    if (asciiChars == static_cast<int>(data.size())) {
        return Encoding::ASCII;
    }

    return Encoding::Unknown;
}

FileOperationResult FileManager::writeFile(const std::wstring& path, const std::wstring& content, bool createBackupFlag) {
    return writeFileRaw(path, wideToUtf8(content), createBackupFlag);
}

FileOperationResult FileManager::writeFileRaw(const std::wstring& path, const std::vector<uint8_t>& data, bool createBackupFlag) {
    std::wstring absolutePath = toAbsolutePath(path);

    std::wstring backupPath;
    if (createBackupFlag && exists(absolutePath)) {
        backupPath = createBackup(absolutePath);
        if (backupPath.empty()) {
            return FileOperationResult(false, L"Failed to create backup");
        }
    }

    fs::path p(absolutePath);
    if (!p.parent_path().empty()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
        if (ec) {
            return FileOperationResult(false, L"Failed to create directory");
        }
    }

    std::wstring tempPath = createTempPath(absolutePath);
    {
        std::ofstream tempFile(tempPath, std::ios::binary | std::ios::trunc);
        if (!tempFile.is_open()) {
            return FileOperationResult(false, L"Failed to open temp file for writing");
        }
        if (!data.empty()) {
            tempFile.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }
        if (!tempFile.good()) {
            return FileOperationResult(false, L"Failed to write all data");
        }
    }

    if (!MoveFileExW(tempPath.c_str(), absolutePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tempPath.c_str());
        return FileOperationResult(false, formatWinError(GetLastError()));
    }

    FileOperationResult result(true);
    result.backupPath = backupPath;
    return result;
}

FileOperationResult FileManager::createFile(const std::wstring& path) {
    std::wstring absolutePath = toAbsolutePath(path);

    if (exists(absolutePath)) {
        return FileOperationResult(false, L"File already exists");
    }

    HANDLE hFile = CreateFileW(absolutePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FileOperationResult(false, formatWinError(GetLastError()));
    }
    CloseHandle(hFile);
    return FileOperationResult(true);
}

FileOperationResult FileManager::deleteFile(const std::wstring& path, bool moveToTrashFlag) {
    std::wstring absolutePath = toAbsolutePath(path);

    if (!exists(absolutePath)) {
        return FileOperationResult(false, L"File does not exist");
    }

    if (moveToTrashFlag) {
        if (moveToTrash(absolutePath)) {
            return FileOperationResult(true);
        }
    }

    if (!DeleteFileW(absolutePath.c_str())) {
        return FileOperationResult(false, formatWinError(GetLastError()));
    }

    return FileOperationResult(true);
}

FileOperationResult FileManager::renameFile(const std::wstring& oldPath, const std::wstring& newPath) {
    std::wstring absoluteOldPath = toAbsolutePath(oldPath);
    std::wstring absoluteNewPath = toAbsolutePath(newPath);

    if (!exists(absoluteOldPath)) {
        return FileOperationResult(false, L"Source file does not exist");
    }

    if (exists(absoluteNewPath)) {
        return FileOperationResult(false, L"Destination file already exists");
    }

    if (!MoveFileExW(absoluteOldPath.c_str(), absoluteNewPath.c_str(), MOVEFILE_COPY_ALLOWED)) {
        return FileOperationResult(false, formatWinError(GetLastError()));
    }

    return FileOperationResult(true);
}

FileOperationResult FileManager::moveFile(const std::wstring& sourcePath, const std::wstring& destPath) {
    std::wstring absoluteSource = toAbsolutePath(sourcePath);
    std::wstring absoluteDest = toAbsolutePath(destPath);

    if (isDirectory(absoluteDest)) {
        fs::path src(absoluteSource);
        fs::path dst(absoluteDest);
        absoluteDest = (dst / src.filename()).wstring();
    }

    return renameFile(absoluteSource, absoluteDest);
}

FileOperationResult FileManager::copyFile(const std::wstring& sourcePath, const std::wstring& destPath, bool overwrite) {
    std::wstring absoluteSource = toAbsolutePath(sourcePath);
    std::wstring absoluteDest = toAbsolutePath(destPath);

    if (!exists(absoluteSource)) {
        return FileOperationResult(false, L"Source file does not exist");
    }

    if (isDirectory(absoluteDest)) {
        fs::path src(absoluteSource);
        fs::path dst(absoluteDest);
        absoluteDest = (dst / src.filename()).wstring();
    }

    if (exists(absoluteDest) && !overwrite) {
        return FileOperationResult(false, L"Destination file already exists");
    }

    if (!CopyFileW(absoluteSource.c_str(), absoluteDest.c_str(), overwrite ? FALSE : TRUE)) {
        return FileOperationResult(false, formatWinError(GetLastError()));
    }

    return FileOperationResult(true);
}

FileOperationResult FileManager::createDirectory(const std::wstring& path) {
    std::wstring absolutePath = toAbsolutePath(path);
    std::error_code ec;
    fs::create_directories(fs::path(absolutePath), ec);
    if (ec) {
        return FileOperationResult(false, L"Failed to create directory");
    }
    return FileOperationResult(true);
}

FileOperationResult FileManager::deleteDirectory(const std::wstring& path, bool moveToTrashFlag) {
    std::wstring absolutePath = toAbsolutePath(path);
    if (!exists(absolutePath)) {
        return FileOperationResult(false, L"Directory does not exist");
    }

    if (moveToTrashFlag) {
        if (moveToTrash(absolutePath)) {
            return FileOperationResult(true);
        }
    }

    std::error_code ec;
    fs::remove_all(fs::path(absolutePath), ec);
    if (ec) {
        return FileOperationResult(false, L"Failed to delete directory");
    }

    return FileOperationResult(true);
}

FileOperationResult FileManager::copyDirectory(const std::wstring& sourcePath, const std::wstring& destPath) {
    std::wstring absoluteSource = toAbsolutePath(sourcePath);
    std::wstring absoluteDest = toAbsolutePath(destPath);

    if (!isDirectory(absoluteSource)) {
        return FileOperationResult(false, L"Source is not a directory");
    }

    std::error_code ec;
    fs::create_directories(fs::path(absoluteDest), ec);
    if (ec) {
        return FileOperationResult(false, L"Failed to create destination directory");
    }

    for (const auto& entry : fs::recursive_directory_iterator(absoluteSource)) {
        const auto rel = fs::relative(entry.path(), absoluteSource);
        const auto dest = fs::path(absoluteDest) / rel;
        if (entry.is_directory()) {
            fs::create_directories(dest, ec);
            if (ec) return FileOperationResult(false, L"Failed to create destination directory");
        } else if (entry.is_regular_file()) {
            fs::create_directories(dest.parent_path(), ec);
            if (ec) return FileOperationResult(false, L"Failed to create destination directory");
            fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing, ec);
            if (ec) return FileOperationResult(false, L"Failed to copy file");
        }
    }

    return FileOperationResult(true);
}

std::wstring FileManager::toAbsolutePath(const std::wstring& relativePath, const std::wstring& basePath) {
    fs::path path(relativePath);
    if (path.is_absolute()) {
        return path.lexically_normal().wstring();
    }
    fs::path base = basePath.empty() ? fs::current_path() : fs::path(basePath);
    return (base / path).lexically_normal().wstring();
}

std::wstring FileManager::toRelativePath(const std::wstring& absolutePath, const std::wstring& basePath) {
    std::error_code ec;
    auto rel = fs::relative(fs::path(absolutePath), fs::path(basePath), ec);
    if (ec) return absolutePath;
    return rel.wstring();
}

bool FileManager::exists(const std::wstring& path) {
    std::error_code ec;
    return fs::exists(fs::path(path), ec);
}

bool FileManager::isFile(const std::wstring& path) {
    std::error_code ec;
    return fs::is_regular_file(fs::path(path), ec);
}

bool FileManager::isDirectory(const std::wstring& path) {
    std::error_code ec;
    return fs::is_directory(fs::path(path), ec);
}

bool FileManager::isSymlink(const std::wstring& path) {
    std::error_code ec;
    return fs::is_symlink(fs::path(path), ec);
}

bool FileManager::isReadable(const std::wstring& path) {
    return _waccess(path.c_str(), 4) == 0;
}

bool FileManager::isWritable(const std::wstring& path) {
    return _waccess(path.c_str(), 2) == 0;
}

int64_t FileManager::fileSize(const std::wstring& path) {
    std::error_code ec;
    return fs::exists(path, ec) ? static_cast<int64_t>(fs::file_size(path, ec)) : -1;
}

uint64_t FileManager::lastModified(const std::wstring& path) {
    std::error_code ec;
    auto ftime = fs::last_write_time(fs::path(path), ec);
    if (ec) return 0;
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(sctp.time_since_epoch()).count());
}

std::wstring FileManager::createBackup(const std::wstring& path) {
    if (!exists(path)) {
        return L"";
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t ts[32];
    swprintf_s(ts, L"%04d%02d%02d_%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    fs::path p(path);
    std::wstring backupPath = (p.parent_path() / (p.filename().wstring() + L"." + ts + L".bak")).wstring();

    if (!CopyFileW(path.c_str(), backupPath.c_str(), FALSE)) {
        return L"";
    }

    return backupPath;
}

void FileManager::setAutoBackup(bool enable) {
    m_autoBackup = enable;
}

bool FileManager::isAutoBackupEnabled() const {
    return m_autoBackup;
}

std::wstring FileManager::createTempPath(const std::wstring& originalPath) {
    fs::path p(originalPath);
    std::wstring fileName = p.filename().wstring();
    return (p.parent_path() / (L"." + fileName + L".tmp")).wstring();
}

std::wstring FileManager::formatWinError(DWORD code) {
    wchar_t buffer[512] = {0};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, buffer, 512, nullptr);
    return buffer;
}

bool FileManager::moveToTrash(const std::wstring& path) {
    std::wstring doubleNull = path + L"\0";
    SHFILEOPSTRUCTW fileOp = {};
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = doubleNull.c_str();
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    int result = SHFileOperationW(&fileOp);
    return (result == 0);
}

std::wstring FileManager::utf8ToWide(const std::vector<uint8_t>& data) {
    if (data.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), nullptr, 0);
    if (sizeNeeded <= 0) return L"";
    std::wstring out(static_cast<size_t>(sizeNeeded), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), out.data(), sizeNeeded);
    return out;
}

std::vector<uint8_t> FileManager::wideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0) return {};
    std::vector<uint8_t> out(static_cast<size_t>(sizeNeeded));
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), reinterpret_cast<char*>(out.data()), sizeNeeded, nullptr, nullptr);
    return out;
}

} // namespace RawrXD
