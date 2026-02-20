#pragma once

/**
 * FileResourceGuard — RAII file/directory guard (C++20, no Qt)
 * Replaces QFile/QDir with std::fstream and std::filesystem.
 */

#include <string>
#include <fstream>
#include <filesystem>
#include <memory>

namespace rawrxd {

inline void fileResourceGuardLog(const char* /* tag */, const char* /* msg */) {
#ifdef _DEBUG
    // Optionally: OutputDebugStringA(tag); OutputDebugStringA(msg);
#endif
}

class FileResourceGuard {
public:
    using path_type = std::filesystem::path;

    explicit FileResourceGuard(const std::string& filePath, std::FILE* file = nullptr, bool isTemporary = false)
        : m_filePath(filePath)
        , m_file(file)
        , m_shouldCleanup(false)
        , m_isTemporary(isTemporary)
    {}

    ~FileResourceGuard() { release(); }

    void markForCleanup() { m_shouldCleanup = true; }

    void release()
    {
        if (m_file) {
            std::fclose(m_file);
            m_file = nullptr;
        }
        if (m_shouldCleanup && m_isTemporary) {
            std::error_code ec;
            if (std::filesystem::remove(m_filePath, ec)) {
                fileResourceGuardLog("[FileResourceGuard]", ("Cleaned up temp: " + m_filePath.string()).c_str());
            }
        }
    }

    std::string filePath() const { return m_filePath.string(); }
    const path_type& path() const { return m_filePath; }
    bool isTemporary() const { return m_isTemporary; }

private:
    path_type m_filePath;
    std::FILE* m_file;
    bool m_shouldCleanup;
    bool m_isTemporary;
};

class DirectoryResourceGuard {
public:
    using path_type = std::filesystem::path;

    explicit DirectoryResourceGuard(const std::string& dirPath)
        : m_dirPath(dirPath)
        , m_shouldCleanup(false)
    {}

    ~DirectoryResourceGuard() { release(); }

    void markForCleanup() { m_shouldCleanup = true; }

    void release()
    {
        if (m_shouldCleanup) {
            std::error_code ec;
            if (std::filesystem::exists(m_dirPath, ec) && std::filesystem::is_directory(m_dirPath, ec)) {
                if (std::filesystem::remove_all(m_dirPath, ec)) {
                    fileResourceGuardLog("[DirectoryResourceGuard]", ("Cleaned up dir: " + m_dirPath.string()).c_str());
                }
            }
        }
    }

    std::string dirPath() const { return m_dirPath.string(); }
    const path_type& path() const { return m_dirPath; }

private:
    path_type m_dirPath;
    bool m_shouldCleanup;
};

} // namespace rawrxd
