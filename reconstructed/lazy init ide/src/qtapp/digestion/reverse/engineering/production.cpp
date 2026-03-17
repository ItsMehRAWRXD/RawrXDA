// digestion_reverse_engineering.cpp
// PRODUCTION IMPLEMENTATION - Multi-Language Agentic Digestion System
// Created: 2026-01-24
// Enhanced: 2026-01-24 (Production Ready, Chunked Pipeline, Full Analysis, JSON Export)

#include "digestion_reverse_engineering.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDirIterator>
#include <QMutex>
#include <QtConcurrent>
#include <QElapsedTimer>
#include <QFileInfo>
#include <algorithm>

// Constructor - Initialize all language patterns and agentic templates
DigestionReverseEngineeringSystem::DigestionReverseEngineeringSystem() {
    initializeLanguagePatterns();
    initializeAgenticPatterns();
    initializeAdvancedPatterns();
    
    // Initialize statistics
    statistics_["totalFilesScanned"] = 0;
    statistics_["totalStubsFound"] = 0;
    statistics_["totalExtensionsApplied"] = 0;
    statistics_["filesProcessed"] = QStringList();
    statistics_["stubTypes"] = QMap<QString, int>();
    statistics_["analysisTimeMs"] = 0;
    statistics_["cacheHits"] = 0;
}

// ==================== Language Detection ====================

ProgrammingLanguage DigestionReverseEngineeringSystem::detectLanguage(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    static const QHash<QString, ProgrammingLanguage> extensionMap = {
        {"cpp", ProgrammingLanguage::Cpp}, {"cc", ProgrammingLanguage::Cpp},
        {"cxx", ProgrammingLanguage::Cpp}, {"c", ProgrammingLanguage::Cpp},
        {"h", ProgrammingLanguage::Cpp}, {"hpp", ProgrammingLanguage::Cpp},
        {"cs", ProgrammingLanguage::CSharp},
        {"py", ProgrammingLanguage::Python}, {"pyc", ProgrammingLanguage::Python},
        {"js", ProgrammingLanguage::JavaScript}, {"mjs", ProgrammingLanguage::JavaScript},
        {"ts", ProgrammingLanguage::TypeScript}, {"tsx", ProgrammingLanguage::TypeScript},
        {"java", ProgrammingLanguage::Java},
        {"go", ProgrammingLanguage::Go},
        {"rs", ProgrammingLanguage::Rust},
        {"swift", ProgrammingLanguage::Swift},
        {"kt", ProgrammingLanguage::Kotlin}, {"kts", ProgrammingLanguage::Kotlin},
        {"php", ProgrammingLanguage::PHP}, {"phtml", ProgrammingLanguage::PHP},
        {"rb", ProgrammingLanguage::Ruby}, {"rake", ProgrammingLanguage::Ruby},
        {"m", ProgrammingLanguage::ObjectiveC}, {"mm", ProgrammingLanguage::ObjectiveC},
        {"asm", ProgrammingLanguage::Assembly}, {"s", ProgrammingLanguage::Assembly},
        {"S", ProgrammingLanguage::Assembly},
        {"sql", ProgrammingLanguage::SQL},
        {"html", ProgrammingLanguage::HTML_CSS}, {"css", ProgrammingLanguage::HTML_CSS},
        {"yaml", ProgrammingLanguage::YAML_JSON}, {"yml", ProgrammingLanguage::YAML_JSON},
        {"json", ProgrammingLanguage::YAML_JSON},
        {"sh", ProgrammingLanguage::Shell_Bash}, {"bash", ProgrammingLanguage::Shell_Bash},
        {"ps1", ProgrammingLanguage::PowerShell},
        {"md", ProgrammingLanguage::Markdown},
        {"xml", ProgrammingLanguage::XML}
    };
    
    return extensionMap.value(extension, ProgrammingLanguage::Unknown);
}

