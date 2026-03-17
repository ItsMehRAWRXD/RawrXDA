/**
 * @file project_manager.hpp
 * @brief Project management with gitignore filtering and recent projects tracking
 * @author RawrXD Team
 * @version 1.0.0
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QVector>
#include <QMap>
#include <QSet>
#include <QSettings>
#include <QFileSystemWatcher>

/**
 * @struct ProjectInfo
 * @brief Metadata for a project
 */
struct ProjectInfo {
    QString path;
    QString name;
    QDateTime lastOpened;
    QString projectType;      // "MASM", "CMake", "Generic", etc.
    QString description;
    QStringList recentFiles;
    bool pinned = false;
    
    QJsonObject toJSON() const;
    static ProjectInfo fromJSON(const QJsonObject& obj);
};

/**
 * @class GitignoreFilter
 * @brief Parses and applies .gitignore rules for tree filtering
 *
 * Features:
 * - Full .gitignore spec support (wildcards, negation, directory patterns)
 * - Multiple .gitignore file support (project root, subdirectories)
 * - Default ignore patterns (build/, .git/, node_modules/, etc.)
 * - Pattern caching for performance
 * - Real-time file watching for .gitignore changes
 */
class GitignoreFilter : public QObject {
    Q_OBJECT

public:
    explicit GitignoreFilter(QObject* parent = nullptr);
    ~GitignoreFilter();

    // Configuration
    void setRootDirectory(const QString& dir);
    void loadGitignore(const QString& gitignorePath);
    void loadDefaultPatterns();
    void clearPatterns();
    
    // Pattern management
    void addPattern(const QString& pattern);
    void removePattern(const QString& pattern);
    QStringList getPatterns() const { return m_patterns; }
    
    // Filtering
    bool shouldIgnore(const QString& path, bool isDirectory = false) const;
    QStringList filterPaths(const QStringList& paths) const;
    
    // Default exclusions
    static QStringList getDefaultIgnorePatterns();
    void enableDefaultPatterns(bool enable) { m_useDefaultPatterns = enable; }
    
    // File watching
    void enableFileWatching(bool enable);

signals:
    void patternsChanged();
    void gitignoreModified(const QString& path);

private slots:
    void onGitignoreFileChanged(const QString& path);

private:
    struct Pattern {
        QString original;
        QString regex;
        bool negation = false;
        bool directoryOnly = false;
    };
    
    Pattern parsePattern(const QString& pattern) const;
    bool matchesPattern(const QString& path, const Pattern& pattern, bool isDirectory) const;
    QString convertGlobToRegex(const QString& glob) const;
    
    QString m_rootDirectory;
    QStringList m_patterns;
    QVector<Pattern> m_compiledPatterns;
    bool m_useDefaultPatterns = true;
    
    // File watching
    QFileSystemWatcher* m_watcher = nullptr;
    QSet<QString> m_watchedGitignores;
    
    // Cache for performance
    mutable QMap<QString, bool> m_ignoreCache;
};

/**
 * @class RecentProjectsManager
 * @brief Manages recent project list with persistence
 *
 * Features:
 * - Recent projects list with configurable limit
 * - Project pinning
 * - Auto-detection of project types (MASM, CMake, etc.)
 * - Recent files per project
 * - QSettings persistence
 * - Project favorites
 */
class RecentProjectsManager : public QObject {
    Q_OBJECT

public:
    explicit RecentProjectsManager(QObject* parent = nullptr);
    ~RecentProjectsManager();

    // Project operations
    void addRecentProject(const QString& projectPath);
    void removeProject(const QString& projectPath);
    void clearRecentProjects();
    
    // Project queries
    QVector<ProjectInfo> getRecentProjects(int maxCount = 20) const;
    QVector<ProjectInfo> getPinnedProjects() const;
    QVector<ProjectInfo> getAllProjects() const;
    bool hasProject(const QString& projectPath) const;
    ProjectInfo getProjectInfo(const QString& projectPath) const;
    
    // Project management
    void pinProject(const QString& projectPath, bool pinned = true);
    void updateProjectInfo(const QString& projectPath, const ProjectInfo& info);
    void addRecentFile(const QString& projectPath, const QString& filePath);
    
    // Configuration
    void setMaxRecentProjects(int max) { m_maxRecentProjects = max; }
    int getMaxRecentProjects() const { return m_maxRecentProjects; }
    
    // Project type detection
    QString detectProjectType(const QString& projectPath);
    static QString findProjectRoot(const QString& startPath);
    
