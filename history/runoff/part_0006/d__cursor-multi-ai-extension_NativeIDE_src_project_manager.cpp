#include "project_manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace NativeIDE {

// ProjectConfig Implementation
bool ProjectConfig::LoadFromFile(const std::wstring& configPath) {
    // Convert wstring to string for file operations
    std::string narrowPath(configPath.begin(), configPath.end());
    std::ifstream file(narrowPath);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        std::wstring wkey(key.begin(), key.end());
        std::wstring wvalue(value.begin(), value.end());
        
        if (key == "project_name") projectName = wvalue;
        else if (key == "build_system") buildSystem = wvalue;
        else if (key == "compiler") compiler = wvalue;
        else if (key == "build_type") buildType = wvalue;
        else if (key == "include_paths") {
            std::wstringstream ss(wvalue);
            std::wstring path;
            while (std::getline(ss, path, L';')) {
                if (!path.empty()) includePaths.push_back(path);
            }
        }
        else if (key == "library_paths") {
            std::wstringstream ss(wvalue);
            std::wstring path;
            while (std::getline(ss, path, L';')) {
                if (!path.empty()) libraryPaths.push_back(path);
            }
        }
        else if (key == "libraries") {
            std::wstringstream ss(wvalue);
            std::wstring lib;
            while (std::getline(ss, lib, L';')) {
                if (!lib.empty()) libraries.push_back(lib);
            }
        }
        else {
            customSettings[wkey] = wvalue;
        }
    }
    
    return true;
}

