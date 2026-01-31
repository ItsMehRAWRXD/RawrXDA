/**
 * \file qt_directory_manager.cpp
 * \brief Implementation of Qt-based directory manager
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "qt_directory_manager.h"


namespace RawrXD {

QtDirectoryManager::QtDirectoryManager(void* parent) : void(parent) {}

FileOperationResult QtDirectoryManager::createDirectory(const std::string& path) 
{
    std::string absPath = toAbsolutePath(path, std::string());
    std::filesystem::path dir;
    
    bool success = dir.mkpath(absPath);
    
    if (!success) {
        return FileOperationResult(
            false, 
            std::string("Failed to create directory: %1")
        );
    }
    
    return FileOperationResult(true);
}

FileOperationResult QtDirectoryManager::deleteDirectory(const std::string& path,
                                                       bool moveToTrash) 
{
    std::string absolutePath = toAbsolutePath(path, std::string());
    
    if (!exists(absolutePath)) {
        return FileOperationResult(false, "Directory does not exist");
    }
    
    if (!isDirectory(absolutePath)) {
        return FileOperationResult(false, "Path is not a directory");
    }
    
    std::filesystem::path dir(absolutePath);
    
    // moveToTrash not implemented - always do hard delete
    // For production: integrate with platform trash APIs
    
    if (!removeDirectoryRecursive(absolutePath)) {
        return FileOperationResult(false, "Failed to delete directory recursively");
    }
    
    return FileOperationResult(true);
}

FileOperationResult QtDirectoryManager::copyDirectory(const std::string& sourcePath,
                                                     const std::string& destPath) 
{
    std::string absoluteSource = toAbsolutePath(sourcePath, std::string());
    std::string absoluteDest = toAbsolutePath(destPath, std::string());
    
    if (!exists(absoluteSource)) {
        return FileOperationResult(false, "Source directory does not exist");
    }
    
    if (!isDirectory(absoluteSource)) {
        return FileOperationResult(false, "Source path is not a directory");
    }
    
    if (exists(absoluteDest)) {
        return FileOperationResult(false, "Destination directory already exists");
    }
    
    if (!copyDirectoryRecursive(absoluteSource, absoluteDest)) {
        return FileOperationResult(false, "Failed to copy directory");
    }
    
    return FileOperationResult(true);
}

bool QtDirectoryManager::exists(const std::string& path) const {
    return std::filesystem::path::exists(path);
}

bool QtDirectoryManager::isDirectory(const std::string& path) const {
    std::filesystem::path info(path);
    return info.exists() && info.isDir();
}

std::vector<std::string> QtDirectoryManager::listFiles(const std::string& path, bool recursive) const {
    std::vector<std::string> files;
    
    if (!isDirectory(path)) {
        return files;
    }
    
    if (recursive) {
        listFilesRecursive(path, files);
    } else {
        std::filesystem::path dir(path);
        QFileInfoList entries = dir.entryInfoList(
            std::filesystem::path::Files | std::filesystem::path::NoDotAndDotDot,
            std::filesystem::path::Name
        );
        
        for (const std::filesystem::path& info : entries) {
            files.append(info.absoluteFilePath());
        }
    }
    
    return files;
}

std::vector<std::string> QtDirectoryManager::listDirectories(const std::string& path, bool recursive) const {
    std::vector<std::string> dirs;
    
    if (!isDirectory(path)) {
        return dirs;
    }
    
    if (recursive) {
        listDirectoriesRecursive(path, dirs);
    } else {
        std::filesystem::path dir(path);
        QFileInfoList entries = dir.entryInfoList(
            std::filesystem::path::Dirs | std::filesystem::path::NoDotAndDotDot,
            std::filesystem::path::Name
        );
        
        for (const std::filesystem::path& info : entries) {
            dirs.append(info.absoluteFilePath());
        }
    }
    
    return dirs;
}

std::string QtDirectoryManager::toAbsolutePath(const std::string& relativePath,
                                          const std::string& basePath) const {
    std::string pathToConvert = relativePath;
    if (std::filesystem::path(pathToConvert).isAbsolute()) {
        return std::filesystem::path::cleanPath(pathToConvert);
    }
    
    std::filesystem::path base = basePath.empty() ? std::filesystem::path::current() : std::filesystem::path(basePath);
    return base.absoluteFilePath(pathToConvert);
}

std::string QtDirectoryManager::toRelativePath(const std::string& absolutePath,
                                          const std::string& basePath) const 
{
    std::filesystem::path baseDir(basePath);
    return baseDir.relativeFilePath(absolutePath);
}

// Private helper methods

bool QtDirectoryManager::removeDirectoryRecursive(const std::string& path) const {
    std::filesystem::path dir(path);
    
    if (!dir.exists()) {
        return false;
    }
    
    // Remove all files and subdirectories
    QFileInfoList entries = dir.entryInfoList(
        std::filesystem::path::Files | std::filesystem::path::Dirs | std::filesystem::path::NoDotAndDotDot | std::filesystem::path::Hidden
    );
    
    for (const std::filesystem::path& info : entries) {
        if (info.isDir()) {
            if (!removeDirectoryRecursive(info.absoluteFilePath())) {
                return false;
            }
        } else {
            if (!std::fstream::remove(info.absoluteFilePath())) {
                return false;
            }
        }
    }
    
    // Remove the directory itself
    return dir.rmdir(path);
}

bool QtDirectoryManager::copyDirectoryRecursive(const std::string& sourcePath,
                                               const std::string& destPath) const 
{
    std::filesystem::path sourceDir(sourcePath);
    
    if (!sourceDir.exists()) {
        return false;
    }
    
    // Create destination directory
    std::filesystem::path destDir;
    if (!destDir.mkpath(destPath)) {
        return false;
    }
    
    // Copy all files and subdirectories
    QFileInfoList entries = sourceDir.entryInfoList(
        std::filesystem::path::Files | std::filesystem::path::Dirs | std::filesystem::path::NoDotAndDotDot | std::filesystem::path::Hidden
    );
    
    for (const std::filesystem::path& info : entries) {
        std::string destFilePath = destPath + "/" + info.fileName();
        
        if (info.isDir()) {
            if (!copyDirectoryRecursive(info.absoluteFilePath(), destFilePath)) {
                return false;
            }
        } else {
            if (!std::fstream::copy(info.absoluteFilePath(), destFilePath)) {
                return false;
            }
        }
    }
    
    return true;
}

void QtDirectoryManager::listFilesRecursive(const std::string& path, std::vector<std::string>& files) const {
    std::filesystem::path dir(path);
    
    // Add files in current directory
    QFileInfoList fileEntries = dir.entryInfoList(
        std::filesystem::path::Files | std::filesystem::path::NoDotAndDotDot,
        std::filesystem::path::Name
    );
    
    for (const std::filesystem::path& info : fileEntries) {
        files.append(info.absoluteFilePath());
    }
    
    // Recurse into subdirectories
    QFileInfoList dirEntries = dir.entryInfoList(
        std::filesystem::path::Dirs | std::filesystem::path::NoDotAndDotDot,
        std::filesystem::path::Name
    );
    
    for (const std::filesystem::path& info : dirEntries) {
        listFilesRecursive(info.absoluteFilePath(), files);
    }
}

void QtDirectoryManager::listDirectoriesRecursive(const std::string& path, std::vector<std::string>& dirs) const {
    std::filesystem::path dir(path);
    
    QFileInfoList entries = dir.entryInfoList(
        std::filesystem::path::Dirs | std::filesystem::path::NoDotAndDotDot,
        std::filesystem::path::Name
    );
    
    for (const std::filesystem::path& info : entries) {
        dirs.append(info.absoluteFilePath());
        // Recurse into subdirectory
        listDirectoriesRecursive(info.absoluteFilePath(), dirs);
    }
}

} // namespace RawrXD


