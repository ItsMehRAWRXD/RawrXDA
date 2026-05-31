// file_system.hpp - Cross-Platform File System Integration
// Author: RAW RXD IDE Team
// License: MIT

#ifndef FILE_SYSTEM_HPP
#define FILE_SYSTEM_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <optional>
#include <variant>
#include <regex>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <fileapi.h>
    #include <handleapi.h>
    #include <errhandlingapi.h>
    using FileHandle = HANDLE;
    #define INVALID_FILE_HANDLE INVALID_HANDLE_VALUE
#else
    #include <sys/inotify.h>
    #include <sys/epoll.h>
    #include <dirent.h>
    #include <unistd.h>
    #include <ftw.h>
    #include <sys/stat.h>
    using FileHandle = int;
    #define INVALID_FILE_HANDLE -1
#endif

namespace rawrxd {

// ═══════════════════════════════════════════════════════════════════════
// FILE TYPES
// ═══════════════════════════════════════════════════════════════════════

enum class FileType {
    Unknown,
    Regular,
    Directory,
    SymbolicLink,
    BlockDevice,
    CharacterDevice,
    FIFO,
    Socket
};

enum class FileEvent {
    Created,
    Modified,
    Deleted,
    Renamed,
    AttributeChanged
};

struct FileInfo {
    std::string path;
    std::string name;
    std::string extension;
    FileType type;
    size_t size;
    time_t created;
    time_t modified;
    time_t accessed;
    bool is_hidden;
    bool is_readonly;
    bool is_executable;
    std::map<std::string, std::string> attributes;
};

struct FileChange {
    std::string path;
    FileEvent event;
    std::string old_path;  // For renames
    time_t timestamp;
    bool is_directory;
};

struct FileFilter {
    std::set<std::string> include_extensions;
    std::set<std::string> exclude_extensions;
    std::set<std::string> include_names;
    std::set<std::string> exclude_names;
    std::vector<std::regex> include_patterns;
    std::vector<std::regex> exclude_patterns;
    bool include_hidden = false;
    int max_depth = -1;  // -1 = unlimited
};

struct FileTreeNode {
    std::string name;
    std::string path;
    FileType type;
    bool expanded = false;
    std::vector<FileTreeNode> children;
    int depth = 0;
};

struct WatchConfig {
    bool watch_recursive = true;
    bool watch_hidden = false;
    bool debounce_ms = 100;
    int max_watch_handles = 1024;
    bool follow_symlinks = true;
    bool ignore_vcs_folders = true;
};

// ═══════════════════════════════════════════════════════════════════════
// FILE SYSTEM OPERATIONS
// ═══════════════════════════════════════════════════════════════════════

class FileSystem {
public:
    FileSystem();
    ~FileSystem();

    // File Operations
    bool fileExists(const std::string& path);
    bool isDirectory(const std::string& path);
    bool isFile(const std::string& path);
    
    std::optional<FileInfo> getFileInfo(const std::string& path);
    bool setFileInfo(const std::string& path, const FileInfo& info);
    
    // Read/Write
    std::optional<std::string> readFile(const std::string& path);
    std::optional<std::vector<uint8_t>> readFileBinary(const std::string& path);
    bool writeFile(const std::string& path, const std::string& content);
    bool writeFileBinary(const std::string& path, const std::vector<uint8_t>& data);
    bool appendFile(const std::string& path, const std::string& content);
    
    // File Tree
    std::optional<FileTreeNode> buildFileTree(const std::string& root, 
                                               const FileFilter& filter = {});
    std::vector<std::string> listDirectory(const std::string& path,
                                           const FileFilter& filter = {});
    
    // Directory Operations
    bool createDirectory(const std::string& path);
    bool removeDirectory(const std::string& path, bool recursive = false);
    bool copyFile(const std::string& src, const std::string& dst);
    bool moveFile(const std::string& src, const std::string& dst);
    bool deleteFile(const std::string& path);
    
    // File Search
    std::vector<std::string> findFiles(const std::string& root,
                                       const std::string& pattern,
                                       bool recursive = true);
    std::vector<std::string> findFilesByExtension(const std::string& root,
                                                   const std::string& ext,
                                                   bool recursive = true);
    
    // Path Utilities
    std::string normalizePath(const std::string& path);
    std::string getAbsolutePath(const std::string& path);
    std::string getRelativePath(const std::string& from, const std::string& to);
    std::string getDirectory(const std::string& path);
    std::string getFileName(const std::string& path);
    std::string getExtension(const std::string& path);
    std::string joinPath(const std::string& a, const std::string& b);
    
