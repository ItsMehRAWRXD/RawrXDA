/**
 * @file recent_projects_manager.hpp
 * @brief Recent projects management and history
 */

#ifndef RECENT_PROJECTS_MANAGER_HPP
#define RECENT_PROJECTS_MANAGER_HPP

#include <memory>

/**
 * @struct RecentProject
 * @brief Information about a recent project
 */
struct RecentProject
{
    std::string path;              // Full path to project root
    std::string name;              // Display name
    // DateTime lastOpened;      // Last opened timestamp
    int openCount = 0;         // Number of times opened

    /**
     * Serialize to JSON
     */
    nlohmann::json toJSON() const;

    /**
     * Deserialize from JSON
     */
    static RecentProject fromJSON(const nlohmann::json& obj);
};

/**
 * @class RecentProjectsManager
 * @brief Manages recent projects list with persistent storage
 * 
 * Tracks recently opened projects with timestamps and access counts.
 * Provides filtering, sorting, and cleanup functionality.
 * Stores data in // Settings initialization removed

    /**
     * Load recent projects from // Settings initialization removed

    /**
     * Save recent projects to // Settings initialization removed

    /**
     * Add project to recent list
     * @param path Full path to project root
     * @param name Display name (optional, defaults to last path component)
     */
    void addProject(const std::string& path, const std::string& name = std::string());

    /**
     * Remove project from history
     * @param path Project path to remove
     */
    void removeProject(const std::string& path);

    /**
     * Get all recent projects sorted by last opened
     */
    std::vector<RecentProject> getProjects() const;

    /**
     * Get up to N most recent projects
     */
    std::vector<RecentProject> getRecentProjects(int count = 10) const;

    /**
     * Get projects sorted by access count (most frequently used)
     */
    std::vector<RecentProject> getFrequentProjects(int count = 5) const;

    /**
     * Check if project path exists on disk
     */
    bool projectExists(const std::string& path) const;

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
    bool exportToFile(const std::string& filePath) const;

    /**
     * Import projects from JSON file
     */
    bool importFromFile(const std::string& filePath);

    /**
     * Get single project by path
     */
    RecentProject* findProject(const std::string& path);

    /**
     * Generate unique display name for project
     * Handles duplicates by appending path suffix
     */
    static std::string generateProjectName(const std::string& path);

private:
    std::vector<RecentProject> m_projects;
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