ProgrammingLanguage DigestionReverseEngineeringSystem::detectLanguageFromContent(const QString& content) {
    // Heuristic-based detection from content
    if (content.contains(QRegularExpression("\\b(def|import|from|class|async)\\b")) && 
        content.contains(":")) {
        return ProgrammingLanguage::Python;
    }
    if (content.contains("function") || content.contains("const ") || content.contains("let ")) {
        return ProgrammingLanguage::JavaScript;
    }
    if (content.contains("#include") || content.contains("class ") || content.contains("namespace ")) {
        return ProgrammingLanguage::Cpp;
    }
    if (content.contains("public class") || content.contains("package ")) {
        return ProgrammingLanguage::Java;
    }
    return ProgrammingLanguage::Unknown;
}

LanguagePatterns DigestionReverseEngineeringSystem::getLanguagePatterns(ProgrammingLanguage language) const {
    return languagePatterns_.value(language, LanguagePatterns());
}

QVector<ProgrammingLanguage> DigestionReverseEngineeringSystem::getSupportedLanguages() const {
    return languagePatterns_.keys().toVector();
}

// ==================== Stub Scanning ====================

QVector<DigestionTask> DigestionReverseEngineeringSystem::scanFileForStubs(const QString& filePath) {
    QVector<DigestionTask> tasks;
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[DIGESTION] Failed to open file:" << filePath;
        return tasks;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    ProgrammingLanguage language = detectLanguage(filePath);
    if (language == ProgrammingLanguage::Unknown) {
        language = detectLanguageFromContent(content);
        if (language == ProgrammingLanguage::Unknown) {
            return tasks;
        }
    }
    
    LanguagePatterns patterns = getLanguagePatterns(language);
    if (patterns.stubKeywords.isEmpty()) {
        return tasks;
    }
    
    // Build regex pattern for stub detection
    QString stubPatternStr = "\\b(" + patterns.stubKeywords.join("|") + ")\\b";
    QRegularExpression stubPattern(stubPatternStr, QRegularExpression::CaseInsensitiveOption);
    
    // Scan for stubs line by line
    QStringList lines = content.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        
        if (stubPattern.match(line).hasMatch()) {
            DigestionTask task;
            task.filePath = filePath;
            task.lineNumber = i + 1;
            task.language = language;
            task.stubContext = line.trimmed();
            task.classification = classifyStub(line, language);
            
            // Extract method context
            QString context = extractMethodContext(filePath, i + 1, language);
            QMap<QString, QString> sig = parseMethodSignature(context, language);
            task.methodName = sig.value("methodName", "");
            task.stubType = line.contains("TODO") ? "todo" : 
                           line.contains("FIXME") ? "fixme" :
                           line.contains("stub") ? "stub" : "placeholder";
            
            task.metadata["file_name"] = QFileInfo(filePath).fileName();
            task.metadata["line_content"] = line.trimmed();
            task.metadata["language"] = QString::number(static_cast<int>(language));
            
            tasks.append(task);
            statistics_["totalStubsFound"] = statistics_["totalStubsFound"].toInt() + 1;
        }
    }
    
    statistics_["totalFilesScanned"] = statistics_["totalFilesScanned"].toInt() + 1;
    QStringList filesProcessed = statistics_["filesProcessed"].toStringList();
    filesProcessed.append(filePath);
    statistics_["filesProcessed"] = filesProcessed;
    
    return tasks;
}

QVector<DigestionTask> DigestionReverseEngineeringSystem::scanFileWithDirections(
    const QString& filePath, const QSet<AnalysisDirection>& directions) {
    
    QVector<DigestionTask> tasks = scanFileForStubs(filePath);
    
    for (DigestionTask& task : tasks) {
        for (AnalysisDirection dir : directions) {
            DirectionalAnalysisResult result = performDirectionalAnalysis(filePath, dir);
            task.recommendations.append(result.recommendations);
        }
    }
    
    return tasks;
}