    // Watch/Notify
    bool startWatching(const std::string& path);
    bool stopWatching(const std::string& path);
    void stopAllWatching();
    
    void onFileChange(std::function<void(const FileChange&)> callback);
    
    // Statistics
    int64_t getTotalFiles() const { return total_files_; }
    int64_t getTotalDirectories() const { return total_directories_; }
    size_t getTotalSize() const { return total_size_; }

private:
    WatchConfig config_;
    std::map<std::string, FileHandle> watch_handles_;
    std::vector<std::function<void(const FileChange&)>> change_callbacks_;
    
    int64_t total_files_ = 0;
    int64_t total_directories_ = 0;
    size_t total_size_ = 0;
    
    std::thread watcher_thread_;
    std::atomic<bool> watching_{false};
    std::mutex watcher_mutex_;
    
#ifdef _WIN32
    std::map<FileHandle, std::string> handle_to_path_;
#endif
    
    void watcherLoop();
    void processEvents();
    bool matchesFilter(const std::string& path, const FileFilter& filter);
    FileType getFileType(const std::string& path);
    void debouncedNotify(const FileChange& change);
    
    std::map<std::string, std::chrono::steady_clock::time_point> debounce_map_;
    std::vector<FileChange> pending_changes_;
    std::mutex debounce_mutex_;
};

// ═══════════════════════════════════════════════════════════════════════
// INLINE IMPLEMENTATIONS
// ═══════════════════════════════════════════════════════════════════════

inline FileSystem::FileSystem() {
    config_ = WatchConfig{};
    watching_ = false;
}

inline FileSystem::~FileSystem() {
    stopAllWatching();
}

inline bool FileSystem::fileExists(const std::string& path) {
#ifdef _WIN32
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    return access(path.c_str(), F_OK) == 0;
#endif
}

inline bool FileSystem::isDirectory(const std::string& path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

inline bool FileSystem::isFile(const std::string& path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

inline FileType FileSystem::getFileType(const std::string& path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return FileType::Unknown;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) return FileType::Directory;
    if (attr & FILE_ATTRIBUTE_REPARSE_POINT) return FileType::SymbolicLink;
    return FileType::Regular;
#else
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) return FileType::Unknown;
    
    if (S_ISDIR(st.st_mode)) return FileType::Directory;
    if (S_ISLNK(st.st_mode)) return FileType::SymbolicLink;
    if (S_ISREG(st.st_mode)) return FileType::Regular;
    if (S_ISBLK(st.st_mode)) return FileType::BlockDevice;
    if (S_ISCHR(st.st_mode)) return FileType::CharacterDevice;
    if (S_ISFIFO(st.st_mode)) return FileType::FIFO;
    if (S_ISSOCK(st.st_mode)) return FileType::Socket;
    return FileType::Unknown;
#endif
}

inline std::optional<FileInfo> FileSystem::getFileInfo(const std::string& path) {
    if (!fileExists(path)) return std::nullopt;
    
    FileInfo info;
    info.path = normalizePath(path);
    info.name = getFileName(info.path);
    info.extension = getExtension(info.path);
    info.type = getFileType(path);
    
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) {
        info.size = ((uint64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow;
        info.created = ((uint64_t)data.ftCreationTime.dwHighDateTime << 32) |
                       data.ftCreationTime.dwLowDateTime;
        info.modified = ((uint64_t)data.ftLastWriteTime.dwHighDateTime << 32) |
                        data.ftLastWriteTime.dwLowDateTime;
        info.accessed = ((uint64_t)data.ftLastAccessTime.dwHighDateTime << 32) |
                        data.ftLastAccessTime.dwLowDateTime;
        
        DWORD attr = data.dwFileAttributes;
        info.is_hidden = (attr & FILE_ATTRIBUTE_HIDDEN) != 0;
        info.is_readonly = (attr & FILE_ATTRIBUTE_READONLY) != 0;
        info.is_executable = false;
    }
#else
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        info.size = st.st_size;
        info.created = st.st_ctime;
        info.modified = st.st_mtime;
        info.accessed = st.st_atime;
        
        info.is_hidden = info.name[0] == '.';
        info.is_readonly = (st.st_mode & S_IWUSR) == 0;
        info.is_executable = (st.st_mode & S_IXUSR) != 0;
    }
#endif
    
    return info;
}

inline std::optional<std::string> FileSystem::readFile(const std::string& path) {
    if (!isFile(path)) return std::nullopt;
    
#ifdef _WIN32
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::nullopt;
    
    LARGE_INTEGER size;
    GetFileSizeEx(file, &size);
    
    std::string content(size.QuadPart, '\0');
    DWORD read;
    ReadFile(file, content.data(), (DWORD)size.QuadPart, &read, nullptr);
    CloseHandle(file);
    
    return content;
#else
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return std::nullopt;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    std::string content(size, '\0');
    fread(content.data(), 1, size, f);
    fclose(f);
    
    return content;
#endif
}

inline bool FileSystem::writeFile(const std::string& path, const std::string& content) {
    // Ensure directory exists
    std::string dir = getDirectory(path);
    if (!dir.empty() && !fileExists(dir)) {
        createDirectory(dir);
    }
    
#ifdef _WIN32
    HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    
    DWORD written;
    BOOL ok = WriteFile(file, content.data(), (DWORD)content.size(), &written, nullptr);
    CloseHandle(file);
    
    return ok && written == content.size();
#else
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    
    size_t written = fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    
    return written == content.size();
#endif
}

inline bool FileSystem::createDirectory(const std::string& path) {
    if (fileExists(path)) return isDirectory(path);
    
#ifdef _WIN32
    return CreateDirectoryA(path.c_str(), nullptr) != 0;
#else
    return mkdir(path.c_str(), 0755) == 0;
#endif
}

inline bool FileSystem::deleteFile(const std::string& path) {
    if (!fileExists(path)) return false;
    
#ifdef _WIN32
    if (isDirectory(path)) {
        return RemoveDirectoryA(path.c_str()) != 0;
    }
    return DeleteFileA(path.c_str()) != 0;
#else
    if (isDirectory(path)) {
        return rmdir(path.c_str()) == 0;
    }
    return unlink(path.c_str()) == 0;
#endif
}

inline std::string FileSystem::getFileName(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

inline std::string FileSystem::getExtension(const std::string& path) {
    auto name = getFileName(path);
    auto pos = name.find_last_of('.');
    if (pos == std::string::npos || pos == 0) return "";
    return name.substr(pos);
}

inline std::string FileSystem::joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    
    bool a_sep = a.back() == '/' || a.back() == '\\';
    bool b_sep = b.front() == '/' || b.front() == '\\';
    
    if (a_sep && b_sep) return a + b.substr(1);
    if (!a_sep && !b_sep) return a + "/" + b;
    return a + b;
}

inline std::string FileSystem::normalizePath(const std::string& path) {
    std::string result = path;
    
    // Replace backslashes with forward slashes
    std::replace(result.begin(), result.end(), '\\', '/');
    
    // Remove duplicate slashes
    std::regex double_slash("//+");
    result = std::regex_replace(result, double_slash, "/");
    
    // Remove trailing slash (except for root)
    if (result.length() > 1 && result.back() == '/') {
        result.pop_back();
    }
    
    return result;
}

inline std::string FileSystem::getDirectory(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return "";
    return path.substr(0, pos);
}

inline std::vector<std::string> FileSystem::listDirectory(const std::string& path,
                                                          const FileFilter& filter) {
    std::vector<std::string> result;
    
#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA((path + "/*").c_str(), &find_data);
    
    if (find == INVALID_HANDLE_VALUE) return result;
    
    do {
        std::string name = find_data.cFileName;
        if (name == "." || name == "..") continue;
        
        std::string full_path = joinPath(path, name);
        
        if (!filter.include_hidden && name[0] == '.') continue;
        
        if (matchesFilter(full_path, filter)) {
            result.push_back(full_path);
        }
    } while (FindNextFileA(find, &find_data));
    
    FindClose(find);
#else
    DIR* dir = opendir(path.c_str());
    if (!dir) return result;
    
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        
        std::string full_path = joinPath(path, name);
        
        if (!filter.include_hidden && name[0] == '.') continue;
        
        if (matchesFilter(full_path, filter)) {
            result.push_back(full_path);
        }
    }
    
