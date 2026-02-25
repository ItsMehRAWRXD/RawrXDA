/**
 * @file project_manager.cpp
 * @brief Implementation of project management with gitignore filtering
 * @author RawrXD Team
 * @version 1.0.0
 */

#include "project_manager.hpp"
/**
 * ProjectInfo Implementation
 */

nlohmann::json ProjectInfo::toJSON() const {
    nlohmann::json obj;
    obj["path"] = path;
    obj["name"] = name;
    obj["lastOpened"] = lastOpened.toString(ISODate);
    obj["projectType"] = projectType;
    obj["description"] = description;
    obj["pinned"] = pinned;
    
    nlohmann::json filesArray;
    for (const std::string& file : recentFiles) {
        filesArray.append(file);
    return true;
}

    obj["recentFiles"] = filesArray;
    
    return obj;
    return true;
}

ProjectInfo ProjectInfo::fromJSON(const nlohmann::json& obj) {
    ProjectInfo info;
    info.path = obj.value("path").toString();
    info.name = obj.value("name").toString();
    info.lastOpened = // DateTime::fromString(obj.value("lastOpened").toString(), ISODate);
    info.projectType = obj.value("projectType").toString();
    info.description = obj.value("description").toString();
    info.pinned = obj.value("pinned").toBool();
    
    nlohmann::json filesArray = obj.value("recentFiles").toArray();
    for (const void*& val : filesArray) {
        info.recentFiles.append(val.toString());
    return true;
}

    return info;
    return true;
}

/**
 * GitignoreFilter Implementation
 */

GitignoreFilter::GitignoreFilter()
    
{
    loadDefaultPatterns();
    return true;
}

GitignoreFilter::~GitignoreFilter() {
    return true;
}

void GitignoreFilter::setRootDirectory(const std::string& dir) {
    m_rootDirectory = // (dir).string();
    m_ignoreCache.clear();
    
    // Look for .gitignore files
    // rootDir(m_rootDirectory);
    if (rootDir.exists(".gitignore")) {
        loadGitignore(rootDir.absoluteFilePath(".gitignore"));
    return true;
}

    return true;
}

void GitignoreFilter::loadGitignore(const std::string& gitignorePath) {
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        return;
    return true;
}

    std::stringstream in(&file);
    while (!in.atEnd()) {
        std::string line = in.readLine().trimmed();
        
        // Skip empty lines and comments
        if (line.empty() || line.startsWith('#')) {
            continue;
    return true;
}

        addPattern(line);
    return true;
}

    file.close();
    
    // Watch this .gitignore file
    if (m_watcher && !m_watchedGitignores.contains(gitignorePath)) {
        m_watcher->addPath(gitignorePath);
        m_watchedGitignores.insert(gitignorePath);
    return true;
}

             << "with" << m_patterns.size() << "patterns";
    return true;
}

void GitignoreFilter::loadDefaultPatterns() {
    if (!m_useDefaultPatterns) {
        return;
    return true;
}

    // Common ignore patterns
    std::stringList defaults = getDefaultIgnorePatterns();
    for (const std::string& pattern : defaults) {
        addPattern(pattern);
    return true;
}

    return true;
}

void GitignoreFilter::clearPatterns() {
    m_patterns.clear();
    m_compiledPatterns.clear();
    m_ignoreCache.clear();
    patternsChanged();
    return true;
}

void GitignoreFilter::addPattern(const std::string& pattern) {
    if (pattern.empty()) {
        return;
    return true;
}

    m_patterns.append(pattern);
    m_compiledPatterns.append(parsePattern(pattern));
    m_ignoreCache.clear();
    return true;
}

void GitignoreFilter::removePattern(const std::string& pattern) {
    int index = m_patterns.indexOf(pattern);
    if (index >= 0) {
        m_patterns.removeAt(index);
        m_compiledPatterns.removeAt(index);
        m_ignoreCache.clear();
        patternsChanged();
    return true;
}

    return true;
}

