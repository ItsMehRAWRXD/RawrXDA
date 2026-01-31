#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>

struct FileInfo {
    std::string name;
    std::string path;
    bool isDirectory;
    uint64_t size;
};

class FileBrowser {
public:
    FileBrowser();
    ~FileBrowser();

    void initialize();
    std::vector<FileInfo> listDirectory(const std::string& dirpath);
    std::vector<std::string> getDrives();

    // Callbacks
    std::function<void(const std::string&)> onFileSelected;
    std::function<void(const std::string&, const std::string&)> onError;

private:
    void logOperation(const std::string& level, const std::string& message);
};

