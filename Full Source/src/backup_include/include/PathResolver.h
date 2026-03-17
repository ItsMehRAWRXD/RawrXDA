/**
 * PathResolver - Cross-platform path resolution utility (C++20, Win32, no Qt)
 *
 * Centralizes path operations using Windows Known Folders and environment
 * variables. Replaces QStandardPaths / QDir / QFileInfo.
 *
 * Usage:
 *   std::string desktopPath = PathResolver::getDesktopPath();
 *   std::string appDataPath = PathResolver::getAppDataPath();
 *   std::string configPath = PathResolver::getConfigPath();
 */

#pragma once

#include <string>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#pragma comment(lib, "shell32.lib")
#endif

class PathResolver
{
public:
    /**
     * Get user's desktop directory
     * Windows: C:\Users\<username>\Desktop
     */
    static std::string getDesktopPath()
    {
#ifdef _WIN32
        wchar_t* path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &path))) {
            std::wstring w(path);
            CoTaskMemFree(path);
            return wideToUtf8(w);
        }
#endif
        return getHomePath() + "/Desktop";
    }

    /**
     * Get application data directory
     * Windows: C:\Users\<username>\AppData\Local\RawrXD
     */
    static std::string getAppDataPath()
    {
#ifdef _WIN32
        wchar_t* path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
            std::wstring w(path);
            CoTaskMemFree(path);
            return wideToUtf8(w) + "\\RawrXD";
        }
#endif
        return getHomePath() + "/.local/share/RawrXD";
    }

    /**
     * Get application config directory
     * Windows: C:\Users\<username>\AppData\Roaming\RawrXD
     */
    static std::string getConfigPath()
    {
#ifdef _WIN32
        wchar_t* path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
            std::wstring w(path);
            CoTaskMemFree(path);
            return wideToUtf8(w) + "\\RawrXD";
        }
#endif
        return getHomePath() + "/.config/RawrXD";
    }

    /**
     * Get application documents directory
     * Windows: C:\Users\<username>\Documents
     */
    static std::string getDocumentsPath()
    {
#ifdef _WIN32
        wchar_t* path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path))) {
            std::wstring w(path);
            CoTaskMemFree(path);
            return wideToUtf8(w);
        }
#endif
        return getHomePath() + "/Documents";
    }

    /**
     * Get application temporary directory
     */
    static std::string getTempPath()
    {
        return std::filesystem::temp_directory_path().string();
    }

    /**
     * Get project/workspace root path
     * Priority:
     * 1. Environment variable: RAWRXD_WORKSPACE_ROOT
     * 2. Documents folder
     */
    static std::string getWorkspaceRootPath()
    {
        const char* envPath = std::getenv("RAWRXD_WORKSPACE_ROOT");
        if (envPath && envPath[0] != '\0') {
            return envPath;
        }
        return getDocumentsPath();
    }

    /**
     * Get models directory for caching downloaded/imported models
     * Location: <AppData>/models
     */
    static std::string getModelsPath()
    {
        std::string path = getAppDataPath() + "\\models";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get scripts directory for PowerShell/Bash scripts
     * Location: <AppData>/scripts
     */
    static std::string getScriptsPath()
    {
        std::string path = getAppDataPath() + "\\scripts";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get tools directory for utility executables
     * Location: <AppData>/tools
     */
    static std::string getToolsPath()
    {
        std::string path = getAppDataPath() + "\\tools";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get plugins/extensions directory
     * Location: <AppData>/plugins
     */
    static std::string getPluginsPath()
    {
        std::string path = getAppDataPath() + "\\plugins";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get Powershield tools directory
     * Location: <Tools>/powershield
     */
    static std::string getPowershieldPath()
    {
        return getToolsPath() + "\\powershield";
    }

    /**
     * Get logs directory
     * Location: <AppData>/logs
     */
    static std::string getLogsPath()
    {
        std::string path = getAppDataPath() + "\\logs";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get cache directory
     * Location: <AppData>/cache
     */
    static std::string getCachePath()
    {
        std::string path = getAppDataPath() + "\\cache";
        ensurePathExists(path);
        return path;
    }

    /**
     * Resolve a relative path based on workspace root
     */
    static std::string resolveWorkspacePath(const std::string& relativePath)
    {
        std::filesystem::path base(getWorkspaceRootPath());
        return (base / relativePath).lexically_normal().string();
    }

    /**
     * Ensure a directory exists, creating it if necessary
     */
    static bool ensurePathExists(const std::string& path)
    {
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            return std::filesystem::is_directory(path, ec);
        }
        return std::filesystem::create_directories(path, ec);
    }

    /**
     * Verify a path is accessible and not dangerous (e.g., no path traversal)
     */
    static bool isPathSafe(const std::string& path, const std::string& basePath = "")
    {
        std::error_code ec;
        auto absPath = std::filesystem::absolute(path, ec);
        if (ec) return false;
        if (!basePath.empty()) {
            auto baseAbs = std::filesystem::absolute(basePath, ec);
            if (ec) return false;
            auto [a, b] = std::mismatch(baseAbs.begin(), baseAbs.end(), absPath.begin(), absPath.end());
            return a == baseAbs.end();
        }
        return true;
    }

    /**
     * Normalize path to native separators
     */
    static std::string normalizePath(const std::string& path)
    {
        return std::filesystem::path(path).lexically_normal().string();
    }

    /**
     * Get home directory path
     */
    static std::string getHomePath()
    {
#ifdef _WIN32
        wchar_t* path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &path))) {
            std::wstring w(path);
            CoTaskMemFree(path);
            return wideToUtf8(w);
        }
#endif
        const char* home = std::getenv("USERPROFILE");
        if (!home) home = std::getenv("HOME");
        return home ? home : ".";
    }

private:
    PathResolver() = delete;

    static std::string wideToUtf8(const std::wstring& w)
    {
        if (w.empty()) return {};
#ifdef _WIN32
        int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string s(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), size, nullptr, nullptr);
        return s;
#else
        (void)w;
        return {};
#endif
    }
};
