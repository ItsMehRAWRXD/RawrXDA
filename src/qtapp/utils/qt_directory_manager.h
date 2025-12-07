/**
 * \file qt_directory_manager.h
 * \brief Qt-based implementation of IDirectoryManager interface
 * \author RawrXD Team
 * \date 2025-12-05
 */

#ifndef RAWRXD_QT_DIRECTORY_MANAGER_H
#define RAWRXD_QT_DIRECTORY_MANAGER_H

#include "../interfaces/idirectory_manager.h"

namespace RawrXD {

/**
 * \brief Qt-based concrete implementation of directory operations
 * 
 * Uses QDir and QFileInfo for cross-platform directory management.
 * Handles recursive operations with proper error reporting.
 */
class QtDirectoryManager : public IDirectoryManager {
public:
    QtDirectoryManager();
    ~QtDirectoryManager() override = default;
    
    // IDirectoryManager interface implementation
    FileOperationResult createDirectory(const QString& path,
                                       bool createParents = true) override;
    
    FileOperationResult deleteDirectory(const QString& path,
                                       bool recursive = false) override;
    
    FileOperationResult copyDirectory(const QString& sourcePath,
                                     const QString& destPath) override;
    
    bool exists(const QString& path) const override;
    
    bool isDirectory(const QString& path) const override;
    
    QStringList listFiles(const QString& path,
                         bool recursive = false) const override;
    
    QStringList listDirectories(const QString& path,
                               bool recursive = false) const override;
    
    QString absolutePath(const QString& path) const override;
    
    QString relativePath(const QString& basePath,
                        const QString& targetPath) const override;

private:
    bool removeDirectoryRecursive(const QString& path) const;
    bool copyDirectoryRecursive(const QString& sourcePath,
                               const QString& destPath) const;
    void listFilesRecursive(const QString& path, QStringList& files) const;
    void listDirectoriesRecursive(const QString& path, QStringList& dirs) const;
};

} // namespace RawrXD

#endif // RAWRXD_QT_DIRECTORY_MANAGER_H
