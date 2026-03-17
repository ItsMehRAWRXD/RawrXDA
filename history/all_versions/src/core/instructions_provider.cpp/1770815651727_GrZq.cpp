// ============================================================================
// instructions_provider.cpp — Production Instructions File Reader
// Phase 34: Unified Instructions Context Provider
//
// Implements the InstructionsProvider singleton. Thread-safe.
// Reads all *.instructions.md files from configured search paths.
// Provides content to HTTP/CLI/Win32 GUI surfaces.
// ============================================================================

#include "instructions_provider.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// Constructor
// ============================================================================
InstructionsProvider::InstructionsProvider() {
    setDefaultSearchPaths();
}

// ============================================================================
// Search path management
// ============================================================================
void InstructionsProvider::addSearchPath(const std::string& dirPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Avoid duplicates
    for (const auto& existing : m_searchPaths) {
        if (existing == dirPath) return;
    }
    m_searchPaths.push_back(dirPath);
}

void InstructionsProvider::setDefaultSearchPaths() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_searchPaths.clear();

    // 1. Executable directory
    std::string exeDir = getExeDirectory();
    if (!exeDir.empty()) {
        m_searchPaths.push_back(exeDir);
        // Also check config/ subdirectory
        std::string configDir = exeDir + "/config";
        m_searchPaths.push_back(configDir);
    }

    // 2. User home .aitk/instructions/
    std::string home = getUserHomeDirectory();
    if (!home.empty()) {
        m_searchPaths.push_back(home + "/.aitk/instructions");
        m_searchPaths.push_back(home + "/.aitk");
    }

    // 3. Common workspace-relative paths
    m_searchPaths.push_back(".github");
    m_searchPaths.push_back(".");
    m_searchPaths.push_back("config");
    m_searchPaths.push_back("instructions");

    // 4. Known absolute paths on this system
#ifdef _WIN32
    // Check typical locations
    m_searchPaths.push_back("C:\\Users\\HiH8e\\.aitk\\instructions");
    m_searchPaths.push_back("D:\\.github");
    m_searchPaths.push_back("D:\\rawrxd");
    m_searchPaths.push_back("D:\\rawrxd\\config");
#endif
}

// ============================================================================
// Loading
// ============================================================================
InstructionsResult InstructionsProvider::loadAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_files.clear();

    int filesFound = 0;
    int filesLoaded = 0;

    for (const auto& searchPath : m_searchPaths) {
        auto candidates = scanDirectory(searchPath);
        for (const auto& filePath : candidates) {
            // Avoid duplicate file paths
            bool duplicate = false;
            for (const auto& existing : m_files) {
                if (existing.filePath == filePath) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;

            filesFound++;
            InstructionsFile file;
            auto result = readFileInternal(filePath, file);
            if (result.success) {
                m_files.push_back(std::move(file));
                filesLoaded++;
            }
        }
    }

    auto now = std::chrono::system_clock::now();
    auto epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    m_lastLoadTimeMs.store(static_cast<uint64_t>(epochMs));
    m_loaded.store(filesLoaded > 0);

    if (filesLoaded == 0) {
        return InstructionsResult::error("No instruction files found in any search path", -2);
    }

    return InstructionsResult::ok("Instructions loaded");
}

InstructionsResult InstructionsProvider::loadFile(const std::string& filePath) {
    InstructionsFile file;
    auto result = readFileInternal(filePath, file);
    if (!result.success) {
        return result;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    // Replace if already loaded, otherwise append
    bool replaced = false;
    for (auto& existing : m_files) {
        if (existing.filePath == filePath) {
            existing = std::move(file);
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_files.push_back(std::move(file));
    }

    m_loaded.store(true);
    auto now = std::chrono::system_clock::now();
    m_lastLoadTimeMs.store(static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count()));

    return InstructionsResult::ok("File loaded");
}

InstructionsResult InstructionsProvider::reload() {
    return loadAll();
}

// ============================================================================
// Accessors
// ============================================================================
std::vector<InstructionsFile> InstructionsProvider::getAll() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_files;
}

InstructionsFile InstructionsProvider::getByName(const std::string& fileName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& f : m_files) {
        if (f.fileName == fileName) {
            return f;
        }
    }
    return InstructionsFile{"", fileName, "", 0, 0, 0, false};
}

std::string InstructionsProvider::getAllContent() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    for (size_t i = 0; i < m_files.size(); i++) {
        if (i > 0) oss << "\n\n---\n\n";
        oss << "# " << m_files[i].fileName << "\n\n";
        oss << m_files[i].content;
    }
    return oss.str();
}

std::string InstructionsProvider::getContent(const std::string& fileName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& f : m_files) {
        if (f.fileName == fileName) {
            return f.content;
        }
    }
    return "";
}

std::string InstructionsProvider::getPrimaryContent() const {
    return getContent("tools.instructions.md");
}

size_t InstructionsProvider::getLoadedCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_files.size();
}

std::vector<std::string> InstructionsProvider::getSearchPaths() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_searchPaths;
}

