#pragma once
/**
 * \file file_operations.h
 * \brief Production-grade file and directory operations with safety guarantees
 * \author RawrXD Team
 * \date 2025-12-05
 * 
 * Provides safe file operations including:
 * - Atomic writes (write to temp, then rename)
 * - Encoding detection (UTF-8, UTF-16, ASCII)
 * - Backup creation before overwrites
 * - Path resolution and validation
 * - Symlink handling
 * - Permission checking
 */

#include <QString>
#include <QByteArray>
#include <QFileInfo>
#include <QDir>

namespace RawrXD {

/**
 * \enum Encoding
 * \brief Supported file encodings for automatic detection
 */
enum class Encoding {
    UTF8,           ///< UTF-8 encoding (most common)
    UTF16_LE,       ///< UTF-16 Little Endian
    UTF16_BE,       ///< UTF-16 Big Endian
    ASCII,          ///< ASCII encoding
    Unknown         ///< Could not detect encoding
};

/**
 * \struct FileOperationResult
 * \brief Result of a file operation with success/failure details
 */
struct FileOperationResult {
    bool success;           ///< Whether the operation succeeded
    QString errorMessage;   ///< Human-readable error message (empty if success)
    QString backupPath;     ///< Path to backup file (if created)
    
    FileOperationResult() : success(false) {}
    explicit FileOperationResult(bool ok) : success(ok) {}
    FileOperationResult(bool ok, const QString& msg) : success(ok), errorMessage(msg) {}
};

/**
 * \class FileManager
 * \brief Centralized file operations with safety guarantees
 * 
 * All operations are safe and atomic where possible. Failed operations
 * leave the filesystem in a consistent state.
 * 
 * Example usage:
 * \code
 * FileManager fm;
 * QString content;
 * if (fm.readFile("/path/to/file.cpp", content)) {
 *     content += "\n// Modified";
 *     fm.writeFile("/path/to/file.cpp", content);
 * }
 * \endcode
 */
class FileManager {
public:
    FileManager();
    ~FileManager();
    
    // ========== Reading Operations ==========
    
    /**
     * \brief Read file contents with automatic encoding detection
     * \param path Absolute or relative path to file
     * \param content Output parameter for file contents
     * \param detectedEncoding Optional output parameter for detected encoding
     * \return true if successful, false otherwise
     */
    bool readFile(const QString& path, QString& content, Encoding* detectedEncoding = nullptr);
    
    /**
     * \brief Read file as raw bytes (no encoding conversion)
     * \param path Absolute or relative path to file
     * \param data Output parameter for raw bytes
     * \return true if successful, false otherwise
     */
    bool readFileRaw(const QString& path, QByteArray& data);
    
    /**
     * \brief Detect file encoding from byte order mark (BOM) and content analysis
     * \param data First few bytes of file
     * \return Detected encoding
     */
    static Encoding detectEncoding(const QByteArray& data);
    
    // ========== Writing Operations ==========
    
    /**
     * \brief Write file atomically (write to temp, then rename)
     * \param path Absolute or relative path to file
     * \param content Text content to write
     * \param createBackup If true, create backup before overwriting existing file
     * \return FileOperationResult with success status and details
     */
    FileOperationResult writeFile(const QString& path, const QString& content, bool createBackup = true);
    
    /**
     * \brief Write raw bytes to file atomically
     * \param path Absolute or relative path to file
     * \param data Raw bytes to write
     * \param createBackup If true, create backup before overwriting existing file
     * \return FileOperationResult with success status and details
     */
    FileOperationResult writeFileRaw(const QString& path, const QByteArray& data, bool createBackup = true);
    
    // ========== File CRUD Operations ==========
    
    /**
     * \brief Create a new empty file
     * \param path Absolute or relative path to file
     * \return FileOperationResult with success status and details
     */
    FileOperationResult createFile(const QString& path);
    
    /**
     * \brief Delete a file safely (with optional confirmation)
     * \param path Absolute or relative path to file
     * \param moveToTrash If true, move to system trash instead of permanent delete
     * \return FileOperationResult with success status and details
     */
    FileOperationResult deleteFile(const QString& path, bool moveToTrash = true);
    
    /**
     * \brief Rename a file or directory
     * \param oldPath Current path
     * \param newPath New path (can be just new name, or full path for move)
     * \return FileOperationResult with success status and details
     */
    FileOperationResult renameFile(const QString& oldPath, const QString& newPath);
    
