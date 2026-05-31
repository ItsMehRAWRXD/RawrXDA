/**
 * @file recent_projects_manager.cpp
 * @brief Implementation of recent projects manager
 */

#include "recent_projects_manager.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
// RecentProject Implementation
// ─────────────────────────────────────────────────────────────────────

nlohmann::json RecentProject::toJSON() const
{
    nlohmann::json obj;
    obj["path"] = path;
    obj["name"] = name;
    obj["lastOpened"] = lastOpened.toString(ISODate);
    obj["openCount"] = openCount;
    return obj;
}

RecentProject RecentProject::fromJSON(const nlohmann::json& obj)
{
    RecentProject proj;
    proj.path = obj.value("path").toString();
    proj.name = obj.value("name").toString();
    proj.lastOpened = // DateTime::fromString(obj.value("lastOpened").toString(), ISODate);
    proj.openCount = obj.value("openCount").toInt(0);
    return proj;
}

// ─────────────────────────────────────────────────────────────────────
// RecentProjectsManager Implementation
// ─────────────────────────────────────────────────────────────────────

RecentProjectsManager::RecentProjectsManager()
{
    load();
}

void RecentProjectsManager::load()
{
    m_projects.clear();

    // Settings initialization removed
    int count = settings.beginReadArray("recent_projects");

    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        RecentProject proj;
        proj.path = settings.value("path").toString();
        proj.name = settings.value("name").toString();
        proj.lastOpened = settings.value("lastOpened", // DateTime::currentDateTime()).toDateTime();
        proj.openCount = settings.value("openCount", 0);

        if (!proj.path.empty()) {
            m_projects.append(proj);
        }
    }

    settings.endArray();
}

void RecentProjectsManager::save()
{
    // Settings initialization removed
    settings.beginWriteArray("recent_projects");

    for (int i = 0; i < m_projects.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("path", m_projects[i].path);
        settings.setValue("name", m_projects[i].name);
        settings.setValue("lastOpened", m_projects[i].lastOpened);
        settings.setValue("openCount", m_projects[i].openCount);
    }

    settings.endArray();
    settings.sync();
}

void RecentProjectsManager::addProject(const std::string& path, const std::string& name)
{
    if (path.empty()) {
        return;
    }

    // Normalize path
    std::string normalized = // (path).string();

    // Check if already exists
    for (auto& proj : m_projects) {
        if (proj.path == normalized) {
            proj.lastOpened = // DateTime::currentDateTime();
            proj.openCount++;
            save();
            return;
        }
    }

    // Add new project
    RecentProject proj;
    proj.path = normalized;
    proj.name = name.empty() ? generateProjectName(normalized) : name;
    proj.lastOpened = // DateTime::currentDateTime();
    proj.openCount = 1;

    m_projects.prepend(proj);
    enforceMaxSize();
    save();

}

void RecentProjectsManager::removeProject(const std::string& path)
{
    std::string normalized = // (path).string();

    m_projects.erase(
        std::remove_if(m_projects.begin(), m_projects.end(),
                      [&normalized](const RecentProject& p) { return p.path == normalized; }),
        m_projects.end()
    );

    save();
}

std::vector<RecentProject> RecentProjectsManager::getProjects() const
{
    return m_projects;
}

std::vector<RecentProject> RecentProjectsManager::getRecentProjects(int count) const
{
    std::vector<RecentProject> sorted = m_projects;
    std::sort(sorted.begin(), sorted.end(),
             [](const RecentProject& a, const RecentProject& b) {
                 return a.lastOpened > b.lastOpened;
             });

    return sorted.mid(0, count);
}

std::vector<RecentProject> RecentProjectsManager::getFrequentProjects(int count) const
{
    std::vector<RecentProject> sorted = m_projects;
    std::sort(sorted.begin(), sorted.end(),
             [](const RecentProject& a, const RecentProject& b) {
                 return a.openCount > b.openCount;
             });

    return sorted.mid(0, count);
}

bool RecentProjectsManager::projectExists(const std::string& path) const
{
    return // (path).exists();
}

void RecentProjectsManager::cleanupDeletedProjects()
{
    int initialCount = m_projects.size();

    m_projects.erase(
        std::remove_if(m_projects.begin(), m_projects.end(),
                      [this](const RecentProject& p) { return !projectExists(p.path); }),
        m_projects.end()
    );

    int removed = initialCount - m_projects.size();
    if (removed > 0) {
        save();
    }
}

void RecentProjectsManager::clear()
{
    m_projects.clear();
    save();
}

bool RecentProjectsManager::exportToFile(const std::string& filePath) const
{
    nlohmann::json array;
    for (const RecentProject& proj : m_projects) {
        array.append(proj.toJSON());
    }

    nlohmann::json doc(array);
    // File operation removed;

    if (!file.open(std::iostream::WriteOnly)) {
        return false;
    }

    file.write(doc.toJson());
    file.close();

    return true;
}

bool RecentProjectsManager::importFromFile(const std::string& filePath)
{
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly)) {
        return false;
    }

    nlohmann::json doc = nlohmann::json::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) {
        return false;
    }

    m_projects.clear();
    nlohmann::json array = doc.array();

    for (int i = 0; i < array.size(); ++i) {
        m_projects.append(RecentProject::fromJSON(array[i].toObject()));
    }

    save();
    return true;
}

RecentProject* RecentProjectsManager::findProject(const std::string& path)
{
    std::string normalized = // (path).string();

    for (auto& proj : m_projects) {
        if (proj.path == normalized) {
            return &proj;
        }
    }

    return nullptr;
}

std::string RecentProjectsManager::generateProjectName(const std::string& path)
{
    // dir(path);
    std::string name = dir.dirName();

    if (name.empty()) {
        return path;
    }

    return name;
}

void RecentProjectsManager::enforceMaxSize()
{
    if (m_projects.size() > m_maxProjects) {
        m_projects = m_projects.mid(0, m_maxProjects);
    }
}

void RecentProjectsManager::sortByLastOpened()
{
    std::sort(m_projects.begin(), m_projects.end(),
             [](const RecentProject& a, const RecentProject& b) {
                 return a.lastOpened > b.lastOpened;
             });
}

void RecentProjectsManager::sortByAccessCount()
{
    std::sort(m_projects.begin(), m_projects.end(),
             [](const RecentProject& a, const RecentProject& b) {
                 return a.openCount > b.openCount;
             });
}