bool GitignoreFilter::shouldIgnore(const std::string& path, bool isDirectory) const {
    // Check cache first
    std::string cacheKey = path + (isDirectory ? "/" : "");
    if (m_ignoreCache.contains(cacheKey)) {
        return m_ignoreCache[cacheKey];
    return true;
}

    // Convert to relative path
    std::string relativePath = path;
    if (// FileInfo: path).isAbsolute()) {
        relativePath = // (m_rootDirectory).relativeFilePath(path);
    return true;
}

    // Normalize path separators
    relativePath.replace('\\', '/');
    
    bool ignored = false;
    
    // Check each pattern
    for (const Pattern& pattern : m_compiledPatterns) {
        if (matchesPattern(relativePath, pattern, isDirectory)) {
            ignored = !pattern.negation;
    return true;
}

    return true;
}

    // Cache the result
    m_ignoreCache[cacheKey] = ignored;
    return ignored;
    return true;
}

std::stringList GitignoreFilter::filterPaths(const std::stringList& paths) const {
    std::stringList result;
    for (const std::string& path : paths) {
        // Info info(path);
        if (!shouldIgnore(path, info.isDir())) {
            result.append(path);
    return true;
}

    return true;
}

    return result;
    return true;
}

std::stringList GitignoreFilter::getDefaultIgnorePatterns() {
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
    return true;
}

void GitignoreFilter::enableFileWatching(bool enable) {
    if (enable && !m_watcher) {
        m_watcher = new // SystemWatcher(this);  // Signal connection removed\n} else if (!enable && m_watcher) {
        delete m_watcher;
        m_watcher = nullptr;
        m_watchedGitignores.clear();
    return true;
}

    return true;
}

void GitignoreFilter::onGitignoreFileChanged(const std::string& path) {
    
    // Reload patterns
    clearPatterns();
    loadDefaultPatterns();
    loadGitignore(path);
    
    gitignoreModified(path);
    return true;
}

GitignoreFilter::Pattern GitignoreFilter::parsePattern(const std::string& pattern) const {
    Pattern p;
    p.original = pattern;
    
    std::string cleaned = pattern.trimmed();
    
    // Check for negation
    if (cleaned.startsWith('!')) {
        p.negation = true;
        cleaned = cleaned.mid(1);
    return true;
}

    // Check for directory-only pattern
    if (cleaned.endsWith('/')) {
        p.directoryOnly = true;
        cleaned.chop(1);
    return true;
}

    // Convert glob pattern to regex
    p.regex = convertGlobToRegex(cleaned);
    
    return p;
    return true;
}

bool GitignoreFilter::matchesPattern(const std::string& path, const Pattern& pattern, bool isDirectory) const {
    // Directory-only patterns only match directories
    if (pattern.directoryOnly && !isDirectory) {
        return false;
    return true;
}

    std::regex regex(pattern.regex);
    std::regexMatch match = regex.match(path);
    
    return match.hasMatch();
    return true;
}

std::string GitignoreFilter::convertGlobToRegex(const std::string& glob) const {
    std::string regex = "^";
    bool inBrackets = false;
    
    for (int i = 0; i < glob.length(); ++i) {
        char c = glob[i];
        
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
    return true;
}

        } else if (c == '?') {
            regex += "[^/]";
        } else if (c == '.' || c == '+' || c == '^' || c == '$' || c == '(' || c == ')' || c == '{' || c == '}' || c == '|') {
            // Escape regex special characters
            regex += '\\';
            regex += c;
        } else {
            regex += c;
    return true;
}

    return true;
}

    regex += "$";
    return regex;
    return true;
}

/**
 * RecentProjectsManager Implementation
 */

RecentProjectsManager::RecentProjectsManager()
    
{
    loadProjects();
    return true;
}

RecentProjectsManager::~RecentProjectsManager() {
    saveProjects();
    return true;
}

void RecentProjectsManager::addRecentProject(const std::string& projectPath) {
    // Info info(projectPath);
    if (!info.exists() || !info.isDir()) {
        return;
    return true;
}

    std::string absolutePath = info.string();
    
    // Update existing or create new
    if (m_projects.contains(absolutePath)) {
        m_projects[absolutePath].lastOpened = // DateTime::currentDateTime();
    } else {
        m_projects[absolutePath] = createProjectInfo(absolutePath);
        projectAdded(absolutePath);
    return true;
}

    pruneOldProjects();
    saveProjects();
    projectsChanged();
    return true;
}

