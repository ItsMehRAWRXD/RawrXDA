#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

// SCALAR-ONLY: File browser tree for IDE with full navigation

namespace RawrXD {

struct FileNode {
    std::string name;
    std::string full_path;
    bool is_directory;
    size_t size_bytes;
    std::string last_modified;
    std::vector<std::shared_ptr<FileNode>> children;
    bool is_expanded;
    int depth_level;
};

class FileBrowserTree {
public:
    FileBrowserTree();
    ~FileBrowserTree();

    // Tree operations (scalar)
    bool LoadDirectory(const std::string& root_path);
    void RefreshTree();
    void ExpandNode(const std::string& path);
    void CollapseNode(const std::string& path);
    void ToggleNode(const std::string& path);

    // File operations (scalar)
    bool CreateFile(const std::string& path, const std::string& content = "");
    bool CreateDirectory(const std::string& path);
    bool DeleteFile(const std::string& path);
    bool DeleteDirectory(const std::string& path, bool recursive = false);
    bool RenameFile(const std::string& old_path, const std::string& new_path);
    bool MoveFile(const std::string& source, const std::string& dest);
    bool CopyFile(const std::string& source, const std::string& dest);

    // Navigation (scalar)
    std::shared_ptr<FileNode> FindNode(const std::string& path);
    std::vector<std::shared_ptr<FileNode>> GetVisibleNodes();
    std::vector<std::string> GetAllFiles(const std::string& extension = "");
    std::vector<std::string> SearchFiles(const std::string& query);

    // Getters
    std::shared_ptr<FileNode> GetRoot() const { return root_; }
    std::string GetRootPath() const { return root_path_; }
    size_t GetTotalFiles() const { return total_files_; }
    size_t GetTotalDirectories() const { return total_directories_; }

private:
    std::shared_ptr<FileNode> root_;
    std::string root_path_;
    size_t total_files_;
    size_t total_directories_;
    std::map<std::string, std::shared_ptr<FileNode>> path_cache_;

    // Helper methods (scalar)
    void ScanDirectory(std::shared_ptr<FileNode> parent, const std::string& path, int depth);
    bool IsHiddenFile(const std::string& name);
    std::string FormatFileSize(size_t bytes);
    std::string GetFileExtension(const std::string& path);
};

} // namespace RawrXD
