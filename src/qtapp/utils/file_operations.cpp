/**
 * \file file_operations.cpp
 * \brief Implementation of production-grade file operations
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "file_operations.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QTextCodec>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDebug>

namespace RawrXD {

FileManager::FileManager() : m_autoBackup(true) {}

FileManager::~FileManager() = default;

// ========== Reading Operations ==========

bool FileManager::readFile(const QString& path, QString& content, Encoding* detectedEncoding) {
    QByteArray rawData;
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
            content = QString::fromUtf8(rawData);
            break;
        case Encoding::UTF16_LE:
            content = QString::fromUtf16(reinterpret_cast<const char16_t*>(rawData.constData()), rawData.size() / 2);
            break;
        case Encoding::UTF16_BE: {
            // Swap bytes for big endian
            QByteArray swapped;
            swapped.reserve(rawData.size());
            for (int i = 0; i < rawData.size() - 1; i += 2) {
                swapped.append(rawData[i + 1]);
                swapped.append(rawData[i]);
            }
            content = QString::fromUtf16(reinterpret_cast<const char16_t*>(swapped.constData()), swapped.size() / 2);
            break;
        }
        case Encoding::ASCII:
        case Encoding::Unknown:
        default:
            // Try UTF-8 first, fall back to Latin-1
            content = QString::fromUtf8(rawData);
            if (content.contains(QChar::ReplacementCharacter)) {
                content = QString::fromLatin1(rawData);
            }
            break;
    }
    
    return true;
}

bool FileManager::readFileRaw(const QString& path, QByteArray& data) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for reading:" << path << "-" << file.errorString();
        return false;
    }
    
    data = file.readAll();
    return !data.isNull();
}

Encoding FileManager::detectEncoding(const QByteArray& data) {
    if (data.isEmpty()) {
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

FileOperationResult FileManager::writeFile(const QString& path, const QString& content, bool createBackup) {
    return writeFileRaw(path, content.toUtf8(), createBackup);
}

FileOperationResult FileManager::writeFileRaw(const QString& path, const QByteArray& data, bool createBackup) {
    QString absolutePath = toAbsolutePath(path);
    
    // Create backup if file exists and backup requested
    QString backupPath;
    if (createBackup && exists(absolutePath)) {
        backupPath = this->createBackup(absolutePath);
        if (backupPath.isEmpty()) {
            return FileOperationResult(false, "Failed to create backup");
        }
    }
    
    // Ensure directory exists
    QFileInfo fileInfo(absolutePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        return FileOperationResult(false, QString("Failed to create directory: %1").arg(dir.absolutePath()));
    }
    
    // Use QSaveFile for atomic write (write to temp, then rename)
    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return FileOperationResult(false, QString("Failed to open file for writing: %1").arg(file.errorString()));
    }
    
    qint64 written = file.write(data);
    if (written != data.size()) {
        file.cancelWriting();
        return FileOperationResult(false, "Failed to write all data");
    }
    
    if (!file.commit()) {
        return FileOperationResult(false, QString("Failed to commit file: %1").arg(file.errorString()));
    }
    
    FileOperationResult result(true);
    result.backupPath = backupPath;
    return result;
}

// ========== File CRUD Operations ==========

FileOperationResult FileManager::createFile(const QString& path) {
    QString absolutePath = toAbsolutePath(path);
    
    if (exists(absolutePath)) {
        return FileOperationResult(false, "File already exists");
    }
    
    // Create empty file
    QFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return FileOperationResult(false, QString("Failed to create file: %1").arg(file.errorString()));
    }
    
    file.close();
    return FileOperationResult(true);
}

FileOperationResult FileManager::deleteFile(const QString& path, bool moveToTrash) {
    QString absolutePath = toAbsolutePath(path);
    
    if (!exists(absolutePath)) {
        return FileOperationResult(false, "File does not exist");
    }
    
    if (moveToTrash) {
        // Move to trash (platform-specific)
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        if (QFile::moveToTrash(absolutePath)) {
            return FileOperationResult(true);
        }
#endif
        // Fall back to permanent delete if trash fails
        qWarning() << "Failed to move to trash, deleting permanently:" << absolutePath;
    }
    
    // Permanent delete
    QFile file(absolutePath);
    if (!file.remove()) {
        return FileOperationResult(false, QString("Failed to delete file: %1").arg(file.errorString()));
    }
    
    return FileOperationResult(true);
}

FileOperationResult FileManager::renameFile(const QString& oldPath, const QString& newPath) {
    QString absoluteOldPath = toAbsolutePath(oldPath);
    QString absoluteNewPath = toAbsolutePath(newPath);
    
    if (!exists(absoluteOldPath)) {
        return FileOperationResult(false, "Source file does not exist");
    }
    
    if (exists(absoluteNewPath)) {
        return FileOperationResult(false, "Destination file already exists");
    }
    
    QFile file(absoluteOldPath);
    if (!file.rename(absoluteNewPath)) {
        return FileOperationResult(false, QString("Failed to rename file: %1").arg(file.errorString()));
    }
    
    return FileOperationResult(true);
}

FileOperationResult FileManager::moveFile(const QString& sourcePath, const QString& destPath) {
    QString absoluteSource = toAbsolutePath(sourcePath);
    QString absoluteDest = toAbsolutePath(destPath);
    
    // If destination is a directory, append source filename
    if (isDirectory(absoluteDest)) {
        QFileInfo sourceInfo(absoluteSource);
        absoluteDest = QDir(absoluteDest).filePath(sourceInfo.fileName());
    }
    
    return renameFile(absoluteSource, absoluteDest);
}

FileOperationResult FileManager::copyFile(const QString& sourcePath, const QString& destPath, bool overwrite) {
    QString absoluteSource = toAbsolutePath(sourcePath);
    QString absoluteDest = toAbsolutePath(destPath);
    
    if (!exists(absoluteSource)) {
        return FileOperationResult(false, "Source file does not exist");
    }
    
    // If destination is a directory, append source filename
    if (isDirectory(absoluteDest)) {
        QFileInfo sourceInfo(absoluteSource);
        absoluteDest = QDir(absoluteDest).filePath(sourceInfo.fileName());
    }
    
    if (exists(absoluteDest) && !overwrite) {
        return FileOperationResult(false, "Destination file already exists");
    }
    
    // Remove existing file if overwriting
    if (exists(absoluteDest)) {
        QFile::remove(absoluteDest);
    }
    
    QFile file(absoluteSource);
    if (!file.copy(absoluteDest)) {
        return FileOperationResult(false, QString("Failed to copy file: %1").arg(file.errorString()));
    }
    
    return FileOperationResult(true);
}

// ========== Directory Operations ==========

FileOperationResult FileManager::createDirectory(const QString& path) {
    QString absolutePath = toAbsolutePath(path);
    
    QDir dir;
    if (!dir.mkpath(absolutePath)) {
        return FileOperationResult(false, "Failed to create directory");
    }
    
    return FileOperationResult(true);
}

FileOperationResult FileManager::deleteDirectory(const QString& path, bool moveToTrash) {
    QString absolutePath = toAbsolutePath(path);
    
    if (!exists(absolutePath)) {
        return FileOperationResult(false, "Directory does not exist");
    }
    
    if (moveToTrash) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        if (QFile::moveToTrash(absolutePath)) {
            return FileOperationResult(true);
        }
#endif
        qWarning() << "Failed to move to trash, deleting permanently:" << absolutePath;
    }
    
    // Permanent delete
    QDir dir(absolutePath);
    if (!dir.removeRecursively()) {
        return FileOperationResult(false, "Failed to delete directory");
    }
    
    return FileOperationResult(true);
}

FileOperationResult FileManager::copyDirectory(const QString& sourcePath, const QString& destPath) {
    QString absoluteSource = toAbsolutePath(sourcePath);
    QString absoluteDest = toAbsolutePath(destPath);
    
    if (!isDirectory(absoluteSource)) {
        return FileOperationResult(false, "Source is not a directory");
    }
    
    QDir sourceDir(absoluteSource);
    QDir destDir(absoluteDest);
    
    if (!destDir.exists() && !destDir.mkpath(".")) {
        return FileOperationResult(false, "Failed to create destination directory");
    }
    
    // Copy all files and subdirectories
    QFileInfoList entries = sourceDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        QString destItemPath = destDir.filePath(entry.fileName());
        
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

QString FileManager::toAbsolutePath(const QString& relativePath, const QString& basePath) {
    if (QFileInfo(relativePath).isAbsolute()) {
        return QDir::cleanPath(relativePath);
    }
    
    QString base = basePath.isEmpty() ? QDir::currentPath() : basePath;
    return QDir(base).absoluteFilePath(relativePath);
}

QString FileManager::toRelativePath(const QString& absolutePath, const QString& basePath) {
    QDir base(basePath);
    return base.relativeFilePath(absolutePath);
}

bool FileManager::exists(const QString& path) {
    return QFileInfo::exists(path);
}

bool FileManager::isFile(const QString& path) {
    QFileInfo info(path);
    return info.exists() && info.isFile();
}

bool FileManager::isDirectory(const QString& path) {
    QFileInfo info(path);
    return info.exists() && info.isDir();
}

bool FileManager::isSymlink(const QString& path) {
    QFileInfo info(path);
    return info.isSymLink();
}

bool FileManager::isReadable(const QString& path) {
    QFileInfo info(path);
    return info.isReadable();
}

bool FileManager::isWritable(const QString& path) {
    QFileInfo info(path);
    return info.isWritable();
}

qint64 FileManager::fileSize(const QString& path) {
    QFileInfo info(path);
    return info.exists() ? info.size() : -1;
}

QDateTime FileManager::lastModified(const QString& path) {
    QFileInfo info(path);
    return info.lastModified();
}

// ========== Backup Operations ==========

QString FileManager::createBackup(const QString& path) {
    if (!exists(path)) {
        return QString();
    }
    
    QFileInfo info(path);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString backupPath = QString("%1/%2.%3.bak")
        .arg(info.absolutePath())
        .arg(info.fileName())
        .arg(timestamp);
    
    QFile file(path);
    if (!file.copy(backupPath)) {
        qWarning() << "Failed to create backup:" << file.errorString();
        return QString();
    }
    
    return backupPath;
}

void FileManager::setAutoBackup(bool enable) {
    m_autoBackup = enable;
}

bool FileManager::isAutoBackupEnabled() const {
    return m_autoBackup;
}

QString FileManager::createTempPath(const QString& originalPath) {
    QFileInfo info(originalPath);
    return QString("%1/.%2.tmp").arg(info.absolutePath()).arg(info.fileName());
}

} // namespace RawrXD
