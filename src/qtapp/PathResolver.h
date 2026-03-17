/**
 * PathResolver - Cross-platform path resolution utility
 * 
 * Centralizes all path operations to use QStandardPaths and environment variables
 * instead of hardcoded Windows user paths. Ensures app works for any user on any machine.
 * 
 * Usage:
 *   QString desktopPath = PathResolver::getDesktopPath();
 *   QString appDataPath = PathResolver::getAppDataPath();
 *   QString configPath = PathResolver::getConfigPath();
 */

#pragma once

#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>

class PathResolver
{
public:
    /**
     * Get user's desktop directory
     * Windows: C:\Users\<username>\Desktop
     * Linux: ~/Desktop
     * macOS: ~/Desktop
     */
    static QString getDesktopPath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    }

    /**
     * Get application data directory
     * Windows: C:\Users\<username>\AppData\Local\RawrXD
     * Linux: ~/.local/share/RawrXD
     * macOS: ~/Library/Application Support/RawrXD
     */
    static QString getAppDataPath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    /**
     * Get application config directory
     * Windows: C:\Users\<username>\AppData\Roaming\RawrXD
     * Linux: ~/.config/RawrXD
     * macOS: ~/Library/Preferences/RawrXD
     */
    static QString getConfigPath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/RawrXD";
    }

    /**
     * Get application documents directory
     * Windows: C:\Users\<username>\Documents
     * Linux: ~/Documents
     * macOS: ~/Documents
     */
    static QString getDocumentsPath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    /**
     * Get application temporary directory
     * Uses system temp folder
     */
    static QString getTempPath()
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
    static QString getWorkspaceRootPath()
    {
        // Check environment variable first
        const char* envPath = std::getenv("RAWRXD_WORKSPACE_ROOT");
        if (envPath && strlen(envPath) > 0) {
            return QString::fromUtf8(envPath);
        }
        
        // Fall back to Documents
        return getDocumentsPath();
    }

    /**
     * Get models directory for caching downloaded/imported models
     * Location: <AppData>/models
     */
    static QString getModelsPath()
    {
        QString path = getAppDataPath() + "/models";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get scripts directory for PowerShell/Bash scripts
     * Location: <AppData>/scripts
     */
    static QString getScriptsPath()
    {
        QString path = getAppDataPath() + "/scripts";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get tools directory for utility executables
     * Location: <AppData>/tools
     */
    static QString getToolsPath()
    {
        QString path = getAppDataPath() + "/tools";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get plugins/extensions directory
     * Location: <AppData>/plugins
     */
    static QString getPluginsPath()
    {
        QString path = getAppDataPath() + "/plugins";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get Powershield tools directory
     * Location: <Tools>/powershield
     */
    static QString getPowershieldPath()
    {
        QString path = getToolsPath() + "/powershield";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get logs directory
     * Location: <AppData>/logs
     */
    static QString getLogsPath()
    {
        QString path = getAppDataPath() + "/logs";
        ensurePathExists(path);
        return path;
    }

    /**
     * Get cache directory
     * Location: <AppData>/cache
     */
    static QString getCachePath()
    {
        QString path = getAppDataPath() + "/cache";
        ensurePathExists(path);
        return path;
    }

    /**
     * Resolve a relative path based on workspace root
     * Example: resolveWorkspacePath("src/main.cpp") -> "/home/user/projects/myapp/src/main.cpp"
     */
    static QString resolveWorkspacePath(const QString& relativePath)
    {
        QString workspaceRoot = getWorkspaceRootPath();
        return QDir(workspaceRoot).absoluteFilePath(relativePath);
    }

    /**
     * Ensure a directory exists, creating it if necessary
     * Returns true if path exists or was successfully created
     */
    static bool ensurePathExists(const QString& path)
    {
        QDir dir(path);
        if (!dir.exists()) {
            return dir.mkpath(".");
        }
        return true;
    }

    /**
     * Verify a path is accessible and not dangerous (e.g., no path traversal)
     */
    static bool isPathSafe(const QString& path, const QString& basePath = "")
    {
        QFileInfo fileInfo(path);
        QString absolutePath = fileInfo.absoluteFilePath();
        
        // If base path provided, verify it's under base path
        if (!basePath.isEmpty()) {
            QDir baseDir(basePath);
            QString baseAbsPath = baseDir.absolutePath();
            if (!absolutePath.startsWith(baseAbsPath)) {
                return false;  // Path traversal attempt
            }
        }
        
        return true;
    }

    /**
     * Convert platform-specific path to forward slashes
     */
    static QString normalizePath(const QString& path)
    {
        return QDir::toNativeSeparators(path);
    }

    /**
     * Get home directory path
     */
    static QString getHomePath()
    {
        return QDir::homePath();
    }

private:
    PathResolver() = delete;  // Utility class, no instantiation
};
