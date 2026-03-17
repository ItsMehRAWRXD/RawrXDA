// intelligent_codebase_engine.cpp - Multi-Language Codebase Intelligence Engine
// Converted from Qt to pure C++17
#include "intelligent_codebase_engine.h"
#include "common/logger.hpp"
#include "common/file_utils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <regex>

namespace fs = std::filesystem;

IntelligentCodebaseEngine::IntelligentCodebaseEngine() {
    m_supportedExtensions = {
        ".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hxx",
        ".py", ".pyw",
        ".js", ".jsx", ".ts", ".tsx",
        ".java",
        ".rs",
        ".go",
        ".cs",
        ".rb"
    };
}

IntelligentCodebaseEngine::~IntelligentCodebaseEngine() {}

void IntelligentCodebaseEngine::scanProject(const std::string& projectPath) {
    std::cout << "[IntelligentCodebaseEngine] Scanning project: " << projectPath << std::endl;
    m_projectPath = projectPath;
    m_fileAnalyses.clear();
    m_stats = CodebaseStats{};

    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(projectPath, ec)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (m_supportedExtensions.count(ext)) {
            scanFile(entry.path().string());
        }
    }

    buildDependencyGraph();

    // Calculate stats
    for (const auto& [path, analysis] : m_fileAnalyses) {
        m_stats.totalFiles++;
        m_stats.totalLines += analysis.totalLines;
        m_stats.totalCodeLines += analysis.codeLines;
        m_stats.totalCommentLines += analysis.commentLines;
        m_stats.totalBlankLines += analysis.blankLines;
        m_stats.languageDistribution[analysis.language]++;
        m_stats.totalSymbols += static_cast<int>(analysis.symbols.size());
        m_stats.averageComplexity += analysis.complexity;
        for (const auto& sym : analysis.symbols) m_stats.symbolCounts[sym.type]++;
    }
    if (m_stats.totalFiles > 0)
        m_stats.averageComplexity /= m_stats.totalFiles;

    std::cout << "[IntelligentCodebaseEngine] Scan complete: " << m_stats.totalFiles
              << " files, " << m_stats.totalLines << " lines, "
              << m_stats.totalSymbols << " symbols" << std::endl;
    onProjectScanComplete.emit(projectPath);
}

void IntelligentCodebaseEngine::scanFile(const std::string& filePath) {
    std::string content = FileUtils::readFile(filePath);
    if (content.empty()) return;

    std::string language = detectLanguage(filePath);
    FileAnalysis analysis;

    if (language == "cpp" || language == "c") {
        analysis = parseCppFile(filePath, content);
    } else if (language == "python") {
        analysis = parsePythonFile(filePath, content);
    } else if (language == "javascript" || language == "typescript") {
        analysis = parseJavaScriptFile(filePath, content);
    } else if (language == "rust") {
        analysis = parseRustFile(filePath, content);
    } else if (language == "go") {
        analysis = parseGoFile(filePath, content);
    } else if (language == "java") {
        analysis = parseJavaFile(filePath, content);
    } else {
        analysis.filePath = filePath;
        analysis.language = language;
        analysis.totalLines = static_cast<int>(std::count(content.begin(), content.end(), '\n')) + 1;
        countLines(content, analysis.codeLines, analysis.commentLines, analysis.blankLines);
        analysis.analyzedAt = TimeUtils::now();
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_fileAnalyses[filePath] = analysis;
    onFileScanComplete.emit(filePath);
}

void IntelligentCodebaseEngine::rescanModifiedFiles() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [path, analysis] : m_fileAnalyses) {
        std::error_code ec;
        auto modTime = fs::last_write_time(path, ec);
        if (!ec) {
            // Re-scan if file was modified (simplified check)
            scanFile(path);
        }
    }
}

std::vector<CodeSymbol> IntelligentCodebaseEngine::findSymbol(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CodeSymbol> results;
    for (const auto& [path, analysis] : m_fileAnalyses) {
        for (const auto& sym : analysis.symbols) {
            if (sym.name == name || StringUtils::containsCI(sym.name, name)) {
                results.push_back(sym);
            }
        }
    }
    return results;
}

std::vector<CodeSymbol> IntelligentCodebaseEngine::findSymbolsInFile(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_fileAnalyses.find(filePath);
    return (it != m_fileAnalyses.end()) ? it->second.symbols : std::vector<CodeSymbol>{};
}

std::vector<CodeSymbol> IntelligentCodebaseEngine::findSymbolsByType(const std::string& type) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CodeSymbol> results;
    for (const auto& [path, analysis] : m_fileAnalyses) {
        for (const auto& sym : analysis.symbols) {
            if (sym.type == type) results.push_back(sym);
        }
    }
    return results;
}

