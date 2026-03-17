#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>

// SCALAR-ONLY: File browser tree widget with no threading

namespace RawrXD {

struct FileNode {
    std::string name;
    std::string full_path;
    bool is_directory;
    int64_t size;
    std::string last_modified;
    std::vector<std::shared_ptr<FileNode>> children;
    bool expanded;
};

class FileBrowser {
public:
    FileBrowser();
    ~FileBrowser();

    // Initialize with root path
    void SetRootPath(const std::string& path);
    std::string GetRootPath() const { return root_path_; }

    // Tree operations (scalar)
    std::shared_ptr<FileNode> GetRoot() const { return root_node_; }
    void Refresh();
    void ExpandNode(const std::string& path);
    void CollapseNode(const std::string& path);
    
    // File operations (scalar)
    bool OpenFile(const std::string& path);
    bool CreateFile(const std::string& path);
    bool CreateDirectory(const std::string& path);
    bool DeleteFile(const std::string& path);
    bool RenameFile(const std::string& old_path, const std::string& new_path);
    
    // Search (scalar)
    std::vector<std::string> SearchFiles(const std::string& pattern);
    
    // Callbacks
    void SetOnFileOpen(std::function<void(const std::string&)> callback);
    void SetOnFileSelect(std::function<void(const std::string&)> callback);
    
private:
    std::string root_path_;
    std::shared_ptr<FileNode> root_node_;
    std::function<void(const std::string&)> on_file_open_;
    std::function<void(const std::string&)> on_file_select_;
    
    // Scalar directory scanning
    std::shared_ptr<FileNode> ScanDirectory(const std::string& path);
    void PopulateNode(std::shared_ptr<FileNode> node);
    std::shared_ptr<FileNode> FindNode(const std::string& path);
};

} // namespace RawrXD