bool ProjectConfig::SaveToFile(const std::wstring& configPath) const {
    std::string narrowPath(configPath.begin(), configPath.end());
    std::ofstream file(narrowPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# Native IDE Project Configuration\n";
    file << "project_name=" << std::string(projectName.begin(), projectName.end()) << "\n";
    file << "build_system=" << std::string(buildSystem.begin(), buildSystem.end()) << "\n";
    file << "compiler=" << std::string(compiler.begin(), compiler.end()) << "\n";
    file << "build_type=" << std::string(buildType.begin(), buildType.end()) << "\n";
    
    if (!includePaths.empty()) {
        file << "include_paths=";
        for (size_t i = 0; i < includePaths.size(); ++i) {
            if (i > 0) file << ";";
            file << std::string(includePaths[i].begin(), includePaths[i].end());
        }
        file << "\n";
    }
    
    if (!libraryPaths.empty()) {
        file << "library_paths=";
        for (size_t i = 0; i < libraryPaths.size(); ++i) {
            if (i > 0) file << ";";
            file << std::string(libraryPaths[i].begin(), libraryPaths[i].end());
        }
        file << "\n";
    }
    
    if (!libraries.empty()) {
        file << "libraries=";
        for (size_t i = 0; i < libraries.size(); ++i) {
            if (i > 0) file << ";";
            file << std::string(libraries[i].begin(), libraries[i].end());
        }
        file << "\n";
    }
    
    for (const auto& [key, value] : customSettings) {
        file << std::string(key.begin(), key.end()) << "=" 
             << std::string(value.begin(), value.end()) << "\n";
    }
    
    return true;
}

// ProjectManager Implementation
bool ProjectManager::CreateNewProject(const std::wstring& templateName, 
                                     const std::wstring& projectPath, 
                                     const std::wstring& projectName) {
    if (!IsValidProjectPath(projectPath)) {
        return false;
    }
    
    // Create project directory
    std::filesystem::path projPath(projectPath);
    try {
        std::filesystem::create_directories(projPath);
    }
    catch (const std::exception&) {
        return false;
    }
    
    // Find template
    auto templates = GetAvailableTemplates();
    auto templateIt = std::find_if(templates.begin(), templates.end(),
        [&templateName](const ProjectTemplate& t) { return t.name == templateName; });
    
    if (templateIt == templates.end()) {
        return false;
    }
    
    // Copy template
    if (!CopyTemplate(templateIt->templatePath, projectPath, projectName)) {
        return false;
    }
    
    // Initialize project config
    currentProjectPath_ = projectPath;
    currentProjectName_ = projectName;
    projectConfig_.projectPath = projectPath;
    projectConfig_.projectName = projectName;
    projectConfig_.buildSystem = L"cmake";
    projectConfig_.compiler = L"gcc";
    projectConfig_.buildType = L"debug";
    
    // Detect build system
    DetectBuildSystem(projectPath);
    
    // Save configuration
    if (!SaveProjectConfig()) {
        return false;
    }
    
    // Build file tree
    RefreshFileTree();
    
    // Start file watching
    fileWatcher_ = std::make_unique<FileWatcher>(projectPath, this);
    fileWatcher_->StartWatching();
    
    // Add to recent projects
    AddToRecentProjects(projectPath);
    
    return true;
}

bool ProjectManager::OpenProject(const std::wstring& projectPath) {
    if (!IsValidProjectPath(projectPath)) {
        return false;
    }
    
    // Close current project
    if (HasOpenProject()) {
        CloseProject();
    }
    
    currentProjectPath_ = projectPath;
    
    // Extract project name from path
    std::filesystem::path path(projectPath);
    currentProjectName_ = path.filename().wstring();
    
    // Load project configuration
    if (!LoadProjectConfig()) {
        // Create default configuration if none exists
        projectConfig_.projectPath = projectPath;
        projectConfig_.projectName = currentProjectName_;
        projectConfig_.buildSystem = L"cmake";
        projectConfig_.compiler = L"gcc";
        projectConfig_.buildType = L"debug";
        
        DetectBuildSystem(projectPath);
        SaveProjectConfig();
    }
    
    // Build file tree
    if (!RefreshFileTree()) {
        return false;
    }
    
    // Start file watching
    fileWatcher_ = std::make_unique<FileWatcher>(projectPath, this);
    fileWatcher_->StartWatching();
    
    // Add to recent projects
    AddToRecentProjects(projectPath);
    
    return true;
}

bool ProjectManager::SaveProject() {
    if (!HasOpenProject()) {
        return false;
    }
    
    return SaveProjectConfig();
}

bool ProjectManager::CloseProject() {
    if (!HasOpenProject()) {
        return true;
    }
    
    // Stop file watching
    if (fileWatcher_) {
        fileWatcher_->StopWatching();
        fileWatcher_.reset();
    }
    
    // Save configuration
    SaveProjectConfig();
    
    // Clear state
    currentProjectPath_.clear();
    currentProjectName_.clear();
    rootItem_.reset();
    
    return true;
}

ProjectItem* ProjectManager::FindItemByPath(const std::wstring& path) const {
    if (!rootItem_) return nullptr;
    
    std::function<ProjectItem*(ProjectItem*, const std::wstring&)> search = 
        [&search](ProjectItem* item, const std::wstring& targetPath) -> ProjectItem* {
            if (item->fullPath == targetPath) {
                return item;
            }
            
            for (auto& child : item->children) {
                if (ProjectItem* found = search(child.get(), targetPath)) {
                    return found;
                }
            }
            
            return nullptr;
        };
    
    return search(rootItem_.get(), path);
}

bool ProjectManager::RefreshFileTree() {
    if (!HasOpenProject()) {
        return false;
    }
    
    std::filesystem::path projectPath(currentProjectPath_);
    if (!std::filesystem::exists(projectPath)) {
        return false;
    }
    
    rootItem_ = std::make_unique<ProjectItem>(currentProjectName_, currentProjectPath_, true);
    BuildFileTree(rootItem_.get(), projectPath);
    
    return true;
}

bool ProjectManager::AddFileToProject(const std::wstring& filePath) {
    if (!HasOpenProject()) {
        return false;
    }
    
    std::filesystem::path path(filePath);
    if (std::filesystem::exists(path)) {
        RefreshFileTree();
        NotifyFileEvent(L"added", filePath);
        return true;
    }
    
    return false;
}

bool ProjectManager::RemoveFileFromProject(const std::wstring& filePath) {
    if (!HasOpenProject()) {
        return false;
    }
    
    try {
        std::filesystem::remove(filePath);
        RefreshFileTree();
        NotifyFileEvent(L"removed", filePath);
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool ProjectManager::RenameItem(ProjectItem* item, const std::wstring& newName) {
    if (!item || !HasOpenProject()) {
        return false;
    }
    
    std::filesystem::path oldPath(item->fullPath);
    std::filesystem::path newPath = oldPath.parent_path() / newName;
    
    try {
        std::filesystem::rename(oldPath, newPath);
        item->name = newName;
        item->fullPath = newPath.wstring();
        RefreshFileTree();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

std::vector<ProjectTemplate> ProjectManager::GetAvailableTemplates() const {
    std::vector<ProjectTemplate> templates;
    
    std::wstring templatesDir = GetTemplatesDirectory();
    if (!std::filesystem::exists(templatesDir)) {
        return templates;
    }
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(templatesDir)) {
            if (entry.is_directory()) {
                std::wstring templateName = entry.path().filename().wstring();
                std::wstring templatePath = entry.path().wstring();
                std::wstring description = L"Template: " + templateName;
                std::wstring defaultName = L"New" + templateName + L"Project";
                
                templates.emplace_back(templateName, description, templatePath, defaultName);
            }
        }
    }
    catch (const std::exception&) {
        // Ignore filesystem errors
    }
    
    return templates;
}

bool ProjectManager::CreateFile(const std::wstring& filePath) {
    std::string narrowPath(filePath.begin(), filePath.end());
    std::ofstream file(narrowPath);
    if (file.is_open()) {
        file.close();
        if (HasOpenProject()) {
            AddFileToProject(filePath);
        }
        return true;
    }
    return false;
}

bool ProjectManager::CreateDirectory(const std::wstring& dirPath) {
    try {
        std::filesystem::create_directories(dirPath);
        if (HasOpenProject()) {
            RefreshFileTree();
        }
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

std::vector<std::wstring> ProjectManager::GetRecentProjects() const {
    return LoadRecentProjects();
}

void ProjectManager::AddToRecentProjects(const std::wstring& projectPath) {
    auto projects = LoadRecentProjects();
    
    // Remove if already exists
    projects.erase(std::remove(projects.begin(), projects.end(), projectPath), projects.end());
    
    // Add to front
    projects.insert(projects.begin(), projectPath);
    
    // Limit to 10 recent projects
    if (projects.size() > 10) {
        projects.resize(10);
    }
    
    SaveRecentProjects(projects);
}

bool ProjectManager::DetectBuildSystem(const std::wstring& projectPath) {
    std::filesystem::path path(projectPath);
    
    if (std::filesystem::exists(path / L"CMakeLists.txt")) {
        projectConfig_.buildSystem = L"cmake";
        return true;
    }
    
    if (std::filesystem::exists(path / L"Makefile") || 
        std::filesystem::exists(path / L"makefile")) {
        projectConfig_.buildSystem = L"make";
        return true;
    }
    
    if (std::filesystem::exists(path / L"build.bat") || 
        std::filesystem::exists(path / L"build.sh")) {
        projectConfig_.buildSystem = L"custom";
        return true;
    }
    
    // Default to cmake
    projectConfig_.buildSystem = L"cmake";
    return true;
}

// Static utility functions
bool ProjectManager::IsValidProjectPath(const std::wstring& path) {
    try {
        std::filesystem::path fsPath(path);
        return !path.empty() && fsPath.is_absolute();
    }
    catch (const std::exception&) {
        return false;
    }
}

std::wstring ProjectManager::GetProjectConfigPath(const std::wstring& projectPath) {
    std::filesystem::path path(projectPath);
    return (path / L".nativeide" / L"project.conf").wstring();
}

std::wstring ProjectManager::GetTemplatesDirectory() {
    std::filesystem::path exePath;
    wchar_t buffer[MAX_PATH];
    if (GetModuleFileNameW(NULL, buffer, MAX_PATH) > 0) {
        exePath = std::filesystem::path(buffer).parent_path();
    }
    
    return (exePath / L"templates").wstring();
}

// Private methods
bool ProjectManager::LoadProjectConfig() {
    std::wstring configPath = GetProjectConfigPath(currentProjectPath_);
    return projectConfig_.LoadFromFile(configPath);
}

bool ProjectManager::SaveProjectConfig() {
    std::wstring configPath = GetProjectConfigPath(currentProjectPath_);
    
    // Create .nativeide directory if it doesn't exist
    std::filesystem::path configDir = std::filesystem::path(configPath).parent_path();
    try {
        std::filesystem::create_directories(configDir);
    }
    catch (const std::exception&) {
        return false;
    }
    
    return projectConfig_.SaveToFile(configPath);
}

void ProjectManager::BuildFileTree(ProjectItem* parentItem, const std::filesystem::path& dirPath) {
    try {
        std::vector<std::filesystem::directory_entry> entries;
        
        // Collect all entries
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            // Skip hidden directories and files starting with .
            std::wstring filename = entry.path().filename().wstring();
            if (filename[0] == L'.') continue;
            
            entries.push_back(entry);
        }
        
        // Sort entries (directories first, then files, alphabetically)
        std::sort(entries.begin(), entries.end(),
            [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
                if (a.is_directory() != b.is_directory()) {
                    return a.is_directory() > b.is_directory();
                }
                return a.path().filename() < b.path().filename();
            });
        
        // Create project items
        for (const auto& entry : entries) {
            std::wstring name = entry.path().filename().wstring();
            std::wstring fullPath = entry.path().wstring();
            bool isDirectory = entry.is_directory();
            
            auto item = std::make_unique<ProjectItem>(name, fullPath, isDirectory, parentItem);
            
            if (isDirectory) {
                BuildFileTree(item.get(), entry.path());
            }
            
            parentItem->children.push_back(std::move(item));
        }
    }
    catch (const std::exception&) {
        // Ignore filesystem errors
    }
}

void ProjectManager::NotifyFileEvent(const std::wstring& eventType, const std::wstring& filePath) {
    if (eventType == L"changed" && onFileChanged_) {
        onFileChanged_(eventType, filePath);
    }
    else if (eventType == L"added" && onFileAdded_) {
        onFileAdded_(eventType, filePath);
    }
    else if (eventType == L"removed" && onFileRemoved_) {
        onFileRemoved_(eventType, filePath);
    }
}

bool ProjectManager::CopyTemplate(const std::wstring& templatePath, const std::wstring& targetPath, 
                                 const std::wstring& projectName) {
    try {
        std::filesystem::copy(templatePath, targetPath, 
                             std::filesystem::copy_options::recursive);
        
        // Process template files
        for (const auto& entry : std::filesystem::recursive_directory_iterator(targetPath)) {
            if (entry.is_regular_file()) {
                ProcessTemplateFile(entry.path().wstring(), projectName);
            }
        }
        
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

void ProjectManager::ProcessTemplateFile(const std::wstring& filePath, const std::wstring& projectName) {
    std::string narrowPath(filePath.begin(), filePath.end());
    std::ifstream file(narrowPath);
    if (!file.is_open()) return;
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    // Replace template variables
    std::string projectNameStr(projectName.begin(), projectName.end());
    std::regex projectNameRegex(R"(\{\{PROJECT_NAME\}\})");
    content = std::regex_replace(content, projectNameRegex, projectNameStr);
    
    std::ofstream outFile(narrowPath);
    if (outFile.is_open()) {
        outFile << content;
    }
}

std::vector<std::wstring> ProjectManager::LoadRecentProjects() {
    std::vector<std::wstring> projects;
    
    // Load from registry or config file
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\NativeIDE", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buffer[MAX_PATH];
        DWORD bufferSize = sizeof(buffer);
        DWORD index = 0;
        
        while (RegEnumValueW(hKey, index, buffer, &bufferSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            std::wstring valueName(buffer);
            if (valueName.find(L"RecentProject") == 0) {
                DWORD dataSize = MAX_PATH * sizeof(wchar_t);
                wchar_t projectPath[MAX_PATH];
                if (RegQueryValueExW(hKey, buffer, NULL, NULL, (BYTE*)projectPath, &dataSize) == ERROR_SUCCESS) {
                    projects.push_back(std::wstring(projectPath));
                }
            }
            bufferSize = sizeof(buffer);
            index++;
        }
        
        RegCloseKey(hKey);
    }
    
    return projects;
}

void ProjectManager::SaveRecentProjects(const std::vector<std::wstring>& projects) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\NativeIDE", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        // Clear existing recent projects
        for (int i = 0; i < 10; ++i) {
            std::wstring valueName = L"RecentProject" + std::to_wstring(i);
            RegDeleteValueW(hKey, valueName.c_str());
        }
        
        // Save new projects
        for (size_t i = 0; i < projects.size() && i < 10; ++i) {
            std::wstring valueName = L"RecentProject" + std::to_wstring(i);
            RegSetValueExW(hKey, valueName.c_str(), 0, REG_SZ, 
                          (const BYTE*)projects[i].c_str(), 
                          (projects[i].length() + 1) * sizeof(wchar_t));
        }
        
        RegCloseKey(hKey);
    }
}

// FileWatcher Implementation
FileWatcher::FileWatcher(const std::wstring& watchPath, ProjectManager* projectManager)
    : watchPath_(watchPath), projectManager_(projectManager), 
      directoryHandle_(INVALID_HANDLE_VALUE), watchThread_(NULL),
      isWatching_(false), shouldStop_(false) {
}

FileWatcher::~FileWatcher() {
    StopWatching();
}

bool FileWatcher::StartWatching() {
    if (isWatching_) {
        return true;
    }
    
    directoryHandle_ = CreateFileW(
        watchPath_.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL);
    
    if (directoryHandle_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    shouldStop_ = false;
    watchThread_ = CreateThread(NULL, 0, WatchThreadProc, this, 0, NULL);
    
    if (watchThread_ == NULL) {
        CloseHandle(directoryHandle_);
        directoryHandle_ = INVALID_HANDLE_VALUE;
        return false;
    }
    
    isWatching_ = true;
    return true;
}

void FileWatcher::StopWatching() {
    if (!isWatching_) {
        return;
    }
    
    shouldStop_ = true;
    
    if (watchThread_ != NULL) {
        WaitForSingleObject(watchThread_, 1000);
        CloseHandle(watchThread_);
        watchThread_ = NULL;
    }
    
    if (directoryHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(directoryHandle_);
        directoryHandle_ = INVALID_HANDLE_VALUE;
    }
    
    isWatching_ = false;
}

DWORD WINAPI FileWatcher::WatchThreadProc(LPVOID lpParameter) {
    FileWatcher* watcher = static_cast<FileWatcher*>(lpParameter);
    
    char buffer[4096];
    DWORD bytesReturned;
    
    while (!watcher->shouldStop_) {
        if (ReadDirectoryChangesW(
            watcher->directoryHandle_,
            buffer,
            sizeof(buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE,
            &bytesReturned,
            NULL,
            NULL)) {
            
            FILE_NOTIFY_INFORMATION* notification = (FILE_NOTIFY_INFORMATION*)buffer;
            watcher->ProcessFileChanges(notification);
        }
        
        if (watcher->shouldStop_) break;
        Sleep(100);
    }
    
    return 0;
}

void FileWatcher::ProcessFileChanges(const FILE_NOTIFY_INFORMATION* notification) {
    if (!projectManager_) return;
    
    std::wstring fileName(notification->FileName, notification->FileNameLength / sizeof(wchar_t));
    std::wstring fullPath = watchPath_ + L"\\" + fileName;
    
    switch (notification->Action) {
        case FILE_ACTION_ADDED:
            projectManager_->NotifyFileEvent(L"added", fullPath);
            break;
        case FILE_ACTION_REMOVED:
            projectManager_->NotifyFileEvent(L"removed", fullPath);
            break;
        case FILE_ACTION_MODIFIED:
            projectManager_->NotifyFileEvent(L"changed", fullPath);
            break;
        case FILE_ACTION_RENAMED_OLD_NAME:
            projectManager_->NotifyFileEvent(L"removed", fullPath);
            break;
        case FILE_ACTION_RENAMED_NEW_NAME:
            projectManager_->NotifyFileEvent(L"added", fullPath);
            break;
    }
}

} // namespace NativeIDE