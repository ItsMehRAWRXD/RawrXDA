/**
 * \file ifile_reader.h
 * \brief Abstract interface for file reading operations (DIP)
 * \author RawrXD Team
 * \date 2025-12-05
 */

#ifndef RAWRXD_IFILE_READER_H
#define RAWRXD_IFILE_READER_H

#include <QString>
#include <QByteArray>

namespace RawrXD {

/**
 * \brief File encoding types
 */
enum class Encoding {
    UTF8,
    UTF16_LE,
    UTF16_BE,
    ASCII,
    Unknown
};

/**
 * \brief Abstract interface for file reading operations
 * 
 * This interface follows the Dependency Inversion Principle (DIP),
 * allowing high-level modules to depend on abstractions rather than
 * concrete implementations.
 * 
 * Key Design Principles:
 * - All methods are const (no side effects)
 * - Pure virtual (= 0) to enforce implementation
 * - Platform-agnostic interface
 */
class IFileReader {
public:
    virtual ~IFileReader() = default;
    
    /**
     * \brief Read entire file content as string
     * \param path Absolute or relative file path
     * \param content Output parameter for file content
     * \param detectedEncoding Optional output parameter for detected encoding
     * \return true if successful, false on error
     */
    virtual bool readFile(const QString& path, 
                         QString& content, 
                         Encoding* detectedEncoding = nullptr) const = 0;
    
    /**
     * \brief Read raw file content as byte array
     * \param path Absolute or relative file path
     * \param data Output parameter for raw bytes
     * \return true if successful, false on error
     */
    virtual bool readFileRaw(const QString& path, QByteArray& data) const = 0;
    
    /**
     * \brief Detect encoding of raw data
     * \param data Raw byte array
     * \return Detected encoding type
     */
    virtual Encoding detectEncoding(const QByteArray& data) const = 0;
    
    /**
     * \brief Check if file exists
     * \param path File path
     * \return true if file exists
     */
    virtual bool exists(const QString& path) const = 0;
    
    /**
     * \brief Check if path is a regular file
     * \param path File path
     * \return true if path points to a regular file
     */
    virtual bool isFile(const QString& path) const = 0;
    
    /**
     * \brief Check if file is readable
     * \param path File path
     * \return true if file can be read
     */
    virtual bool isReadable(const QString& path) const = 0;
    
    /**
     * \brief Get file size in bytes
     * \param path File path
     * \return File size, or -1 on error
     */
    virtual qint64 fileSize(const QString& path) const = 0;
};

} // namespace RawrXD

#endif // RAWRXD_IFILE_READER_H