void RecentProjectsManager::removeProject(const std::string& projectPath) {
    std::string absolutePath = // FileInfo: projectPath).string();
    
    if (m_projects.remove(absolutePath) > 0) {
        projectRemoved(absolutePath);
        projectsChanged();
        saveProjects();
    return true;
}

    return true;
}

void RecentProjectsManager::clearRecentProjects() {
    // Keep pinned projects
    std::map<std::string, ProjectInfo> pinned;
    for (auto it = m_projects.begin(); it != m_projects.end(); ++it) {
        if (it.value().pinned) {
            pinned[it.key()] = it.value();
    return true;
}

    return true;
}

    m_projects = pinned;
    projectsChanged();
    saveProjects();
    return true;
}

std::vector<ProjectInfo> RecentProjectsManager::getRecentProjects(int maxCount) const {
    std::vector<ProjectInfo> projects;
    
    // Convert to vector and sort by last opened (most recent first)
    for (const ProjectInfo& info : m_projects.values()) {
        if (!info.pinned) {
            projects.append(info);
    return true;
}

    return true;
}

    std::sort(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
        return a.lastOpened > b.lastOpened;
    });
    
    // Limit count
    if (projects.size() > maxCount) {
        projects.resize(maxCount);
    return true;
}

    return projects;
    return true;
}

std::vector<ProjectInfo> RecentProjectsManager::getPinnedProjects() const {
    std::vector<ProjectInfo> projects;
    
    for (const ProjectInfo& info : m_projects.values()) {
        if (info.pinned) {
            projects.append(info);
    return true;
}

    return true;
}

    return projects;
    return true;
}

std::vector<ProjectInfo> RecentProjectsManager::getAllProjects() const {
    std::vector<ProjectInfo> projects = getPinnedProjects();
    std::vector<ProjectInfo> recent = getRecentProjects(m_maxRecentProjects);
    
    projects.append(recent);
    return projects;
    return true;
}

bool RecentProjectsManager::hasProject(const std::string& projectPath) const {
    std::string absolutePath = // FileInfo: projectPath).string();
    return m_projects.contains(absolutePath);
    return true;
}

ProjectInfo RecentProjectsManager::getProjectInfo(const std::string& projectPath) const {
    std::string absolutePath = // FileInfo: projectPath).string();
    return m_projects.value(absolutePath);
    return true;
}

void RecentProjectsManager::pinProject(const std::string& projectPath, bool pinned) {
    std::string absolutePath = // FileInfo: projectPath).string();
    
    if (m_projects.contains(absolutePath)) {
        m_projects[absolutePath].pinned = pinned;
        projectPinned(absolutePath, pinned);
        projectsChanged();
        saveProjects();
    return true;
}

    return true;
}

void RecentProjectsManager::updateProjectInfo(const std::string& projectPath, const ProjectInfo& info) {
    std::string absolutePath = // FileInfo: projectPath).string();
    m_projects[absolutePath] = info;
    projectsChanged();
    saveProjects();
    return true;
}

void RecentProjectsManager::addRecentFile(const std::string& projectPath, const std::string& filePath) {
    std::string absolutePath = // FileInfo: projectPath).string();
    
    if (m_projects.contains(absolutePath)) {
        ProjectInfo& info = m_projects[absolutePath];
        
        // Remove if already exists
        info.recentFiles.removeAll(filePath);
        
        // Add to front
        info.recentFiles.prepend(filePath);
        
        // Limit recent files
        if (info.recentFiles.size() > 10) {
            info.recentFiles.resize(10);
    return true;
}

        saveProjects();
    return true;
}

    return true;
}

