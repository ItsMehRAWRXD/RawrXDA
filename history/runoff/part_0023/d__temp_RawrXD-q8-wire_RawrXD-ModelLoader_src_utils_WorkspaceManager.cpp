/**
 * WorkspaceManager implementation
 */

#include "WorkspaceManager.h"
#include "SettingsManager.h"
#include "PathResolver.h"
#include "IDELogger.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <algorithm>

static WorkspaceManager* g_instance = nullptr;
static std::mutex g_instanceMutex;

WorkspaceManager& WorkspaceManager::getInstance()
{
    if (!g_instance) {
        std::lock_guard<std::mutex> lock(g_instanceMutex);
        if (!g_instance) {
            g_instance = new WorkspaceManager();
        }
    }
    return *g_instance;
}

WorkspaceManager::WorkspaceManager()
    : m_initialized(false)
{
}

WorkspaceManager::~WorkspaceManager()
{
    savePersistentWorkspace();
}

void WorkspaceManager::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return;
    }

    // Try to load last workspace from settings
    try {
        SettingsManager& settings = SettingsManager::getInstance();
        QString lastWorkspace = QString::fromStdString(settings.loadLastWorkspace());

        if (!lastWorkspace.isEmpty() && QDir(lastWorkspace).exists()) {
            m_currentWorkspacePath = lastWorkspace;
            m_workspaceDir = QDir(lastWorkspace);
            LOG_INFO("Loaded previous workspace: " + lastWorkspace.toStdString());
        } else {
            // Fall back to home directory
            m_currentWorkspacePath = PathResolver::getHomePath();
            m_workspaceDir = QDir(m_currentWorkspacePath);
            LOG_INFO("Using home directory as default workspace");
        }
    } catch (...) {
        // Fall back to home directory on any error
        m_currentWorkspacePath = PathResolver::getHomePath();
        m_workspaceDir = QDir(m_currentWorkspacePath);
        LOG_WARNING("Failed to load workspace from settings, using home directory");
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
    std::lock_guard<std::mutex> lock(m_mutex);

    // Validate path
    QString validationError = validateWorkspacePath(path);
    if (!validationError.isEmpty()) {
        LOG_WARNING("Invalid workspace path: " + validationError.toStdString());
        return false;
    }

    // Get absolute path
    QDir newDir(path);
    QString newPath = newDir.absolutePath();

    // Check if actually changed
    if (newPath == m_currentWorkspacePath) {
        return true;  // Already at this workspace
    }

    QString oldPath = m_currentWorkspacePath;
    m_currentWorkspacePath = newPath;
    m_workspaceDir = QDir(newPath);

    LOG_INFO("Workspace changed to: " + newPath.toStdString());

    // Emit callbacks (without lock held to avoid deadlock)
    lock.unlock();
    emitWorkspaceChanged(oldPath, newPath);

    return true;
}

QString WorkspaceManager::getDisplayPath() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QString path = m_currentWorkspacePath;

    // Shorten home directory paths
    QString home = PathResolver::getHomePath();
    if (path.startsWith(home)) {
        path = "~" + path.mid(home.length());
    }

    return path;
}

bool WorkspaceManager::isWorkspaceLoaded() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_currentWorkspacePath.isEmpty() && m_workspaceDir.exists();
}

QString WorkspaceManager::resolveWorkspacePath(const QString& relativePath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_workspaceDir.absoluteFilePath(relativePath);
}

QString WorkspaceManager::makeRelativePath(const QString& absolutePath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QDir currentDir(m_currentWorkspacePath);
    return currentDir.relativeFilePath(absolutePath);
}

bool WorkspaceManager::autoDetectWorkspace()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Try to find project root starting from current directory
    QString appDir = QCoreApplication::applicationDirPath();
    QString projectRoot = findProjectRoot(appDir);

    if (!projectRoot.isEmpty() && projectRoot != m_currentWorkspacePath) {
        QString oldPath = m_currentWorkspacePath;
        m_currentWorkspacePath = projectRoot;
        m_workspaceDir = QDir(projectRoot);

        LOG_INFO("Auto-detected workspace: " + projectRoot.toStdString());

        lock.unlock();
        emitWorkspaceChanged(oldPath, projectRoot);
        return true;
    }

    return false;
}