    /**
     * \brief Move file to different directory
     * \param sourcePath Current file path
     * \param destPath Destination directory or full path
     * \return FileOperationResult with success status and details
     */
    FileOperationResult moveFile(const QString& sourcePath, const QString& destPath);
    
    /**
     * \brief Copy file to destination
     * \param sourcePath Source file path
     * \param destPath Destination directory or full path
     * \param overwrite If false, fail if destination exists
     * \return FileOperationResult with success status and details
     */
    FileOperationResult copyFile(const QString& sourcePath, const QString& destPath, bool overwrite = false);
    
    // ========== Directory Operations ==========
    
    /**
     * \brief Create directory (recursive, like mkdir -p)
     * \param path Absolute or relative path to directory
     * \return FileOperationResult with success status and details
     */
    FileOperationResult createDirectory(const QString& path);
    
    /**
     * \brief Delete directory and all contents
     * \param path Absolute or relative path to directory
     * \param moveToTrash If true, move to system trash instead of permanent delete
     * \return FileOperationResult with success status and details
     */
    FileOperationResult deleteDirectory(const QString& path, bool moveToTrash = true);
    
    /**
     * \brief Copy entire directory tree
     * \param sourcePath Source directory
     * \param destPath Destination directory
     * \return FileOperationResult with success status and details
     */
    FileOperationResult copyDirectory(const QString& sourcePath, const QString& destPath);
    
    // ========== Path Operations ==========
    
    /**
     * \brief Convert relative path to absolute path
     * \param relativePath Relative path (can be absolute, will be normalized)
     * \param basePath Base directory for relative paths (defaults to current dir)
     * \return Absolute canonical path
     */
    static QString toAbsolutePath(const QString& relativePath, const QString& basePath = QString());
    
    /**
     * \brief Convert absolute path to relative path from base
     * \param absolutePath Absolute path
     * \param basePath Base directory to make path relative to
     * \return Relative path, or original path if not under base
     */
    static QString toRelativePath(const QString& absolutePath, const QString& basePath);
    
    /**
     * \brief Check if path exists (file or directory)
     * \param path Path to check
     * \return true if exists, false otherwise
     */
    static bool exists(const QString& path);
    
    /**
     * \brief Check if path is a file
     * \param path Path to check
     * \return true if exists and is a file, false otherwise
     */
    static bool isFile(const QString& path);
    
    /**
     * \brief Check if path is a directory
     * \param path Path to check
     * \return true if exists and is a directory, false otherwise
     */
    static bool isDirectory(const QString& path);
    
    /**
     * \brief Check if path is a symbolic link
     * \param path Path to check
     * \return true if exists and is a symlink, false otherwise
     */
    static bool isSymlink(const QString& path);
    
    /**
     * \brief Check if file is readable
     * \param path Path to file
     * \return true if readable, false otherwise
     */
    static bool isReadable(const QString& path);
    
    /**
     * \brief Check if file is writable
     * \param path Path to file
     * \return true if writable, false otherwise
     */
    static bool isWritable(const QString& path);
    
    /**
     * \brief Get file size in bytes
     * \param path Path to file
     * \return File size, or -1 if file doesn't exist
     */
    static qint64 fileSize(const QString& path);
    
    /**
     * \brief Get file modification time
     * \param path Path to file
     * \return Last modified timestamp
     */
    static QDateTime lastModified(const QString& path);
    
    // ========== Backup Operations ==========
    
    /**
     * \brief Create backup of file with timestamp suffix
     * \param path Path to file to backup
     * \return Path to backup file, or empty string if failed
     */
    QString createBackup(const QString& path);
    
    /**
     * \brief Set whether to create backups by default for write operations
     * \param enable If true, backups are created by default
     */
    void setAutoBackup(bool enable);
    
    /**
     * \brief Get whether auto-backup is enabled
     * \return true if auto-backup is enabled
     */
    bool isAutoBackupEnabled() const;
    
private:
    bool m_autoBackup;  ///< Whether to automatically create backups
    
    /**
     * \brief Create unique temporary file path for atomic writes
     * \param originalPath Original file path
     * \return Path to temporary file
     */
    static QString createTempPath(const QString& originalPath);
};

} // namespace RawrXD
