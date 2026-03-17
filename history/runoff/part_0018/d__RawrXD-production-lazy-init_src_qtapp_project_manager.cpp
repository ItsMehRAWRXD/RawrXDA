/**
 * @file project_manager.cpp
 * @brief Implementation of project management with gitignore filtering
 * @author RawrXD Team
 * @version 1.0.0
 */

#include "project_manager.hpp"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QStandardPaths>

/**
 * ProjectInfo Implementation
 */

QJsonObject ProjectInfo::toJSON() const {
    QJsonObject obj;
    obj["path"] = path;
    obj["name"] = name;
    obj["lastOpened"] = lastOpened.toString(Qt::ISODate);
    obj["projectType"] = projectType;
    obj["description"] = description;
    obj["pinned"] = pinned;
    
    QJsonArray filesArray;
    for (const QString& file : recentFiles) {
        filesArray.append(file);
    }
    obj["recentFiles"] = filesArray;
    
    return obj;
}

ProjectInfo ProjectInfo::fromJSON(const QJsonObject& obj) {
    ProjectInfo info;
    info.path = obj.value("path").toString();
    info.name = obj.value("name").toString();
    info.lastOpened = QDateTime::fromString(obj.value("lastOpened").toString(), Qt::ISODate);
    info.projectType = obj.value("projectType").toString();
    info.description = obj.value("description").toString();
    info.pinned = obj.value("pinned").toBool();
    
    QJsonArray filesArray = obj.value("recentFiles").toArray();
    for (const QJsonValue& val : filesArray) {
        info.recentFiles.append(val.toString());
    }
    
    return info;
}

/**
 * GitignoreFilter Implementation
 */

GitignoreFilter::GitignoreFilter(QObject* parent)
    : QObject(parent)
{
    loadDefaultPatterns();
}

GitignoreFilter::~GitignoreFilter() {
}

void GitignoreFilter::setRootDirectory(const QString& dir) {
    m_rootDirectory = QDir(dir).absolutePath();
    m_ignoreCache.clear();
    
    // Look for .gitignore files
    QDir rootDir(m_rootDirectory);
    if (rootDir.exists(".gitignore")) {
        loadGitignore(rootDir.absoluteFilePath(".gitignore"));
    }
}

void GitignoreFilter::loadGitignore(const QString& gitignorePath) {
    QFile file(gitignorePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[GitignoreFilter] Cannot open .gitignore:" << gitignorePath;
        return;
    }
    
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        
        addPattern(line);
    }
    
    file.close();
    
    // Watch this .gitignore file
    if (m_watcher && !m_watchedGitignores.contains(gitignorePath)) {
        m_watcher->addPath(gitignorePath);
        m_watchedGitignores.insert(gitignorePath);
    }
    
    qDebug() << "[GitignoreFilter] Loaded .gitignore:" << gitignorePath 
             << "with" << m_patterns.size() << "patterns";
}

void GitignoreFilter::loadDefaultPatterns() {
    if (!m_useDefaultPatterns) {
        return;
    }
    
    // Common ignore patterns
    QStringList defaults = getDefaultIgnorePatterns();
    for (const QString& pattern : defaults) {
        addPattern(pattern);
    }
}

void GitignoreFilter::clearPatterns() {
    m_patterns.clear();
    m_compiledPatterns.clear();
    m_ignoreCache.clear();
    emit patternsChanged();
}

void GitignoreFilter::addPattern(const QString& pattern) {
    if (pattern.isEmpty()) {
        return;
    }
    
    m_patterns.append(pattern);
    m_compiledPatterns.append(parsePattern(pattern));
    m_ignoreCache.clear();
}

void GitignoreFilter::removePattern(const QString& pattern) {
    int index = m_patterns.indexOf(pattern);
    if (index >= 0) {
        m_patterns.removeAt(index);
        m_compiledPatterns.removeAt(index);
        m_ignoreCache.clear();
        emit patternsChanged();
    }
}