CodeSymbol IntelligentCodebaseEngine::getSymbolDefinition(const std::string& name) const {
    auto symbols = findSymbol(name);
    return symbols.empty() ? CodeSymbol{} : symbols.front();
}

std::vector<CodeSearchResult> IntelligentCodebaseEngine::findReferences(const std::string& symbolName) const {
    return searchCode(symbolName);
}

std::vector<CodeSearchResult> IntelligentCodebaseEngine::searchCode(const std::string& query) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CodeSearchResult> results;
    for (const auto& [path, analysis] : m_fileAnalyses) {
        std::string content = FileUtils::readFile(path);
        std::istringstream stream(content);
        std::string line;
        int lineNum = 0;
        while (std::getline(stream, line)) {
            lineNum++;
            if (StringUtils::contains(line, query)) {
                CodeSearchResult result;
                result.filePath = path;
                result.lineNumber = lineNum;
                result.lineContent = StringUtils::trimmed(line);
                result.relevance = 1.0;
                results.push_back(result);
            }
        }
    }
    std::sort(results.begin(), results.end(),
              [](const CodeSearchResult& a, const CodeSearchResult& b) {
                  return a.relevance > b.relevance;
              });
    return results;
}

std::vector<CodeSearchResult> IntelligentCodebaseEngine::searchRegex(const std::string& pattern) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CodeSearchResult> results;
    std::regex rx;
    try { rx = std::regex(pattern); } catch (...) { return results; }
    for (const auto& [path, analysis] : m_fileAnalyses) {
        std::string content = FileUtils::readFile(path);
        std::istringstream stream(content);
        std::string line;
        int lineNum = 0;
        while (std::getline(stream, line)) {
            lineNum++;
            if (std::regex_search(line, rx)) {
                CodeSearchResult result;
                result.filePath = path;
                result.lineNumber = lineNum;
                result.lineContent = StringUtils::trimmed(line);
                result.relevance = 1.0;
                results.push_back(result);
            }
        }
    }
    return results;
}

std::vector<CodeSearchResult> IntelligentCodebaseEngine::searchByLanguage(
    const std::string& query, const std::string& language) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CodeSearchResult> results;
    for (const auto& [path, analysis] : m_fileAnalyses) {
        if (analysis.language != language) continue;
        std::string content = FileUtils::readFile(path);
        std::istringstream stream(content);
        std::string line;
        int lineNum = 0;
        while (std::getline(stream, line)) {
            lineNum++;
            if (StringUtils::contains(line, query)) {
                CodeSearchResult result;
                result.filePath = path;
                result.lineNumber = lineNum;
                result.lineContent = StringUtils::trimmed(line);
                result.relevance = 1.0;
                results.push_back(result);
            }
        }
    }
    return results;
}

std::vector<DependencyNode> IntelligentCodebaseEngine::getDependencyGraph() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dependencyGraph;
}

std::vector<std::string> IntelligentCodebaseEngine::getDependencies(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& node : m_dependencyGraph) {
        if (node.filePath == filePath) return node.dependsOn;
    }
    return {};
}

std::vector<std::string> IntelligentCodebaseEngine::getDependents(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& node : m_dependencyGraph) {
        if (node.filePath == filePath) return node.dependedBy;
    }
    return {};
}

std::vector<std::vector<std::string>> IntelligentCodebaseEngine::findCircularDependencies() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::vector<std::string>> cycles;
    // Simplified: detect direct circular deps
    for (const auto& node : m_dependencyGraph) {
        for (const auto& dep : node.dependsOn) {
            for (const auto& other : m_dependencyGraph) {
                if (other.filePath == dep) {
                    for (const auto& otherDep : other.dependsOn) {
                        if (otherDep == node.filePath) {
                            cycles.push_back({node.filePath, dep});
                        }
                    }
                }
            }
        }
    }
    return cycles;
}

CodebaseStats IntelligentCodebaseEngine::getCodebaseStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

FileAnalysis IntelligentCodebaseEngine::getFileAnalysis(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_fileAnalyses.find(filePath);
    return (it != m_fileAnalyses.end()) ? it->second : FileAnalysis{};
}

