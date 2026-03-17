/**
 * WorkspaceManager - Central workspace context management
 * 
 * Manages the current workspace/project path and provides a single source of truth
 * for workspace context used by chat interface, file explorer, and other components.
 * 
 * Features:
 * - Singleton pattern for centralized access
 * - Thread-safe workspace path management
 * - Automatic validation and fallback to home directory
 * - Project detection (looks for .git, .rawrxd, etc.)
 * - Integration with SettingsManager for persistence
 */

#pragma once

#include <QString>
#include <QDir>
#include <memory>
#include <mutex>
#include <functional>

class WorkspaceManager
{
public:
    /**
     * Get singleton instance
     */
    static WorkspaceManager& getInstance();

    /**
     * Initialize workspace manager
     * Loads last workspace from settings or uses home directory
     */
    void initialize();

    /**
     * Get current workspace path
     * Returns the root directory of current workspace
     */
    QString getCurrentWorkspacePath() const;

    /**
     * Get the name of current workspace (folder name)
     */
    QString getCurrentWorkspaceName() const;

    /**
     * Set current workspace path
     * Validates path exists and is accessible
     * Returns true if successful
     */
    bool setWorkspacePath(const QString& path);

    /**
     * Get workspace's relative path for display
     * Shortens long paths for UI
     */
    QString getDisplayPath() const;

    /**
     * Check if a valid workspace is loaded
     */
    bool isWorkspaceLoaded() const;

    /**
     * Get absolute path of file relative to workspace
     */
    QString resolveWorkspacePath(const QString& relativePath) const;

    /**
     * Get relative path from absolute path
     */
    QString makeRelativePath(const QString& absolutePath) const;

    /**
     * Auto-detect workspace based on current directory
     * Looks for .git, .rawrxd, package.json, etc.
     */
    bool autoDetectWorkspace();

    /**
     * List subdirectories in current workspace
     */
    QStringList getWorkspaceSubdirectories() const;

    /**
     * List files in current workspace directory
     */
    QStringList getWorkspaceFiles(const QString& pattern = "*") const;

    /**
     * Check if file is within workspace
     */
    bool isFileInWorkspace(const QString& filePath) const;

    /**
     * Get workspace root markers (.git, .rawrxd, etc.)
     */
    QStringList getWorkspaceMarkers() const;

    /**
     * Register callback for workspace change events
     */
    using WorkspaceChangedCallback = std::function<void(const QString& oldPath, const QString& newPath)>;
    void onWorkspaceChanged(const WorkspaceChangedCallback& callback);

    /**
     * Save current workspace to settings for next session
     */
    void savePersistentWorkspace();

    /**
     * Test workspace path validity
     * Returns error message if path is invalid
     */
    QString validateWorkspacePath(const QString& path) const;

    /**
     * Get workspace statistics
     */
    struct WorkspaceStats {
        int fileCount = 0;
        int directoryCount = 0;
        qint64 totalSize = 0;
        QString lastModified;
    };
    WorkspaceStats getWorkspaceStats() const;

    // Delete copy/move semantics
    WorkspaceManager(const WorkspaceManager&) = delete;
    WorkspaceManager& operator=(const WorkspaceManager&) = delete;

private:
    WorkspaceManager();
    ~WorkspaceManager();

    QString m_currentWorkspacePath;
    QDir m_workspaceDir;
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    std::vector<WorkspaceChangedCallback> m_changeCallbacks;

    /**
     * Emit workspace changed signal to all registered callbacks
     */
    void emitWorkspaceChanged(const QString& oldPath, const QString& newPath);

    /**
     * Detect if directory is a project root
     */
    bool isProjectRoot(const QString& dirPath) const;

    /**
     * Recursively scan up to find project root
     */
    QString findProjectRoot(const QString& startPath) const;
};
