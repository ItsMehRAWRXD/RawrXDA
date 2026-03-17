/**
 * @file recent_projects_manager.hpp
 * @brief Recent projects management and history
 */

#ifndef RECENT_PROJECTS_MANAGER_HPP
#define RECENT_PROJECTS_MANAGER_HPP

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QJsonObject>
#include <memory>

/**
 * @struct RecentProject
 * @brief Information about a recent project
 */
struct RecentProject
{
    QString path;              // Full path to project root
    QString name;              // Display name
    QDateTime lastOpened;      // Last opened timestamp
    int openCount = 0;         // Number of times opened

    /**
     * Serialize to JSON
     */
    QJsonObject toJSON() const;

    /**
     * Deserialize from JSON
     */
    static RecentProject fromJSON(const QJsonObject& obj);
};

/**
 * @class RecentProjectsManager
 * @brief Manages recent projects list with persistent storage
 * 
 * Tracks recently opened projects with timestamps and access counts.
 * Provides filtering, sorting, and cleanup functionality.
 * Stores data in QSettings for persistence across sessions.
 * 
 * Features:
 * - Automatic addition on project open
 * - Maximum history limit (configurable, default 10)
 * - Removal of deleted projects
 * - Sorting by last opened / access count
 * - JSON export/import for backup
 * - Signal emission on project list changes
 */
class RecentProjectsManager
{
public:
    /**
     * Create manager with default settings storage
     */
    RecentProjectsManager();

    /**
     * Load recent projects from QSettings
     */
    void load();

    /**
     * Save recent projects to QSettings
     */
    void save();

    /**
     * Add project to recent list
     * @param path Full path to project root
     * @param name Display name (optional, defaults to last path component)
     */
    void addProject(const QString& path, const QString& name = QString());

    /**
     * Remove project from history
     * @param path Project path to remove
     */
    void removeProject(const QString& path);

    /**
     * Get all recent projects sorted by last opened
     */
    QList<RecentProject> getProjects() const;

    /**
     * Get up to N most recent projects
     */
    QList<RecentProject> getRecentProjects(int count = 10) const;

    /**
     * Get projects sorted by access count (most frequently used)
     */
    QList<RecentProject> getFrequentProjects(int count = 5) const;

    /**
     * Check if project path exists on disk
     */
    bool projectExists(const QString& path) const;

    /**
     * Remove deleted projects from history
     * Scans all entries and removes those that no longer exist on disk
     */
    void cleanupDeletedProjects();

    /**
     * Get number of stored projects
     */
    int count() const { return m_projects.size(); }

    /**
     * Clear all history
     */
    void clear();

    /**
     * Set maximum number of projects to keep (default 10)
     */
    void setMaxProjects(int max) { m_maxProjects = max; }

    /**
     * Get maximum number of projects
     */
    int maxProjects() const { return m_maxProjects; }

    /**
     * Export projects to JSON file
     */
    bool exportToFile(const QString& filePath) const;

    /**
     * Import projects from JSON file
     */
    bool importFromFile(const QString& filePath);

    /**
     * Get single project by path
     */
    RecentProject* findProject(const QString& path);

    /**
     * Generate unique display name for project
     * Handles duplicates by appending path suffix
     */
    static QString generateProjectName(const QString& path);

private:
    QList<RecentProject> m_projects;
    int m_maxProjects = 10;

    /**
     * Trim list to max size, removing oldest entries
     */
    void enforceMaxSize();

    /**
     * Sort projects by last opened time (newest first)
     */
    void sortByLastOpened();

    /**
     * Sort projects by open count (most used first)
     */
    void sortByAccessCount();
};

#endif // RECENT_PROJECTS_MANAGER_HPP