bool GitignoreFilter::shouldIgnore(const QString& path, bool isDirectory) const {
    // Check cache first
    QString cacheKey = path + (isDirectory ? "/" : "");
    if (m_ignoreCache.contains(cacheKey)) {
        return m_ignoreCache[cacheKey];
    }
    
    // Convert to relative path
    QString relativePath = path;
    if (QFileInfo(path).isAbsolute()) {
        relativePath = QDir(m_rootDirectory).relativeFilePath(path);
    }
    
    // Normalize path separators
    relativePath.replace('\\', '/');
    
    bool ignored = false;
    
    // Check each pattern
    for (const Pattern& pattern : m_compiledPatterns) {
        if (matchesPattern(relativePath, pattern, isDirectory)) {
            ignored = !pattern.negation;
        }
    }
    
    // Cache the result
    m_ignoreCache[cacheKey] = ignored;
    return ignored;
}

QStringList GitignoreFilter::filterPaths(const QStringList& paths) const {
    QStringList result;
    for (const QString& path : paths) {
        QFileInfo info(path);
        if (!shouldIgnore(path, info.isDir())) {
            result.append(path);
        }
    }
    return result;
}

QStringList GitignoreFilter::getDefaultIgnorePatterns() {
    return {
        // Build outputs
        "build/",
        "builds/",
        "dist/",
        "out/",
        "bin/",
        "obj/",
        "target/",
        "*.o",
        "*.obj",
        "*.exe",
        "*.dll",
        "*.so",
        "*.dylib",
        "*.a",
        "*.lib",
        
        // Version control
        ".git/",
        ".svn/",
        ".hg/",
        
        // IDEs
        ".vscode/",
        ".idea/",
        "*.suo",
        "*.user",
        "*.sln.docstates",
        
        // Temporary files
        "temp/",
        "tmp/",
        "*.tmp",
        "*.bak",
        "*.swp",
        "*~",
        
        // Node/JavaScript
        "node_modules/",
        "bower_components/",
        
        // Python
        "__pycache__/",
        "*.pyc",
        "*.pyo",
        ".venv/",
        "venv/",
        
        // OS files
        ".DS_Store",
        "Thumbs.db",
        "desktop.ini",
        
        // Logs
        "*.log",
        "logs/"
    };
}

void GitignoreFilter::enableFileWatching(bool enable) {
    if (enable && !m_watcher) {
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::fileChanged,
                this, &GitignoreFilter::onGitignoreFileChanged);
    } else if (!enable && m_watcher) {
        delete m_watcher;
        m_watcher = nullptr;
        m_watchedGitignores.clear();
    }
}

void GitignoreFilter::onGitignoreFileChanged(const QString& path) {
    qDebug() << "[GitignoreFilter] .gitignore modified:" << path;
    
    // Reload patterns
    clearPatterns();
    loadDefaultPatterns();
    loadGitignore(path);
    
    emit gitignoreModified(path);
}

GitignoreFilter::Pattern GitignoreFilter::parsePattern(const QString& pattern) const {
    Pattern p;
    p.original = pattern;
    
    QString cleaned = pattern.trimmed();
    
    // Check for negation
    if (cleaned.startsWith('!')) {
        p.negation = true;
        cleaned = cleaned.mid(1);
    }
    
    // Check for directory-only pattern
    if (cleaned.endsWith('/')) {
        p.directoryOnly = true;
        cleaned.chop(1);
    }
    
    // Convert glob pattern to regex
    p.regex = convertGlobToRegex(cleaned);
    
    return p;
}

bool GitignoreFilter::matchesPattern(const QString& path, const Pattern& pattern, bool isDirectory) const {
    // Directory-only patterns only match directories
    if (pattern.directoryOnly && !isDirectory) {
        return false;
    }
    
    QRegularExpression regex(pattern.regex);
    QRegularExpressionMatch match = regex.match(path);
    
    return match.hasMatch();
}

QString GitignoreFilter::convertGlobToRegex(const QString& glob) const {
    QString regex = "^";
    bool inBrackets = false;
    
    for (int i = 0; i < glob.length(); ++i) {
        QChar c = glob[i];
        
        if (c == '[') {
            inBrackets = true;
            regex += '[';
        } else if (c == ']') {
            inBrackets = false;
            regex += ']';
        } else if (c == '*') {
            if (i + 1 < glob.length() && glob[i + 1] == '*') {
                // ** matches any number of directories
                regex += ".*";
                ++i; // Skip next *
            } else {
                // * matches anything except /
                regex += "[^/]*";
            }
        } else if (c == '?') {
            regex += "[^/]";
        } else if (c == '.' || c == '+' || c == '^' || c == '$' || c == '(' || c == ')' || c == '{' || c == '}' || c == '|') {
            // Escape regex special characters
            regex += '\\';
            regex += c;
        } else {
            regex += c;
        }
    }
    
    regex += "$";
    return regex;
}