    // Persistence
    void saveProjects();
    void loadProjects();
    bool exportProjects(const QString& filePath);
    bool importProjects(const QString& filePath);

signals:
    void projectAdded(const QString& projectPath);
    void projectRemoved(const QString& projectPath);
    void projectsChanged();
    void projectPinned(const QString& projectPath, bool pinned);

private:
    ProjectInfo createProjectInfo(const QString& projectPath);
    void pruneOldProjects();
    
    QMap<QString, ProjectInfo> m_projects;  // Path -> ProjectInfo
    int m_maxRecentProjects = 20;
};

/**
 * @class ProjectTreeFilter
 * @brief Combines gitignore filtering with custom filters for tree view
 *
 * Features:
 * - Gitignore-based filtering
 * - Custom include/exclude patterns
 * - File type filtering (show only .asm, .cpp, etc.)
 * - Size-based filtering
 * - Date-based filtering
 * - Search filtering
 */
class ProjectTreeFilter : public QObject {
    Q_OBJECT

public:
    enum FilterMode {
        ShowAll,
        GitignoreOnly,
        CustomOnly,
        Combined
    };
    Q_ENUM(FilterMode)

    explicit ProjectTreeFilter(QObject* parent = nullptr);
    ~ProjectTreeFilter();

    // Configuration
    void setRootDirectory(const QString& dir);
    void setFilterMode(FilterMode mode) { m_filterMode = mode; }
    FilterMode getFilterMode() const { return m_filterMode; }
    
    // Gitignore integration
    void setGitignoreFilter(GitignoreFilter* filter);
    GitignoreFilter* getGitignoreFilter() const { return m_gitignoreFilter; }
    
    // Custom patterns
    void addIncludePattern(const QString& pattern);
    void addExcludePattern(const QString& pattern);
    void clearCustomPatterns();
    
    // File type filtering
    void setAllowedExtensions(const QStringList& extensions);
    QStringList getAllowedExtensions() const { return m_allowedExtensions; }
    
    // Filtering
    bool shouldShow(const QString& path, bool isDirectory = false) const;
    QStringList filterTree(const QStringList& paths) const;
    
    // Search
    void setSearchQuery(const QString& query);
    QString getSearchQuery() const { return m_searchQuery; }
    
    // Statistics
    struct FilterStats {
        int totalItems = 0;
        int visibleItems = 0;
        int hiddenByGitignore = 0;
        int hiddenByCustom = 0;
        int hiddenBySearch = 0;
    };
    
    FilterStats getFilterStats() const { return m_stats; }

signals:
    void filterChanged();

private:
    bool matchesCustomPatterns(const QString& path) const;
    bool matchesExtension(const QString& path) const;
    bool matchesSearch(const QString& path) const;
    
    QString m_rootDirectory;
    FilterMode m_filterMode = Combined;
    GitignoreFilter* m_gitignoreFilter = nullptr;
    
    QStringList m_includePatterns;
    QStringList m_excludePatterns;
    QStringList m_allowedExtensions;
    QString m_searchQuery;
    
    mutable FilterStats m_stats;
};

/**
 * @class ProjectManager
 * @brief High-level project management system
 *
 * Integrates:
 * - Recent projects tracking
 * - Gitignore filtering
 * - Project tree filtering
 * - Project root auto-detection
 * - Workspace management
 */
class ProjectManager : public QObject {
    Q_OBJECT

public:
    explicit ProjectManager(QObject* parent = nullptr);
    ~ProjectManager();

    // Project operations
    bool openProject(const QString& projectPath);
    bool closeProject();
    QString getCurrentProject() const { return m_currentProjectPath; }
    ProjectInfo getCurrentProjectInfo() const;
    
    // Recent projects
    RecentProjectsManager* getRecentProjectsManager() { return m_recentProjects; }
    
    // Filtering
    GitignoreFilter* getGitignoreFilter() { return m_gitignoreFilter; }
    ProjectTreeFilter* getTreeFilter() { return m_treeFilter; }
    
    // Project detection
    QString detectProjectRoot(const QString& filePath);
    QString detectProjectType(const QString& projectPath);
    
    // Project creation
    bool createNewProject(const QString& path, const QString& type);
    
    // Workspace
    void saveWorkspace();
    void loadWorkspace();

signals:
    void projectOpened(const QString& projectPath);
    void projectClosed(const QString& projectPath);
    void currentProjectChanged(const QString& projectPath);

private:
    QString m_currentProjectPath;
    RecentProjectsManager* m_recentProjects;
    GitignoreFilter* m_gitignoreFilter;
    ProjectTreeFilter* m_treeFilter;
};