std::string IntelligentCodebaseEngine::detectLanguage(const std::string& filePath) const {
    std::string ext = fs::path(filePath).extension().string();
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".hpp" || ext == ".hxx") return "cpp";
    if (ext == ".c" || ext == ".h") return "c";
    if (ext == ".py" || ext == ".pyw") return "python";
    if (ext == ".js" || ext == ".jsx") return "javascript";
    if (ext == ".ts" || ext == ".tsx") return "typescript";
    if (ext == ".java") return "java";
    if (ext == ".rs") return "rust";
    if (ext == ".go") return "go";
    if (ext == ".cs") return "csharp";
    if (ext == ".rb") return "ruby";
    return "unknown";
}

std::vector<std::string> IntelligentCodebaseEngine::getProjectFiles() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> files;
    for (const auto& [path, _] : m_fileAnalyses) files.push_back(path);
    return files;
}

std::vector<std::string> IntelligentCodebaseEngine::getProjectFilesByLanguage(
    const std::string& language) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> files;
    for (const auto& [path, analysis] : m_fileAnalyses) {
        if (analysis.language == language) files.push_back(path);
    }
    return files;
}

// ── Language Parsers ────────────────────────────────────────────────────────

FileAnalysis IntelligentCodebaseEngine::parseCppFile(const std::string& filePath,
                                                      const std::string& content)
{
    FileAnalysis analysis;
    analysis.filePath = filePath;
    analysis.language = "cpp";
    analysis.totalLines = static_cast<int>(std::count(content.begin(), content.end(), '\n')) + 1;
    countLines(content, analysis.codeLines, analysis.commentLines, analysis.blankLines);
    analysis.analyzedAt = TimeUtils::now();

    // Extract includes
    std::regex includeRx(R"(#include\s*[<"]([^>"]+)[>"])");
    auto it = std::sregex_iterator(content.begin(), content.end(), includeRx);
    for (; it != std::sregex_iterator(); ++it) {
        analysis.imports.push_back((*it)[1].str());
    }

    // Extract classes/structs
    std::regex classRx(R"(\b(class|struct)\s+(\w+)\s*[:{])");
    it = std::sregex_iterator(content.begin(), content.end(), classRx);
    for (; it != std::sregex_iterator(); ++it) {
        CodeSymbol sym;
        sym.name = (*it)[2].str();
        sym.type = (*it)[1].str();
        sym.filePath = filePath;
        sym.lineNumber = static_cast<int>(std::count(content.begin(),
                                                       content.begin() + (*it).position(), '\n')) + 1;
        analysis.symbols.push_back(sym);
    }

    // Extract functions
    std::regex funcRx(R"(\b(\w[\w:]*)\s+(\w+)\s*\(([^)]*)\)\s*(?:const\s*)?[{;])");
    it = std::sregex_iterator(content.begin(), content.end(), funcRx);
    for (; it != std::sregex_iterator(); ++it) {
        std::string retType = (*it)[1].str();
        if (retType == "class" || retType == "struct" || retType == "if" ||
            retType == "while" || retType == "for" || retType == "switch") continue;
        CodeSymbol sym;
        sym.name = (*it)[2].str();
        sym.type = "function";
        sym.signature = (*it)[1].str() + " " + (*it)[2].str() + "(" + (*it)[3].str() + ")";
        sym.filePath = filePath;
        sym.lineNumber = static_cast<int>(std::count(content.begin(),
                                                       content.begin() + (*it).position(), '\n')) + 1;
        analysis.symbols.push_back(sym);
    }

    // Calculate cyclomatic complexity
    analysis.complexity = 1;
    analysis.complexity += StringUtils::count(content, "if ");
    analysis.complexity += StringUtils::count(content, "else if");
    analysis.complexity += StringUtils::count(content, "for ");
    analysis.complexity += StringUtils::count(content, "while ");
    analysis.complexity += StringUtils::count(content, "case ");
    analysis.complexity += StringUtils::count(content, "&&");
    analysis.complexity += StringUtils::count(content, "||");

    return analysis;
}