    closedir(dir);
#endif
    
    std::sort(result.begin(), result.end());
    return result;
}

inline std::optional<FileTreeNode> FileSystem::buildFileTree(const std::string& root,
                                                            const FileFilter& filter) {
    if (!isDirectory(root)) return std::nullopt;
    
    FileTreeNode root_node;
    root_node.name = getFileName(root);
    root_node.path = root;
    root_node.type = FileType::Directory;
    root_node.depth = 0;
    
    std::function<void(FileTreeNode&, const std::string&, int)> buildRecursive;
    buildRecursive = [&](FileTreeNode& node, const std::string& path, int depth) {
        if (filter.max_depth >= 0 && depth > filter.max_depth) return;
        
        auto entries = listDirectory(path, filter);
        
        for (const auto& entry_path : entries) {
            FileTreeNode child;
            child.name = getFileName(entry_path);
            child.path = entry_path;
            child.type = getFileType(entry_path);
            child.depth = depth;
            
            if (child.type == FileType::Directory) {
                buildRecursive(child, entry_path, depth + 1);
            }
            
            node.children.push_back(std::move(child));
        }
    };
    
    buildRecursive(root_node, root, 0);
    return root_node;
}

inline bool FileSystem::matchesFilter(const std::string& path, const FileFilter& filter) {
    std::string name = getFileName(path);
    std::string ext = getExtension(path);
    
    // Check exclusions first
    if (filter.exclude_extensions.count(ext)) return false;
    if (filter.exclude_names.count(name)) return false;
    
    for (const auto& pattern : filter.exclude_patterns) {
        if (std::regex_match(name, pattern)) return false;
    }
    
    // Check inclusions
    if (!filter.include_extensions.empty() && 
        !filter.include_extensions.count(ext)) return false;
    
    if (!filter.include_names.empty() && 
        !filter.include_names.count(name)) return false;
    
    for (const auto& pattern : filter.include_patterns) {
        if (std::regex_match(name, pattern)) return true;
    }
    
    // If no include patterns, accept by default
    return filter.include_patterns.empty();
}

