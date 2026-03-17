#include "agentic/project_context.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <chrono>
#include <algorithm>

namespace fs = std::filesystem;

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

void ProjectContext::analyzeProject(const std::filesystem::path& projectRootPath)
{
    m_projectRootPath = projectRootPath;
    scanProjectFiles(projectRootPath);
    identifyDependencies(projectRootPath);
    recognizePatterns();
    inferCodeStyle();
    calculateMetrics();
    
    // Generate architecture overview
    m_architectureOverview = "{ \"projectRoot\": \"" + projectRootPath.string() + "\", " +
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

void ProjectContext::scanProjectFiles(const std::filesystem::path& projectRootPath)
{
    try {
        for (const auto& entry : fs::recursive_directory_iterator(projectRootPath)) {
            if (entry.is_regular_file() && isSourceFile(entry.path())) {
                analyzeFileContent(entry.path());
            }
        }
    } catch (const std::exception&) { }
}

void ProjectContext::analyzeFileContent(const std::filesystem::path& filePath)
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
        entity.filePath = filePath.string();
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
        entity.filePath = filePath.string();
        entity.lineNumber = std::count(content.begin(), content.begin() + match.position(), '\n') + 1;
        entity.returnType = match[1].str();
        entity.visibility = "public";
        m_codeEntities.push_back(entity);
        searchStart = match.suffix().first;
    }
}

void ProjectContext::identifyDependencies(const std::filesystem::path& projectRootPath)
{
    const std::vector<std::string> configFiles = {
        "CMakeLists.txt", "package.json", "requirements.txt"
    };
    
    for (const auto& cfgFile : configFiles) {
        auto cfgPath = projectRootPath / cfgFile;
        if (fs::exists(cfgPath)) {
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
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(m_projectRootPath)) {
            if (entry.is_regular_file() && (isSourceFile(entry.path()) || isConfigFile(entry.path()))) {
                totalFiles++;
                std::ifstream file(entry.path());
                if (file.is_open()) {
                    std::string line;
                    while (std::getline(file, line)) totalLines++;
                    file.close();
                }
            }
        }
    } catch (const std::exception&) { }
    
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

std::string ProjectContext::getFileExtension(const std::filesystem::path& p) const
{
    std::string ext = p.extension().string();
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return ext;
}

bool ProjectContext::isSourceFile(const std::filesystem::path& p) const
{
    std::string ext = getFileExtension(p);
    return ext == "cpp" || ext == "h" || ext == "hpp" || ext == "c" || ext == "cc" || ext == "cxx";
}

bool ProjectContext::isConfigFile(const std::filesystem::path& p) const
{
    std::string fileName = p.filename().string();
    std::transform(fileName.begin(), fileName.end(), fileName.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return fileName == "cmakelists.txt" || fileName == "package.json" || fileName == "requirements.txt" ||
           fileName == "pom.xml" || fileName == "gemfile" || fileName == "composer.json";
}
