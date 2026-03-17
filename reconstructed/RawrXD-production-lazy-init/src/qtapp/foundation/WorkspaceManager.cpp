#include "WorkspaceManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTextStream>

QJsonObject WorkspaceManager::WorkspaceMeta::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["path"] = path;
    obj["lastOpened"] = lastOpened.toMSecsSinceEpoch();
    obj["favorite"] = favorite;
    QJsonArray tagsArray;
    for (const QString& t : tags) tagsArray.append(t);
    obj["tags"] = tagsArray;
    return obj;
}

WorkspaceManager::WorkspaceMeta WorkspaceManager::WorkspaceMeta::fromJson(const QJsonObject& obj) {
    WorkspaceMeta meta;
    meta.name = obj.value("name").toString();
    meta.path = obj.value("path").toString();
    meta.lastOpened = QDateTime::fromMSecsSinceEpoch(obj.value("lastOpened").toVariant().toLongLong());
    meta.favorite = obj.value("favorite").toBool(false);
    QJsonArray tagsArray = obj.value("tags").toArray();
    for (const auto& v : tagsArray) meta.tags.append(v.toString());
    return meta;
}

WorkspaceManager::WorkspaceManager(QObject* parent) : QObject(parent) {}
WorkspaceManager::~WorkspaceManager() {}

QString WorkspaceManager::normalizePath(const QString& path) const {
    return QDir(path).canonicalPath();
}

bool WorkspaceManager::addWorkspace(const QString& path, const QString& name) {
    QMutexLocker locker(&m_mutex);
    QString norm = normalizePath(path);
    if (norm.isEmpty() || m_workspaces.contains(norm)) return false;
    QFileInfo info(norm);
    if (!info.exists() || !info.isDir()) return false;

    WorkspaceMeta meta;
    meta.path = norm;
    meta.name = name.isEmpty() ? info.baseName() : name;
    meta.lastOpened = QDateTime::currentDateTimeUtc();

    m_workspaces[norm] = meta;
    emit workspaceAdded(meta);
    return true;
}

bool WorkspaceManager::removeWorkspace(const QString& path) {
    QMutexLocker locker(&m_mutex);
    QString norm = normalizePath(path);
    if (!m_workspaces.contains(norm)) return false;
    m_workspaces.remove(norm);
    emit workspaceRemoved(norm);
    return true;
}

bool WorkspaceManager::markFavorite(const QString& path, bool favorite) {
    QMutexLocker locker(&m_mutex);
    QString norm = normalizePath(path);
    if (!m_workspaces.contains(norm)) return false;
    m_workspaces[norm].favorite = favorite;
    emit favoritesChanged([&]{ QStringList favs; for (const auto& meta : m_workspaces) if (meta.favorite) favs << meta.path; return favs; }());
    emit workspaceUpdated(m_workspaces[norm]);
    return true;
}

bool WorkspaceManager::renameWorkspace(const QString& path, const QString& newName) {
    QMutexLocker locker(&m_mutex);
    QString norm = normalizePath(path);
    if (!m_workspaces.contains(norm) || newName.isEmpty()) return false;
    m_workspaces[norm].name = newName;
    emit workspaceUpdated(m_workspaces[norm]);
    return true;
}

WorkspaceManager::WorkspaceMeta WorkspaceManager::getWorkspace(const QString& path) const {
    QMutexLocker locker(&m_mutex);
    QString norm = normalizePath(path);
    return m_workspaces.value(norm);
}

QList<WorkspaceManager::WorkspaceMeta> WorkspaceManager::getRecent(int limit) const {
    QMutexLocker locker(&m_mutex);
    QList<WorkspaceMeta> metas = m_workspaces.values();
    std::sort(metas.begin(), metas.end(), [](const WorkspaceMeta& a, const WorkspaceMeta& b) {
        return a.lastOpened > b.lastOpened;
    });
    if (limit > 0 && metas.size() > limit) metas = metas.mid(0, limit);
    return metas;
}

QStringList WorkspaceManager::listWorkspaces() const {
    QMutexLocker locker(&m_mutex);
    return m_workspaces.keys();
}

bool WorkspaceManager::exists(const QString& path) const {
    QFileInfo info(normalizePath(path));
    return info.exists() && info.isDir();
}

bool WorkspaceManager::isReadable(const QString& path) const {
    QFileInfo info(normalizePath(path));
    return info.isReadable();
}

void WorkspaceManager::touch(const QString& path) {
    QString norm = normalizePath(path);
    if (!m_workspaces.contains(norm)) return;
    m_workspaces[norm].lastOpened = QDateTime::currentDateTimeUtc();
}

bool WorkspaceManager::load(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return false;
    m_workspaces.clear();
    for (const auto& v : doc.array()) {
        WorkspaceMeta meta = WorkspaceMeta::fromJson(v.toObject());
        if (!meta.path.isEmpty()) {
            m_workspaces[normalizePath(meta.path)] = meta;
        }
    }
    return true;
}

bool WorkspaceManager::save(const QString& filePath) const {
    QMutexLocker locker(&m_mutex);
    QJsonArray arr;
    for (const auto& meta : m_workspaces) arr.append(meta.toJson());
    QJsonDocument doc(arr);
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    out << doc.toJson(QJsonDocument::Indented);
    return true;
}
