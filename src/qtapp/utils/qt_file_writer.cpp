/**
 * \file qt_file_writer.cpp
 * \brief Implementation of Qt-based file writer with atomic operations
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "qt_file_writer.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QSaveFile>
#include <QDebug>

namespace RawrXD {

QtFileWriter::QtFileWriter() : m_autoBackup(true) {}

FileOperationResult QtFileWriter::writeFile(const QString& path,
                                           const QString& content,
                                           bool createBackup) 
{
    return writeFileRaw(path, content.toUtf8(), createBackup);
}

FileOperationResult QtFileWriter::writeFileRaw(const QString& path,
                                              const QByteArray& data,
                                              bool createBackup) 
{
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
        return FileOperationResult(
            false, 
            QString("Failed to create directory: %1").arg(dir.absolutePath())
        );
    }
    
    // Use QSaveFile for atomic write (write to temp, then rename)
    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return FileOperationResult(
            false, 
            QString("Failed to open file for writing: %1").arg(file.errorString())
        );
    }
    
    qint64 written = file.write(data);
    if (written != data.size()) {
        file.cancelWriting();
        return FileOperationResult(false, "Failed to write all data");
    }
    
    if (!file.commit()) {
        return FileOperationResult(
            false, 
            QString("Failed to commit file: %1").arg(file.errorString())
        );
    }
    
    FileOperationResult result(true);
    result.backupPath = backupPath;
    return result;
}

FileOperationResult QtFileWriter::createFile(const QString& path) {
    QString absolutePath = toAbsolutePath(path);
    
    if (exists(absolutePath)) {
        return FileOperationResult(false, "File already exists");
    }
    
    QFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return FileOperationResult(
            false, 
            QString("Failed to create file: %1").arg(file.errorString())
        );
    }
    
    file.close();
    return FileOperationResult(true);
}

FileOperationResult QtFileWriter::deleteFile(const QString& path, bool moveToTrash) {
    QString absolutePath = toAbsolutePath(path);
    
    if (!exists(absolutePath)) {
        return FileOperationResult(false, "File does not exist");
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
    QFile file(absolutePath);
    if (!file.remove()) {
        return FileOperationResult(
            false, 
            QString("Failed to delete file: %1").arg(file.errorString())
        );
    }
    
    return FileOperationResult(true);
}

FileOperationResult QtFileWriter::renameFile(const QString& oldPath, const QString& newPath) {
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
        return FileOperationResult(
            false, 
            QString("Failed to rename file: %1").arg(file.errorString())
        );
    }
    
    return FileOperationResult(true);
}

FileOperationResult QtFileWriter::copyFile(const QString& sourcePath,
                                          const QString& destPath,
                                          bool overwrite) 
{
    QString absoluteSource = toAbsolutePath(sourcePath);
    QString absoluteDest = toAbsolutePath(destPath);
    
    if (!exists(absoluteSource)) {
        return FileOperationResult(false, "Source file does not exist");
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
        return FileOperationResult(
            false, 
            QString("Failed to copy file: %1").arg(file.errorString())
        );
    }
    
    return FileOperationResult(true);
}

QString QtFileWriter::createBackup(const QString& path) {
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

void QtFileWriter::setAutoBackup(bool enable) {
    m_autoBackup = enable;
}

bool QtFileWriter::isAutoBackupEnabled() const {
    return m_autoBackup;
}

// Private helper methods

QString QtFileWriter::toAbsolutePath(const QString& path) const {
    if (QFileInfo(path).isAbsolute()) {
        return QDir::cleanPath(path);
    }
    return QDir::current().absoluteFilePath(path);
}

bool QtFileWriter::exists(const QString& path) const {
    return QFileInfo::exists(path);
}

} // namespace RawrXD
