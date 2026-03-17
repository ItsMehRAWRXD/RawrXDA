/**
 * @file project_detector.h
 * @brief Project type detection and metadata management
 * @author RawrXD Team
 * @date 2025-12-12
 */

#pragma once

#include <QString>
#include <QObject>
#include <QStringList>
#include <QDateTime>

namespace RawrXD {

enum class ProjectType {
    Unknown = 0,
    CMakeProject = 1,
    NodeProject = 2,
    PythonProject = 3,
    RustProject = 4,
    GenericFolder = 5
};

/**
 * @struct ProjectMetadata
 * @brief Metadata describing a project
 */
struct ProjectMetadata {
    QString rootPath;
    QString name;
    ProjectType type = ProjectType::Unknown;
    QDateTime created;
    QDateTime modified;
    QStringList recentFiles;
    bool isVirtual = false;
    bool isRemote = false;
    bool hasGitRepo = false;
    
    ProjectMetadata();
};

/**
 * @class ProjectDetector
 * @brief Detects project types and manages project metadata
 */
class ProjectDetector : public QObject {
    Q_OBJECT
    
public:
    explicit ProjectDetector(QObject* parent = nullptr);
    ~ProjectDetector() override;
    
    // Core functionality
    ProjectMetadata detectProject(const QString& path);
    bool saveProjectMetadata(const ProjectMetadata& metadata);
    ProjectMetadata loadProjectMetadata(const QString& path);
    bool hasProjectMetadata(const QString& path);
    
    // Static helpers
    static QString projectTypeName(ProjectType type);
    static void addRecentFile(ProjectMetadata& metadata, const QString& filePath, int maxRecent = 10);
};

} // namespace RawrXD
