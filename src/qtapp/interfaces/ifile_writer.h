/**
 * \file ifile_writer.h
 * \brief Abstract interface for file writing operations (DIP)
 * \author RawrXD Team
 * \date 2025-12-05
 */

#ifndef RAWRXD_IFILE_WRITER_H
#define RAWRXD_IFILE_WRITER_H

#include <QString>
#include <QByteArray>

namespace RawrXD {

/**
 * \brief Result of a file operation
 */
struct FileOperationResult {
    bool success;
    QString errorMessage;
    QString backupPath;  ///< Path to backup file (if created)
    
    FileOperationResult(bool ok = false, const QString& error = QString())
        : success(ok), errorMessage(error) {}
};

/**
 * \brief Abstract interface for file writing operations
 * 
 * This interface enforces atomic write operations using the
 * "write to temp, then rename" pattern for data integrity.
 * 
 * Key Design Principles:
 * - Atomic writes via QSaveFile
 * - Optional automatic backups
 * - Clear error reporting via FileOperationResult
 */
class IFileWriter {
public:
    virtual ~IFileWriter() = default;
    
    /**
     * \brief Write string content to file (UTF-8 encoding)
     * \param path Absolute or relative file path
     * \param content String content to write
     * \param createBackup Create backup before overwriting
     * \return Operation result with success status and error details
     */
    virtual FileOperationResult writeFile(const QString& path,
                                         const QString& content,
                                         bool createBackup = false) = 0;
    
    /**
     * \brief Write raw byte array to file
     * \param path Absolute or relative file path
     * \param data Raw bytes to write
     * \param createBackup Create backup before overwriting
     * \return Operation result with success status and error details
     */
    virtual FileOperationResult writeFileRaw(const QString& path,
                                            const QByteArray& data,
                                            bool createBackup = false) = 0;
    
    /**
     * \brief Create an empty file
     * \param path File path
     * \return Operation result
     */
    virtual FileOperationResult createFile(const QString& path) = 0;
    
    /**
     * \brief Delete a file
     * \param path File path
     * \param moveToTrash Move to trash instead of permanent delete
     * \return Operation result
     */
    virtual FileOperationResult deleteFile(const QString& path, 
                                          bool moveToTrash = true) = 0;
    
    /**
     * \brief Rename/move a file
     * \param oldPath Source path
     * \param newPath Destination path
     * \return Operation result
     */
    virtual FileOperationResult renameFile(const QString& oldPath,
                                          const QString& newPath) = 0;
    
    /**
     * \brief Copy a file
     * \param sourcePath Source file path
     * \param destPath Destination file path
     * \param overwrite Allow overwriting existing file
     * \return Operation result
     */
    virtual FileOperationResult copyFile(const QString& sourcePath,
                                        const QString& destPath,
                                        bool overwrite = false) = 0;
    
    /**
     * \brief Create a backup of a file
     * \param path File path
     * \return Path to backup file, or empty string on failure
     */
    virtual QString createBackup(const QString& path) = 0;
    
    /**
     * \brief Enable/disable automatic backups
     * \param enable true to enable automatic backups
     */
    virtual void setAutoBackup(bool enable) = 0;
    
    /**
     * \brief Check if automatic backups are enabled
     * \return true if auto-backup is enabled
     */
    virtual bool isAutoBackupEnabled() const = 0;
};

} // namespace RawrXD

#endif // RAWRXD_IFILE_WRITER_H
