/**
 * @file project_detector.cpp
 * @brief Project type detection and metadata management
 * @author RawrXD Team
 * @date 2025-12-12
 */

#include "project_detector.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace RawrXD {

// ============================================================================
// ProjectMetadata Implementation
// ============================================================================

ProjectMetadata::ProjectMetadata()
    : type(ProjectType::Unknown),
      created(QDateTime::currentDateTime()),
      modified(QDateTime::currentDateTime()),
      isVirtual(false),
      isRemote(false),
      hasGitRepo(false)
{
}

// ============================================================================
// ProjectDetector Implementation
// ============================================================================

ProjectDetector::ProjectDetector(QObject* parent)
    : QObject(parent)
{
    qDebug() << "[ProjectDetector] Initializing project detector";
}

ProjectDetector::~ProjectDetector()
{
    qDebug() << "[ProjectDetector] Destroying project detector";
}

ProjectMetadata ProjectDetector::detectProject(const QString& path)
{
    ProjectMetadata metadata;
    metadata.rootPath = path;
    metadata.name = QDir(path).dirName();
    metadata.created = QDateTime::currentDateTime();
    metadata.modified = QDateTime::currentDateTime();
    
    // Stub implementation - detect project type by examining directory contents
    QDir dir(path);
    QStringList entries = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    
    // Simple heuristics for project type detection
    if (entries.contains("CMakeLists.txt")) {
        metadata.type = ProjectType::CMakeProject;
    } else if (entries.contains("package.json")) {
        metadata.type = ProjectType::NodeProject;
    } else if (entries.contains("Cargo.toml")) {
        metadata.type = ProjectType::RustProject;
    } else if (entries.contains("setup.py") || entries.contains("pyproject.toml")) {
        metadata.type = ProjectType::PythonProject;
    } else {
        metadata.type = ProjectType::GenericFolder;
    }
    
    // Check for git repository
    metadata.hasGitRepo = dir.exists(".git");
    
    qInfo() << "[ProjectDetector] Detected project type:" << static_cast<int>(metadata.type)
            << "in" << path;
    
    return metadata;
}

bool ProjectDetector::saveProjectMetadata(const ProjectMetadata& metadata)
{
    // Stub implementation - save metadata to project file
    QString metadataPath = metadata.rootPath + "/.rawrxd/project.json";
    QDir(metadata.rootPath).mkpath(".rawrxd");
    
    QJsonObject json;
    json["name"] = metadata.name;
    json["type"] = static_cast<int>(metadata.type);
    json["created"] = metadata.created.toString(Qt::ISODate);
    json["modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QFile file(metadataPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[ProjectDetector] Failed to save metadata to" << metadataPath;
        return false;
    }
    
    file.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
    file.close();
    return true;
}

QString ProjectDetector::projectTypeName(ProjectType type)
{
    switch (type) {
        case ProjectType::CMakeProject:
            return "CMake Project";
        case ProjectType::NodeProject:
            return "Node.js Project";
        case ProjectType::PythonProject:
            return "Python Project";
        case ProjectType::RustProject:
            return "Rust Project";
        case ProjectType::GenericFolder:
            return "Generic Folder";
        default:
            return "Unknown";
    }
}

ProjectMetadata ProjectDetector::loadProjectMetadata(const QString& path)
{
    ProjectMetadata metadata;
    metadata.rootPath = path;
    metadata.name = QDir(path).dirName();
    
    QString metadataPath = path + "/.rawrxd/project.json";
    QFile file(metadataPath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        // Fallback: detect if file doesn't exist
        return detectProject(path);
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return detectProject(path);
    }
    
    QJsonObject json = doc.object();
    metadata.type = static_cast<ProjectType>(json["type"].toInt(0));
    metadata.name = json["name"].toString(metadata.name);
    
    return metadata;
}

bool ProjectDetector::hasProjectMetadata(const QString& path)
{
    return QFile::exists(path + "/.rawrxd/project.json");
}

void ProjectDetector::addRecentFile(ProjectMetadata& metadata, const QString& filePath, int maxRecent)
{
    if (!metadata.recentFiles.contains(filePath)) {
        metadata.recentFiles.prepend(filePath);
        if (metadata.recentFiles.size() > maxRecent) {
            metadata.recentFiles.removeLast();
        }
    }
}

} // namespace RawrXD
