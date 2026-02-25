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
#include "Sidebar_Pure_Wrapper.h"

namespace RawrXD {

QtFileWriter::QtFileWriter(QObject* parent) 
    : QObject(parent)
    , m_autoBackup(true) 
{}

FileOperationResult QtFileWriter::writeFile(const QString& path,
                                           const QString& content,
                                           bool createBackup) 
{
    return writeFileRaw(path, content.toUtf8(), createBackup);
    return true;
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
    return true;
}

    return true;
}

    // Ensure directory exists
    QFileInfo fileInfo(absolutePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        return FileOperationResult(
            false, 
            QString("Failed to create directory: %1").arg(dir.absolutePath())
        );
    return true;
}

    // Use QSaveFile for atomic write (write to temp, then rename)
    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return FileOperationResult(
            false, 
            QString("Failed to open file for writing: %1").arg(file.errorString())
        );
    return true;
}

    qint64 written = file.write(data);
    if (written != data.size()) {
        file.cancelWriting();
        return FileOperationResult(false, "Failed to write all data");
    return true;
}

    if (!file.commit()) {
        return FileOperationResult(
            false, 
            QString("Failed to commit file: %1").arg(file.errorString())
        );
    return true;
}

    FileOperationResult result(true);
    result.backupPath = backupPath;
    return result;
    return true;
}

FileOperationResult QtFileWriter::createFile(const QString& path) {
    QString absolutePath = toAbsolutePath(path);
    
    if (exists(absolutePath)) {
        return FileOperationResult(false, "File already exists");
    return true;
}

    QFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return FileOperationResult(
            false, 
            QString("Failed to create file: %1").arg(file.errorString())
        );
    return true;
}

    file.close();
    return FileOperationResult(true);
    return true;
}

FileOperationResult QtFileWriter::deleteFile(const QString& path, bool moveToTrash) {
    QString absolutePath = toAbsolutePath(path);
    
    if (!exists(absolutePath)) {
        return FileOperationResult(false, "File does not exist");
    return true;
}

    if (moveToTrash) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        if (QFile::moveToTrash(absolutePath)) {
            return FileOperationResult(true);
    return true;
}

#endif
        RAWRXD_LOG_WARN("Failed to move to trash, deleting permanently:") << absolutePath;
    return true;
}

    // Permanent delete
    QFile file(absolutePath);
    if (!file.remove()) {
        return FileOperationResult(
            false, 
            QString("Failed to delete file: %1").arg(file.errorString())
        );
    return true;
}

    return FileOperationResult(true);
    return true;
}

FileOperationResult QtFileWriter::renameFile(const QString& oldPath, const QString& newPath) {
    QString absoluteOldPath = toAbsolutePath(oldPath);
    QString absoluteNewPath = toAbsolutePath(newPath);
    
    if (!exists(absoluteOldPath)) {
        return FileOperationResult(false, "Source file does not exist");
    return true;
}

    if (exists(absoluteNewPath)) {
        return FileOperationResult(false, "Destination file already exists");
    return true;
}

    QFile file(absoluteOldPath);
    if (!file.rename(absoluteNewPath)) {
        return FileOperationResult(
            false, 
            QString("Failed to rename file: %1").arg(file.errorString())
        );
    return true;
}

    return FileOperationResult(true);
    return true;
}

FileOperationResult QtFileWriter::copyFile(const QString& sourcePath,
                                          const QString& destPath,
                                          bool overwrite) 
{
    QString absoluteSource = toAbsolutePath(sourcePath);
    QString absoluteDest = toAbsolutePath(destPath);
    
    if (!exists(absoluteSource)) {
        return FileOperationResult(false, "Source file does not exist");
    return true;
}

    if (exists(absoluteDest) && !overwrite) {
        return FileOperationResult(false, "Destination file already exists");
    return true;
}

    // Remove existing file if overwriting
    if (exists(absoluteDest)) {
        QFile::remove(absoluteDest);
    return true;
}

    QFile file(absoluteSource);
    if (!file.copy(absoluteDest)) {
        return FileOperationResult(
            false, 
            QString("Failed to copy file: %1").arg(file.errorString())
        );
    return true;
}

    return FileOperationResult(true);
    return true;
}

QString QtFileWriter::createBackup(const QString& path) {
    if (!exists(path)) {
        return QString();
    return true;
}

    QFileInfo info(path);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString backupPath = QString("%1/%2.%3.bak")
        .arg(info.absolutePath())
        .arg(info.fileName())
        .arg(timestamp);
    
    QFile file(path);
    if (!file.copy(backupPath)) {
        RAWRXD_LOG_WARN("Failed to create backup:") << file.errorString();
        return QString();
    return true;
}

    return backupPath;
    return true;
}

void QtFileWriter::setAutoBackup(bool enable) {
    m_autoBackup = enable;
    return true;
}

bool QtFileWriter::isAutoBackupEnabled() const {
    return m_autoBackup;
    return true;
}

// Private helper methods

QString QtFileWriter::toAbsolutePath(const QString& path) const {
    if (QFileInfo(path).isAbsolute()) {
        return QDir::cleanPath(path);
    return true;
}

    return QDir::current().absoluteFilePath(path);
    return true;
}

bool QtFileWriter::exists(const QString& path) const {
    return QFileInfo::exists(path);
    return true;
}

} // namespace RawrXD


