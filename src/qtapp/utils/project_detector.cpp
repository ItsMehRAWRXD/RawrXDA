/**
 * \file project_detector.cpp
 * \brief Implementation of project type detection
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "project_detector.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QProcess>

namespace RawrXD {

// ========== ProjectMetadata ==========

ProjectMetadata::ProjectMetadata()
    : type(ProjectType::Unknown)
    , lastOpened(QDateTime::currentDateTime())
{}

QJsonObject ProjectMetadata::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["rootPath"] = rootPath;
    obj["type"] = static_cast<int>(type);
    obj["buildDirectory"] = buildDirectory;
    obj["gitBranch"] = gitBranch;
    obj["lastOpened"] = lastOpened.toString(Qt::ISODate);
    
    QJsonArray recentArray;
    for (const QString& file : recentFiles) {
        recentArray.append(file);
    }
    obj["recentFiles"] = recentArray;
    
    QJsonArray includeArray;
    for (const QString& path : includePaths) {
        includeArray.append(path);
    }
    obj["includePaths"] = includeArray;
    
    QJsonArray sourceArray;
    for (const QString& path : sourcePaths) {
        sourceArray.append(path);
    }
    obj["sourcePaths"] = sourceArray;
    
    obj["customData"] = customData;
    
    return obj;
}

bool ProjectMetadata::fromJson(const QJsonObject& json) {
    name = json["name"].toString();
    rootPath = json["rootPath"].toString();
    type = static_cast<ProjectType>(json["type"].toInt());
    buildDirectory = json["buildDirectory"].toString();
    gitBranch = json["gitBranch"].toString();
    lastOpened = QDateTime::fromString(json["lastOpened"].toString(), Qt::ISODate);
    
    recentFiles.clear();
    QJsonArray recentArray = json["recentFiles"].toArray();
    for (const QJsonValue& val : recentArray) {
        recentFiles.append(val.toString());
    }
    
    includePaths.clear();
    QJsonArray includeArray = json["includePaths"].toArray();
    for (const QJsonValue& val : includeArray) {
        includePaths.append(val.toString());
    }
    
    sourcePaths.clear();
    QJsonArray sourceArray = json["sourcePaths"].toArray();
    for (const QJsonValue& val : sourceArray) {
        sourcePaths.append(val.toString());
    }
    
    customData = json["customData"].toObject();
    
    return !rootPath.isEmpty();
}

// ========== ProjectDetector ==========

ProjectDetector::ProjectDetector() = default;
ProjectDetector::~ProjectDetector() = default;

ProjectMetadata ProjectDetector::detectProject(const QString& path) {
    ProjectMetadata meta;
    
    // Find project root
    QString root = findProjectRoot(path);
    if (root.isEmpty()) {
        root = QFileInfo(path).isDir() ? path : QFileInfo(path).absolutePath();
    }
    
    meta.rootPath = root;
    meta.name = QFileInfo(root).fileName();
    meta.type = detectProjectType(root);
    meta.buildDirectory = defaultBuildDirectory(meta.type);
    meta.sourcePaths = defaultSourceDirectories(meta.type);
    meta.gitBranch = detectGitBranch(root);
    meta.lastOpened = QDateTime::currentDateTime();
    
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

QString ProjectDetector::findProjectRoot(const QString& anyPath) {
    QFileInfo info(anyPath);
    QString currentDir = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    
    // Search up the directory tree for project markers
    QDir dir(currentDir);
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
    return QString();
}

ProjectType ProjectDetector::detectProjectType(const QString& rootPath) {
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

QString ProjectDetector::projectTypeName(ProjectType type) {
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

QString ProjectDetector::defaultBuildDirectory(ProjectType type) {
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

QStringList ProjectDetector::defaultSourceDirectories(ProjectType type) {
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
        qWarning() << "Cannot save project metadata: no root path";
        return false;
    }
    
    // Create .rawrxd directory if needed
    QString configDir = projectConfigDirectory(metadata.rootPath);
    QDir dir;
    if (!dir.mkpath(configDir)) {
        qWarning() << "Failed to create config directory:" << configDir;
        return false;
    }
    
    // Write JSON file
    QString configFile = projectConfigFile(metadata.rootPath);
    QFile file(configFile);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open config file for writing:" << configFile;
        return false;
    }
    
    QJsonDocument doc(metadata.toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

ProjectMetadata ProjectDetector::loadProjectMetadata(const QString& projectRoot) {
    ProjectMetadata meta;
    
    QString configFile = projectConfigFile(projectRoot);
    QFile file(configFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return meta;  // Return empty metadata
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qWarning() << "Invalid project metadata JSON:" << configFile;
        return meta;
    }
    
    meta.fromJson(doc.object());
    return meta;
}

bool ProjectDetector::hasProjectMetadata(const QString& projectRoot) {
    return QFileInfo::exists(projectConfigFile(projectRoot));
}

QString ProjectDetector::projectConfigDirectory(const QString& projectRoot) {
    return QDir(projectRoot).filePath(".rawrxd");
}

QString ProjectDetector::projectConfigFile(const QString& projectRoot) {
    return QDir(projectConfigDirectory(projectRoot)).filePath("project.json");
}

QString ProjectDetector::detectGitBranch(const QString& projectRoot) {
    QDir dir(projectRoot);
    if (!dir.exists(".git")) {
        return QString();
    }
    
    // Try to read .git/HEAD
    QString headFile = dir.filePath(".git/HEAD");
    QFile file(headFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    QString content = QString::fromUtf8(file.readAll()).trimmed();
    
    // Format: "ref: refs/heads/main" or just a commit hash
    if (content.startsWith("ref: refs/heads/")) {
        return content.mid(16);  // Extract branch name
    }
    
    // Detached HEAD state (commit hash)
    if (content.length() == 40) {
        return "detached HEAD";
    }
    
    return QString();
}

void ProjectDetector::addRecentFile(ProjectMetadata& metadata, const QString& filePath, int maxRecent) {
    // Remove if already in list
    metadata.recentFiles.removeAll(filePath);
    
    // Add to front
    metadata.recentFiles.prepend(filePath);
    
    // Trim to max size
    while (metadata.recentFiles.size() > maxRecent) {
        metadata.recentFiles.removeLast();
    }
}

bool ProjectDetector::hasMarkerFile(const QString& dirPath, const QString& markerFile) {
    QDir dir(dirPath);
    return dir.exists(markerFile);
}

bool ProjectDetector::hasFilePattern(const QString& dirPath, const QString& pattern) {
    QDir dir(dirPath);
    QStringList matches = dir.entryList({pattern}, QDir::Files);
    return !matches.isEmpty();
}

bool ProjectDetector::checkProjectType(const QString& rootPath, ProjectType type) {
    // This is handled by detectProjectType
    return detectProjectType(rootPath) == type;
}

} // namespace RawrXD
