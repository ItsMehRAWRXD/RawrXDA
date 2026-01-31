/**
 * \file qt_file_writer.cpp
 * \brief Implementation of Qt-based file writer with atomic operations
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "qt_file_writer.h"


namespace RawrXD {

QtFileWriter::QtFileWriter(void* parent) 
    : void(parent)
    , m_autoBackup(true) 
{}

FileOperationResult QtFileWriter::writeFile(const std::string& path,
                                           const std::string& content,
                                           bool createBackup) 
{
    return writeFileRaw(path, content.toUtf8(), createBackup);
}

FileOperationResult QtFileWriter::writeFileRaw(const std::string& path,
                                              const std::vector<uint8_t>& data,
                                              bool createBackup) 
{
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
        return FileOperationResult(
            false, 
            std::string("Failed to create directory: %1"))
        );
    }
    
    // Use QSaveFile for atomic write (write to temp, then rename)
    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return FileOperationResult(
            false, 
            std::string("Failed to open file for writing: %1"))
        );
    }
    
    int64_t written = file.write(data);
    if (written != data.size()) {
        file.cancelWriting();
        return FileOperationResult(false, "Failed to write all data");
    }
    
    if (!file.commit()) {
        return FileOperationResult(
            false, 
            std::string("Failed to commit file: %1"))
        );
    }
    
    FileOperationResult result(true);
    result.backupPath = backupPath;
    return result;
}

FileOperationResult QtFileWriter::createFile(const std::string& path) {
    std::string absolutePath = toAbsolutePath(path);
    
    if (exists(absolutePath)) {
        return FileOperationResult(false, "File already exists");
    }
    
    std::fstream file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return FileOperationResult(
            false, 
            std::string("Failed to create file: %1"))
        );
    }
    
    file.close();
    return FileOperationResult(true);
}

FileOperationResult QtFileWriter::deleteFile(const std::string& path, bool moveToTrash) {
    std::string absolutePath = toAbsolutePath(path);
    
    if (!exists(absolutePath)) {
        return FileOperationResult(false, "File does not exist");
    }
    
    if (moveToTrash) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        if (std::fstream::moveToTrash(absolutePath)) {
            return FileOperationResult(true);
        }
#endif
    }
    
    // Permanent delete
    std::fstream file(absolutePath);
    if (!file.remove()) {
        return FileOperationResult(
            false, 
            std::string("Failed to delete file: %1"))
        );
    }
    
    return FileOperationResult(true);
}

FileOperationResult QtFileWriter::renameFile(const std::string& oldPath, const std::string& newPath) {
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
        return FileOperationResult(
            false, 
            std::string("Failed to rename file: %1"))
        );
    }
    
    return FileOperationResult(true);
}

FileOperationResult QtFileWriter::copyFile(const std::string& sourcePath,
                                          const std::string& destPath,
                                          bool overwrite) 
{
    std::string absoluteSource = toAbsolutePath(sourcePath);
    std::string absoluteDest = toAbsolutePath(destPath);
    
    if (!exists(absoluteSource)) {
        return FileOperationResult(false, "Source file does not exist");
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
        return FileOperationResult(
            false, 
            std::string("Failed to copy file: %1"))
        );
    }
    
    return FileOperationResult(true);
}

std::string QtFileWriter::createBackup(const std::string& path) {
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

void QtFileWriter::setAutoBackup(bool enable) {
    m_autoBackup = enable;
}

bool QtFileWriter::isAutoBackupEnabled() const {
    return m_autoBackup;
}

// Private helper methods

std::string QtFileWriter::toAbsolutePath(const std::string& path) const {
    if (std::filesystem::path(path).isAbsolute()) {
        return std::filesystem::path::cleanPath(path);
    }
    return std::filesystem::path::current().absoluteFilePath(path);
}

bool QtFileWriter::exists(const std::string& path) const {
    return std::filesystem::path::exists(path);
}

} // namespace RawrXD


