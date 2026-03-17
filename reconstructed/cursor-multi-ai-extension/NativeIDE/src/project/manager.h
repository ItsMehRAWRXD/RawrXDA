#pragma once

#include "native_ide.h"
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <memory>
#include <map>

namespace NativeIDE {

// Forward declarations
class FileWatcher;

/**
 * Represents a file or directory in the project tree
 */
struct ProjectItem {
    std::wstring name;
    std::wstring fullPath;
    bool isDirectory;
    bool isExpanded;
    std::vector<std::unique_ptr<ProjectItem>> children;
    ProjectItem* parent;

    ProjectItem(const std::wstring& name = L"", const std::wstring& path = L"", 
                bool isDir = false, ProjectItem* parentPtr = nullptr)
        : name(name), fullPath(path), isDirectory(isDir), isExpanded(false), parent(parentPtr) {}
};

/**
 * Project template information
 */
struct ProjectTemplate {
    std::wstring name;
    std::wstring description;
    std::wstring templatePath;
    std::wstring defaultName;
    std::vector<std::wstring> requiredFiles;
    
    ProjectTemplate(const std::wstring& n, const std::wstring& desc, 
                   const std::wstring& path, const std::wstring& defName)
        : name(n), description(desc), templatePath(path), defaultName(defName) {}
};

/**
 * Project configuration settings
 */
struct ProjectConfig {
    std::wstring projectPath;
    std::wstring projectName;
    std::wstring buildSystem;     // "cmake", "make", "custom"
    std::wstring compiler;        // "gcc", "clang", "msvc"
    std::wstring buildType;       // "debug", "release"
    std::vector<std::wstring> includePaths;
    std::vector<std::wstring> libraryPaths;
    std::vector<std::wstring> libraries;
    std::map<std::wstring, std::wstring> customSettings;
    
    // Load/save configuration
    bool LoadFromFile(const std::wstring& configPath);
    bool SaveToFile(const std::wstring& configPath) const;
};

/**
 * Manages project operations, file tree, and project settings
 */
class ProjectManager {
public:
    // Project lifecycle
    bool CreateNewProject(const std::wstring& templateName, const std::wstring& projectPath, 
                         const std::wstring& projectName);
    bool OpenProject(const std::wstring& projectPath);
    bool SaveProject();
    bool CloseProject();
    
    // Project information
    bool HasOpenProject() const { return !currentProjectPath_.empty(); }
    const std::wstring& GetProjectPath() const { return currentProjectPath_; }
    const std::wstring& GetProjectName() const { return currentProjectName_; }
    const ProjectConfig& GetProjectConfig() const { return projectConfig_; }
    
    // File tree operations
    ProjectItem* GetRootItem() const { return rootItem_.get(); }
    ProjectItem* FindItemByPath(const std::wstring& path) const;
    bool RefreshFileTree();
    bool AddFileToProject(const std::wstring& filePath);
    bool RemoveFileFromProject(const std::wstring& filePath);
    bool RenameItem(ProjectItem* item, const std::wstring& newName);
    
    // Template management
    std::vector<ProjectTemplate> GetAvailableTemplates() const;
    bool InstallTemplate(const std::wstring& templatePath);
    bool CreateTemplate(const std::wstring& sourcePath, const std::wstring& templateName);
    
    // Build integration
    bool BuildProject();
    bool CleanProject();
    bool RunProject();
    bool DebugProject();
    
    // File operations
    bool CreateFile(const std::wstring& filePath);
    bool CreateDirectory(const std::wstring& dirPath);
    bool DeleteItem(const std::wstring& itemPath);
    
    // Event callbacks
    using FileEventCallback = std::function<void(const std::wstring&, const std::wstring&)>;
    void SetOnFileChanged(FileEventCallback callback) { onFileChanged_ = callback; }
    void SetOnFileAdded(FileEventCallback callback) { onFileAdded_ = callback; }
    void SetOnFileRemoved(FileEventCallback callback) { onFileRemoved_ = callback; }
    
    // Recent projects
    std::vector<std::wstring> GetRecentProjects() const;
    void AddToRecentProjects(const std::wstring& projectPath);
    
    // Configuration
    bool UpdateProjectConfig(const ProjectConfig& config);
    bool DetectBuildSystem(const std::wstring& projectPath);
    
    // Static utility functions
    static bool IsValidProjectPath(const std::wstring& path);
    static std::wstring GetProjectConfigPath(const std::wstring& projectPath);
    static std::wstring GetTemplatesDirectory();

private:
    // Core state
    std::wstring currentProjectPath_;
    std::wstring currentProjectName_;
    ProjectConfig projectConfig_;
    std::unique_ptr<ProjectItem> rootItem_;
    std::unique_ptr<FileWatcher> fileWatcher_;
    
    // Event callbacks
    FileEventCallback onFileChanged_;
    FileEventCallback onFileAdded_;
    FileEventCallback onFileRemoved_;
    
    // Internal methods
    bool LoadProjectConfig();
    bool SaveProjectConfig();
    void BuildFileTree(ProjectItem* parentItem, const std::filesystem::path& dirPath);
    void NotifyFileEvent(const std::wstring& eventType, const std::wstring& filePath);
    bool CopyTemplate(const std::wstring& templatePath, const std::wstring& targetPath, 
                     const std::wstring& projectName);
    void ProcessTemplateFile(const std::wstring& filePath, const std::wstring& projectName);
    std::vector<std::wstring> LoadRecentProjects();
    void SaveRecentProjects(const std::vector<std::wstring>& projects);
};

/**
 * Simple file system watcher for project changes
 */
class FileWatcher {
public:
    FileWatcher(const std::wstring& watchPath, ProjectManager* projectManager);
    ~FileWatcher();
    
    bool StartWatching();
    void StopWatching();
    bool IsWatching() const { return isWatching_; }

private:
    std::wstring watchPath_;
    ProjectManager* projectManager_;
    HANDLE directoryHandle_;
    HANDLE watchThread_;
    bool isWatching_;
    bool shouldStop_;
    
    static DWORD WINAPI WatchThreadProc(LPVOID lpParameter);
    void ProcessFileChanges(const FILE_NOTIFY_INFORMATION* notification);
};

} // namespace NativeIDE