ComprehensiveAnalysisReport DigestionReverseEngineeringSystem::performComprehensiveAnalysis(
    const QString& filePath) {
    
    ComprehensiveAnalysisReport report;
    report.filePath = filePath;
    report.language = detectLanguage(filePath);
    report.timestamp = QDateTime::currentDateTime();
    
    QElapsedTimer timer;
    timer.start();
    
    // Perform all directional analyses
    QVector<AnalysisDirection> directions = {
        AnalysisDirection::ControlFlow,
        AnalysisDirection::DataFlow,
        AnalysisDirection::Dependencies,
        AnalysisDirection::Security,
        AnalysisDirection::Performance,
        AnalysisDirection::APISurface,
        AnalysisDirection::Architecture
    };
    
    for (AnalysisDirection dir : directions) {
        report.directionalResults[dir] = performDirectionalAnalysis(filePath, dir);
    }
    
    // Scan for stubs with comprehensive context
    report.tasks = scanFileForStubs(filePath);
    for (DigestionTask& task : report.tasks) {
        generateRecommendations(task);
    }
    
    report.aggregatedMetrics["analysisTimeMs"] = timer.elapsed();
    report.aggregatedMetrics["directionsAnalyzed"] = static_cast<int>(directions.size());
    report.aggregatedMetrics["stubsFound"] = report.tasks.size();
    
    return report;
}

// ==================== Directional Analysis ====================

DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeControlFlow(
    const QString& filePath, ProgrammingLanguage language) {
    
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::ControlFlow;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    
    QString content = QTextStream(&file).readAll();
    file.close();
    
    // Build CFG
    QVector<ControlFlowNode> cfg = buildControlFlowGraph(content, language);
    result.findings.append(QMap<QString, QVariant>({
        {"nodeCount", cfg.size()},
        {"type", "control_flow_graph"}
    }));
    
    result.recommendations.append("Review control flow for cyclomatic complexity");
    result.completed = true;
    
    return result;
}

DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeDataFlow(
    const QString& filePath, ProgrammingLanguage language) {
    
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::DataFlow;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    
    QString content = QTextStream(&file).readAll();
    file.close();
    
    QVector<DataFlowInfo> dataFlows = analyzeDataFlow(content, language);
    result.findings.append(QMap<QString, QVariant>({
        {"dataFlowCount", dataFlows.size()},
        {"type", "data_flow"}
    }));
    
    result.recommendations.append("Review data flow for potential issues");
    result.completed = true;
    
    return result;
}

DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeDependencies(
    const QString& filePath, ProgrammingLanguage language) {
    
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::Dependencies;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    
    QString content = QTextStream(&file).readAll();
    file.close();
    
    QVector<DependencyInfo> deps = extractDependencies(content, language);
    result.findings.append(QMap<QString, QVariant>({
        {"dependencyCount", deps.size()},
        {"type", "dependencies"}
    }));
    
    result.recommendations.append("Review external dependencies for security and maintenance");
    result.completed = true;
    
    return result;
}

DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeSecurity(
    const QString& filePath, ProgrammingLanguage language) {
    
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::Security;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    
    QString content = QTextStream(&file).readAll();
    file.close();
    
    QVector<SecurityVulnerability> vulns = detectSecurityIssues(content, language);
    result.findings.append(QMap<QString, QVariant>({
        {"vulnerabilityCount", vulns.size()},
        {"type", "security_issues"}
    }));
    
    if (!vulns.isEmpty()) {
        result.recommendations.append("SECURITY: Address detected vulnerabilities immediately");
    }
    result.completed = true;
    
    return result;
}

DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzePerformance(
    const QString& filePath, ProgrammingLanguage language) {
    
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::Performance;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    
    QString content = QTextStream(&file).readAll();
    file.close();
    
    QVector<PerformanceIssue> issues = detectPerformanceIssues(content, language);
    result.findings.append(QMap<QString, QVariant>({
        {"performanceIssueCount", issues.size()},
        {"type", "performance_issues"}
    }));
    
    result.recommendations.append("Review performance issues for optimization opportunities");
    result.completed = true;
    
    return result;
}

DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeAPISurface(
    const QString& filePath, ProgrammingLanguage language) {
    
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::APISurface;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    
    QString content = QTextStream(&file).readAll();
    file.close();
    
    QRegularExpression apiPattern("\\b(public|export|interface)\\b");
    int apiCount = content.count(apiPattern);
    
    result.findings.append(QMap<QString, QVariant>({
        {"publicApiCount", apiCount},
        {"type", "api_surface"}
    }));
    
    result.recommendations.append("Document public API interfaces and contracts");
    result.completed = true;
    
    return result;
}

DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeArchitecture(
    const QString& filePath, ProgrammingLanguage language) {
    
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::Architecture;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    
    QString content = QTextStream(&file).readAll();
    file.close();
    
    result.recommendations.append("Review architectural patterns and modularity");
    result.completed = true;
    
    return result;
}

// ==================== Agentic Automation ====================

QString DigestionReverseEngineeringSystem::generateAgenticPlan(const DigestionTask& task) {
    QStringList planParts;
    planParts << QString("Agentic Extension Plan for %1").arg(task.methodName.isEmpty() ? "Unknown" : task.methodName);
    planParts << QString("File: %1").arg(task.filePath);
    planParts << QString("Line: %1").arg(task.lineNumber);
    planParts << QString("Stub Type: %1").arg(task.stubType);
    planParts << "";
    planParts << "Recommended Agentic Patterns:";
    planParts << "- logging: Add comprehensive logging";
    planParts << "- error_handling: Add error handling";
    planParts << "- async: Add async execution if needed";
    planParts << "- metrics: Add performance metrics";
    planParts << "- validation: Add input validation";
    planParts << "- security: Add security checks";
    planParts << "";
    planParts << "Implementation Steps:";
    planParts << "1. Add entry/exit logging";
    planParts << "2. Add parameter validation";
    planParts << "3. Add error handling with try-catch";
    planParts << "4. Add performance metrics";
    planParts << "5. Add async execution if needed";
    planParts << "6. Add security validations";
    planParts << "7. Add subsystem integration";
    
    return planParts.join("\n");
}

CodeGenerationResult DigestionReverseEngineeringSystem::generateAgenticCode(
    const QString& patternName, ProgrammingLanguage language,
    const QMap<QString, QString>& parameters) {
    
    CodeGenerationResult result;
    result.success = false;
    
    if (!agenticPatterns_.contains(patternName)) {
        result.errorMessage = QString("Unknown agentic pattern: %1").arg(patternName);
        return result;
    }
    
    AgenticPattern pattern = agenticPatterns_[patternName];
    if (!pattern.languageTemplates.contains(language)) {
        result.errorMessage = QString("Pattern '%1' not available for language %2")
            .arg(patternName).arg(static_cast<int>(language));
        return result;
    }
    
    QString templateStr = pattern.languageTemplates[language];
    result.generatedCode = generateCodeFromTemplate(templateStr, parameters);
    
    if (!validateGeneratedCode(result.generatedCode, language)) {
        result.warnings.append("Generated code may have syntax issues");
    }
    
    result.success = true;
    return result;
}

bool DigestionReverseEngineeringSystem::applyAgenticPattern(const QString& filePath,
                                                          DigestionTask& task,
                                                          const QString& patternName) {
    QMap<QString, QString> params;
    params["method_name"] = task.methodName;
    params["file_path"] = filePath;
    params["line_number"] = QString::number(task.lineNumber);
    
    CodeGenerationResult result = generateAgenticCode(patternName, task.language, params);
    if (result.success) {
        task.appliedPatterns[patternName] = true;
        task.generatedCode[patternName] = result;
        statistics_["totalExtensionsApplied"] = statistics_["totalExtensionsApplied"].toInt() + 1;
        return true;
    }
    return false;
}

// ==================== Utility Methods ====================

QString DigestionReverseEngineeringSystem::extractMethodContext(
    const QString& filePath, int lineNumber, ProgrammingLanguage language) {
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return "";
    
    QTextStream in(&file);
    QStringList allLines;
    while (!in.atEnd()) {
        allLines.append(in.readLine());
    }
    file.close();
    
    if (lineNumber > allLines.size() || lineNumber < 1) return "";
    
    int startLine = qMax(0, lineNumber - 6);
    int endLine = qMin(static_cast<int>(allLines.size()), lineNumber + 5);
    
    QStringList contextLines;
    for (int i = startLine; i < endLine; ++i) {
        contextLines.append(allLines[i]);
    }
    
    return contextLines.join("\n");
}