// ============================================================================
// JSON export
// ============================================================================
static std::string escapeJsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 32);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::string InstructionsProvider::toJSON() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"loaded\": " << (m_loaded.load() ? "true" : "false") << ",\n";
    oss << "  \"file_count\": " << m_files.size() << ",\n";
    oss << "  \"last_load_epoch_ms\": " << m_lastLoadTimeMs.load() << ",\n";
    oss << "  \"search_paths\": [";
    for (size_t i = 0; i < m_searchPaths.size(); i++) {
        if (i > 0) oss << ", ";
        oss << "\"" << escapeJsonString(m_searchPaths[i]) << "\"";
    }
    oss << "],\n";
    oss << "  \"files\": [\n";
    for (size_t i = 0; i < m_files.size(); i++) {
        const auto& f = m_files[i];
        oss << "    {\n";
        oss << "      \"file_name\": \"" << escapeJsonString(f.fileName) << "\",\n";
        oss << "      \"file_path\": \"" << escapeJsonString(f.filePath) << "\",\n";
        oss << "      \"size_bytes\": " << f.sizeBytes << ",\n";
        oss << "      \"line_count\": " << f.lineCount << ",\n";
        oss << "      \"loaded_at_epoch_ms\": " << f.loadedAtEpochMs << ",\n";
        oss << "      \"valid\": " << (f.valid ? "true" : "false") << ",\n";
        oss << "      \"content\": \"" << escapeJsonString(f.content) << "\"\n";
        oss << "    }";
        if (i + 1 < m_files.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}";
    return oss.str();
}

std::string InstructionsProvider::toJSONSummary() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"loaded\": " << (m_loaded.load() ? "true" : "false") << ",\n";
    oss << "  \"file_count\": " << m_files.size() << ",\n";
    oss << "  \"files\": [";
    for (size_t i = 0; i < m_files.size(); i++) {
        if (i > 0) oss << ", ";
        oss << "{\n";
        oss << "    \"name\": \"" << escapeJsonString(m_files[i].fileName) << "\",\n";
        oss << "    \"path\": \"" << escapeJsonString(m_files[i].filePath) << "\",\n";
        oss << "    \"lines\": " << m_files[i].lineCount << ",\n";
        oss << "    \"size\": " << m_files[i].sizeBytes << "\n";
        oss << "  }";
    }
    oss << "]\n";
    oss << "}";
    return oss.str();
}

// ============================================================================
// Markdown export
// ============================================================================
std::string InstructionsProvider::toMarkdown() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "# Production Instructions Context\n\n";
    oss << "**Files loaded:** " << m_files.size() << "\n\n";

    for (const auto& f : m_files) {
        oss << "## " << f.fileName << "\n";
        oss << "- **Path:** `" << f.filePath << "`\n";
        oss << "- **Size:** " << f.sizeBytes << " bytes\n";
        oss << "- **Lines:** " << f.lineCount << "\n\n";
        oss << "```markdown\n";
        oss << f.content;
        if (!f.content.empty() && f.content.back() != '\n') oss << "\n";
        oss << "```\n\n";
    }
    return oss.str();
}

// ============================================================================
// Internal: read a file from disk
// ============================================================================
InstructionsResult InstructionsProvider::readFileInternal(
    const std::string& path, InstructionsFile& out)
{
    out.filePath = path;
    out.valid = false;

    // Extract filename from path
    auto pos = path.find_last_of("/\\");
    out.fileName = (pos != std::string::npos) ? path.substr(pos + 1) : path;

    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
        return InstructionsResult::error("Cannot open file", -1);
    }

    // Read entire file
    std::ostringstream ss;
    ss << ifs.rdbuf();
    ifs.close();

    out.content = ss.str();
    out.sizeBytes = out.content.size();

    // Count lines
    uint64_t lines = 0;
    for (char c : out.content) {
        if (c == '\n') lines++;
    }
    if (!out.content.empty() && out.content.back() != '\n') lines++;
    out.lineCount = lines;

    // Timestamp
    auto now = std::chrono::system_clock::now();
    out.loadedAtEpochMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());

    out.valid = true;
    return InstructionsResult::ok("File read successfully");
}

// ============================================================================
// Internal: scan a directory for *.instructions.md
// ============================================================================
std::vector<std::string> InstructionsProvider::scanDirectory(
    const std::string& dirPath) const
{
    std::vector<std::string> results;

    std::error_code ec;
    if (!fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) {
        return results;
    }

    for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        if (ec) continue;

        std::string name = entry.path().filename().string();
        // Match *.instructions.md
        if (name.size() > 16 &&
            name.substr(name.size() - 16) == ".instructions.md") {
            results.push_back(entry.path().string());
        }
    }

    return results;
}

// ============================================================================
// Platform helpers
// ============================================================================
std::string InstructionsProvider::getExeDirectory() const {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::string path(buf, len);
        auto pos = path.find_last_of("\\/");
        if (pos != std::string::npos) {
            return path.substr(0, pos);
        }
    }
#else
    char buf[4096] = {};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::string path(buf);
        auto pos = path.find_last_of('/');
        if (pos != std::string::npos) {
            return path.substr(0, pos);
        }
    }
#endif
    return "";
}

std::string InstructionsProvider::getUserHomeDirectory() const {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PROFILE, nullptr, 0, buf))) {
        return std::string(buf);
    }
    // Fallback to USERPROFILE env
    const char* home = getenv("USERPROFILE");
    if (home) return std::string(home);
#else
    const char* home = getenv("HOME");
    if (home) return std::string(home);
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) return std::string(pw->pw_dir);
#endif
    return "";
}
