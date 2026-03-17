#include "file_browser.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <ctime>

// SCALAR-ONLY: File browser with no threading

namespace fs = std::filesystem;

namespace RawrXD {

FileBrowser::FileBrowser() : root_path_(""), root_node_(nullptr) {
}

FileBrowser::~FileBrowser() {
}

void FileBrowser::SetRootPath(const std::string& path) {
    root_path_ = path;
    Refresh();
}

void FileBrowser::Refresh() {
    if (root_path_.empty()) return;
    
    std::cout << "Scanning directory: " << root_path_ << std::endl;
    root_node_ = ScanDirectory(root_path_);
}

std::shared_ptr<FileNode> FileBrowser::ScanDirectory(const std::string& path) {
    auto node = std::make_shared<FileNode>();
    
    try {
        fs::path fs_path(path);
        
        node->name = fs_path.filename().string();
        if (node->name.empty()) node->name = path;
        node->full_path = path;
        node->is_directory = fs::is_directory(fs_path);
        node->expanded = false;
        
        if (node->is_directory) {
            node->size = 0;
            // Populate children (scalar)
            PopulateNode(node);
        } else {
            node->size = fs::file_size(fs_path);
        }
        
        // Get last modified time (scalar)
        auto ftime = fs::last_write_time(fs_path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        char time_buf[100];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&cftime));
        node->last_modified = time_buf;
        
    } catch (const std::exception& e) {
        std::cerr << "Error scanning " << path << ": " << e.what() << std::endl;
    }
    
    return node;
}

void FileBrowser::PopulateNode(std::shared_ptr<FileNode> node) {
    if (!node || !node->is_directory) return;
    
    node->children.clear();
    
    try {
        // Scalar directory iteration
        for (const auto& entry : fs::directory_iterator(node->full_path)) {
            auto child = std::make_shared<FileNode>();
            
            child->name = entry.path().filename().string();
            child->full_path = entry.path().string();
            child->is_directory = entry.is_directory();
            child->expanded = false;
            
            if (child->is_directory) {
                child->size = 0;
            } else {
                try {
                    child->size = fs::file_size(entry.path());
                } catch (...) {
                    child->size = 0;
                }
            }
            
            // Get last modified (scalar)
            try {
                auto ftime = fs::last_write_time(entry.path());
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                );
                std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
                char time_buf[100];
                std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&cftime));
                child->last_modified = time_buf;
            } catch (...) {
                child->last_modified = "Unknown";
            }
            
            node->children.push_back(child);
        }
        
        // Sort: directories first, then by name (scalar)
        std::sort(node->children.begin(), node->children.end(),
            [](const std::shared_ptr<FileNode>& a, const std::shared_ptr<FileNode>& b) {
                if (a->is_directory != b->is_directory) {
                    return a->is_directory > b->is_directory;
                }
                return a->name < b->name;
            });
            
    } catch (const std::exception& e) {
        std::cerr << "Error populating " << node->full_path << ": " << e.what() << std::endl;
    }
}

void FileBrowser::ExpandNode(const std::string& path) {
    auto node = FindNode(path);
    if (node && node->is_directory && !node->expanded) {
        PopulateNode(node);
        node->expanded = true;
    }
}

void FileBrowser::CollapseNode(const std::string& path) {
    auto node = FindNode(path);
    if (node) {
        node->expanded = false;
        node->children.clear();
    }
}

std::shared_ptr<FileNode> FileBrowser::FindNode(const std::string& path) {
    if (!root_node_) return nullptr;
    
    // Scalar tree search
    std::vector<std::shared_ptr<FileNode>> stack;
    stack.push_back(root_node_);
    
    while (!stack.empty()) {
        auto current = stack.back();
        stack.pop_back();
        
        if (current->full_path == path) {
            return current;
        }
        
        for (auto& child : current->children) {
            stack.push_back(child);
        }
    }
    
    return nullptr;
}

bool FileBrowser::OpenFile(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            std::cerr << "File not found: " << path << std::endl;
            return false;
        }
        
        if (fs::is_directory(path)) {
            std::cerr << "Cannot open directory as file: " << path << std::endl;
            return false;
        }
        
        if (on_file_open_) {
            on_file_open_(path);
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error opening file: " << e.what() << std::endl;
        return false;
    }
}

bool FileBrowser::CreateFile(const std::string& path) {
    try {
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        file.close();
        
        std::cout << "Created file: " << path << std::endl;
        Refresh();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating file: " << e.what() << std::endl;
        return false;
    }
}

bool FileBrowser::CreateDirectory(const std::string& path) {
    try {
        if (fs::create_directories(path)) {
            std::cout << "Created directory: " << path << std::endl;
            Refresh();
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error creating directory: " << e.what() << std::endl;
        return false;
    }
}

bool FileBrowser::DeleteFile(const std::string& path) {
    try {
        if (fs::remove(path)) {
            std::cout << "Deleted: " << path << std::endl;
            Refresh();
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting: " << e.what() << std::endl;
        return false;
    }
}

bool FileBrowser::RenameFile(const std::string& old_path, const std::string& new_path) {
    try {
        fs::rename(old_path, new_path);
        std::cout << "Renamed: " << old_path << " -> " << new_path << std::endl;
        Refresh();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error renaming: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> FileBrowser::SearchFiles(const std::string& pattern) {
    std::vector<std::string> results;
    
    if (root_path_.empty()) return results;
    
    try {
        // Scalar recursive search
        for (const auto& entry : fs::recursive_directory_iterator(root_path_)) {
            if (!entry.is_directory()) {
                std::string filename = entry.path().filename().string();
                
                // Simple pattern matching (scalar)
                if (filename.find(pattern) != std::string::npos) {
                    results.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error searching: " << e.what() << std::endl;
    }
    
    return results;
}

void FileBrowser::SetOnFileOpen(std::function<void(const std::string&)> callback) {
    on_file_open_ = callback;
}

void FileBrowser::SetOnFileSelect(std::function<void(const std::string&)> callback) {
    on_file_select_ = callback;
}

} // namespace RawrXD