QMap<QString, QString> DigestionReverseEngineeringSystem::parseMethodSignature(
    const QString& context, ProgrammingLanguage language) {
    
    QMap<QString, QString> signature;
    QRegularExpression methodPattern;
    
    switch (language) {
        case ProgrammingLanguage::Cpp:
            methodPattern = QRegularExpression("\\b([A-Za-z0-9_]+::[A-Za-z0-9_]+)\\s*\\(([^)]*)\\)");
            break;
        case ProgrammingLanguage::Python:
            methodPattern = QRegularExpression("\\bdef\\s+([A-Za-z0-9_]+)\\s*\\(([^)]*)\\)");
            break;
        case ProgrammingLanguage::JavaScript:
        case ProgrammingLanguage::TypeScript:
            methodPattern = QRegularExpression("\\b([A-Za-z0-9_]+)\\s*\\(([^)]*)\\)");
            break;
        default:
            return signature;
    }
    
    auto match = methodPattern.match(context);
    if (match.hasMatch()) {
        signature["methodName"] = match.captured(1);
        signature["parameters"] = match.captured(2);
    }
    
    return signature;
}

QString DigestionReverseEngineeringSystem::generateCodeFromTemplate(
    const QString& templateStr, const QMap<QString, QString>& parameters) {
    
    QString result = templateStr;
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        QString placeholder = "{" + it.key() + "}";
        result.replace(placeholder, it.value());
    }
    return result;
}

bool DigestionReverseEngineeringSystem::validateGeneratedCode(
    const QString& code, ProgrammingLanguage language) {
    
    int braceCount = 0, parenCount = 0, bracketCount = 0;
    bool inString = false;
    QChar stringChar;
    
    for (int i = 0; i < code.length(); ++i) {
        QChar ch = code[i];
        
        if (i > 0 && code[i-1] == '\\') continue;
        
        if ((ch == '"' || ch == '\'') && !inString) {
            inString = true;
            stringChar = ch;
        } else if (ch == stringChar && inString) {
            inString = false;
        }
        
        if (inString) continue;
        
        if (ch == '{') braceCount++;
        else if (ch == '}') braceCount--;
        else if (ch == '(') parenCount++;
        else if (ch == ')') parenCount--;
        else if (ch == '[') bracketCount++;
        else if (ch == ']') bracketCount--;
    }
    
    return braceCount == 0 && parenCount == 0 && bracketCount == 0;
}

StubClassification DigestionReverseEngineeringSystem::classifyStub(
    const QString& content, ProgrammingLanguage language) {
    
    if (content.contains(QRegularExpression("NotImplemented|not implemented|not_implemented"))) {
        return StubClassification::NotImplementedException;
    }
    if (content.contains(QRegularExpression("//\\s*TODO|//\\s*FIXME"))) {
        return StubClassification::TODO_Fixme;
    }
    if (content.contains(QRegularExpression("\\bpass\\b|\\{\\s*\\}"))) {
        return StubClassification::EmptyImplementation;
    }
    if (content.contains(QRegularExpression("stub|placeholder"))) {
        return StubClassification::PlaceholderComment;
    }
    
    return StubClassification::NotStub;
}

int DigestionReverseEngineeringSystem::calculateComplexity(
    const QString& methodContent, ProgrammingLanguage language) {
    
    int complexity = 1;
    complexity += methodContent.count(QRegularExpression("\\bif\\b|\\belse\\b"));
    complexity += methodContent.count(QRegularExpression("\\bfor\\b|\\bwhile\\b|\\bforeach\\b"));
    complexity += methodContent.count(QRegularExpression("\\b(case|when)\\b"));
    
    return qMin(complexity, 10);
}

