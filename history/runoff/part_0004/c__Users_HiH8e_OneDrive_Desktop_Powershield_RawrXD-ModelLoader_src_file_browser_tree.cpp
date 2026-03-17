#include "file_browser_tree.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace fs = std::filesystem;

namespace RawrXD {

FileBrowserTree::FileBrowserTree() : total_files_(0), total_directories_(0) {
    root_ = std::make_shared<FileNode>();
}

FileBrowserTree::~FileBrowserTree() = default;

bool FileBrowserTree::LoadDirectory(const std::string& root_path) {
    root_path_ = root_path;
    total_files_ = 0;
    total_directories_ = 0;
    path_cache_.clear();

    if (!fs::exists(root_path) || !fs::is_directory(root_path)) {
        return false;
    }

    root_ = std::make_shared<FileNode>();
    root_->name = fs::path(root_path).filename().string();
    root_->full_path = root_path;
    root_->is_directory = true;
    root_->is_expanded = true;
    root_->depth_level = 0;
    root_->size_bytes = 0;

    path_cache_[root_path] = root_;
    ScanDirectory(root_, root_path, 0);

    return true;
}

void FileBrowserTree::ScanDirectory(std::shared_ptr<FileNode> parent, const std::string& path, int depth) {
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            std::string name = entry.path().filename().string();
            
            // Skip hidden files
            if (IsHiddenFile(name)) continue;

            auto node = std::make_shared<FileNode>();
            node->name = name;
            node->full_path = entry.path().string();
            node->is_directory = entry.is_directory();
            node->is_expanded = false;
            node->depth_level = depth + 1;

            // Get file size and modification time
            if (!node->is_directory) {
                try {
                    node->size_bytes = fs::file_size(entry);
                    total_files_++;
                } catch (...) {
                    node->size_bytes = 0;
                }
            } else {
                node->size_bytes = 0;
                total_directories_++;
            }

            // Format last modified time
            try {
                auto ftime = fs::last_write_time(entry);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                );
                std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
                std::ostringstream oss;
                oss << std::put_time(std::localtime(&cftime), "%Y-%m-%d %H:%M:%S");
                node->last_modified = oss.str();
            } catch (...) {
                node->last_modified = "Unknown";
            }

            parent->children.push_back(node);
            path_cache_[node->full_path] = node;

            // Recursively scan subdirectories if depth < 1 (lazy loading for performance)
            if (node->is_directory && depth < 1) {
                ScanDirectory(node, node->full_path, depth + 1);
            }
        }

        // Sort children: directories first, then alphabetically
        std::sort(parent->children.begin(), parent->children.end(),
            [](const std::shared_ptr<FileNode>& a, const std::shared_ptr<FileNode>& b) {
                if (a->is_directory != b->is_directory) {
                    return a->is_directory > b->is_directory;
                }
                return a->name < b->name;
            });

    } catch (const std::exception& e) {
        // Silently handle permission errors or other filesystem issues
    }
}

bool FileBrowserTree::IsHiddenFile(const std::string& name) {
    if (name.empty()) return true;
    if (name[0] == '.') return true;
    if (name == "desktop.ini" || name == "thumbs.db") return true;
    return false;
}

void FileBrowserTree::RefreshTree() {
    if (!root_path_.empty()) {
        LoadDirectory(root_path_);
    }
}

void FileBrowserTree::ExpandNode(const std::string& path) {
    auto node = FindNode(path);
    if (node && node->is_directory) {
        node->is_expanded = true;
        // Lazy load children if not already loaded
        if (node->children.empty()) {
            ScanDirectory(node, node->full_path, node->depth_level);
        }
    }
}

void FileBrowserTree::CollapseNode(const std::string& path) {
    auto node = FindNode(path);
    if (node && node->is_directory) {
        node->is_expanded = false;
    }
}

void FileBrowserTree::ToggleNode(const std::string& path) {
    auto node = FindNode(path);
    if (node && node->is_directory) {
        if (node->is_expanded) {
            CollapseNode(path);
        } else {
            ExpandNode(path);
        }
    }
}

std::shared_ptr<FileNode> FileBrowserTree::FindNode(const std::string& path) {
    auto it = path_cache_.find(path);
    if (it != path_cache_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<FileNode>> FileBrowserTree::GetVisibleNodes() {
    std::vector<std::shared_ptr<FileNode>> visible;
    
    std::function<void(std::shared_ptr<FileNode>)> traverse = [&](std::shared_ptr<FileNode> node) {
        if (!node) return;
        visible.push_back(node);
        
        if (node->is_expanded && node->is_directory) {
            for (const auto& child : node->children) {
                traverse(child);
            }
        }
    };
    
    if (root_) {
        traverse(root_);
    }
    
    return visible;
}

std::vector<std::string> FileBrowserTree::GetAllFiles(const std::string& extension) {
    std::vector<std::string> files;
    
    for (const auto& [path, node] : path_cache_) {
        if (!node->is_directory) {
            if (extension.empty() || GetFileExtension(path) == extension) {
                files.push_back(path);
            }
        }
    }
    
    return files;
}

std::vector<std::string> FileBrowserTree::SearchFiles(const std::string& query) {
    std::vector<std::string> results;
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    
    for (const auto& [path, node] : path_cache_) {
        std::string lower_name = node->name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
        
        if (lower_name.find(lower_query) != std::string::npos) {
            results.push_back(path);
        }
    }
    
    return results;
}

bool FileBrowserTree::CreateFile(const std::string& path, const std::string& content) {
    try {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << content;
        file.close();
        RefreshTree();
        return true;
    } catch (...) {
        return false;
    }
}

bool FileBrowserTree::CreateDirectory(const std::string& path) {
    try {
        bool result = fs::create_directories(path);
        RefreshTree();
        return result;
    } catch (...) {
        return false;
    }
}

bool FileBrowserTree::DeleteFile(const std::string& path) {
    try {
        bool result = fs::remove(path);
        RefreshTree();
        return result;
    } catch (...) {
        return false;
    }
}

bool FileBrowserTree::DeleteDirectory(const std::string& path, bool recursive) {
    try {
        bool result = recursive ? fs::remove_all(path) > 0 : fs::remove(path);
        RefreshTree();
        return result;
    } catch (...) {
        return false;
    }
}

bool FileBrowserTree::RenameFile(const std::string& old_path, const std::string& new_path) {
    try {
        fs::rename(old_path, new_path);
        RefreshTree();
        return true;
    } catch (...) {
        return false;
    }
}

bool FileBrowserTree::MoveFile(const std::string& source, const std::string& dest) {
    return RenameFile(source, dest);
}

bool FileBrowserTree::CopyFile(const std::string& source, const std::string& dest) {
    try {
        fs::copy(source, dest, fs::copy_options::overwrite_existing);
        RefreshTree();
        return true;
    } catch (...) {
        return false;
    }
}

std::string FileBrowserTree::FormatFileSize(size_t bytes) {
    const char* sizes[] = { "B", "KB", "MB", "GB", "TB" };
    int order = 0;
    double size = bytes;
    
    while (size >= 1024 && order < 4) {
        order++;
        size /= 1024;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << sizes[order];
    return oss.str();
}

std::string FileBrowserTree::GetFileExtension(const std::string& path) {
    size_t pos = path.find_last_of('.');
    if (pos != std::string::npos) {
        return path.substr(pos);
    }
    return "";
}

} // namespace RawrXD