FileAnalysis IntelligentCodebaseEngine::parsePythonFile(const std::string& filePath,
                                                         const std::string& content)
{
    FileAnalysis analysis;
    analysis.filePath = filePath;
    analysis.language = "python";
    analysis.totalLines = static_cast<int>(std::count(content.begin(), content.end(), '\n')) + 1;
    countLines(content, analysis.codeLines, analysis.commentLines, analysis.blankLines);
    analysis.analyzedAt = TimeUtils::now();

    std::regex importRx(R"((?:from\s+(\S+)\s+)?import\s+(\S+))");
    auto it = std::sregex_iterator(content.begin(), content.end(), importRx);
    for (; it != std::sregex_iterator(); ++it) {
        analysis.imports.push_back((*it)[0].str());
    }

    std::regex classRx(R"(class\s+(\w+)\s*[\(:])", std::regex_constants::multiline);
    it = std::sregex_iterator(content.begin(), content.end(), classRx);
    for (; it != std::sregex_iterator(); ++it) {
        CodeSymbol sym;
        sym.name = (*it)[1].str(); sym.type = "class";
        sym.filePath = filePath;
        sym.lineNumber = static_cast<int>(std::count(content.begin(),
                                                       content.begin() + (*it).position(), '\n')) + 1;
        analysis.symbols.push_back(sym);
    }

    std::regex funcRx(R"(def\s+(\w+)\s*\(([^)]*)\))", std::regex_constants::multiline);
    it = std::sregex_iterator(content.begin(), content.end(), funcRx);
    for (; it != std::sregex_iterator(); ++it) {
        CodeSymbol sym;
        sym.name = (*it)[1].str(); sym.type = "function";
        sym.signature = "def " + (*it)[1].str() + "(" + (*it)[2].str() + ")";
        sym.filePath = filePath;
        sym.lineNumber = static_cast<int>(std::count(content.begin(),
                                                       content.begin() + (*it).position(), '\n')) + 1;
        analysis.symbols.push_back(sym);
    }

    analysis.complexity = 1 + StringUtils::count(content, "if ") + StringUtils::count(content, "elif ") +
                          StringUtils::count(content, "for ") + StringUtils::count(content, "while ") +
                          StringUtils::count(content, " and ") + StringUtils::count(content, " or ");
    return analysis;
}