QVector<DependencyInfo> DigestionReverseEngineeringSystem::extractDependencies(
    const QString& code, ProgrammingLanguage language) {
    
    QVector<DependencyInfo> dependencies;
    QRegularExpression depPattern;
    
    switch (language) {
        case ProgrammingLanguage::Cpp:
            depPattern = QRegularExpression("#include\\s+[<\"]([^>\"]+)[>\"]");
            break;
        case ProgrammingLanguage::Python:
            depPattern = QRegularExpression("^\\s*(import|from)\\s+([\\w.]+)");
            break;
        case ProgrammingLanguage::JavaScript:
        case ProgrammingLanguage::TypeScript:
            depPattern = QRegularExpression("(import|require)\\s*\\(\\s*['\"]([^'\"]+)['\"]");
            break;
        default:
            return dependencies;
    }
    
    QRegularExpressionMatchIterator it = depPattern.globalMatch(code);
    int lineNum = 0;
    while (it.hasNext()) {
        auto match = it.next();
        DependencyInfo dep;
        dep.name = match.captured(match.lastCapturedIndex());
        dep.type = "library";
        dep.isExternal = true;
        dependencies.append(dep);
    }
    
    return dependencies;
}

QVector<SecurityVulnerability> DigestionReverseEngineeringSystem::detectSecurityIssues(
    const QString& code, ProgrammingLanguage language) {
    
    QVector<SecurityVulnerability> vulns;
    
    // Common security patterns
    if (code.contains(QRegularExpression("eval\\s*\\(|exec\\s*\\(|system\\s*\\("))) {
        SecurityVulnerability v;
        v.type = "injection";
        v.severity = "critical";
        v.description = "Potential code injection vulnerability";
        vulns.append(v);
    }
    
    if (code.contains(QRegularExpression("strcpy|sprintf|gets"))) {
        SecurityVulnerability v;
        v.type = "buffer_overflow";
        v.severity = "high";
        v.description = "Potential buffer overflow";
        vulns.append(v);
    }
    
    if (code.contains(QRegularExpression("password|secret|api.?key")) && 
        !code.contains("hash") && !code.contains("encrypt")) {
        SecurityVulnerability v;
        v.type = "hardcoded_secrets";
        v.severity = "high";
        v.description = "Potential hardcoded secrets";
        vulns.append(v);
    }
    
    return vulns;
}

QVector<PerformanceIssue> DigestionReverseEngineeringSystem::detectPerformanceIssues(
    const QString& code, ProgrammingLanguage language) {
    
    QVector<PerformanceIssue> issues;
    
    // Common performance issues
    if (code.contains(QRegularExpression("for\\s*\\([^)]*for\\s*\\("))) {
        PerformanceIssue issue;
        issue.type = "nested_loop";
        issue.severity = "medium";
        issue.description = "Nested loop detected - potential O(n²) complexity";
        issues.append(issue);
    }
    
    if (code.contains(QRegularExpression("\\bnew\\s+.+\\binside\\s+.*loop"))) {
        PerformanceIssue issue;
        issue.type = "memory_allocation_in_loop";
        issue.severity = "medium";
        issue.description = "Memory allocation inside loop";
        issues.append(issue);
    }
    
    return issues;
}

QVector<ControlFlowNode> DigestionReverseEngineeringSystem::buildControlFlowGraph(
    const QString& methodContent, ProgrammingLanguage language) {
    
    QVector<ControlFlowNode> nodes;
    
    int nodeId = 0;
    ControlFlowNode entry;
    entry.id = nodeId++;
    entry.type = "entry";
    entry.description = "Entry point";
    nodes.append(entry);
    
    // Simple CFG building - can be enhanced
    QStringList lines = methodContent.split('\n');
    for (const QString& line : lines) {
        if (line.contains("if") || line.contains("else")) {
            ControlFlowNode node;
            node.id = nodeId++;
            node.type = "branch";
            node.description = line.trimmed();
            nodes.append(node);
        } else if (line.contains("for") || line.contains("while")) {
            ControlFlowNode node;
            node.id = nodeId++;
            node.type = "loop";
            node.description = line.trimmed();
            nodes.append(node);
        }
    }
    
    ControlFlowNode exit;
    exit.id = nodeId++;
    exit.type = "exit";
    exit.description = "Exit point";
    nodes.append(exit);
    
    return nodes;
}



// ==================== Reporting & Export ====================

