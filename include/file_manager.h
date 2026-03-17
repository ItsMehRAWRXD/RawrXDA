/**
 * @file file_manager.h
 * @brief File management utilities (C++20, no Qt). std::string, std::filesystem.
 */
#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

struct MultiFileSearchResult {
    std::string file;
    int line = 0;
    int column = 0;
    std::string lineText;
    std::string matchedText;

    MultiFileSearchResult() = default;
    MultiFileSearchResult(const std::string& file_, int line_, int column_,
                          const std::string& lineText_, const std::string& matchedText_)
        : file(file_), line(line_), column(column_)
        , lineText(lineText_), matchedText(matchedText_) {}

    bool isValid() const { return !file.empty() && line > 0; }
};

class FileManager {
public:
    static std::string readFile(const std::string& filePath) {
        std::ifstream f(filePath, std::ios::in);
        if (!f) return {};
        std::ostringstream os;
        os << f.rdbuf();
        return os.str();
    }

    static std::string toRelativePath(const std::string& absolutePath, const std::string& basePath) {
        std::error_code ec;
        std::filesystem::path abs(absolutePath);
        std::filesystem::path base(basePath);
        auto rel = std::filesystem::relative(abs, base, ec);
        return ec ? absolutePath : rel.lexically_normal().string();
    }

    static std::string getFileName(const std::string& filePath) {
        return std::filesystem::path(filePath).filename().string();
    }

    static std::string getDirectory(const std::string& filePath) {
        return std::filesystem::path(filePath).parent_path().lexically_normal().string();
    }

    static bool fileExists(const std::string& filePath) {
        std::error_code ec;
        auto p = std::filesystem::path(filePath);
        return std::filesystem::exists(p, ec) && std::filesystem::is_regular_file(p, ec);
    }
};