FileAnalysis IntelligentCodebaseEngine::parseJavaScriptFile(const std::string& filePath,
                                                              const std::string& content)
{
    FileAnalysis analysis;
    analysis.filePath = filePath;
    analysis.language = StringUtils::endsWith(filePath, ".ts") ||
                        StringUtils::endsWith(filePath, ".tsx") ? "typescript" : "javascript";
    analysis.totalLines = static_cast<int>(std::count(content.begin(), content.end(), '\n')) + 1;
    countLines(content, analysis.codeLines, analysis.commentLines, analysis.blankLines);
    analysis.analyzedAt = TimeUtils::now();

    std::regex importRx(R"((?:import|require)\s*\(?['"]([\w./\-@]+)['"]\)?)");
    auto it = std::sregex_iterator(content.begin(), content.end(), importRx);
    for (; it != std::sregex_iterator(); ++it) analysis.imports.push_back((*it)[1].str());

    std::regex funcRx(R"((?:function|const|let|var)\s+(\w+)\s*(?:=\s*(?:async\s*)?\([^)]*\)\s*=>|\([^)]*\)\s*\{))");
    it = std::sregex_iterator(content.begin(), content.end(), funcRx);
    for (; it != std::sregex_iterator(); ++it) {
        CodeSymbol sym;
        sym.name = (*it)[1].str(); sym.type = "function";
        sym.filePath = filePath;
        sym.lineNumber = static_cast<int>(std::count(content.begin(),
                                                       content.begin() + (*it).position(), '\n')) + 1;
        analysis.symbols.push_back(sym);
    }

    std::regex classRx(R"(class\s+(\w+))");
    it = std::sregex_iterator(content.begin(), content.end(), classRx);
    for (; it != std::sregex_iterator(); ++it) {
        CodeSymbol sym;
        sym.name = (*it)[1].str(); sym.type = "class";
        sym.filePath = filePath;
        sym.lineNumber = static_cast<int>(std::count(content.begin(),
                                                       content.begin() + (*it).position(), '\n')) + 1;
        analysis.symbols.push_back(sym);
    }

    analysis.complexity = 1 + StringUtils::count(content, "if ") + StringUtils::count(content, "for ") +
                          StringUtils::count(content, "while ") + StringUtils::count(content, "&&") +
                          StringUtils::count(content, "||");
    return analysis;
}

FileAnalysis IntelligentCodebaseEngine::parseRustFile(const std::string& filePath,
                                                       const std::string& content)
{
    FileAnalysis analysis;
    analysis.filePath = filePath; analysis.language = "rust";
    analysis.totalLines = static_cast<int>(std::count(content.begin(), content.end(), '\n')) + 1;
    countLines(content, analysis.codeLines, analysis.commentLines, analysis.blankLines);
    analysis.analyzedAt = TimeUtils::now();

    std::regex funcRx(R"(fn\s+(\w+)\s*\()");
    auto it = std::sregex_iterator(content.begin(), content.end(), funcRx);
    for (; it != std::sregex_iterator(); ++it) {
        CodeSymbol sym; sym.name = (*it)[1].str(); sym.type = "function";
        sym.filePath = filePath;
        sym.lineNumber = static_cast<int>(std::count(content.begin(),
                                                       content.begin() + (*it).position(), '\n')) + 1;
        analysis.symbols.push_back(sym);
    }

    analysis.complexity = 1 + StringUtils::count(content, "if ") + StringUtils::count(content, "match ") +
                          StringUtils::count(content, "for ") + StringUtils::count(content, "while ");
    return analysis;
}

FileAnalysis IntelligentCodebaseEngine::parseGoFile(const std::string& filePath,
                                                     const std::string& content)
{
    FileAnalysis analysis;
    analysis.filePath = filePath; analysis.language = "go";
    analysis.totalLines = static_cast<int>(std::count(content.begin(), content.end(), '\n')) + 1;
    countLines(content, analysis.codeLines, analysis.commentLines, analysis.blankLines);
    analysis.analyzedAt = TimeUtils::now();

    std::regex funcRx(R"(func\s+(\w+)\s*\()");
    auto it = std::sregex_iterator(content.begin(), content.end(), funcRx);
    for (; it != std::sregex_iterator(); ++it) {
        CodeSymbol sym; sym.name = (*it)[1].str(); sym.type = "function";
        sym.filePath = filePath;
        sym.lineNumber = static_cast<int>(std::count(content.begin(),
                                                       content.begin() + (*it).position(), '\n')) + 1;
        analysis.symbols.push_back(sym);
    }
    analysis.complexity = 1 + StringUtils::count(content, "if ") + StringUtils::count(content, "for ") +
                          StringUtils::count(content, "switch ");
    return analysis;
}

FileAnalysis IntelligentCodebaseEngine::parseJavaFile(const std::string& filePath,
                                                       const std::string& content)
{
    FileAnalysis analysis;
    analysis.filePath = filePath; analysis.language = "java";
    analysis.totalLines = static_cast<int>(std::count(content.begin(), content.end(), '\n')) + 1;
    countLines(content, analysis.codeLines, analysis.commentLines, analysis.blankLines);
    analysis.analyzedAt = TimeUtils::now();

    std::regex classRx(R"(\b(?:public|private|protected)?\s*class\s+(\w+))");
    auto it = std::sregex_iterator(content.begin(), content.end(), classRx);
    for (; it != std::sregex_iterator(); ++it) {
        CodeSymbol sym; sym.name = (*it)[1].str(); sym.type = "class";
        sym.filePath = filePath;
        sym.lineNumber = static_cast<int>(std::count(content.begin(),
                                                       content.begin() + (*it).position(), '\n')) + 1;
        analysis.symbols.push_back(sym);
    }
    analysis.complexity = 1 + StringUtils::count(content, "if ") + StringUtils::count(content, "for ") +
                          StringUtils::count(content, "while ") + StringUtils::count(content, "case ");
    return analysis;
}

void IntelligentCodebaseEngine::buildDependencyGraph() {
    m_dependencyGraph.clear();
    for (const auto& [path, analysis] : m_fileAnalyses) {
        DependencyNode node;
        node.filePath = path;
        for (const auto& imp : analysis.imports) {
            for (const auto& [otherPath, _] : m_fileAnalyses) {
                if (StringUtils::contains(otherPath, imp) || StringUtils::endsWith(otherPath, imp)) {
                    node.dependsOn.push_back(otherPath);
                }
            }
        }
        m_dependencyGraph.push_back(node);
    }
    // Build reverse dependencies
    for (auto& node : m_dependencyGraph) {
        for (const auto& dep : node.dependsOn) {
            for (auto& other : m_dependencyGraph) {
                if (other.filePath == dep) {
                    other.dependedBy.push_back(node.filePath);
                }
            }
        }
    }
}

void IntelligentCodebaseEngine::countLines(const std::string& content,
                                            int& code, int& comment, int& blank)
{
    code = 0; comment = 0; blank = 0;
    std::istringstream stream(content);
    std::string line;
    bool inBlockComment = false;
    while (std::getline(stream, line)) {
        std::string trimmed = StringUtils::trimmed(line);
        if (trimmed.empty()) { blank++; continue; }
        if (inBlockComment) {
            comment++;
            if (trimmed.find("*/") != std::string::npos) inBlockComment = false;
            continue;
        }
        if (StringUtils::startsWith(trimmed, "//") || StringUtils::startsWith(trimmed, "#")) {
            comment++;
        } else if (StringUtils::startsWith(trimmed, "/*")) {
            comment++;
            if (trimmed.find("*/") == std::string::npos) inBlockComment = true;
        } else {
            code++;
        }
    }
}