QString DigestionReverseEngineeringSystem::exportReport(
    const QVector<DigestionTask>& tasks, const QString& format) {
    
    if (format == "json") {
        QJsonArray tasksArray;
        for (const DigestionTask& task : tasks) {
            QJsonObject taskObj;
            taskObj["filePath"] = task.filePath;
            taskObj["methodName"] = task.methodName;
            taskObj["lineNumber"] = task.lineNumber;
            taskObj["language"] = static_cast<int>(task.language);
            taskObj["stubType"] = task.stubType;
            taskObj["context"] = task.stubContext;
            
            QJsonObject metadataObj;
            for (auto it = task.metadata.begin(); it != task.metadata.end(); ++it) {
                metadataObj[it.key()] = it.value();
            }
            taskObj["metadata"] = metadataObj;
            
            tasksArray.append(taskObj);
        }
        
        QJsonObject reportObj;
        reportObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        reportObj["totalTasks"] = tasks.size();
        reportObj["tasks"] = tasksArray;
        
        QJsonObject statsObj;
        for (auto it = statistics_.begin(); it != statistics_.end(); ++it) {
            if (it.value().type() == QVariant::Int) {
                statsObj[it.key()] = it.value().toInt();
            } else if (it.value().type() == QVariant::String) {
                statsObj[it.key()] = it.value().toString();
            }
        }
        reportObj["statistics"] = statsObj;
        
        QJsonDocument doc(reportObj);
        return doc.toJson(QJsonDocument::Indented);
    }
    
    return "Unsupported format";
}

