/**
 * @file project_manager.hpp
 * @brief Project management with gitignore filtering and recent projects tracking
 * @author RawrXD Team
 * @version 1.0.0
 */

#pragma once
/**
 * @struct ProjectInfo
 * @brief Metadata for a project
 */
struct ProjectInfo {
    std::string path;
    std::string name;
    // DateTime lastOpened;
    std::string projectType;      // "MASM", "CMake", "Generic", etc.
    std::string description;
    std::stringList recentFiles;
    bool pinned = false;
    
    nlohmann::json toJSON() const;
    static ProjectInfo fromJSON(const nlohmann::json& obj);
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
class GitignoreFilter  {

public:
    explicit GitignoreFilter( = nullptr);
    ~GitignoreFilter();

    // Configuration
    void setRootDirectory(const std::string& dir);
    void loadGitignore(const std::string& gitignorePath);
    void loadDefaultPatterns();
    void clearPatterns();
    
    // Pattern management
    void addPattern(const std::string& pattern);
    void removePattern(const std::string& pattern);
    std::stringList getPatterns() const { return m_patterns; }
    
    // Filtering
    bool shouldIgnore(const std::string& path, bool isDirectory = false) const;
    std::stringList filterPaths(const std::stringList& paths) const;
    
    // Default exclusions
    static std::stringList getDefaultIgnorePatterns();
    void enableDefaultPatterns(bool enable) { m_useDefaultPatterns = enable; }
    
    // File watching
    void enableFileWatching(bool enable);
\npublic:\n    void patternsChanged();
    void gitignoreModified(const std::string& path);
\nprivate:\n    void onGitignoreFileChanged(const std::string& path);

private:
    struct Pattern {
        std::string original;
        std::string regex;
        bool negation = false;
        bool directoryOnly = false;
    };
    
    Pattern parsePattern(const std::string& pattern) const;
    bool matchesPattern(const std::string& path, const Pattern& pattern, bool isDirectory) const;
    std::string convertGlobToRegex(const std::string& glob) const;
    
    std::string m_rootDirectory;
    std::stringList m_patterns;
    std::vector<Pattern> m_compiledPatterns;
    bool m_useDefaultPatterns = true;
    
    // File watching
    // SystemWatcher* m_watcher = nullptr;
    std::set<std::string> m_watchedGitignores;
    
    // Cache for performance
    mutable std::map<std::string, bool> m_ignoreCache;
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
 * - // Settings initialization removed
    ~RecentProjectsManager();

    // Project operations
    void addRecentProject(const std::string& projectPath);
    void removeProject(const std::string& projectPath);
    void clearRecentProjects();
    
    // Project queries
    std::vector<ProjectInfo> getRecentProjects(int maxCount = 20) const;
    std::vector<ProjectInfo> getPinnedProjects() const;
    std::vector<ProjectInfo> getAllProjects() const;
    bool hasProject(const std::string& projectPath) const;
    ProjectInfo getProjectInfo(const std::string& projectPath) const;
    
    // Project management
    void pinProject(const std::string& projectPath, bool pinned = true);
    void updateProjectInfo(const std::string& projectPath, const ProjectInfo& info);
    void addRecentFile(const std::string& projectPath, const std::string& filePath);
    
    // Configuration
    void setMaxRecentProjects(int max) { m_maxRecentProjects = max; }
    int getMaxRecentProjects() const { return m_maxRecentProjects; }
    
    // Project type detection
    std::string detectProjectType(const std::string& projectPath);
    static std::string findProjectRoot(const std::string& startPath);
    
    // Persistence
    void saveProjects();
    void loadProjects();
    bool exportProjects(const std::string& filePath);
    bool importProjects(const std::string& filePath);
\npublic:\n    void projectAdded(const std::string& projectPath);
    void projectRemoved(const std::string& projectPath);
    void projectsChanged();
    void projectPinned(const std::string& projectPath, bool pinned);

private:
    ProjectInfo createProjectInfo(const std::string& projectPath);
    void pruneOldProjects();
    
    std::map<std::string, ProjectInfo> m_projects;  // Path -> ProjectInfo
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
class ProjectTreeFilter  {

public:
    enum FilterMode {
        ShowAll,
        GitignoreOnly,
        CustomOnly,
        Combined
    };

    explicit ProjectTreeFilter( = nullptr);
    ~ProjectTreeFilter();

    // Configuration
    void setRootDirectory(const std::string& dir);
    void setFilterMode(FilterMode mode) { m_filterMode = mode; }
    FilterMode getFilterMode() const { return m_filterMode; }
    
    // Gitignore integration
    void setGitignoreFilter(GitignoreFilter* filter);
    GitignoreFilter* getGitignoreFilter() const { return m_gitignoreFilter; }
    
    // Custom patterns
    void addIncludePattern(const std::string& pattern);
    void addExcludePattern(const std::string& pattern);
    void clearCustomPatterns();
    
    // File type filtering
    void setAllowedExtensions(const std::stringList& extensions);
    std::stringList getAllowedExtensions() const { return m_allowedExtensions; }
    
    // Filtering
    bool shouldShow(const std::string& path, bool isDirectory = false) const;
    std::stringList filterTree(const std::stringList& paths) const;
    
    // Search
    void setSearchQuery(const std::string& query);
    std::string getSearchQuery() const { return m_searchQuery; }
    
    // Statistics
    struct FilterStats {
        int totalItems = 0;
        int visibleItems = 0;
        int hiddenByGitignore = 0;
        int hiddenByCustom = 0;
        int hiddenBySearch = 0;
    };
    
    FilterStats getFilterStats() const { return m_stats; }
\npublic:\n    void filterChanged();

private:
    bool matchesCustomPatterns(const std::string& path) const;
    bool matchesExtension(const std::string& path) const;
    bool matchesSearch(const std::string& path) const;
    
    std::string m_rootDirectory;
    FilterMode m_filterMode = Combined;
    GitignoreFilter* m_gitignoreFilter = nullptr;
    
    std::stringList m_includePatterns;
    std::stringList m_excludePatterns;
    std::stringList m_allowedExtensions;
    std::string m_searchQuery;
    
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
class ProjectManager  {

public:
    explicit ProjectManager( = nullptr);
    ~ProjectManager();

    // Project operations
    bool openProject(const std::string& projectPath);
    bool closeProject();
    std::string getCurrentProject() const { return m_currentProjectPath; }
    ProjectInfo getCurrentProjectInfo() const;
    
    // Recent projects
    RecentProjectsManager* getRecentProjectsManager() { return m_recentProjects; }
    
    // Filtering
    GitignoreFilter* getGitignoreFilter() { return m_gitignoreFilter; }
    ProjectTreeFilter* getTreeFilter() { return m_treeFilter; }
    
    // Project detection
    std::string detectProjectRoot(const std::string& filePath);
    std::string detectProjectType(const std::string& projectPath);
    
    // Project creation
    bool createNewProject(const std::string& path, const std::string& type);
    
    // Workspace
    void saveWorkspace();
    void loadWorkspace();
\npublic:\n    void projectOpened(const std::string& projectPath);
    void projectClosed(const std::string& projectPath);
    void currentProjectChanged(const std::string& projectPath);

private:
    std::string m_currentProjectPath;
    RecentProjectsManager* m_recentProjects;
    GitignoreFilter* m_gitignoreFilter;
    ProjectTreeFilter* m_treeFilter;
};