QStringList WorkspaceManager::getWorkspaceSubdirectories() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QStringList dirs;
    QFileInfoList entries = m_workspaceDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
        QDir::Name
    );

    for (const QFileInfo& entry : entries) {
        dirs << entry.fileName();
    }

    return dirs;
}

QStringList WorkspaceManager::getWorkspaceFiles(const QString& pattern) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QStringList files;
    QFileInfoList entries = m_workspaceDir.entryInfoList(
        QStringList(pattern),
        QDir::Files | QDir::NoSymLinks,
        QDir::Name
    );

    for (const QFileInfo& entry : entries) {
        files << entry.fileName();
    }

    return files;
}

bool WorkspaceManager::isFileInWorkspace(const QString& filePath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QFileInfo fileInfo(filePath);
    QString absPath = fileInfo.absoluteFilePath();
    QString workspaceAbs = m_workspaceDir.absolutePath();

    return absPath.startsWith(workspaceAbs);
}

QStringList WorkspaceManager::getWorkspaceMarkers() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QStringList markers;
    QStringList checkFor = {".git", ".rawrxd", ".vscode", "package.json", ".gitignore", "CMakeLists.txt"};

    for (const QString& marker : checkFor) {
        if (m_workspaceDir.exists(marker)) {
            markers << marker;
        }
    }

    return markers;
}

void WorkspaceManager::onWorkspaceChanged(const WorkspaceChangedCallback& callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_changeCallbacks.push_back(callback);
}

void WorkspaceManager::savePersistentWorkspace()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        SettingsManager& settings = SettingsManager::getInstance();
        settings.saveLastWorkspace(m_currentWorkspacePath.toStdString());
        settings.save();
        LOG_DEBUG("Workspace persisted: " + m_currentWorkspacePath.toStdString());
    } catch (const std::exception& e) {
        LOG_WARNING(std::string("Failed to save workspace: ") + e.what());
    }
}

QString WorkspaceManager::validateWorkspacePath(const QString& path) const
{
    if (path.isEmpty()) {
        return "Path is empty";
    }

    QDir dir(path);
    if (!dir.exists()) {
        return "Path does not exist: " + path;
    }

    if (!dir.isReadable()) {
        return "Path is not readable: " + path;
    }

    return "";  // Valid
}

WorkspaceManager::WorkspaceStats WorkspaceManager::getWorkspaceStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    WorkspaceStats stats;

    // Count files and directories
    QFileInfoList entries = m_workspaceDir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::NoSymLinks
    );

    for (const QFileInfo& entry : entries) {
        if (entry.isDir()) {
            stats.directoryCount++;
        } else {
            stats.fileCount++;
            stats.totalSize += entry.size();
        }
    }

    // Get last modified
    QFileInfo workspaceInfo(m_currentWorkspacePath);
    stats.lastModified = workspaceInfo.lastModified().toString(Qt::ISODate).toStdString().c_str();

    return stats;
}

void WorkspaceManager::emitWorkspaceChanged(const QString& oldPath, const QString& newPath)
{
    for (const auto& callback : m_changeCallbacks) {
        try {
            callback(oldPath, newPath);
        } catch (const std::exception& e) {
            LOG_ERROR(std::string("Error in workspace changed callback: ") + e.what());
        }
    }
}

bool WorkspaceManager::isProjectRoot(const QString& dirPath) const
{
    QDir dir(dirPath);

    // Check for project markers
    QStringList markers = {".git", ".rawrxd", ".vscode", "package.json", "CMakeLists.txt"};

    for (const QString& marker : markers) {
        if (dir.exists(marker)) {
            return true;
        }
    }

    return false;
}

QString WorkspaceManager::findProjectRoot(const QString& startPath) const
{
    QDir dir(startPath);
    int maxDepth = 10;  // Don't search too far up

    while (maxDepth-- > 0) {
        if (isProjectRoot(dir.absolutePath())) {
            return dir.absolutePath();
        }

        // Move up one directory
        if (!dir.cdUp()) {
            break;  // Can't go higher
        }
    }

    return "";  // Not found
}
