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
 * Uses std::filesystem::path and std::filesystem::path for cross-platform directory management.
 * Handles recursive operations with proper error reporting.
 */
class QtDirectoryManager : public void, public IDirectoryManager {

public:
    explicit QtDirectoryManager(void* parent = nullptr);
    ~QtDirectoryManager() override = default;
    
    // IDirectoryManager interface implementation
    FileOperationResult createDirectory(const std::string& path) override;
    
    FileOperationResult deleteDirectory(const std::string& path,
                                       bool moveToTrash = true) override;
    
    FileOperationResult copyDirectory(const std::string& sourcePath,
                                     const std::string& destPath) override;
    
    bool exists(const std::string& path) const override;
    
    bool isDirectory(const std::string& path) const override;
    
    std::string toAbsolutePath(const std::string& relativePath,
                          const std::string& basePath = std::string()) const override;
    
    std::string toRelativePath(const std::string& absolutePath,
                          const std::string& basePath) const override;
    
    // Additional non-interface helpers
    std::vector<std::string> listFiles(const std::string& path,
                         bool recursive = false) const;
    
    std::vector<std::string> listDirectories(const std::string& path,
                               bool recursive = false) const;

private:
    bool removeDirectoryRecursive(const std::string& path) const;
    bool copyDirectoryRecursive(const std::string& sourcePath,
                               const std::string& destPath) const;
    void listFilesRecursive(const std::string& path, std::vector<std::string>& files) const;
    void listDirectoriesRecursive(const std::string& path, std::vector<std::string>& dirs) const;
};

} // namespace RawrXD

#endif // RAWRXD_QT_DIRECTORY_MANAGER_H

