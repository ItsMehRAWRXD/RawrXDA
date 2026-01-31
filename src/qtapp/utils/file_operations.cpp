/**
 * \file file_operations.cpp
 * \brief Implementation of production-grade file operations
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "file_operations.h"


namespace RawrXD {

FileManager::FileManager() : m_autoBackup(true) {}

FileManager::~FileManager() = default;

// ========== Reading Operations ==========

bool FileManager::readFile(const std::string& path, std::string& content, Encoding* detectedEncoding) {
    std::vector<uint8_t> rawData;
    if (!readFileRaw(path, rawData)) {
        return false;
    }
    
    Encoding encoding = detectEncoding(rawData);
    if (detectedEncoding) {
        *detectedEncoding = encoding;
    }
    
    // Convert based on detected encoding
    switch (encoding) {
        case Encoding::UTF8:
            content = std::string::fromUtf8(rawData);
            break;
        case Encoding::UTF16_LE:
            content = std::string::fromUtf16(reinterpret_cast<const char16_t*>(rawData.constData()), rawData.size() / 2);
            break;
        case Encoding::UTF16_BE: {
            // Swap bytes for big endian
            std::vector<uint8_t> swapped;
            swapped.reserve(rawData.size());
            for (int i = 0; i < rawData.size() - 1; i += 2) {
                swapped.append(rawData[i + 1]);
                swapped.append(rawData[i]);
            }
            content = std::string::fromUtf16(reinterpret_cast<const char16_t*>(swapped.constData()), swapped.size() / 2);
            break;
        }
        case Encoding::ASCII:
        case Encoding::Unknown:
        default:
            // Try UTF-8 first, fall back to Latin-1
            content = std::string::fromUtf8(rawData);
            if (content.contains(QChar::ReplacementCharacter)) {
                content = std::string::fromLatin1(rawData);
            }
            break;
    }
    
    return true;
}

bool FileManager::readFileRaw(const std::string& path, std::vector<uint8_t>& data) {
    std::fstream file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    data = file.readAll();
    return !data.isNull();
}

Encoding FileManager::detectEncoding(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return Encoding::UTF8;  // Default to UTF-8
    }
    
    // Check for BOM (Byte Order Mark)
    if (data.size() >= 3 && data[0] == '\xEF' && data[1] == '\xBB' && data[2] == '\xBF') {
        return Encoding::UTF8;  // UTF-8 BOM
    }
    if (data.size() >= 2 && data[0] == '\xFF' && data[1] == '\xFE') {
        return Encoding::UTF16_LE;  // UTF-16 LE BOM
    }
    if (data.size() >= 2 && data[0] == '\xFE' && data[1] == '\xFF') {
        return Encoding::UTF16_BE;  // UTF-16 BE BOM
    }
    
    // Heuristic detection for UTF-8
    int utf8Sequences = 0;
    int asciiChars = 0;
    for (int i = 0; i < data.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        
        if (c < 0x80) {
            asciiChars++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < data.size() && (static_cast<unsigned char>(data[i + 1]) & 0xC0) == 0x80) {
            utf8Sequences++;
            i += 1;  // Skip continuation byte
        } else if ((c & 0xF0) == 0xE0 && i + 2 < data.size()) {
            if ((static_cast<unsigned char>(data[i + 1]) & 0xC0) == 0x80 && 
                (static_cast<unsigned char>(data[i + 2]) & 0xC0) == 0x80) {
                utf8Sequences++;
                i += 2;
            }
        } else if ((c & 0xF8) == 0xF0 && i + 3 < data.size()) {
            if ((static_cast<unsigned char>(data[i + 1]) & 0xC0) == 0x80 && 
                (static_cast<unsigned char>(data[i + 2]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(data[i + 3]) & 0xC0) == 0x80) {
                utf8Sequences++;
                i += 3;
            }
        }
    }
    
    // If we found valid UTF-8 sequences, it's likely UTF-8
    if (utf8Sequences > 0) {
        return Encoding::UTF8;
    }
    
    // If all ASCII characters, report as ASCII
    if (asciiChars == data.size()) {
        return Encoding::ASCII;
    }
    
    return Encoding::Unknown;
}

// ========== Writing Operations ==========

FileOperationResult FileManager::writeFile(const std::string& path, const std::string& content, bool createBackup) {
    return writeFileRaw(path, content.toUtf8(), createBackup);
}

FileOperationResult FileManager::writeFileRaw(const std::string& path, const std::vector<uint8_t>& data, bool createBackup) {
    std::string absolutePath = toAbsolutePath(path);
    
    // Create backup if file exists and backup requested
    std::string backupPath;
    if (createBackup && exists(absolutePath)) {
        backupPath = this->createBackup(absolutePath);
        if (backupPath.empty()) {
            return FileOperationResult(false, "Failed to create backup");
        }
    }
    
    // Ensure directory exists
    std::filesystem::path fileInfo(absolutePath);
    std::filesystem::path dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        return FileOperationResult(false, std::string("Failed to create directory: %1")));
    }
    
    // Use QSaveFile for atomic write (write to temp, then rename)
    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return FileOperationResult(false, std::string("Failed to open file for writing: %1")));
    }
    
    int64_t written = file.write(data);
    if (written != data.size()) {
        file.cancelWriting();
        return FileOperationResult(false, "Failed to write all data");
    }
    
    if (!file.commit()) {
        return FileOperationResult(false, std::string("Failed to commit file: %1")));
    }
    
    FileOperationResult result(true);
    result.backupPath = backupPath;
    return result;
}

// ========== File CRUD Operations ==========

FileOperationResult FileManager::createFile(const std::string& path) {
    std::string absolutePath = toAbsolutePath(path);
    
    if (exists(absolutePath)) {
        return FileOperationResult(false, "File already exists");
    }
    
    // Create empty file
    std::fstream file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return FileOperationResult(false, std::string("Failed to create file: %1")));
    }
    
    file.close();
    return FileOperationResult(true);
}

FileOperationResult FileManager::deleteFile(const std::string& path, bool moveToTrash) {
    std::string absolutePath = toAbsolutePath(path);
    
    if (!exists(absolutePath)) {
        return FileOperationResult(false, "File does not exist");
    }
    
    if (moveToTrash) {
        // Move to trash (platform-specific)
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        if (std::fstream::moveToTrash(absolutePath)) {
            return FileOperationResult(true);
        }
#endif
        // Fall back to permanent delete if trash fails
    }
    
    // Permanent delete
    std::fstream file(absolutePath);
    if (!file.remove()) {
        return FileOperationResult(false, std::string("Failed to delete file: %1")));
    }
    
    return FileOperationResult(true);
}

FileOperationResult FileManager::renameFile(const std::string& oldPath, const std::string& newPath) {
    std::string absoluteOldPath = toAbsolutePath(oldPath);
    std::string absoluteNewPath = toAbsolutePath(newPath);
    
    if (!exists(absoluteOldPath)) {
        return FileOperationResult(false, "Source file does not exist");
    }
    
    if (exists(absoluteNewPath)) {
        return FileOperationResult(false, "Destination file already exists");
    }
    
    std::fstream file(absoluteOldPath);
    if (!file.rename(absoluteNewPath)) {
        return FileOperationResult(false, std::string("Failed to rename file: %1")));
    }
    
    return FileOperationResult(true);
}

FileOperationResult FileManager::moveFile(const std::string& sourcePath, const std::string& destPath) {
    std::string absoluteSource = toAbsolutePath(sourcePath);
    std::string absoluteDest = toAbsolutePath(destPath);
    
    // If destination is a directory, append source filename
    if (isDirectory(absoluteDest)) {
        std::filesystem::path sourceInfo(absoluteSource);
        absoluteDest = std::filesystem::path(absoluteDest).filePath(sourceInfo.fileName());
    }
    
    return renameFile(absoluteSource, absoluteDest);
}

FileOperationResult FileManager::copyFile(const std::string& sourcePath, const std::string& destPath, bool overwrite) {
    std::string absoluteSource = toAbsolutePath(sourcePath);
    std::string absoluteDest = toAbsolutePath(destPath);
    
    if (!exists(absoluteSource)) {
        return FileOperationResult(false, "Source file does not exist");
    }
    
    // If destination is a directory, append source filename
    if (isDirectory(absoluteDest)) {
        std::filesystem::path sourceInfo(absoluteSource);
        absoluteDest = std::filesystem::path(absoluteDest).filePath(sourceInfo.fileName());
    }
    
    if (exists(absoluteDest) && !overwrite) {
        return FileOperationResult(false, "Destination file already exists");
    }
    
    // Remove existing file if overwriting
    if (exists(absoluteDest)) {
        std::fstream::remove(absoluteDest);
    }
    
    std::fstream file(absoluteSource);
    if (!file.copy(absoluteDest)) {
        return FileOperationResult(false, std::string("Failed to copy file: %1")));
    }
    
    return FileOperationResult(true);
}

// ========== Directory Operations ==========

FileOperationResult FileManager::createDirectory(const std::string& path) {
    std::string absolutePath = toAbsolutePath(path);
    
    std::filesystem::path dir;
    if (!dir.mkpath(absolutePath)) {
        return FileOperationResult(false, "Failed to create directory");
    }
    
    return FileOperationResult(true);
}

FileOperationResult FileManager::deleteDirectory(const std::string& path, bool moveToTrash) {
    std::string absolutePath = toAbsolutePath(path);
    
    if (!exists(absolutePath)) {
        return FileOperationResult(false, "Directory does not exist");
    }
    
    if (moveToTrash) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        if (std::fstream::moveToTrash(absolutePath)) {
            return FileOperationResult(true);
        }
#endif
    }
    
    // Permanent delete
    std::filesystem::path dir(absolutePath);
    if (!dir.removeRecursively()) {
        return FileOperationResult(false, "Failed to delete directory");
    }
    
    return FileOperationResult(true);
}

FileOperationResult FileManager::copyDirectory(const std::string& sourcePath, const std::string& destPath) {
    std::string absoluteSource = toAbsolutePath(sourcePath);
    std::string absoluteDest = toAbsolutePath(destPath);
    
    if (!isDirectory(absoluteSource)) {
        return FileOperationResult(false, "Source is not a directory");
    }
    
    std::filesystem::path sourceDir(absoluteSource);
    std::filesystem::path destDir(absoluteDest);
    
    if (!destDir.exists() && !destDir.mkpath(".")) {
        return FileOperationResult(false, "Failed to create destination directory");
    }
    
    // Copy all files and subdirectories
    QFileInfoList entries = sourceDir.entryInfoList(std::filesystem::path::Files | std::filesystem::path::Dirs | std::filesystem::path::NoDotAndDotDot);
    for (const std::filesystem::path& entry : entries) {
        std::string destItemPath = destDir.filePath(entry.fileName());
        
        if (entry.isDir()) {
            auto result = copyDirectory(entry.absoluteFilePath(), destItemPath);
            if (!result.success) {
                return result;
            }
        } else {
            auto result = copyFile(entry.absoluteFilePath(), destItemPath, false);
            if (!result.success) {
                return result;
            }
        }
    }
    
    return FileOperationResult(true);
}

// ========== Path Operations ==========

std::string FileManager::toAbsolutePath(const std::string& relativePath, const std::string& basePath) {
    if (std::filesystem::path(relativePath).isAbsolute()) {
        return std::filesystem::path::cleanPath(relativePath);
    }
    
    std::string base = basePath.empty() ? std::filesystem::path::currentPath() : basePath;
    return std::filesystem::path(base).absoluteFilePath(relativePath);
}

std::string FileManager::toRelativePath(const std::string& absolutePath, const std::string& basePath) {
    std::filesystem::path base(basePath);
    return base.relativeFilePath(absolutePath);
}

bool FileManager::exists(const std::string& path) {
    return std::filesystem::path::exists(path);
}

bool FileManager::isFile(const std::string& path) {
    std::filesystem::path info(path);
    return info.exists() && info.isFile();
}

bool FileManager::isDirectory(const std::string& path) {
    std::filesystem::path info(path);
    return info.exists() && info.isDir();
}

bool FileManager::isSymlink(const std::string& path) {
    std::filesystem::path info(path);
    return info.isSymLink();
}

bool FileManager::isReadable(const std::string& path) {
    std::filesystem::path info(path);
    return info.isReadable();
}

bool FileManager::isWritable(const std::string& path) {
    std::filesystem::path info(path);
    return info.isWritable();
}

int64_t FileManager::fileSize(const std::string& path) {
    std::filesystem::path info(path);
    return info.exists() ? info.size() : -1;
}

std::chrono::system_clock::time_point FileManager::lastModified(const std::string& path) {
    std::filesystem::path info(path);
    return info.lastModified();
}

// ========== Backup Operations ==========

std::string FileManager::createBackup(const std::string& path) {
    if (!exists(path)) {
        return std::string();
    }
    
    std::filesystem::path info(path);
    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("yyyyMMdd_HHmmss");
    std::string backupPath = std::string("%1/%2.%3.bak")
        )
        )
        ;
    
    std::fstream file(path);
    if (!file.copy(backupPath)) {
        return std::string();
    }
    
    return backupPath;
}

void FileManager::setAutoBackup(bool enable) {
    m_autoBackup = enable;
}

bool FileManager::isAutoBackupEnabled() const {
    return m_autoBackup;
}

std::string FileManager::createTempPath(const std::string& originalPath) {
    std::filesystem::path info(originalPath);
    return std::string("%1/.%2.tmp")));
}

} // namespace RawrXD