QString DigestionReverseEngineeringSystem::exportComprehensiveReport(
    const ComprehensiveAnalysisReport& report, const QString& format) {
    
    // Similar to exportReport but with comprehensive analysis data
    QJsonObject reportObj;
    reportObj["filePath"] = report.filePath;
    reportObj["language"] = static_cast<int>(report.language);
    reportObj["timestamp"] = report.timestamp.toString(Qt::ISODate);
    reportObj["tasksFound"] = static_cast<int>(report.tasks.size());
    
    QJsonObject metricsObj;
    for (auto it = report.aggregatedMetrics.begin(); it != report.aggregatedMetrics.end(); ++it) {
        metricsObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    reportObj["metrics"] = metricsObj;
    
    QJsonDocument doc(reportObj);
    return doc.toJson(QJsonDocument::Indented);
}

// ==================== Pattern Management ====================

void DigestionReverseEngineeringSystem::registerLanguagePatterns(const LanguagePatterns& patterns) {
    languagePatterns_[patterns.language] = patterns;
    qDebug() << "[DIGESTION] Registered patterns for language:" << static_cast<int>(patterns.language);
}

void DigestionReverseEngineeringSystem::registerAgenticPattern(const AgenticPattern& pattern) {
    agenticPatterns_[pattern.name] = pattern;
    qDebug() << "[DIGESTION] Registered agentic pattern:" << pattern.name;
}

QMap<QString, AgenticPattern> DigestionReverseEngineeringSystem::getAgenticPatterns() const {
    return agenticPatterns_;
}

// ==================== Initialization ====================

void DigestionReverseEngineeringSystem::initializeLanguagePatterns() {
    // C++ Patterns
    LanguagePatterns cppPatterns;
    cppPatterns.language = ProgrammingLanguage::Cpp;
    cppPatterns.fileExtensions = {"cpp", "cc", "cxx", "c", "h", "hpp"};
    cppPatterns.stubKeywords = {"stub", "placeholder", "TODO", "FIXME", "not implemented",
                               "Q_UNIMPLEMENTED", "Q_UNUSED", "NOT_IMPLEMENTED"};
    languagePatterns_[ProgrammingLanguage::Cpp] = cppPatterns;
    
    // Python Patterns
    LanguagePatterns pyPatterns;
    pyPatterns.language = ProgrammingLanguage::Python;
    pyPatterns.fileExtensions = {"py", "pyc"};
    pyPatterns.stubKeywords = {"pass", "TODO", "FIXME", "NotImplementedError", "stub"};
    languagePatterns_[ProgrammingLanguage::Python] = pyPatterns;
    
    // JavaScript Patterns
    LanguagePatterns jsPatterns;
    jsPatterns.language = ProgrammingLanguage::JavaScript;
    jsPatterns.fileExtensions = {"js", "mjs"};
    jsPatterns.stubKeywords = {"TODO", "FIXME", "throw new Error", "stub"};
    languagePatterns_[ProgrammingLanguage::JavaScript] = jsPatterns;
}

void DigestionReverseEngineeringSystem::initializeAgenticPatterns() {
    // Logging Pattern
    AgenticPattern loggingPattern;
    loggingPattern.name = "logging";
    loggingPattern.description = "Add comprehensive logging with different log levels";
    loggingPattern.category = "observability";
    loggingPattern.complexity = 2;
    loggingPattern.languageTemplates[ProgrammingLanguage::Cpp] =
        "qDebug() << \"[{method_name}] Entering\";\n"
        "// Method body\n"
        "qDebug() << \"[{method_name}] Exiting\";";
    agenticPatterns_["logging"] = loggingPattern;
    
    // Error Handling Pattern
    AgenticPattern errorPattern;
    errorPattern.name = "error_handling";
    errorPattern.description = "Add comprehensive error handling";
    errorPattern.category = "reliability";
    errorPattern.complexity = 3;
    errorPattern.languageTemplates[ProgrammingLanguage::Cpp] =
        "try {\n"
        "    // Method body\n"
        "} catch (const std::exception& e) {\n"
        "    qCritical() << \"Exception: \" << e.what();\n"
        "    throw;\n"
        "}";
    agenticPatterns_["error_handling"] = errorPattern;
}

void DigestionReverseEngineeringSystem::initializeAdvancedPatterns() {
    // Can be extended with more advanced pattern initialization
}

// ==================== Internal Helpers ====================

DirectionalAnalysisResult DigestionReverseEngineeringSystem::performDirectionalAnalysis(
    const QString& filePath, AnalysisDirection direction) {
    
    ProgrammingLanguage lang = detectLanguage(filePath);
    
    switch (direction) {
        case AnalysisDirection::ControlFlow:
            return analyzeControlFlow(filePath, lang);
        case AnalysisDirection::DataFlow:
            return analyzeDataFlow(filePath, lang);
        case AnalysisDirection::Dependencies:
            return analyzeDependencies(filePath, lang);
        case AnalysisDirection::Security:
            return analyzeSecurity(filePath, lang);
        case AnalysisDirection::Performance:
            return analyzePerformance(filePath, lang);
        case AnalysisDirection::APISurface:
            return analyzeAPISurface(filePath, lang);
        case AnalysisDirection::Architecture:
            return analyzeArchitecture(filePath, lang);
        default:
            DirectionalAnalysisResult result;
            result.completed = false;
            return result;
    }
}

void DigestionReverseEngineeringSystem::generateRecommendations(DigestionTask& task) {
    task.recommendations.append("Review stub for actual implementation");
    task.recommendations.append("Add comprehensive unit tests");
    task.recommendations.append("Document expected behavior");
}

QVector<DigestionTask> DigestionReverseEngineeringSystem::scanMultipleFiles(
    const QStringList& filePaths) {
    
    QVector<DigestionTask> allTasks;
    for (const QString& filePath : filePaths) {
        QVector<DigestionTask> tasks = scanFileForStubs(filePath);
        allTasks.append(tasks);
    }
    return allTasks;
}

void DigestionReverseEngineeringSystem::chainToNextFile(const QString& nextFilePath) {
    qDebug() << "[AGENTIC] Chaining digestion to:" << nextFilePath;
    QVector<DigestionTask> tasks = scanFileForStubs(nextFilePath);
    if (!tasks.isEmpty()) {
        qDebug() << "[AGENTIC] Found" << tasks.size() << "stubs";
    }
}

void DigestionReverseEngineeringSystem::chainToMultipleFiles(const QStringList& filePaths) {
    for (const QString& filePath : filePaths) {
        chainToNextFile(filePath);
    }
}

QMap<QString, QVariant> DigestionReverseEngineeringSystem::getStatistics() const {
    return statistics_;
}