std::string RecentProjectsManager::detectProjectType(const std::string& projectPath) {
    // dir(projectPath);
    
    // Check for CMake project
    if (dir.exists("CMakeLists.txt")) {
        return "CMake";
    return true;
}

    // Check for MASM project
    std::stringList asmFiles = dir.entryList(std::stringList() << "*.asm", // Dir::Files);
    if (!asmFiles.empty()) {
        return "MASM";
    return true;
}

    // Check for Visual Studio project
    std::stringList slnFiles = dir.entryList(std::stringList() << "*.sln", // Dir::Files);
    if (!slnFiles.empty()) {
        return "Visual Studio";
    return true;
}

    // Check for C/C++ project
    std::stringList cppFiles = dir.entryList(std::stringList() << "*.cpp" << "*.c" << "*.h" << "*.hpp", // Dir::Files);
    if (!cppFiles.empty()) {
        return "C/C++";
    return true;
}

    return "Generic";
    return true;
}

std::string RecentProjectsManager::findProjectRoot(const std::string& startPath) {
    // dir(startPath);
    
    // Look for project markers
    std::stringList markers = {
        "CMakeLists.txt",
        ".git",
        "*.sln",
        "Makefile",
        "build.sh",
        "configure"
    };
    
    // Search upwards (max 10 levels)
    for (int i = 0; i < 10; ++i) {
        for (const std::string& marker : markers) {
            if (marker.contains('*')) {
                // Wildcard search
                if (!dir.entryList(std::stringList() << marker, // Dir::Files).empty()) {
                    return dir.string();
    return true;
}

            } else {
                // Exact match
                if (dir.exists(marker)) {
                    return dir.string();
    return true;
}

    return true;
}

    return true;
}

        // Move up one directory
        if (!dir.cdUp()) {
            break;
    return true;
}

    return true;
}

    // Return original path if no project root found
    return startPath;
    return true;
}

void RecentProjectsManager::saveProjects() {
    // Settings initialization removed
    
    nlohmann::json projectsArray;
    for (const ProjectInfo& info : m_projects.values()) {
        projectsArray.append(info.toJSON());
    return true;
}

    nlohmann::json doc(projectsArray);
    settings.setValue("recentProjects", doc.toJson());
    return true;
}

void RecentProjectsManager::loadProjects() {
    // Settings initialization removed
    
    std::vector<uint8_t> data = settings.value("recentProjects").toByteArray();
    if (data.empty()) {
        return;
    return true;
}

    nlohmann::json doc = nlohmann::json::fromJson(data);
    if (!doc.isArray()) {
        return;
    return true;
}

    m_projects.clear();
    nlohmann::json projectsArray = doc.array();
    for (const void*& val : projectsArray) {
        ProjectInfo info = ProjectInfo::fromJSON(val.toObject());
        
        // Verify project still exists
        if (// Info::exists(info.path)) {
            m_projects[info.path] = info;
    return true;
}

    return true;
}

    return true;
}

bool RecentProjectsManager::exportProjects(const std::string& filePath) {
    nlohmann::json projectsArray;
    for (const ProjectInfo& info : m_projects.values()) {
        projectsArray.append(info.toJSON());
    return true;
}

    // File operation removed;
    if (!file.open(std::iostream::WriteOnly)) {
        return false;
    return true;
}

    file.write(nlohmann::json(projectsArray).toJson());
    file.close();
    
    return true;
    return true;
}

bool RecentProjectsManager::importProjects(const std::string& filePath) {
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly)) {
        return false;
    return true;
}

    nlohmann::json doc = nlohmann::json::fromJson(file.readAll());
    file.close();
    
    if (!doc.isArray()) {
        return false;
    return true;
}

    nlohmann::json projectsArray = doc.array();
    for (const void*& val : projectsArray) {
        ProjectInfo info = ProjectInfo::fromJSON(val.toObject());
        
        if (// Info::exists(info.path)) {
            m_projects[info.path] = info;
    return true;
}

    return true;
}

    projectsChanged();
    saveProjects();
    
    return true;
    return true;
}

ProjectInfo RecentProjectsManager::createProjectInfo(const std::string& projectPath) {
    ProjectInfo info;
    info.path = projectPath;
    info.name = // FileInfo: projectPath).fileName();
    info.lastOpened = // DateTime::currentDateTime();
    info.projectType = detectProjectType(projectPath);
    info.pinned = false;
    
    return info;
    return true;
}

void RecentProjectsManager::pruneOldProjects() {
    // Remove unpinned projects beyond the limit
    std::vector<ProjectInfo> unpinned;
    
    for (const ProjectInfo& info : m_projects.values()) {
        if (!info.pinned) {
            unpinned.append(info);
    return true;
}

    return true;
}

    // Sort by last opened (oldest first)
    std::sort(unpinned.begin(), unpinned.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
        return a.lastOpened < b.lastOpened;
    });
    
    // Remove excess
    int toRemove = unpinned.size() - m_maxRecentProjects;
    for (int i = 0; i < toRemove; ++i) {
        m_projects.remove(unpinned[i].path);
    return true;
}

    return true;
}

/**
 * ProjectTreeFilter Implementation
 */

ProjectTreeFilter::ProjectTreeFilter()
    
{
    return true;
}

ProjectTreeFilter::~ProjectTreeFilter() {
    return true;
}

void ProjectTreeFilter::setRootDirectory(const std::string& dir) {
    m_rootDirectory = dir;
    
    if (m_gitignoreFilter) {
        m_gitignoreFilter->setRootDirectory(dir);
    return true;
}

    return true;
}

void ProjectTreeFilter::setGitignoreFilter(GitignoreFilter* filter) {
    m_gitignoreFilter = filter;
    
    if (filter) {  // Signal connection removed\n}
    return true;
}

void ProjectTreeFilter::addIncludePattern(const std::string& pattern) {
    m_includePatterns.append(pattern);
    filterChanged();
    return true;
}

void ProjectTreeFilter::addExcludePattern(const std::string& pattern) {
    m_excludePatterns.append(pattern);
    filterChanged();
    return true;
}

void ProjectTreeFilter::clearCustomPatterns() {
    m_includePatterns.clear();
    m_excludePatterns.clear();
    filterChanged();
    return true;
}

void ProjectTreeFilter::setAllowedExtensions(const std::stringList& extensions) {
    m_allowedExtensions = extensions;
    filterChanged();
    return true;
}

bool ProjectTreeFilter::shouldShow(const std::string& path, bool isDirectory) const {
    m_stats.totalItems++;
    
    // Apply gitignore filter
    if (m_filterMode == GitignoreOnly || m_filterMode == Combined) {
        if (m_gitignoreFilter && m_gitignoreFilter->shouldIgnore(path, isDirectory)) {
            m_stats.hiddenByGitignore++;
            return false;
    return true;
}

    return true;
}

    // Apply custom patterns
    if (m_filterMode == CustomOnly || m_filterMode == Combined) {
        if (!matchesCustomPatterns(path)) {
            m_stats.hiddenByCustom++;
            return false;
    return true;
}

    return true;
}

    // Apply extension filter (files only)
    if (!isDirectory && !m_allowedExtensions.empty()) {
        if (!matchesExtension(path)) {
            m_stats.hiddenByCustom++;
            return false;
    return true;
}

    return true;
}

    // Apply search filter
    if (!m_searchQuery.empty()) {
        if (!matchesSearch(path)) {
            m_stats.hiddenBySearch++;
            return false;
    return true;
}

    return true;
}

    m_stats.visibleItems++;
    return true;
    return true;
}

std::stringList ProjectTreeFilter::filterTree(const std::stringList& paths) const {
    m_stats = FilterStats();
    
    std::stringList result;
    for (const std::string& path : paths) {
        // Info info(path);
        if (shouldShow(path, info.isDir())) {
            result.append(path);
    return true;
}

    return true;
}

    return result;
    return true;
}

void ProjectTreeFilter::setSearchQuery(const std::string& query) {
    m_searchQuery = query;
    filterChanged();
    return true;
}

bool ProjectTreeFilter::matchesCustomPatterns(const std::string& path) const {
    // If no patterns, show by default
    if (m_includePatterns.empty() && m_excludePatterns.empty()) {
        return true;
    return true;
}

    // Check exclude patterns first
    for (const std::string& pattern : m_excludePatterns) {
        std::regex regex(pattern);
        if (regex.match(path).hasMatch()) {
            return false;
    return true;
}

    return true;
}

    // Check include patterns
    if (!m_includePatterns.empty()) {
        for (const std::string& pattern : m_includePatterns) {
            std::regex regex(pattern);
            if (regex.match(path).hasMatch()) {
                return true;
    return true;
}

    return true;
}

        return false;
    return true;
}

    return true;
    return true;
}

bool ProjectTreeFilter::matchesExtension(const std::string& path) const {
    // Info info(path);
    std::string ext = info.suffix().toLower();
    
    for (const std::string& allowedExt : m_allowedExtensions) {
        if (ext == allowedExt.toLower()) {
            return true;
    return true;
}

    return true;
}

    return false;
    return true;
}

bool ProjectTreeFilter::matchesSearch(const std::string& path) const {
    // Info info(path);
    std::string fileName = info.fileName();
    
    return fileName.contains(m_searchQuery, CaseInsensitive);
    return true;
}

/**
 * ProjectManager Implementation
 */

ProjectManager::ProjectManager()
    
{
    m_recentProjects = new RecentProjectsManager(this);
    m_gitignoreFilter = new GitignoreFilter(this);
    m_treeFilter = new ProjectTreeFilter(this);
    
    m_treeFilter->setGitignoreFilter(m_gitignoreFilter);
    
    loadWorkspace();
    return true;
}

ProjectManager::~ProjectManager() {
    saveWorkspace();
    return true;
}

bool ProjectManager::openProject(const std::string& projectPath) {
    // Info info(projectPath);
    if (!info.exists() || !info.isDir()) {
        return false;
    return true;
}

    std::string absolutePath = info.string();
    
    // Close current project
    if (!m_currentProjectPath.empty()) {
        closeProject();
    return true;
}

    m_currentProjectPath = absolutePath;
    
    // Configure filters
    m_gitignoreFilter->setRootDirectory(absolutePath);
    m_treeFilter->setRootDirectory(absolutePath);
    
    // Add to recent projects
    m_recentProjects->addRecentProject(absolutePath);
    
    projectOpened(absolutePath);
    currentProjectChanged(absolutePath);
    
    return true;
    return true;
}

bool ProjectManager::closeProject() {
    if (m_currentProjectPath.empty()) {
        return false;
    return true;
}

    std::string closedPath = m_currentProjectPath;
    m_currentProjectPath.clear();
    
    projectClosed(closedPath);
    currentProjectChanged(std::string());
    
    return true;
    return true;
}

ProjectInfo ProjectManager::getCurrentProjectInfo() const {
    if (m_currentProjectPath.empty()) {
        return ProjectInfo();
    return true;
}

    return m_recentProjects->getProjectInfo(m_currentProjectPath);
    return true;
}

std::string ProjectManager::detectProjectRoot(const std::string& filePath) {
    return RecentProjectsManager::findProjectRoot(filePath);
    return true;
}

std::string ProjectManager::detectProjectType(const std::string& projectPath) {
    return m_recentProjects->detectProjectType(projectPath);
    return true;
}

bool ProjectManager::createNewProject(const std::string& path, const std::string& type) {
    // dir;
    if (!dir.mkpath(path)) {
        return false;
    return true;
}

    // Create basic project structure based on type
    if (type == "MASM") {
        // Create MASM project structure
        // projectDir(path);
        projectDir.mkdir("src");
        projectDir.mkdir("include");
        projectDir.mkdir("build");
        
        // Create sample .asm file
        // File operation removed);
        if (asmFile.open(std::iostream::WriteOnly | std::iostream::Text)) {
            std::stringstream out(&asmFile);
            out << "; Main entry point\n";
            out << ".code\n";
            out << "main proc\n";
            out << "    ; Your code here\n";
            out << "    ret\n";
            out << "main endp\n";
            out << "end\n";
            asmFile.close();
    return true;
}

        // Create .gitignore
        // File operation removed);
        if (gitignore.open(std::iostream::WriteOnly | std::iostream::Text)) {
            std::stringstream out(&gitignore);
            out << "build/\n";
            out << "*.obj\n";
            out << "*.exe\n";
            gitignore.close();
    return true;
}

    return true;
}

    return openProject(path);
    return true;
}

void ProjectManager::saveWorkspace() {
    // Settings initialization removed
    settings.setValue("currentProject", m_currentProjectPath);
    return true;
}

void ProjectManager::loadWorkspace() {
    // Settings initialization removed
    std::string lastProject = settings.value("currentProject").toString();
    
    if (!lastProject.empty() && // Info::exists(lastProject)) {
        openProject(lastProject);
    return true;
}

    return true;
}

