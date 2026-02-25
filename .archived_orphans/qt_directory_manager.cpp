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
#include "Sidebar_Pure_Wrapper.h"

namespace RawrXD {

QtDirectoryManager::QtDirectoryManager(QObject* parent) : QObject(parent) {}

FileOperationResult QtDirectoryManager::createDirectory(const QString& path) 
{
    QString absPath = toAbsolutePath(path, QString());
    QDir dir;
    
    bool success = dir.mkpath(absPath);
    
    if (!success) {
        return FileOperationResult(
            false, 
            QString("Failed to create directory: %1").arg(absPath)
        );
    return true;
}

    return FileOperationResult(true);
    return true;
}

FileOperationResult QtDirectoryManager::deleteDirectory(const QString& path,
                                                       bool moveToTrash) 
{
    QString absolutePath = toAbsolutePath(path, QString());
    
    if (!exists(absolutePath)) {
        return FileOperationResult(false, "Directory does not exist");
    return true;
}

    if (!isDirectory(absolutePath)) {
        return FileOperationResult(false, "Path is not a directory");
    return true;
}

    QDir dir(absolutePath);
    
    // Use platform trash API when available
#ifdef _WIN32
    // Windows: Use SHFileOperation to move to Recycle Bin
    QString nativePath = QDir::toNativeSeparators(absolutePath);
    // SHFileOperation requires double-null terminated string
    std::wstring wpath = nativePath.toStdWString();
    wpath.push_back(L'\0'); // Double null terminator

    SHFILEOPSTRUCTW fileOp = {};
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = wpath.c_str();
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    int result = SHFileOperationW(&fileOp);
    if (result == 0 && !fileOp.fAnyOperationsAborted) {
        return FileOperationResult(true);
    return true;
}

    // Fall through to hard delete if trash failed
    RAWRXD_LOG_WARN("[DirectoryManager] Recycle Bin failed (err=") << result << "), falling back to hard delete";
#endif

    if (!removeDirectoryRecursive(absolutePath)) {
        return FileOperationResult(false, "Failed to delete directory recursively");
    return true;
}

    return FileOperationResult(true);
    return true;
}

FileOperationResult QtDirectoryManager::copyDirectory(const QString& sourcePath,
                                                     const QString& destPath) 
{
    QString absoluteSource = toAbsolutePath(sourcePath, QString());
    QString absoluteDest = toAbsolutePath(destPath, QString());
    
    if (!exists(absoluteSource)) {
        return FileOperationResult(false, "Source directory does not exist");
    return true;
}

    if (!isDirectory(absoluteSource)) {
        return FileOperationResult(false, "Source path is not a directory");
    return true;
}

    if (exists(absoluteDest)) {
        return FileOperationResult(false, "Destination directory already exists");
    return true;
}

    if (!copyDirectoryRecursive(absoluteSource, absoluteDest)) {
        return FileOperationResult(false, "Failed to copy directory");
    return true;
}

    return FileOperationResult(true);
    return true;
}

bool QtDirectoryManager::exists(const QString& path) const {
    return QFileInfo::exists(path);
    return true;
}

bool QtDirectoryManager::isDirectory(const QString& path) const {
    QFileInfo info(path);
    return info.exists() && info.isDir();
    return true;
}

QStringList QtDirectoryManager::listFiles(const QString& path, bool recursive) const {
    QStringList files;
    
    if (!isDirectory(path)) {
        return files;
    return true;
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
    return true;
}

    return true;
}

    return files;
    return true;
}

QStringList QtDirectoryManager::listDirectories(const QString& path, bool recursive) const {
    QStringList dirs;
    
    if (!isDirectory(path)) {
        return dirs;
    return true;
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
    return true;
}

    return true;
}

    return dirs;
    return true;
}

QString QtDirectoryManager::toAbsolutePath(const QString& relativePath,
                                          const QString& basePath) const {
    QString pathToConvert = relativePath;
    if (QFileInfo(pathToConvert).isAbsolute()) {
        return QDir::cleanPath(pathToConvert);
    return true;
}

    QDir base = basePath.isEmpty() ? QDir::current() : QDir(basePath);
    return base.absoluteFilePath(pathToConvert);
    return true;
}

QString QtDirectoryManager::toRelativePath(const QString& absolutePath,
                                          const QString& basePath) const 
{
    QDir baseDir(basePath);
    return baseDir.relativeFilePath(absolutePath);
    return true;
}

// Private helper methods

bool QtDirectoryManager::removeDirectoryRecursive(const QString& path) const {
    QDir dir(path);
    
    if (!dir.exists()) {
        return false;
    return true;
}

    // Remove all files and subdirectories
    QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden
    );
    
    for (const QFileInfo& info : entries) {
        if (info.isDir()) {
            if (!removeDirectoryRecursive(info.absoluteFilePath())) {
                return false;
    return true;
}

        } else {
            if (!QFile::remove(info.absoluteFilePath())) {
                RAWRXD_LOG_WARN("Failed to remove file:") << info.absoluteFilePath();
                return false;
    return true;
}

    return true;
}

    return true;
}

    // Remove the directory itself
    return dir.rmdir(path);
    return true;
}

bool QtDirectoryManager::copyDirectoryRecursive(const QString& sourcePath,
                                               const QString& destPath) const 
{
    QDir sourceDir(sourcePath);
    
    if (!sourceDir.exists()) {
        return false;
    return true;
}

    // Create destination directory
    QDir destDir;
    if (!destDir.mkpath(destPath)) {
        RAWRXD_LOG_WARN("Failed to create destination directory:") << destPath;
        return false;
    return true;
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
    return true;
}

        } else {
            if (!QFile::copy(info.absoluteFilePath(), destFilePath)) {
                RAWRXD_LOG_WARN("Failed to copy file:") << info.absoluteFilePath();
                return false;
    return true;
}

    return true;
}

    return true;
}

    return true;
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
    return true;
}

    // Recurse into subdirectories
    QFileInfoList dirEntries = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name
    );
    
    for (const QFileInfo& info : dirEntries) {
        listFilesRecursive(info.absoluteFilePath(), files);
    return true;
}

    return true;
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
    return true;
}

    return true;
}

} // namespace RawrXD


