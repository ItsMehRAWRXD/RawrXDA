#include "project_context.hpp"
#include <windows.h>
#include <shlwapi.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <chrono>
#include <algorithm>
#include <stack>
#include <vector>
#include <string>

ProjectContext::ProjectContext()
    : m_idCounter(0)
{
    // Initialize default code style
    m_codeStyle.indentationStyle = "spaces";
    m_codeStyle.indentationSize = 4;
    m_codeStyle.namingConvention = "camelCase";
    m_codeStyle.braceStyle = "K&R";
    m_codeStyle.useConstCorrectness = true;
    m_codeStyle.commentStyle = "//";
}

ProjectContext::~ProjectContext() = default;

void ProjectContext::analyzeProject(const std::string& projectRootPath)
{
    m_projectRootPath = projectRootPath;
    scanProjectFiles(projectRootPath);
    identifyDependencies(projectRootPath);
    recognizePatterns();
    inferCodeStyle();
    calculateMetrics();
    
    // Generate architecture overview
    m_architectureOverview = "{ \"projectRoot\": \"" + projectRootPath + "\", " +
                            "\"totalEntities\": " + std::to_string(m_codeEntities.size()) + " }";
    
    if (m_onAnalyzed)  { m_onAnalyzed(m_architectureOverview); }
    if (m_onEntities)  { m_onEntities(m_codeEntities); }
    if (m_onDependencies) { m_onDependencies(m_dependencies); }
    if (m_onPatterns) { m_onPatterns(m_patterns); }
}

const std::vector<CodeEntity>& ProjectContext::getCodeEntities() const
{ return m_codeEntities; }

const std::vector<ProjectDependency>& ProjectContext::getDependencies() const
{ return m_dependencies; }

const std::vector<ProjectPattern>& ProjectContext::getPatterns() const
{ return m_patterns; }

const CodeStyle& ProjectContext::getCodeStyle() const
{ return m_codeStyle; }

std::vector<std::string> ProjectContext::findRelatedEntities(const std::string& entityId) const
{
    auto it = m_entityRelations.find(entityId);
    return (it != m_entityRelations.end()) ? it->second : std::vector<std::string>{};
}

std::vector<std::string> ProjectContext::getFilesUsingDependency(const std::string& depName) const
{
    auto it = m_dependencyUsage.find(depName);
    return (it != m_dependencyUsage.end()) ? it->second : std::vector<std::string>{};
}

std::string ProjectContext::getProjectMetrics() const
{ return m_projectMetrics; }

std::vector<std::string> ProjectContext::getBestPracticeRecommendations() const
{
    std::vector<std::string> recommendations;
    if (m_codeStyle.indentationSize != 4) {
        recommendations.push_back("Standardize indentation to 4 spaces");
    }
    if (m_dependencies.size() > 20) {
        recommendations.push_back("Consider dependency consolidation");
    }
    if (m_patterns.empty()) {
        recommendations.push_back("Establish architectural patterns");
    }
    recommendations.push_back("Document all public APIs");
    recommendations.push_back("Add unit tests for critical components");
    return recommendations;
}

void ProjectContext::scanProjectFiles(const std::string& projectRootPath)
{
    std::stack<std::string> directories;
    directories.push(projectRootPath);

    while (!directories.empty()) {
        std::string currentDir = directories.top();
        directories.pop();

        std::string searchPath = currentDir + "\\*";

        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

        if (hFind == INVALID_HANDLE_VALUE) continue;

        do {
            std::string fileName = findData.cFileName;

            if (fileName == "." || fileName == "..") continue;

            std::string fullPath = currentDir + "\\" + fileName;

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                directories.push(fullPath);
            } else if (isSourceFile(fullPath)) {
                analyzeFileContent(fullPath);
            }
        } while (FindNextFileA(hFind, &findData));

        FindClose(hFind);
    }
}

void ProjectContext::analyzeFileContent(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) return;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    std::string content = buffer.str();
    
    // Find classes
    std::regex classRegex(R"((?:class|struct)\s+(\w+))");
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());
    
    while (std::regex_search(searchStart, content.cend(), match, classRegex)) {
        CodeEntity entity;
        entity.id = generateUniqueId();
        entity.type = "class";
        entity.name = match[1].str();
        entity.filePath = filePath;
        entity.lineNumber = std::count(content.begin(), content.begin() + match.position(), '\n') + 1;
        entity.visibility = "public";
        m_codeEntities.push_back(entity);
        searchStart = match.suffix().first;
    }
    
    // Find functions
    std::regex functionRegex(R"((\w+)\s+(\w+)\s*\([^)]*\)\s*\{)");
    searchStart = content.cbegin();
    
    while (std::regex_search(searchStart, content.cend(), match, functionRegex)) {
        CodeEntity entity;
        entity.id = generateUniqueId();
        entity.type = "function";
        entity.name = match[2].str();
        entity.filePath = filePath;
        entity.lineNumber = std::count(content.begin(), content.begin() + match.position(), '\n') + 1;
        entity.returnType = match[1].str();
        entity.visibility = "public";
        m_codeEntities.push_back(entity);
        searchStart = match.suffix().first;
    }
}

