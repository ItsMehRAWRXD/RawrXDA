/**
 * PathResolver - Cross-platform path resolution utility
 * 
 * Centralizes all path operations to use QStandardPaths and environment variables
 * instead of hardcoded Windows user paths. Ensures app works for any user on any machine.
 * 
 * Usage:
 *   std::string desktopPath = PathResolver::getDesktopPath();
 *   std::string appDataPath = PathResolver::getAppDataPath();
 *   std::string configPath = PathResolver::getConfigPath();
 */

#pragma once


class PathResolver
{
public:
    /**
     * Get user's desktop directory
     * Windows: C:\Users\<username>\Desktop
     * Linux: ~/Desktop
     * macOS: ~/Desktop
     */
    static std::string getDesktopPath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    }

    /**
     * Get application data directory
     * Windows: C:\Users\<username>\AppData\Local\RawrXD
     * Linux: ~/.local/share/RawrXD
     * macOS: ~/Library/Application Support/RawrXD
     */
    static std::string getAppDataPath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    /**
     * Get application config directory
     * Windows: C:\Users\<username>\AppData\Roaming\RawrXD
     * Linux: ~/.config/RawrXD
     * macOS: ~/Library/Preferences/RawrXD
     */
    static std::string getConfigPath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/RawrXD";
    }

    /**
     * Get application documents directory
     * Windows: C:\Users\<username>\Documents
     * Linux: ~/Documents
     * macOS: ~/Documents
     */
    static std::string getDocumentsPath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    /**
     * Get application temporary directory
     * Uses system temp folder
     */
    static std::string getTempPath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }

    /**
     * Get project/workspace root path
     * Priority:
     * 1. Environment variable: RAWRXD_WORKSPACE_ROOT
     * 2. Config file setting
     * 3. Documents folder
     */
    static std::string getWorkspaceRootPath()
    {
        // Check environment variable first
        const char* envPath = std::getenv("RAWRXD_WORKSPACE_ROOT");
        if (envPath && strlen(envPath) > 0) {
            return std::string::fromUtf8(envPath);
        }
        
        // Fall back to Documents
        return getDocumentsPath();
    }

    /**
     * Get models directory for caching downloaded/imported models
     * Location: <AppData>/models
     */
    static std::string getModelsPath()
    {
        std::string path = getAppDataPath() + "/models";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get scripts directory for PowerShell/Bash scripts
     * Location: <AppData>/scripts
     */
    static std::string getScriptsPath()
    {
        std::string path = getAppDataPath() + "/scripts";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get tools directory for utility executables
     * Location: <AppData>/tools
     */
    static std::string getToolsPath()
    {
        std::string path = getAppDataPath() + "/tools";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get plugins/extensions directory
     * Location: <AppData>/plugins
     */
    static std::string getPluginsPath()
    {
        std::string path = getAppDataPath() + "/plugins";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get Powershield tools directory
     * Location: <Tools>/powershield
     */
    static std::string getPowershieldPath()
    {
        std::string path = getToolsPath() + "/powershield";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get logs directory
     * Location: <AppData>/logs
     */
    static std::string getLogsPath()
    {
        std::string path = getAppDataPath() + "/logs";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get cache directory
     * Location: <AppData>/cache
     */
    static std::string getCachePath()
    {
        std::string path = getAppDataPath() + "/cache";
        ensurePathExists(path);
        return path;
    }

    /**
     * Resolve a relative path based on workspace root
     * Example: resolveWorkspacePath("src/main.cpp") -> "/home/user/projects/myapp/src/main.cpp"
     */
    static std::string resolveWorkspacePath(const std::string& relativePath)
    {
        std::string workspaceRoot = getWorkspaceRootPath();
        return std::filesystem::path(workspaceRoot).absoluteFilePath(relativePath);
    }

    /**
     * Ensure a directory exists, creating it if necessary
     * Returns true if path exists or was successfully created
     */
    static bool ensurePathExists(const std::string& path)
    {
        std::filesystem::path dir(path);
        if (!dir.exists()) {
            return dir.mkpath(".");
        }
        return true;
    }

    /**
     * Verify a path is accessible and not dangerous (e.g., no path traversal)
     */
    static bool isPathSafe(const std::string& path, const std::string& basePath = "")
    {
        std::filesystem::path fileInfo(path);
        std::string absolutePath = fileInfo.absoluteFilePath();
        
        // If base path provided, verify it's under base path
        if (!basePath.empty()) {
            std::filesystem::path baseDir(basePath);
            std::string baseAbsPath = baseDir.absolutePath();
            if (!absolutePath.startsWith(baseAbsPath)) {
                return false;  // Path traversal attempt
            }
        }
        
        return true;
    }

    /**
     * Convert platform-specific path to forward slashes
     */
    static std::string normalizePath(const std::string& path)
    {
        return std::filesystem::path::toNativeSeparators(path);
    }

    /**
     * Get home directory path
     */
    static std::string getHomePath()
    {
        return std::filesystem::path::homePath();
    }

private:
    PathResolver() = delete;  // Utility class, no instantiation
};


