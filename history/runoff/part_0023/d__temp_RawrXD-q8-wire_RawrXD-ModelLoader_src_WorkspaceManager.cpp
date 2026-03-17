/**
 * WorkspaceManager - Central workspace context management implementation
 */

#include "WorkspaceManager.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDirIterator>
#include <QSettings>
#include <QDateTime>
#include <algorithm>

WorkspaceManager::WorkspaceManager()
    : m_initialized(false)
{
}

WorkspaceManager::~WorkspaceManager() = default;

WorkspaceManager& WorkspaceManager::getInstance()
{
    static WorkspaceManager instance;
    return instance;
}

void WorkspaceManager::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        return;
    }

    // Try to load last workspace from settings
    QSettings settings("RawrXD", "AgenticIDE");
    QString lastWorkspace = settings.value("workspace/lastPath").toString();
    
    if (!lastWorkspace.isEmpty() && QDir(lastWorkspace).exists()) {
        m_currentWorkspacePath = lastWorkspace;
        m_workspaceDir = QDir(lastWorkspace);
    } else {
        // Fall back to home directory
        m_currentWorkspacePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        m_workspaceDir = QDir::home();
    }
    
    m_initialized = true;
}

QString WorkspaceManager::getCurrentWorkspacePath() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentWorkspacePath;
}

QString WorkspaceManager::getCurrentWorkspaceName() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_workspaceDir.dirName();
}

bool WorkspaceManager::setWorkspacePath(const QString& path)
{
    QString validationError = validateWorkspacePath(path);
    if (!validationError.isEmpty()) {
        return false;
    }

    QString oldPath;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        oldPath = m_currentWorkspacePath;
        m_currentWorkspacePath = path;
        m_workspaceDir = QDir(path);
    }
    
    // Notify listeners
    emitWorkspaceChanged(oldPath, path);
    
    // Persist to settings
    savePersistentWorkspace();
    
    return true;
}

QString WorkspaceManager::getDisplayPath() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    QString path = m_currentWorkspacePath;
    QString home = QDir::homePath();
    
    // Replace home directory with ~
    if (path.startsWith(home)) {
        path = "~" + path.mid(home.length());
    }
    
    // Truncate if too long
    const int maxLen = 50;
    if (path.length() > maxLen) {
        path = "..." + path.right(maxLen - 3);
    }
    
    return path;
}

bool WorkspaceManager::isWorkspaceLoaded() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized && !m_currentWorkspacePath.isEmpty() && m_workspaceDir.exists();
}

QString WorkspaceManager::resolveWorkspacePath(const QString& relativePath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_workspaceDir.absoluteFilePath(relativePath);
}

QString WorkspaceManager::makeRelativePath(const QString& absolutePath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_workspaceDir.relativeFilePath(absolutePath);
}

bool WorkspaceManager::autoDetectWorkspace()
{
    QString detected = findProjectRoot(QDir::currentPath());
    if (!detected.isEmpty()) {
        return setWorkspacePath(detected);
    }
    return false;
}

QStringList WorkspaceManager::getWorkspaceSubdirectories() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    QStringList dirs;
    
    QDirIterator it(m_currentWorkspacePath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        it.next();
        dirs.append(it.fileName());
    }
    
    dirs.sort();
    return dirs;
}

QStringList WorkspaceManager::getWorkspaceFiles(const QString& pattern) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    QStringList files;
    
    QStringList filters;
    filters << pattern;
    
    QDirIterator it(m_currentWorkspacePath, filters, QDir::Files, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        it.next();
        files.append(it.fileName());
    }
    
    files.sort();
    return files;
}

bool WorkspaceManager::isFileInWorkspace(const QString& filePath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    QString absPath = QFileInfo(filePath).absoluteFilePath();
    QString wsPath = m_workspaceDir.absolutePath();
    
    return absPath.startsWith(wsPath);
}

QStringList WorkspaceManager::getWorkspaceMarkers() const
{
    return QStringList{
        ".git",
        ".rawrxd",
        ".vscode",
        "package.json",
        "CMakeLists.txt",
        "Makefile",
        "Cargo.toml",
        "pyproject.toml",
        "go.mod",
        ".project"
    };
}

void WorkspaceManager::onWorkspaceChanged(const WorkspaceChangedCallback& callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_changeCallbacks.push_back(callback);
}

void WorkspaceManager::savePersistentWorkspace()
{
    QSettings settings("RawrXD", "AgenticIDE");
    settings.setValue("workspace/lastPath", m_currentWorkspacePath);
}

QString WorkspaceManager::validateWorkspacePath(const QString& path) const
{
    if (path.isEmpty()) {
        return "Path is empty";
    }
    
    QFileInfo info(path);
    
    if (!info.exists()) {
        return "Path does not exist: " + path;
    }
    
    if (!info.isDir()) {
        return "Path is not a directory: " + path;
    }
    
    if (!info.isReadable()) {
        return "Directory is not readable: " + path;
    }
    
    return QString(); // Empty means valid
}

WorkspaceManager::WorkspaceStats WorkspaceManager::getWorkspaceStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    WorkspaceStats stats;
    
    QDirIterator it(m_currentWorkspacePath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        it.next();
        QFileInfo info = it.fileInfo();
        
        if (info.isDir()) {
            stats.directoryCount++;
        } else {
            stats.fileCount++;
            stats.totalSize += info.size();
        }
    }
    
    QFileInfo wsInfo(m_currentWorkspacePath);
    stats.lastModified = wsInfo.lastModified().toString(Qt::ISODate);
    
    return stats;
}

void WorkspaceManager::emitWorkspaceChanged(const QString& oldPath, const QString& newPath)
{
    std::vector<WorkspaceChangedCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callbacks = m_changeCallbacks;
    }
    
    for (const auto& callback : callbacks) {
        if (callback) {
            callback(oldPath, newPath);
        }
    }
}

bool WorkspaceManager::isProjectRoot(const QString& dirPath) const
{
    QDir dir(dirPath);
    
    for (const QString& marker : getWorkspaceMarkers()) {
        if (dir.exists(marker)) {
            return true;
        }
    }
    
    return false;
}

QString WorkspaceManager::findProjectRoot(const QString& startPath) const
{
    QDir dir(startPath);
    
    // Walk up the directory tree
    const int maxDepth = 10;
    for (int i = 0; i < maxDepth && !dir.isRoot(); ++i) {
        if (isProjectRoot(dir.absolutePath())) {
            return dir.absolutePath();
        }
        dir.cdUp();
    }
    
    // No project root found, return start path
    return startPath;
}