void ProjectContext::identifyDependencies(const std::string& projectRootPath)
{
    const std::vector<std::string> configFiles = {
        "CMakeLists.txt", "package.json", "requirements.txt"
    };
    
    for (const auto& cfgFile : configFiles) {
        std::string cfgPath = projectRootPath + "\\" + cfgFile;
        if (PathFileExistsA(cfgPath.c_str())) {
            std::ifstream file(cfgPath);
            if (file.is_open()) {
                ProjectDependency dep;
                dep.name = cfgFile;
                dep.source = "config";
                m_dependencies.push_back(dep);
                file.close();
            }
        }
    }
}

void ProjectContext::recognizePatterns()
{
    int singletonCount = 0;
    for (const auto& entity : m_codeEntities) {
        std::string lower_name = entity.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        if (lower_name.find("singleton") != std::string::npos) {
            singletonCount++;
        }
    }
    
    if (singletonCount > 0) {
        ProjectPattern pattern;
        pattern.patternId = generateUniqueId();
        pattern.patternType = "architectural";
        pattern.name = "Singleton Pattern";
        pattern.frequency = singletonCount;
        m_patterns.push_back(pattern);
    }
}

void ProjectContext::inferCodeStyle()
{
    // Default to spaces, 4-width
    m_codeStyle.indentationStyle = "spaces";
    m_codeStyle.indentationSize = 4;
    m_codeStyle.namingConvention = "camelCase";
}

void ProjectContext::calculateMetrics()
{
    int totalFiles = 0;
    int totalLines = 0;
    
    std::stack<std::string> directories;
    directories.push(m_projectRootPath);

    while (!directories.empty()) {
        std::string currentDir = directories.top();
        directories.pop();

        std::string searchPath = currentDir + "\\*";

        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

        if (hFind == INVALID_HANDLE_VALUE) continue;

        do {
            std::string fileName = findData.cFileName;

            if (fileName == "." || fileName == "..") continue;

            std::string fullPath = currentDir + "\\" + fileName;

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                directories.push(fullPath);
            } else if (isSourceFile(fullPath) || isConfigFile(fullPath)) {
                totalFiles++;
                std::ifstream file(fullPath);
                if (file.is_open()) {
                    std::string line;
                    while (std::getline(file, line)) totalLines++;
                    file.close();
                }
            }
        } while (FindNextFileA(hFind, &findData));

        FindClose(hFind);
    }
    
    int totalClasses = 0, totalFunctions = 0;
    for (const auto& entity : m_codeEntities) {
        if (entity.type == "class") totalClasses++;
        else if (entity.type == "function") totalFunctions++;
    }
    
    std::stringstream ss;
    ss << "{ \"files\": " << totalFiles << ", \"lines\": " << totalLines 
       << ", \"classes\": " << totalClasses << ", \"functions\": " << totalFunctions << " }";
    m_projectMetrics = ss.str();
}

std::string ProjectContext::generateUniqueId()
{ return std::to_string(m_idCounter++); }

std::string ProjectContext::getFileExtension(const std::string& p) const
{
    size_t dotPos = p.find_last_of('.');
    if (dotPos == std::string::npos) return "";
    std::string ext = p.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return ext;
}

bool ProjectContext::isSourceFile(const std::string& p) const
{
    std::string ext = getFileExtension(p);
    return ext == "cpp" || ext == "h" || ext == "hpp" || ext == "c" || ext == "cc" || ext == "cxx";
}

bool ProjectContext::isConfigFile(const std::string& p) const
{
    size_t slashPos = p.find_last_of('\\');
    std::string fileName = (slashPos != std::string::npos) ? p.substr(slashPos + 1) : p;
    std::transform(fileName.begin(), fileName.end(), fileName.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return fileName == "cmakelists.txt" || fileName == "package.json" || fileName == "requirements.txt" ||
           fileName == "pom.xml" || fileName == "gemfile" || fileName == "composer.json";
}
