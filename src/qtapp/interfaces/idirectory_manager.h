/**
 * \file idirectory_manager.h
 * \brief Abstract interface for directory operations (DIP)
 * \author RawrXD Team
 * \date 2025-12-05
 */

#ifndef RAWRXD_IDIRECTORY_MANAGER_H
#define RAWRXD_IDIRECTORY_MANAGER_H

#include "ifile_writer.h"
#include <QString>

namespace RawrXD {

/**
 * \brief Abstract interface for directory management operations
 * 
 * This interface separates directory-specific operations from
 * file operations, following the Single Responsibility Principle.
 */
class IDirectoryManager {
public:
    virtual ~IDirectoryManager() = default;
    
    /**
     * \brief Create a directory (including parent directories)
     * \param path Directory path
     * \return Operation result
     */
    virtual FileOperationResult createDirectory(const QString& path) = 0;
    
    /**
     * \brief Delete a directory recursively
     * \param path Directory path
     * \param moveToTrash Move to trash instead of permanent delete
     * \return Operation result
     */
    virtual FileOperationResult deleteDirectory(const QString& path,
                                               bool moveToTrash = true) = 0;
    
    /**
     * \brief Copy a directory recursively
     * \param sourcePath Source directory path
     * \param destPath Destination directory path
     * \return Operation result
     */
    virtual FileOperationResult copyDirectory(const QString& sourcePath,
                                             const QString& destPath) = 0;
    
    /**
     * \brief Check if path is a directory
     * \param path Path to check
     * \return true if path points to a directory
     */
    virtual bool isDirectory(const QString& path) const = 0;
    
    /**
     * \brief Convert relative path to absolute
     * \param relativePath Relative path
     * \param basePath Base path (defaults to current directory)
     * \return Absolute path
     */
    virtual QString toAbsolutePath(const QString& relativePath,
                                   const QString& basePath = QString()) const = 0;
    
    /**
     * \brief Convert absolute path to relative
     * \param absolutePath Absolute path
     * \param basePath Base path for relativity
     * \return Relative path
     */
    virtual QString toRelativePath(const QString& absolutePath,
                                   const QString& basePath) const = 0;
    
    /**
     * \brief Check if path exists (file or directory)
     * \param path Path to check
     * \return true if path exists
     */
    virtual bool exists(const QString& path) const = 0;
};

} // namespace RawrXD

#endif // RAWRXD_IDIRECTORY_MANAGER_H