/**
 * RecentProjectsManager Implementation
 */

RecentProjectsManager::RecentProjectsManager(QObject* parent)
    : QObject(parent)
{
    loadProjects();
}

RecentProjectsManager::~RecentProjectsManager() {
    saveProjects();
}

void RecentProjectsManager::addRecentProject(const QString& projectPath) {
    QFileInfo info(projectPath);
    if (!info.exists() || !info.isDir()) {
        qWarning() << "[RecentProjects] Invalid project path:" << projectPath;
        return;
    }
    
    QString absolutePath = info.absoluteFilePath();
    
    // Update existing or create new
    if (m_projects.contains(absolutePath)) {
        m_projects[absolutePath].lastOpened = QDateTime::currentDateTime();
    } else {
        m_projects[absolutePath] = createProjectInfo(absolutePath);
        emit projectAdded(absolutePath);
    }
    
    pruneOldProjects();
    saveProjects();
    emit projectsChanged();
}

void RecentProjectsManager::removeProject(const QString& projectPath) {
    QString absolutePath = QFileInfo(projectPath).absoluteFilePath();
    
    if (m_projects.remove(absolutePath) > 0) {
        emit projectRemoved(absolutePath);
        emit projectsChanged();
        saveProjects();
    }
}

void RecentProjectsManager::clearRecentProjects() {
    // Keep pinned projects
    QMap<QString, ProjectInfo> pinned;
    for (auto it = m_projects.begin(); it != m_projects.end(); ++it) {
        if (it.value().pinned) {
            pinned[it.key()] = it.value();
        }
    }
    
    m_projects = pinned;
    emit projectsChanged();
    saveProjects();
}

QVector<ProjectInfo> RecentProjectsManager::getRecentProjects(int maxCount) const {
    QVector<ProjectInfo> projects;
    
    // Convert to vector and sort by last opened (most recent first)
    for (const ProjectInfo& info : m_projects.values()) {
        if (!info.pinned) {
            projects.append(info);
        }
    }
    
    std::sort(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
        return a.lastOpened > b.lastOpened;
    });
    
    // Limit count
    if (projects.size() > maxCount) {
        projects.resize(maxCount);
    }
    
    return projects;
}

QVector<ProjectInfo> RecentProjectsManager::getPinnedProjects() const {
    QVector<ProjectInfo> projects;
    
    for (const ProjectInfo& info : m_projects.values()) {
        if (info.pinned) {
            projects.append(info);
        }
    }
    
    return projects;
}

QVector<ProjectInfo> RecentProjectsManager::getAllProjects() const {
    QVector<ProjectInfo> projects = getPinnedProjects();
    QVector<ProjectInfo> recent = getRecentProjects(m_maxRecentProjects);
    
    projects.append(recent);
    return projects;
}

bool RecentProjectsManager::hasProject(const QString& projectPath) const {
    QString absolutePath = QFileInfo(projectPath).absoluteFilePath();
    return m_projects.contains(absolutePath);
}

ProjectInfo RecentProjectsManager::getProjectInfo(const QString& projectPath) const {
    QString absolutePath = QFileInfo(projectPath).absoluteFilePath();
    return m_projects.value(absolutePath);
}

void RecentProjectsManager::pinProject(const QString& projectPath, bool pinned) {
    QString absolutePath = QFileInfo(projectPath).absoluteFilePath();
    
    if (m_projects.contains(absolutePath)) {
        m_projects[absolutePath].pinned = pinned;
        emit projectPinned(absolutePath, pinned);
        emit projectsChanged();
        saveProjects();
    }
}

void RecentProjectsManager::updateProjectInfo(const QString& projectPath, const ProjectInfo& info) {
    QString absolutePath = QFileInfo(projectPath).absoluteFilePath();
    m_projects[absolutePath] = info;
    emit projectsChanged();
    saveProjects();
}

void RecentProjectsManager::addRecentFile(const QString& projectPath, const QString& filePath) {
    QString absolutePath = QFileInfo(projectPath).absoluteFilePath();
    
    if (m_projects.contains(absolutePath)) {
        ProjectInfo& info = m_projects[absolutePath];
        
        // Remove if already exists
        info.recentFiles.removeAll(filePath);
        
        // Add to front
        info.recentFiles.prepend(filePath);
        
        // Limit recent files
        if (info.recentFiles.size() > 10) {
            info.recentFiles.resize(10);
        }
        
        saveProjects();
    }
}

