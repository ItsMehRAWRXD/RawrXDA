/**
 * @file recent_projects_manager.cpp
 * @brief Implementation of recent projects manager
 */

#include "recent_projects_manager.hpp"
#include <QSettings>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
// RecentProject Implementation
// ─────────────────────────────────────────────────────────────────────

QJsonObject RecentProject::toJSON() const
{
    QJsonObject obj;
    obj["path"] = path;
    obj["name"] = name;
    obj["lastOpened"] = lastOpened.toString(Qt::ISODate);
    obj["openCount"] = openCount;
    return obj;
}

RecentProject RecentProject::fromJSON(const QJsonObject& obj)
{
    RecentProject proj;
    proj.path = obj.value("path").toString();
    proj.name = obj.value("name").toString();
    proj.lastOpened = QDateTime::fromString(obj.value("lastOpened").toString(), Qt::ISODate);
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

    QSettings settings("RawrXD", "QtShell");
    int count = settings.beginReadArray("recent_projects");

    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        RecentProject proj;
        proj.path = settings.value("path").toString();
        proj.name = settings.value("name").toString();
        proj.lastOpened = settings.value("lastOpened", QDateTime::currentDateTime()).toDateTime();
        proj.openCount = settings.value("openCount", 0).toInt();

        if (!proj.path.isEmpty()) {
            m_projects.append(proj);
        }
    }

    settings.endArray();
    qInfo() << "[RecentProjectsManager] Loaded" << m_projects.size() << "projects";
}

void RecentProjectsManager::save()
{
    QSettings settings("RawrXD", "QtShell");
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
    qInfo() << "[RecentProjectsManager] Saved" << m_projects.size() << "projects";
}

void RecentProjectsManager::addProject(const QString& path, const QString& name)
{
    if (path.isEmpty()) {
        return;
    }

    // Normalize path
    QString normalized = QDir(path).absolutePath();

    // Check if already exists
    for (auto& proj : m_projects) {
        if (proj.path == normalized) {
            proj.lastOpened = QDateTime::currentDateTime();
            proj.openCount++;
            save();
            qInfo() << "[RecentProjectsManager] Updated project:" << normalized;
            return;
        }
    }

    // Add new project
    RecentProject proj;
    proj.path = normalized;
    proj.name = name.isEmpty() ? generateProjectName(normalized) : name;
    proj.lastOpened = QDateTime::currentDateTime();
    proj.openCount = 1;

    m_projects.prepend(proj);
    enforceMaxSize();
    save();

    qInfo() << "[RecentProjectsManager] Added project:" << normalized;
}

void RecentProjectsManager::removeProject(const QString& path)
{
    QString normalized = QDir(path).absolutePath();

    m_projects.erase(
        std::remove_if(m_projects.begin(), m_projects.end(),
                      [&normalized](const RecentProject& p) { return p.path == normalized; }),
        m_projects.end()
    );

    save();
    qInfo() << "[RecentProjectsManager] Removed project:" << normalized;
}

QList<RecentProject> RecentProjectsManager::getProjects() const
{
    return m_projects;
}

QList<RecentProject> RecentProjectsManager::getRecentProjects(int count) const
{
    QList<RecentProject> sorted = m_projects;
    std::sort(sorted.begin(), sorted.end(),
             [](const RecentProject& a, const RecentProject& b) {
                 return a.lastOpened > b.lastOpened;
             });

    return sorted.mid(0, count);
}

QList<RecentProject> RecentProjectsManager::getFrequentProjects(int count) const
{
    QList<RecentProject> sorted = m_projects;
    std::sort(sorted.begin(), sorted.end(),
             [](const RecentProject& a, const RecentProject& b) {
                 return a.openCount > b.openCount;
             });

    return sorted.mid(0, count);
}

bool RecentProjectsManager::projectExists(const QString& path) const
{
    return QDir(path).exists();
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
        qInfo() << "[RecentProjectsManager] Cleaned up" << removed << "deleted projects";
    }
}

void RecentProjectsManager::clear()
{
    m_projects.clear();
    save();
    qInfo() << "[RecentProjectsManager] History cleared";
}

bool RecentProjectsManager::exportToFile(const QString& filePath) const
{
    QJsonArray array;
    for (const RecentProject& proj : m_projects) {
        array.append(proj.toJSON());
    }

    QJsonDocument doc(array);
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[RecentProjectsManager] Could not export to" << filePath;
        return false;
    }

    file.write(doc.toJson());
    file.close();

    qInfo() << "[RecentProjectsManager] Exported" << m_projects.size() << "projects to" << filePath;
    return true;
}

bool RecentProjectsManager::importFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[RecentProjectsManager] Could not import from" << filePath;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) {
        qWarning() << "[RecentProjectsManager] Invalid JSON format in" << filePath;
        return false;
    }

    m_projects.clear();
    QJsonArray array = doc.array();

    for (int i = 0; i < array.size(); ++i) {
        m_projects.append(RecentProject::fromJSON(array[i].toObject()));
    }

    save();
    qInfo() << "[RecentProjectsManager] Imported" << m_projects.size() << "projects from" << filePath;
    return true;
}

RecentProject* RecentProjectsManager::findProject(const QString& path)
{
    QString normalized = QDir(path).absolutePath();

    for (auto& proj : m_projects) {
        if (proj.path == normalized) {
            return &proj;
        }
    }

    return nullptr;
}

QString RecentProjectsManager::generateProjectName(const QString& path)
{
    QDir dir(path);
    QString name = dir.dirName();

    if (name.isEmpty()) {
        return path;
    }

    return name;
}

void RecentProjectsManager::enforceMaxSize()
{
    if (m_projects.size() > m_maxProjects) {
        m_projects = m_projects.mid(0, m_maxProjects);
        qDebug() << "[RecentProjectsManager] Trimmed to" << m_maxProjects << "projects";
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
