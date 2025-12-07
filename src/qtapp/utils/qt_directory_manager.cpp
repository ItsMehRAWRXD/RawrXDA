/**
 * \file qt_directory_manager.cpp
 * \brief Implementation of Qt-based directory manager
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "qt_directory_manager.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDebug>

namespace RawrXD {

QtDirectoryManager::QtDirectoryManager() {}

FileOperationResult QtDirectoryManager::createDirectory(const QString& path,
                                                       bool createParents) 
{
    QString absolutePath = this->absolutePath(path);
    QDir dir;
    
    bool success = createParents 
        ? dir.mkpath(absolutePath)
        : dir.mkdir(absolutePath);
    
    if (!success) {
        return FileOperationResult(
            false, 
            QString("Failed to create directory: %1").arg(absolutePath)
        );
    }
    
    return FileOperationResult(true);
}

FileOperationResult QtDirectoryManager::deleteDirectory(const QString& path,
                                                       bool recursive) 
{
    QString absolutePath = this->absolutePath(path);
    
    if (!exists(absolutePath)) {
        return FileOperationResult(false, "Directory does not exist");
    }
    
    if (!isDirectory(absolutePath)) {
        return FileOperationResult(false, "Path is not a directory");
    }
    
    QDir dir(absolutePath);
    
    if (recursive) {
        if (!removeDirectoryRecursive(absolutePath)) {
            return FileOperationResult(false, "Failed to delete directory recursively");
        }
    } else {
        // Non-recursive: directory must be empty
        if (!dir.isEmpty()) {
            return FileOperationResult(false, "Directory is not empty");
        }
        
        if (!dir.rmdir(absolutePath)) {
            return FileOperationResult(false, "Failed to delete directory");
        }
    }
    
    return FileOperationResult(true);
}

FileOperationResult QtDirectoryManager::copyDirectory(const QString& sourcePath,
                                                     const QString& destPath) 
{
    QString absoluteSource = this->absolutePath(sourcePath);
    QString absoluteDest = this->absolutePath(destPath);
    
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

bool QtDirectoryManager::exists(const QString& path) const {
    return QFileInfo::exists(path);
}

bool QtDirectoryManager::isDirectory(const QString& path) const {
    QFileInfo info(path);
    return info.exists() && info.isDir();
}

QStringList QtDirectoryManager::listFiles(const QString& path, bool recursive) const {
    QStringList files;
    
    if (!isDirectory(path)) {
        return files;
    }
    
    if (recursive) {
        listFilesRecursive(path, files);
    } else {
        QDir dir(path);
        QFileInfoList entries = dir.entryInfoList(
            QDir::Files | QDir::NoDotAndDotDot,
            QDir::Name
        );
        
        for (const QFileInfo& info : entries) {
            files.append(info.absoluteFilePath());
        }
    }
    
    return files;
}

QStringList QtDirectoryManager::listDirectories(const QString& path, bool recursive) const {
    QStringList dirs;
    
    if (!isDirectory(path)) {
        return dirs;
    }
    
    if (recursive) {
        listDirectoriesRecursive(path, dirs);
    } else {
        QDir dir(path);
        QFileInfoList entries = dir.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name
        );
        
        for (const QFileInfo& info : entries) {
            dirs.append(info.absoluteFilePath());
        }
    }
    
    return dirs;
}

QString QtDirectoryManager::absolutePath(const QString& path) const {
    if (QFileInfo(path).isAbsolute()) {
        return QDir::cleanPath(path);
    }
    return QDir::current().absoluteFilePath(path);
}

QString QtDirectoryManager::relativePath(const QString& basePath,
                                        const QString& targetPath) const 
{
    QDir baseDir(basePath);
    QString absoluteTarget = this->absolutePath(targetPath);
    return baseDir.relativeFilePath(absoluteTarget);
}

// Private helper methods

bool QtDirectoryManager::removeDirectoryRecursive(const QString& path) const {
    QDir dir(path);
    
    if (!dir.exists()) {
        return false;
    }
    
    // Remove all files and subdirectories
    QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden
    );
    
    for (const QFileInfo& info : entries) {
        if (info.isDir()) {
            if (!removeDirectoryRecursive(info.absoluteFilePath())) {
                return false;
            }
        } else {
            if (!QFile::remove(info.absoluteFilePath())) {
                qWarning() << "Failed to remove file:" << info.absoluteFilePath();
                return false;
            }
        }
    }
    
    // Remove the directory itself
    return dir.rmdir(path);
}

bool QtDirectoryManager::copyDirectoryRecursive(const QString& sourcePath,
                                               const QString& destPath) const 
{
    QDir sourceDir(sourcePath);
    
    if (!sourceDir.exists()) {
        return false;
    }
    
    // Create destination directory
    QDir destDir;
    if (!destDir.mkpath(destPath)) {
        qWarning() << "Failed to create destination directory:" << destPath;
        return false;
    }
    
    // Copy all files and subdirectories
    QFileInfoList entries = sourceDir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden
    );
    
    for (const QFileInfo& info : entries) {
        QString destFilePath = destPath + "/" + info.fileName();
        
        if (info.isDir()) {
            if (!copyDirectoryRecursive(info.absoluteFilePath(), destFilePath)) {
                return false;
            }
        } else {
            if (!QFile::copy(info.absoluteFilePath(), destFilePath)) {
                qWarning() << "Failed to copy file:" << info.absoluteFilePath();
                return false;
            }
        }
    }
    
    return true;
}

void QtDirectoryManager::listFilesRecursive(const QString& path, QStringList& files) const {
    QDir dir(path);
    
    // Add files in current directory
    QFileInfoList fileEntries = dir.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name
    );
    
    for (const QFileInfo& info : fileEntries) {
        files.append(info.absoluteFilePath());
    }
    
    // Recurse into subdirectories
    QFileInfoList dirEntries = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name
    );
    
    for (const QFileInfo& info : dirEntries) {
        listFilesRecursive(info.absoluteFilePath(), files);
    }
}

void QtDirectoryManager::listDirectoriesRecursive(const QString& path, QStringList& dirs) const {
    QDir dir(path);
    
    QFileInfoList entries = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name
    );
    
    for (const QFileInfo& info : entries) {
        dirs.append(info.absoluteFilePath());
        // Recurse into subdirectory
        listDirectoriesRecursive(info.absoluteFilePath(), dirs);
    }
}

} // namespace RawrXD