inline bool FileSystem::startWatching(const std::string& path) {
    std::lock_guard<std::mutex> lock(watcher_mutex_);
    
    if (watch_handles_.count(path)) return true;  // Already watching
    
#ifdef _WIN32
    HANDLE handle = CreateFileA(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );
    
    if (handle == INVALID_HANDLE_VALUE) return false;
    
    handle_to_path_[handle] = path;
    watch_handles_[path] = handle;
#else
    int handle = inotify_init();
    if (handle < 0) return false;
    
    uint32_t mask = IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;
    if (config_.watch_recursive) {
        mask |= IN_ATTRIB;
    }
    
    int wd = inotify_add_watch(handle, path.c_str(), mask);
    if (wd < 0) {
        close(handle);
        return false;
    }
    
    watch_handles_[path] = (FileHandle)wd;
#endif
    
    if (!watching_) {
        watching_ = true;
        watcher_thread_ = std::thread([this] { watcherLoop(); });
    }
    
    return true;
}

inline void FileSystem::watcherLoop() {
#ifdef _WIN32
    std::vector<uint8_t> buffer(64 * 1024);
    OVERLAPPED ol = {};
    
    while (watching_) {
        for (auto& [path, handle] : watch_handles_) {
            DWORD bytes_returned = 0;
            ReadDirectoryChangesW(
                handle,
                buffer.data(),
                buffer.size(),
                config_.watch_recursive,
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME,
                &bytes_returned,
                &ol,
                nullptr
            );
            
            if (bytes_returned > 0) {
                auto* notify = (FILE_NOTIFY_INFORMATION*)buffer.data();
                
                while (true) {
                    std::wstring wname(notify->FileName, 
                                       notify->FileNameLength / sizeof(WCHAR));
                    std::string name(wname.begin(), wname.end());
                    
                    FileChange change;
                    change.path = joinPath(path, name);
                    change.timestamp = std::time(nullptr);
                    change.is_directory = 
                        (notify->Action == FILE_ACTION_ADDED_DIRECTORY ||
                         notify->Action == FILE_ACTION_REMOVED_DIRECTORY);
                    
                    switch (notify->Action) {
                        case FILE_ACTION_ADDED:
                        case FILE_ACTION_ADDED_DIRECTORY:
                            change.event = FileEvent::Created;
                            break;
                        case FILE_ACTION_REMOVED:
                        case FILE_ACTION_REMOVED_DIRECTORY:
                            change.event = FileEvent::Deleted;
                            break;
                        case FILE_ACTION_MODIFIED:
                            change.event = FileEvent::Modified;
                            break;
                        case FILE_ACTION_RENAMED_OLD_NAME:
                            change.old_path = change.path;
                            break;
                        case FILE_ACTION_RENAMED_NEW_NAME:
                            change.event = FileEvent::Renamed;
                            break;
                    }
                    
                    debouncedNotify(change);
                    
                    if (notify->NextEntryOffset == 0) break;
                    notify = (FILE_NOTIFY_INFORMATION*)(
                        (uint8_t*)notify + notify->NextEntryOffset);
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        processEvents();
    }
#else
    int epoll_fd = epoll_create(1);
    struct epoll_event ev;
    
    int inotify_fd = watch_handles_.begin()->second;
    ev.events = EPOLLIN;
    ev.data.fd = inotify_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inotify_fd, &ev);
    
    std::vector<uint8_t> buffer(64 * 1024);
    
    while (watching_) {
        struct epoll_event events[10];
        int n = epoll_wait(epoll_fd, events, 10, 100);
        
        for (int i = 0; i < n; i++) {
            ssize_t len = read(inotify_fd, buffer.data(), buffer.size());
            
            ssize_t pos = 0;
            while (pos < len) {
                auto* event = (struct inotify_event*)(buffer.data() + pos);
                
                FileChange change;
                change.timestamp = std::time(nullptr);
                
                if (event->mask & IN_CREATE) change.event = FileEvent::Created;
                else if (event->mask & IN_DELETE) change.event = FileEvent::Deleted;
                else if (event->mask & IN_MODIFY) change.event = FileEvent::Modified;
                else if (event->mask & IN_MOVED_FROM) change.event = FileEvent::Renamed;
                else if (event->mask & IN_MOVED_TO) change.event = FileEvent::Renamed;
                
                change.path = event->name;
                change.is_directory = (event->mask & IN_ISDIR) != 0;
                
                debouncedNotify(change);
                
                pos += sizeof(struct inotify_event) + event->len;
            }
        }
        
        processEvents();
    }
    
    close(epoll_fd);
#endif
}

inline void FileSystem::processEvents() {
    std::lock_guard<std::mutex> lock(debounce_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    std::vector<FileChange> to_notify;
    
    for (auto it = pending_changes_.begin(); it != pending_changes_.end(); ) {
        auto elapsed = now - debounce_map_[it->path];
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() 
            >= config_.debounce_ms) {
            to_notify.push_back(*it);
            it = pending_changes_.erase(it);
            debounce_map_.erase(it->path);
        } else {
            ++it;
        }
    }
    
    for (const auto& change : to_notify) {
        for (const auto& callback : change_callbacks_) {
            callback(change);
        }
    }
}

inline void FileSystem::debouncedNotify(const FileChange& change) {
    std::lock_guard<std::mutex> lock(debounce_mutex_);
    
    auto& stored = debounce_map_[change.path];
    stored = std::chrono::steady_clock::now();
    
    // Update or add change
    auto it = std::find_if(pending_changes_.begin(), pending_changes_.end(),
        [&change](const FileChange& c) { return c.path == change.path; });
    
    if (it != pending_changes_.end()) {
        *it = change;
    } else {
        pending_changes_.push_back(change);
    }
}

inline void FileSystem::onFileChange(std::function<void(const FileChange&)> callback) {
    change_callbacks_.push_back(callback);
}

inline bool FileSystem::stopWatching(const std::string& path) {
    std::lock_guard<std::mutex> lock(watcher_mutex_);
    
    auto it = watch_handles_.find(path);
    if (it == watch_handles_.end()) return false;
    
#ifdef _WIN32
    CloseHandle(it->second);
    handle_to_path_.erase(it->second);
#else
    close(it->second);
#endif
    
    watch_handles_.erase(it);
    return true;
}

inline void FileSystem::stopAllWatching() {
    watching_ = false;
    
    if (watcher_thread_.joinable()) {
        watcher_thread_.join();
    }
    
    std::lock_guard<std::mutex> lock(watcher_mutex_);
    
    for (auto& [path, handle] : watch_handles_) {
#ifdef _WIN32
        CloseHandle(handle);
#else
        close(handle);
#endif
    }
    
    watch_handles_.clear();
    handle_to_path_.clear();
}

inline std::vector<std::string> FileSystem::findFiles(const std::string& root,
                                                      const std::string& pattern,
                                                      bool recursive) {
    std::vector<std::string> results;
    std::regex regex_pattern(pattern);
    
    std::function<void(const std::string&)> search;
    search = [&](const std::string& dir) {
        auto entries = listDirectory(dir);
        
        for (const auto& entry : entries) {
            std::string name = getFileName(entry);
            
            if (std::regex_match(name, regex_pattern)) {
                results.push_back(entry);
            }
            
            if (recursive && isDirectory(entry)) {
                search(entry);
            }
        }
    };
    
    search(root);
    return results;
}

} // namespace rawrxd

#endif // FILE_SYSTEM_HPP