QString RecentProjectsManager::detectProjectType(const QString& projectPath) {
    QDir dir(projectPath);
    
    // Check for CMake project
    if (dir.exists("CMakeLists.txt")) {
        return "CMake";
    }
    
    // Check for MASM project
    QStringList asmFiles = dir.entryList(QStringList() << "*.asm", QDir::Files);
    if (!asmFiles.isEmpty()) {
        return "MASM";
    }
    
    // Check for Visual Studio project
    QStringList slnFiles = dir.entryList(QStringList() << "*.sln", QDir::Files);
    if (!slnFiles.isEmpty()) {
        return "Visual Studio";
    }
    
    // Check for C/C++ project
    QStringList cppFiles = dir.entryList(QStringList() << "*.cpp" << "*.c" << "*.h" << "*.hpp", QDir::Files);
    if (!cppFiles.isEmpty()) {
        return "C/C++";
    }
    
    return "Generic";
}

QString RecentProjectsManager::findProjectRoot(const QString& startPath) {
    QDir dir(startPath);
    
    // Look for project markers
    QStringList markers = {
        "CMakeLists.txt",
        ".git",
        "*.sln",
        "Makefile",
        "build.sh",
        "configure"
    };
    
    // Search upwards (max 10 levels)
    for (int i = 0; i < 10; ++i) {
        for (const QString& marker : markers) {
            if (marker.contains('*')) {
                // Wildcard search
                if (!dir.entryList(QStringList() << marker, QDir::Files).isEmpty()) {
                    return dir.absolutePath();
                }
            } else {
                // Exact match
                if (dir.exists(marker)) {
                    return dir.absolutePath();
                }
            }
        }
        
        // Move up one directory
        if (!dir.cdUp()) {
            break;
        }
    }
    
    // Return original path if no project root found
    return startPath;
}

void RecentProjectsManager::saveProjects() {
    QSettings settings("RawrXD", "ProjectManager");
    
    QJsonArray projectsArray;
    for (const ProjectInfo& info : m_projects.values()) {
        projectsArray.append(info.toJSON());
    }
    
    QJsonDocument doc(projectsArray);
    settings.setValue("recentProjects", doc.toJson());
    
    qDebug() << "[RecentProjects] Saved" << m_projects.size() << "projects";
}

void RecentProjectsManager::loadProjects() {
    QSettings settings("RawrXD", "ProjectManager");
    
    QByteArray data = settings.value("recentProjects").toByteArray();
    if (data.isEmpty()) {
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return;
    }
    
    m_projects.clear();
    QJsonArray projectsArray = doc.array();
    for (const QJsonValue& val : projectsArray) {
        ProjectInfo info = ProjectInfo::fromJSON(val.toObject());
        
        // Verify project still exists
        if (QFileInfo::exists(info.path)) {
            m_projects[info.path] = info;
        }
    }
    
    qDebug() << "[RecentProjects] Loaded" << m_projects.size() << "projects";
}

bool RecentProjectsManager::exportProjects(const QString& filePath) {
    QJsonArray projectsArray;
    for (const ProjectInfo& info : m_projects.values()) {
        projectsArray.append(info.toJSON());
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[RecentProjects] Cannot write to" << filePath;
        return false;
    }
    
    file.write(QJsonDocument(projectsArray).toJson());
    file.close();
    
    return true;
}

bool RecentProjectsManager::importProjects(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[RecentProjects] Cannot read from" << filePath;
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isArray()) {
        return false;
    }
    
    QJsonArray projectsArray = doc.array();
    for (const QJsonValue& val : projectsArray) {
        ProjectInfo info = ProjectInfo::fromJSON(val.toObject());
        
        if (QFileInfo::exists(info.path)) {
            m_projects[info.path] = info;
        }
    }
    
    emit projectsChanged();
    saveProjects();
    
    return true;
}

ProjectInfo RecentProjectsManager::createProjectInfo(const QString& projectPath) {
    ProjectInfo info;
    info.path = projectPath;
    info.name = QFileInfo(projectPath).fileName();
    info.lastOpened = QDateTime::currentDateTime();
    info.projectType = detectProjectType(projectPath);
    info.pinned = false;
    
    return info;
}

