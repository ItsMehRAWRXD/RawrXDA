// Simple in-memory file system representation
#include <string>
#include <map>
#include <vector>
#include <memory>

// A simple, in-memory file system representation
class InMemoryFileSystem {
private:
    std::map<std::string, std::string> files;
    std::map<std::string, std::vector<std::string>> directories;
    
public:
    void write_file(const std::string& path, const std::string& content) {
        files[path] = content;
        
        // Create directory structure
        size_t last_slash = path.find_last_of('/');
        if (last_slash != std::string::npos) {
            std::string dir_path = path.substr(0, last_slash);
            std::string filename = path.substr(last_slash + 1);
            
            if (directories.find(dir_path) == directories.end()) {
                directories[dir_path] = std::vector<std::string>();
            }
            directories[dir_path].push_back(filename);
        }
    }
    
    std::string read_file(const std::string& path) const {
        auto it = files.find(path);
        if (it != files.end()) {
            return it->second;
        }
        return "";
    }
    
    bool file_exists(const std::string& path) const {
        return files.find(path) != files.end();
    }
    
    std::vector<std::string> list_directory(const std::string& path) const {
        auto it = directories.find(path);
        if (it != directories.end()) {
            return it->second;
        }
        return std::vector<std::string>();
    }
    
    void create_directory(const std::string& path) {
        if (directories.find(path) == directories.end()) {
            directories[path] = std::vector<std::string>();
        }
    }
    
    bool directory_exists(const std::string& path) const {
        return directories.find(path) != directories.end();
    }
    
    void delete_file(const std::string& path) {
        files.erase(path);
        
        // Remove from directory listing
        size_t last_slash = path.find_last_of('/');
        if (last_slash != std::string::npos) {
            std::string dir_path = path.substr(0, last_slash);
            std::string filename = path.substr(last_slash + 1);
            
            auto it = directories.find(dir_path);
            if (it != directories.end()) {
                auto& files_in_dir = it->second;
                files_in_dir.erase(
                    std::remove(files_in_dir.begin(), files_in_dir.end(), filename),
                    files_in_dir.end()
                );
            }
        }
    }
    
    std::vector<std::string> get_all_files() const {
        std::vector<std::string> all_files;
        for (const auto& [path, content] : files) {
            all_files.push_back(path);
        }
        return all_files;
    }
    
    size_t get_file_count() const {
        return files.size();
    }
    
    size_t get_directory_count() const {
        return directories.size();
    }
};
