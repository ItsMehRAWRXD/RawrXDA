/**
 * \file project_detector.cpp
 * \brief Implementation of project type detection
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "project_detector.h"


namespace RawrXD {

// ========== ProjectMetadata ==========

ProjectMetadata::ProjectMetadata()
    : type(ProjectType::Unknown)
    , lastOpened(std::chrono::system_clock::time_point::currentDateTime())
{}

void* ProjectMetadata::toJson() const {
    void* obj;
    obj["name"] = name;
    obj["rootPath"] = rootPath;
    obj["type"] = static_cast<int>(type);
    obj["buildDirectory"] = buildDirectory;
    obj["gitBranch"] = gitBranch;
    obj["lastOpened"] = lastOpened.toString(//ISODate);
    
    void* recentArray;
    for (const std::string& file : recentFiles) {
        recentArray.append(file);
    }
    obj["recentFiles"] = recentArray;
    
    void* includeArray;
    for (const std::string& path : includePaths) {
        includeArray.append(path);
    }
    obj["includePaths"] = includeArray;
    
    void* sourceArray;
    for (const std::string& path : sourcePaths) {
        sourceArray.append(path);
    }
    obj["sourcePaths"] = sourceArray;
    
    obj["customData"] = customData;
    
    return obj;
}

bool ProjectMetadata::fromJson(const void*& json) {
    name = json["name"].toString();
    rootPath = json["rootPath"].toString();
    type = static_cast<ProjectType>(json["type"].toInt());
    buildDirectory = json["buildDirectory"].toString();
    gitBranch = json["gitBranch"].toString();
    lastOpened = std::chrono::system_clock::time_point::fromString(json["lastOpened"].toString(), //ISODate);
    
    recentFiles.clear();
    void* recentArray = json["recentFiles"].toArray();
    for (const void*& val : recentArray) {
        recentFiles.append(val.toString());
    }
    
    includePaths.clear();
    void* includeArray = json["includePaths"].toArray();
    for (const void*& val : includeArray) {
        includePaths.append(val.toString());
    }
    
    sourcePaths.clear();
    void* sourceArray = json["sourcePaths"].toArray();
    for (const void*& val : sourceArray) {
        sourcePaths.append(val.toString());
    }
    
    customData = json["customData"].toObject();
    
    return !rootPath.isEmpty();
}

// ========== ProjectDetector ==========

ProjectDetector::ProjectDetector() = default;
ProjectDetector::~ProjectDetector() = default;

ProjectMetadata ProjectDetector::detectProject(const std::string& path) {
    ProjectMetadata meta;
    
    // Find project root
    std::string root = findProjectRoot(path);
    if (root.isEmpty()) {
        root = std::filesystem::path(path).isDir() ? path : std::filesystem::path(path).absolutePath();
    }
    
    meta.rootPath = root;
    meta.name = std::filesystem::path(root).fileName();
    meta.type = detectProjectType(root);
    meta.buildDirectory = defaultBuildDirectory(meta.type);
    meta.sourcePaths = defaultSourceDirectories(meta.type);
    meta.gitBranch = detectGitBranch(root);
    meta.lastOpened = std::chrono::system_clock::time_point::currentDateTime();
    
    // Try to load existing metadata and merge
    if (hasProjectMetadata(root)) {
        ProjectMetadata existing = loadProjectMetadata(root);
        // Keep user customizations
        if (!existing.name.isEmpty()) meta.name = existing.name;
        if (!existing.buildDirectory.isEmpty()) meta.buildDirectory = existing.buildDirectory;
        meta.recentFiles = existing.recentFiles;
        if (!existing.includePaths.isEmpty()) meta.includePaths = existing.includePaths;
        if (!existing.sourcePaths.isEmpty()) meta.sourcePaths = existing.sourcePaths;
        meta.customData = existing.customData;
    }
    
    return meta;
}

std::string ProjectDetector::findProjectRoot(const std::string& anyPath) {
    std::filesystem::path info(anyPath);
    std::string currentDir = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    
    // Search up the directory tree for project markers
    std::filesystem::path dir(currentDir);
    int maxLevels = 10;  // Don't search too far up
    
    for (int level = 0; level < maxLevels; ++level) {
        // Check for common project root markers
        if (hasMarkerFile(dir.absolutePath(), ".git") ||
            hasMarkerFile(dir.absolutePath(), ".rawrxd") ||
            hasMarkerFile(dir.absolutePath(), "CMakeLists.txt") ||
            hasFilePattern(dir.absolutePath(), "*.pro") ||
            hasFilePattern(dir.absolutePath(), "*.sln") ||
            hasMarkerFile(dir.absolutePath(), "package.json") ||
            hasMarkerFile(dir.absolutePath(), "Cargo.toml") ||
            hasMarkerFile(dir.absolutePath(), "go.mod") ||
            hasMarkerFile(dir.absolutePath(), "pyproject.toml")) {
            return dir.absolutePath();
        }
        
        // Move up one level
        if (!dir.cdUp()) {
            break;
        }
    }
    
    // No project root found
    return std::string();
}

ProjectType ProjectDetector::detectProjectType(const std::string& rootPath) {
    // Check in priority order (most specific first)
    
    // Git repository (can coexist with other types)
    bool isGit = hasMarkerFile(rootPath, ".git");
    
    // CMake
    if (hasMarkerFile(rootPath, "CMakeLists.txt")) {
        return ProjectType::CMake;
    }
    
    // Visual Studio
    if (hasFilePattern(rootPath, "*.sln")) {
        return ProjectType::VisualStudio;
    }
    
    // .NET
    if (hasFilePattern(rootPath, "*.csproj") || hasFilePattern(rootPath, "*.vbproj")) {
        return ProjectType::DotNet;
    }
    
    // QMake
    if (hasFilePattern(rootPath, "*.pro")) {
        return ProjectType::QMake;
    }
    
    // Rust
    if (hasMarkerFile(rootPath, "Cargo.toml")) {
        return ProjectType::Rust;
    }
    
    // Go
    if (hasMarkerFile(rootPath, "go.mod")) {
        return ProjectType::Go;
    }
    
    // Node.js
    if (hasMarkerFile(rootPath, "package.json")) {
        return ProjectType::NodeJS;
    }
    
    // Python
    if (hasMarkerFile(rootPath, "setup.py") || 
        hasMarkerFile(rootPath, "pyproject.toml") ||
        hasMarkerFile(rootPath, "requirements.txt")) {
        return ProjectType::Python;
    }
    
    // MASM (assembly)
    if (hasFilePattern(rootPath, "*.asm")) {
        return ProjectType::MASM;
    }
    
    // Just git repo
    if (isGit) {
        return ProjectType::Git;
    }
    
    // Generic project
    return ProjectType::Generic;
}

std::string ProjectDetector::projectTypeName(ProjectType type) {
    switch (type) {
        case ProjectType::Git: return "Git Repository";
        case ProjectType::CMake: return "CMake Project";
        case ProjectType::QMake: return "QMake Project";
        case ProjectType::NodeJS: return "Node.js Project";
        case ProjectType::Python: return "Python Project";
        case ProjectType::DotNet: return ".NET Project";
        case ProjectType::Rust: return "Rust Project";
        case ProjectType::Go: return "Go Module";
        case ProjectType::VisualStudio: return "Visual Studio Solution";
        case ProjectType::MASM: return "MASM Assembly Project";
        case ProjectType::Generic: return "Generic Project";
        case ProjectType::Unknown:
        default: return "Unknown Project";
    }
}

std::string ProjectDetector::defaultBuildDirectory(ProjectType type) {
    switch (type) {
        case ProjectType::CMake: return "build";
        case ProjectType::Rust: return "target";
        case ProjectType::Go: return "bin";
        case ProjectType::NodeJS: return "dist";
        case ProjectType::Python: return "dist";
        case ProjectType::DotNet: return "bin";
        case ProjectType::VisualStudio: return "Debug";
        case ProjectType::MASM: return "bin";
        default: return "build";
    }
}

std::vector<std::string> ProjectDetector::defaultSourceDirectories(ProjectType type) {
    switch (type) {
        case ProjectType::CMake:
        case ProjectType::QMake:
            return {"src", "include"};
        case ProjectType::Rust:
            return {"src"};
        case ProjectType::Go:
            return {"."};
        case ProjectType::NodeJS:
            return {"src", "lib"};
        case ProjectType::Python:
            return {"src", "."};
        case ProjectType::DotNet:
        case ProjectType::VisualStudio:
            return {"src"};
        case ProjectType::MASM:
            return {"."};
        default:
            return {"src"};
    }
}

bool ProjectDetector::saveProjectMetadata(const ProjectMetadata& metadata) {
    if (metadata.rootPath.isEmpty()) {
        return false;
    }
    
    // Create .rawrxd directory if needed
    std::string configDir = projectConfigDirectory(metadata.rootPath);
    std::filesystem::path dir;
    if (!dir.mkpath(configDir)) {
        return false;
    }
    
    // Write JSON file
    std::string configFile = projectConfigFile(metadata.rootPath);
    std::fstream file(configFile);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    void* doc(metadata.toJson());
    file.write(doc.toJson(void*::Indented));
    return true;
}

ProjectMetadata ProjectDetector::loadProjectMetadata(const std::string& projectRoot) {
    ProjectMetadata meta;
    
    std::string configFile = projectConfigFile(projectRoot);
    std::fstream file(configFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return meta;  // Return empty metadata
    }
    
    void* doc = void*::fromJson(file.readAll());
    if (!doc.isObject()) {
        return meta;
    }
    
    meta.fromJson(doc.object());
    return meta;
}

bool ProjectDetector::hasProjectMetadata(const std::string& projectRoot) {
    return std::filesystem::path::exists(projectConfigFile(projectRoot));
}

std::string ProjectDetector::projectConfigDirectory(const std::string& projectRoot) {
    return std::filesystem::path(projectRoot).filePath(".rawrxd");
}

std::string ProjectDetector::projectConfigFile(const std::string& projectRoot) {
    return std::filesystem::path(projectConfigDirectory(projectRoot)).filePath("project.json");
}

std::string ProjectDetector::detectGitBranch(const std::string& projectRoot) {
    std::filesystem::path dir(projectRoot);
    if (!dir.exists(".git")) {
        return std::string();
    }
    
    // Try to read .git/HEAD
    std::string headFile = dir.filePath(".git/HEAD");
    std::fstream file(headFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::string();
    }
    
    std::string content = std::string::fromUtf8(file.readAll()).trimmed();
    
    // Format: "ref: refs/heads/main" or just a commit hash
    if (content.startsWith("ref: refs/heads/")) {
        return content.mid(16);  // Extract branch name
    }
    
    // Detached HEAD state (commit hash)
    if (content.length() == 40) {
        return "detached HEAD";
    }
    
    return std::string();
}

void ProjectDetector::addRecentFile(ProjectMetadata& metadata, const std::string& filePath, int maxRecent) {
    // Remove if already in list
    metadata.recentFiles.removeAll(filePath);
    
    // Add to front
    metadata.recentFiles.prepend(filePath);
    
    // Trim to max size
    while (metadata.recentFiles.size() > maxRecent) {
        metadata.recentFiles.removeLast();
    }
}

bool ProjectDetector::hasMarkerFile(const std::string& dirPath, const std::string& markerFile) {
    std::filesystem::path dir(dirPath);
    return dir.exists(markerFile);
}

bool ProjectDetector::hasFilePattern(const std::string& dirPath, const std::string& pattern) {
    std::filesystem::path dir(dirPath);
    std::vector<std::string> matches = dir.entryList({pattern}, std::filesystem::path::Files);
    return !matches.isEmpty();
}

bool ProjectDetector::checkProjectType(const std::string& rootPath, ProjectType type) {
    // Static method implementation - checks if directory contains markers for given type
    switch (type) {
        case ProjectType::CMake:
            return hasMarkerFile(rootPath, "CMakeLists.txt");
        case ProjectType::VisualStudio:
            return hasFilePattern(rootPath, "*.sln");
        case ProjectType::DotNet:
            return hasFilePattern(rootPath, "*.csproj") || hasFilePattern(rootPath, "*.vbproj");
        case ProjectType::QMake:
            return hasFilePattern(rootPath, "*.pro");
        case ProjectType::Rust:
            return hasMarkerFile(rootPath, "Cargo.toml");
        case ProjectType::Go:
            return hasMarkerFile(rootPath, "go.mod");
        case ProjectType::NodeJS:
            return hasMarkerFile(rootPath, "package.json");
        case ProjectType::Python:
            return hasMarkerFile(rootPath, "setup.py") || 
                   hasMarkerFile(rootPath, "pyproject.toml") ||
                   hasMarkerFile(rootPath, "requirements.txt");
        case ProjectType::MASM:
            return hasFilePattern(rootPath, "*.asm");
        case ProjectType::Git:
            return hasMarkerFile(rootPath, ".git");
        default:
            return false;
    }
}

} // namespace RawrXD