void RecentProjectsManager::pruneOldProjects() {
    // Remove unpinned projects beyond the limit
    QVector<ProjectInfo> unpinned;
    
    for (const ProjectInfo& info : m_projects.values()) {
        if (!info.pinned) {
            unpinned.append(info);
        }
    }
    
    // Sort by last opened (oldest first)
    std::sort(unpinned.begin(), unpinned.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
        return a.lastOpened < b.lastOpened;
    });
    
    // Remove excess
    int toRemove = unpinned.size() - m_maxRecentProjects;
    for (int i = 0; i < toRemove; ++i) {
        m_projects.remove(unpinned[i].path);
    }
}

/**
 * ProjectTreeFilter Implementation
 */

ProjectTreeFilter::ProjectTreeFilter(QObject* parent)
    : QObject(parent)
{
}

ProjectTreeFilter::~ProjectTreeFilter() {
}

void ProjectTreeFilter::setRootDirectory(const QString& dir) {
    m_rootDirectory = dir;
    
    if (m_gitignoreFilter) {
        m_gitignoreFilter->setRootDirectory(dir);
    }
}

void ProjectTreeFilter::setGitignoreFilter(GitignoreFilter* filter) {
    m_gitignoreFilter = filter;
    
    if (filter) {
        connect(filter, &GitignoreFilter::patternsChanged, this, &ProjectTreeFilter::filterChanged);
    }
}

void ProjectTreeFilter::addIncludePattern(const QString& pattern) {
    m_includePatterns.append(pattern);
    emit filterChanged();
}

void ProjectTreeFilter::addExcludePattern(const QString& pattern) {
    m_excludePatterns.append(pattern);
    emit filterChanged();
}

void ProjectTreeFilter::clearCustomPatterns() {
    m_includePatterns.clear();
    m_excludePatterns.clear();
    emit filterChanged();
}

void ProjectTreeFilter::setAllowedExtensions(const QStringList& extensions) {
    m_allowedExtensions = extensions;
    emit filterChanged();
}

bool ProjectTreeFilter::shouldShow(const QString& path, bool isDirectory) const {
    m_stats.totalItems++;
    
    // Apply gitignore filter
    if (m_filterMode == GitignoreOnly || m_filterMode == Combined) {
        if (m_gitignoreFilter && m_gitignoreFilter->shouldIgnore(path, isDirectory)) {
            m_stats.hiddenByGitignore++;
            return false;
        }
    }
    
    // Apply custom patterns
    if (m_filterMode == CustomOnly || m_filterMode == Combined) {
        if (!matchesCustomPatterns(path)) {
            m_stats.hiddenByCustom++;
            return false;
        }
    }
    
    // Apply extension filter (files only)
    if (!isDirectory && !m_allowedExtensions.isEmpty()) {
        if (!matchesExtension(path)) {
            m_stats.hiddenByCustom++;
            return false;
        }
    }
    
    // Apply search filter
    if (!m_searchQuery.isEmpty()) {
        if (!matchesSearch(path)) {
            m_stats.hiddenBySearch++;
            return false;
        }
    }
    
    m_stats.visibleItems++;
    return true;
}

QStringList ProjectTreeFilter::filterTree(const QStringList& paths) const {
    m_stats = FilterStats();
    
    QStringList result;
    for (const QString& path : paths) {
        QFileInfo info(path);
        if (shouldShow(path, info.isDir())) {
            result.append(path);
        }
    }
    
    return result;
}

void ProjectTreeFilter::setSearchQuery(const QString& query) {
    m_searchQuery = query;
    emit filterChanged();
}

bool ProjectTreeFilter::matchesCustomPatterns(const QString& path) const {
    // If no patterns, show by default
    if (m_includePatterns.isEmpty() && m_excludePatterns.isEmpty()) {
        return true;
    }
    
    // Check exclude patterns first
    for (const QString& pattern : m_excludePatterns) {
        QRegularExpression regex(pattern);
        if (regex.match(path).hasMatch()) {
            return false;
        }
    }
    
    // Check include patterns
    if (!m_includePatterns.isEmpty()) {
        for (const QString& pattern : m_includePatterns) {
            QRegularExpression regex(pattern);
            if (regex.match(path).hasMatch()) {
                return true;
            }
        }
        return false;
    }
    
    return true;
}

