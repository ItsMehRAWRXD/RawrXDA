#pragma once

#include <optional>
#include <string>
#include <vector>

namespace RawrXD {
namespace Tools {

struct FileOpResult {
    bool success = false;
    std::string message;
    std::optional<std::string> path;
};

struct CopyOptions {
    bool overwrite = false;
    bool create_dirs = true;
    bool preserve_timestamps = false;
};

class FileOps {
public:
    static FileOpResult readText(const std::string& path, std::string& out);
    static FileOpResult writeText(const std::string& path, const std::string& content, bool create_dirs);
    static FileOpResult appendText(const std::string& path, const std::string& content);
    static FileOpResult remove(const std::string& path);
    static FileOpResult rename(const std::string& from, const std::string& to, bool create_dirs);
    static FileOpResult copy(const std::string& from, const std::string& to, const CopyOptions& opts);
    static FileOpResult move(const std::string& from, const std::string& to, bool overwrite);
    static FileOpResult ensureDir(const std::string& path);
    static FileOpResult list(const std::string& path, std::vector<std::string>& out, bool recursive);
    static bool exists(const std::string& path);
};

} // namespace Tools
} // namespace RawrXD