bool ProjectTreeFilter::matchesExtension(const QString& path) const {
    QFileInfo info(path);
    QString ext = info.suffix().toLower();
    
    for (const QString& allowedExt : m_allowedExtensions) {
        if (ext == allowedExt.toLower()) {
            return true;
        }
    }
    
    return false;
}

bool ProjectTreeFilter::matchesSearch(const QString& path) const {
    QFileInfo info(path);
    QString fileName = info.fileName();
    
    return fileName.contains(m_searchQuery, Qt::CaseInsensitive);
}

/**
 * ProjectManager Implementation
 */

ProjectManager::ProjectManager(QObject* parent)
    : QObject(parent)
{
    m_recentProjects = new RecentProjectsManager(this);
    m_gitignoreFilter = new GitignoreFilter(this);
    m_treeFilter = new ProjectTreeFilter(this);
    
    m_treeFilter->setGitignoreFilter(m_gitignoreFilter);
    
    loadWorkspace();
}

ProjectManager::~ProjectManager() {
    saveWorkspace();
}

bool ProjectManager::openProject(const QString& projectPath) {
    QFileInfo info(projectPath);
    if (!info.exists() || !info.isDir()) {
        qWarning() << "[ProjectManager] Invalid project path:" << projectPath;
        return false;
    }
    
    QString absolutePath = info.absoluteFilePath();
    
    // Close current project
    if (!m_currentProjectPath.isEmpty()) {
        closeProject();
    }
    
    m_currentProjectPath = absolutePath;
    
    // Configure filters
    m_gitignoreFilter->setRootDirectory(absolutePath);
    m_treeFilter->setRootDirectory(absolutePath);
    
    // Add to recent projects
    m_recentProjects->addRecentProject(absolutePath);
    
    emit projectOpened(absolutePath);
    emit currentProjectChanged(absolutePath);
    
    qInfo() << "[ProjectManager] Opened project:" << absolutePath;
    return true;
}

bool ProjectManager::closeProject() {
    if (m_currentProjectPath.isEmpty()) {
        return false;
    }
    
    QString closedPath = m_currentProjectPath;
    m_currentProjectPath.clear();
    
    emit projectClosed(closedPath);
    emit currentProjectChanged(QString());
    
    qInfo() << "[ProjectManager] Closed project:" << closedPath;
    return true;
}

ProjectInfo ProjectManager::getCurrentProjectInfo() const {
    if (m_currentProjectPath.isEmpty()) {
        return ProjectInfo();
    }
    
    return m_recentProjects->getProjectInfo(m_currentProjectPath);
}

QString ProjectManager::detectProjectRoot(const QString& filePath) {
    return RecentProjectsManager::findProjectRoot(filePath);
}

QString ProjectManager::detectProjectType(const QString& projectPath) {
    return m_recentProjects->detectProjectType(projectPath);
}

bool ProjectManager::createNewProject(const QString& path, const QString& type) {
    QDir dir;
    if (!dir.mkpath(path)) {
        qWarning() << "[ProjectManager] Failed to create directory:" << path;
        return false;
    }
    
    // Create basic project structure based on type
    if (type == "MASM") {
        // Create MASM project structure
        QDir projectDir(path);
        projectDir.mkdir("src");
        projectDir.mkdir("include");
        projectDir.mkdir("build");
        
        // Create sample .asm file
        QFile asmFile(projectDir.filePath("src/main.asm"));
        if (asmFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&asmFile);
            out << "; Main entry point\n";
            out << ".code\n";
            out << "main proc\n";
            out << "    ; Your code here\n";
            out << "    ret\n";
            out << "main endp\n";
            out << "end\n";
            asmFile.close();
        }
        
        // Create .gitignore
        QFile gitignore(projectDir.filePath(".gitignore"));
        if (gitignore.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&gitignore);
            out << "build/\n";
            out << "*.obj\n";
            out << "*.exe\n";
            gitignore.close();
        }
    }
    
    return openProject(path);
}

void ProjectManager::saveWorkspace() {
    QSettings settings("RawrXD", "ProjectManager");
    settings.setValue("currentProject", m_currentProjectPath);
}

void ProjectManager::loadWorkspace() {
    QSettings settings("RawrXD", "ProjectManager");
    QString lastProject = settings.value("currentProject").toString();
    
    if (!lastProject.isEmpty() && QFileInfo::exists(lastProject)) {
        openProject(lastProject);
    }
}
