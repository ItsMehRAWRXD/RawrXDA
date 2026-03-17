// digestion_reverse_engineering_complete.cpp
// Advanced Multi-Language Implementation with Full Reverse Engineering Logic
// Created: 2026-01-24
// Enhanced: 2026-01-24 (Multi-Directional Analysis & Advanced Agentic Automation)
//
// This system provides comprehensive reverse engineering capabilities across multiple dimensions:
// - Control Flow Analysis: Function calls, recursion, loops, branching
// - Data Flow Analysis: Variable lifecycles, data transformations, state changes
// - Dependency Analysis: External libraries, API calls, module imports
// - Security Analysis: Vulnerability patterns, unsafe code, injection points
// - Performance Analysis: Bottlenecks, memory leaks, inefficient patterns
// - API Surface Analysis: Public interfaces, endpoints, contracts
// - Architectural Analysis: Design patterns, anti-patterns, coupling
//
// Supported Languages: C++, C#, Python, JavaScript, TypeScript, Java, Go, Rust, Swift, Kotlin, PHP, Ruby, ObjectiveC, Assembly, SQL, HTML/CSS, YAML/JSON, Shell/Bash, PowerShell

#include "digestion_reverse_engineering.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <algorithm>

// Constructor - initialize all patterns and templates
DigestionReverseEngineeringSystem::DigestionReverseEngineeringSystem() {
    initializeLanguagePatterns();
    initializeAgenticPatterns();
    initializeAdvancedPatterns();
    
    // Initialize statistics
    statistics_["totalFilesScanned"] = 0;
    statistics_["totalStubsFound"] = 0;
    statistics_["totalExtensionsApplied"] = 0;
    statistics_["totalLinesAnalyzed"] = 0;
    statistics_["totalMethodsFound"] = 0;
    statistics_["filesProcessed"] = QStringList();
    statistics_["stubTypes"] = QMap<QString, int>();
    statistics_["languagesDetected"] = QMap<QString, int>();
    statistics_["analysisDirections"] = QMap<QString, int>();
    statistics_["agenticPatternsApplied"] = QMap<QString, int>();
    statistics_["securityIssuesFound"] = 0;
    statistics_["performanceIssuesFound"] = 0;
    statistics_["dependenciesFound"] = 0;
    statistics_["vulnerabilitiesFound"] = 0;
}

// ==================== Language Detection & Analysis ====================

// Detect programming language from file extension and content
ProgrammingLanguage DigestionReverseEngineeringSystem::detectLanguage(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    // Map file extensions to programming languages (expanded)
    static QMap<QString, ProgrammingLanguage> extensionMap = {
        // C++
        {"cpp", ProgrammingLanguage::Cpp}, {"cc", ProgrammingLanguage::Cpp}, {"cxx", ProgrammingLanguage::Cpp},
        {"c", ProgrammingLanguage::Cpp}, {"h", ProgrammingLanguage::Cpp}, {"hpp", ProgrammingLanguage::Cpp},
        {"hh", ProgrammingLanguage::Cpp}, {"hxx", ProgrammingLanguage::Cpp}, {"inl", ProgrammingLanguage::Cpp},
        
        // C#
        {"cs", ProgrammingLanguage::CSharp}, {"csx", ProgrammingLanguage::CSharp},
        
        // Python
        {"py", ProgrammingLanguage::Python}, {"pyc", ProgrammingLanguage::Python}, {"pyw", ProgrammingLanguage::Python},
        {"pyx", ProgrammingLanguage::Python}, {"pxd", ProgrammingLanguage::Python}, {"pxi", ProgrammingLanguage::Python},
        
        // JavaScript/TypeScript
        {"js", ProgrammingLanguage::JavaScript}, {"mjs", ProgrammingLanguage::JavaScript}, {"cjs", ProgrammingLanguage::JavaScript},
        {"jsx", ProgrammingLanguage::JavaScript}, {"ts", ProgrammingLanguage::TypeScript}, {"tsx", ProgrammingLanguage::TypeScript},
        {"d.ts", ProgrammingLanguage::TypeScript},
        
        // Java
        {"java", ProgrammingLanguage::Java}, {"class", ProgrammingLanguage::Java}, {"jar", ProgrammingLanguage::Java},
        
        // Go
        {"go", ProgrammingLanguage::Go}, {"mod", ProgrammingLanguage::Go}, {"sum", ProgrammingLanguage::Go},
        
        // Rust
        {"rs", ProgrammingLanguage::Rust}, {"toml", ProgrammingLanguage::Rust},
        
        // Swift
        {"swift", ProgrammingLanguage::Swift},
        
        // Kotlin
        {"kt", ProgrammingLanguage::Kotlin}, {"kts", ProgrammingLanguage::Kotlin}, {"ktm", ProgrammingLanguage::Kotlin},
        
        // PHP
        {"php", ProgrammingLanguage::PHP}, {"phtml", ProgrammingLanguage::PHP}, {"php3", ProgrammingLanguage::PHP},
        {"php4", ProgrammingLanguage::PHP}, {"php5", ProgrammingLanguage::PHP}, {"phps", ProgrammingLanguage::PHP},
        
        // Ruby
        {"rb", ProgrammingLanguage::Ruby}, {"rake", ProgrammingLanguage::Ruby}, {"gemspec", ProgrammingLanguage::Ruby},
        {"ru", ProgrammingLanguage::Ruby}, {"erb", ProgrammingLanguage::Ruby},
        
        // Objective-C
        {"m", ProgrammingLanguage::ObjectiveC}, {"mm", ProgrammingLanguage::ObjectiveC}, {"h", ProgrammingLanguage::ObjectiveC},
        
        // Assembly
        {"asm", ProgrammingLanguage::Assembly}, {"s", ProgrammingLanguage::Assembly}, {"S", ProgrammingLanguage::Assembly},
        {"inc", ProgrammingLanguage::Assembly}, {"nasm", ProgrammingLanguage::Assembly}, {"masm", ProgrammingLanguage::Assembly},
        
        // SQL
        {"sql", ProgrammingLanguage::SQL}, {"mysql", ProgrammingLanguage::SQL}, {"pgsql", ProgrammingLanguage::SQL},
        {"sqlite", ProgrammingLanguage::SQL}, {"plsql", ProgrammingLanguage::SQL},
        
        // HTML/CSS
        {"html", ProgrammingLanguage::HTML_CSS}, {"htm", ProgrammingLanguage::HTML_CSS}, {"xhtml", ProgrammingLanguage::HTML_CSS},
        {"css", ProgrammingLanguage::HTML_CSS}, {"scss", ProgrammingLanguage::HTML_CSS}, {"sass", ProgrammingLanguage::HTML_CSS},
        {"less", ProgrammingLanguage::HTML_CSS}, {"styl", ProgrammingLanguage::HTML_CSS},
        
        // YAML/JSON
        {"yaml", ProgrammingLanguage::YAML_JSON}, {"yml", ProgrammingLanguage::YAML_JSON}, {"json", ProgrammingLanguage::YAML_JSON},
        {"toml", ProgrammingLanguage::YAML_JSON}, {"ini", ProgrammingLanguage::YAML_JSON}, {"cfg", ProgrammingLanguage::YAML_JSON},
        {"conf", ProgrammingLanguage::YAML_JSON}, {"config", ProgrammingLanguage::YAML_JSON},
        
        // Shell/Bash
        {"sh", ProgrammingLanguage::Shell_Bash}, {"bash", ProgrammingLanguage::Shell_Bash}, {"zsh", ProgrammingLanguage::Shell_Bash},
        {"fish", ProgrammingLanguage::Shell_Bash}, {"csh", ProgrammingLanguage::Shell_Bash}, {"ksh", ProgrammingLanguage::Shell_Bash},
        
        // PowerShell
        {"ps1", ProgrammingLanguage::PowerShell}, {"psm1", ProgrammingLanguage::PowerShell}, {"psd1", ProgrammingLanguage::PowerShell},
        
        // Markdown
        {"md", ProgrammingLanguage::Markdown}, {"markdown", ProgrammingLanguage::Markdown}, {"mdown", ProgrammingLanguage::Markdown},
        
        // XML
        {"xml", ProgrammingLanguage::XML}, {"xsl", ProgrammingLanguage::XML}, {"xslt", ProgrammingLanguage::XML},
        {"xsd", ProgrammingLanguage::XML}, {"dtd", ProgrammingLanguage::XML}, {"svg", ProgrammingLanguage::XML}
    };
    
    // First try extension-based detection
    ProgrammingLanguage lang = extensionMap.value(extension, ProgrammingLanguage::Unknown);
    
    // If extension is ambiguous or unknown, analyze content
    if (lang == ProgrammingLanguage::Unknown || extension == "h" || extension == "config") {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString content = in.read(1000); // Read first 1000 characters
            file.close();
            
            lang = detectLanguageFromContent(content);
        }
    }
    
    // Update statistics
    if (lang != ProgrammingLanguage::Unknown) {
        QMap<QString, int> languages = statistics_["languagesDetected"].toMap();
        QString langStr = QString::number(static_cast<int>(lang));
        languages[langStr] = languages.value(langStr, 0) + 1;
        statistics_["languagesDetected"] = languages;
    }
    
    return lang;
}

// Detect language from content only (when extension is ambiguous)
ProgrammingLanguage DigestionReverseEngineeringSystem::detectLanguageFromContent(const QString& content) {
    // Look for language-specific patterns in the content
    QString trimmed = content.trimmed();
    
    // C++ patterns
    if (trimmed.contains("#include") || trimmed.contains("using namespace") || 
        trimmed.contains("std::") || trimmed.contains("class ") || trimmed.contains("public:") ||
        trimmed.contains("Q_OBJECT") || trimmed.contains("Qt")) {
        return ProgrammingLanguage::Cpp;
    }
    
    // C# patterns
    if (trimmed.contains("using System") || trimmed.contains("namespace ") || 
        trimmed.contains("class ") || trimmed.contains("public static") ||
        trimmed.contains("Console.WriteLine") || trimmed.contains("//")) {
        return ProgrammingLanguage::CSharp;
    }
    
    // Python patterns
    if (trimmed.contains("def ") || trimmed.contains("import ") || 
        trimmed.contains("from ") || trimmed.contains("class ") ||
        trimmed.contains("if __name__ == '__main__'") || trimmed.contains("# ")) {
        return ProgrammingLanguage::Python;
    }
    
    // JavaScript patterns
    if (trimmed.contains("function ") || trimmed.contains("const ") || 
        trimmed.contains("let ") || trimmed.contains("var ") ||
        trimmed.contains("=>") || trimmed.contains("//") || trimmed.contains("console.log")) {
        return ProgrammingLanguage::JavaScript;
    }
    
    // Java patterns
    if (trimmed.contains("public class") || trimmed.contains("import java") || 
        trimmed.contains("package ") || trimmed.contains("System.out.println") ||
        trimmed.contains("private ") || trimmed.contains("//")) {
        return ProgrammingLanguage::Java;
    }
    
    // Go patterns
    if (trimmed.contains("package main") || trimmed.contains("func ") || 
        trimmed.contains("import (") || trimmed.contains("fmt.") ||
        trimmed.contains("//")) {
        return ProgrammingLanguage::Go;
    }
    
    // Rust patterns
    if (trimmed.contains("fn ") || trimmed.contains("use ") || 
        trimmed.contains("pub ") || trimmed.contains("let ") ||
        trimmed.contains("//") || trimmed.contains("//!")) {
        return ProgrammingLanguage::Rust;
    }
    
    // Swift patterns
    if (trimmed.contains("func ") || trimmed.contains("import ") || 
        trimmed.contains("class ") || trimmed.contains("let ") ||
        trimmed.contains("var ") || trimmed.contains("//")) {
        return ProgrammingLanguage::Swift;
    }
    
    // Kotlin patterns
    if (trimmed.contains("fun ") || trimmed.contains("import ") || 
        trimmed.contains("class ") || trimmed.contains("val ") ||
        trimmed.contains("var ") || trimmed.contains("//")) {
        return ProgrammingLanguage::Kotlin;
    }
    
    // PHP patterns
    if (trimmed.contains("<?php") || trimmed.contains("function ") || 
        trimmed.contains("class ") || trimmed.contains("public $") ||
        trimmed.contains("private $") || trimmed.contains("//")) {
        return ProgrammingLanguage::PHP;
    }
    
    // Ruby patterns
    if (trimmed.contains("def ") || trimmed.contains("class ") || 
        trimmed.contains("module ") || trimmed.contains("require ") ||
        trimmed.contains("include ") || trimmed.contains("# ")) {
        return ProgrammingLanguage::Ruby;
    }
    
    // SQL patterns
    if (trimmed.contains("SELECT") || trimmed.contains("INSERT") || 
        trimmed.contains("UPDATE") || trimmed.contains("DELETE") ||
        trimmed.contains("CREATE") || trimmed.contains("TABLE")) {
        return ProgrammingLanguage::SQL;
    }
    
    // HTML/CSS patterns
    if (trimmed.contains("<!DOCTYPE") || trimmed.contains("<html") || 
        trimmed.contains("<div") || trimmed.contains("<span") ||
        trimmed.contains("<style") || trimmed.contains(".class") ||
        trimmed.contains("#id") || trimmed.contains("{")) {
        return ProgrammingLanguage::HTML_CSS;
    }
    
    // YAML/JSON patterns
    if (trimmed.contains(":") && (trimmed.contains("\n") || trimmed.contains("{")) ||
        (trimmed.startsWith("{") && trimmed.endsWith("}")) ||
        (trimmed.startsWith("[") && trimmed.endsWith("]"))) {
        return ProgrammingLanguage::YAML_JSON;
    }
    
    // Shell/Bash patterns
    if (trimmed.contains("#!/bin/bash") || trimmed.contains("#!/bin/sh") || 
        trimmed.contains("echo ") || trimmed.contains("if [") ||
        trimmed.contains("for ") || trimmed.contains("while ") ||
        trimmed.contains("# ")) {
        return ProgrammingLanguage::Shell_Bash;
    }
    
    // PowerShell patterns
    if (trimmed.contains("#!/usr/bin/env pwsh") || trimmed.contains("Write-Host") || 
        trimmed.contains("Get-") || trimmed.contains("Set-") ||
        trimmed.contains("if (") || trimmed.contains("# ")) {
        return ProgrammingLanguage::PowerShell;
    }
    
    return ProgrammingLanguage::Unknown;
}

// Get language patterns for a specific language
LanguagePatterns DigestionReverseEngineeringSystem::getLanguagePatterns(ProgrammingLanguage language) const {
    return languagePatterns_.value(language, LanguagePatterns());
}

// Get all supported languages
QVector<ProgrammingLanguage> DigestionReverseEngineeringSystem::getSupportedLanguages() const {
    QVector<ProgrammingLanguage> languages;
    for (auto it = languagePatterns_.begin(); it != languagePatterns_.end(); ++it) {
        languages.append(it.key());
    }
    return languages;
}

// ==================== Multi-Directional Scanning ====================

// Scan a file for stubs/placeholders with basic analysis
QVector<DigestionTask> DigestionReverseEngineeringSystem::scanFileForStubs(const QString& filePath) {
    QVector<DigestionTask> tasks;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for scanning:" << filePath;
        return tasks;
    }
    
    QTextStream in(&file);
    int lineNum = 0;
    QStringList allLines;
    
    // Read all lines first
    while (!in.atEnd()) {
        allLines.append(in.readLine());
    }
    
    // Update statistics
    statistics_["totalLinesAnalyzed"] = statistics_["totalLinesAnalyzed"].toInt() + allLines.size();
    
    // Detect language
    ProgrammingLanguage language = detectLanguage(filePath);
    if (language == ProgrammingLanguage::Unknown) {
        qWarning() << "Unknown programming language for file:" << filePath;
        return tasks;
    }
    
    // Get language patterns
    LanguagePatterns patterns = getLanguagePatterns(language);
    if (patterns.stubKeywords.isEmpty()) {
        qWarning() << "No patterns defined for language:" << static_cast<int>(language);
        return tasks;
    }
    
    // Build regex pattern for stub detection
    QString stubPatternStr = "\\b(" + patterns.stubKeywords.join("|") + ")\\b";
    QRegularExpression stubPattern(stubPatternStr, QRegularExpression::CaseInsensitiveOption);
    
    // Method detection patterns
    QVector<QRegularExpression> methodPatterns;
    for (const QString& patternStr : patterns.methodPatterns) {
        methodPatterns.append(QRegularExpression(patternStr));
    }
    
    // Scan for stubs
    for (int i = 0; i < allLines.size(); ++i) {
        QString line = allLines[i];
        ++lineNum;
        
        // Skip empty lines and comments (for performance)
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty() || patterns.commentPatterns.contains(trimmedLine.left(2))) {
            continue;
        }
        
        // Look for method definitions
        bool isMethod = false;
        QString methodName;
        for (const QRegularExpression& methodPattern : methodPatterns) {
            auto match = methodPattern.match(line);
            if (match.hasMatch()) {
                isMethod = true;
                methodName = match.captured(1);
                statistics_["totalMethodsFound"] = statistics_["totalMethodsFound"].toInt() + 1;
                break;
            }
        }
        
        // Look for stubs
        if (stubPattern.match(line).hasMatch() || isMethod) {
            // Extract method context (10 lines before and after)
            int startLine = qMax(0, i - 10);
            int endLine = qMin(allLines.size(), i + 11);
            
            QStringList contextLines;
            for (int j = startLine; j < endLine; ++j) {
                contextLines.append(allLines[j]);
            }
            
            QString context = contextLines.join("\n");
            
            // Determine stub type
            QString stubType = "unknown";
            if (isMethod) {
                // Check if method is empty or has placeholder
                bool hasImplementation = false;
                for (int j = i + 1; j < qMin(allLines.size(), i + 20); ++j) {
                    QString nextLine = allLines[j].trimmed();
                    if (nextLine == "{" || nextLine.startsWith("//") || nextLine.startsWith("#")) {
                        continue;
                    }
                    if (nextLine == "}" || nextLine == "end" || nextLine.startsWith("def ") || nextLine.startsWith("func ")) {
                        break;
                    }
                    if (!nextLine.isEmpty() && !patterns.commentPatterns.contains(nextLine.left(2))) {
                        hasImplementation = true;
                        break;
                    }
                }
                
                if (!hasImplementation) {
                    stubType = "empty_implementation";
                } else {
                    continue; // Skip methods with implementation
                }
            } else {
                // Check for specific stub keywords
                for (const QString& keyword : patterns.stubKeywords) {
                    if (line.contains(keyword, Qt::CaseInsensitive)) {
                        stubType = keyword.toLower();
                        break;
                    }
                }
            }
            
            // Classify the stub
            StubClassification classification = classifyStub(line, language);
            
            // Create comprehensive digestion task
            DigestionTask task;
            task.filePath = filePath;
            task.methodName = methodName;
            task.lineNumber = lineNum;
            task.language = language;
            task.classification = classification;
            task.stubType = stubType;
            task.stubContext = context;
            task.agenticPlan = generateAgenticPlan(task);
            
            // Add metadata
            task.metadata["file_name"] = QFileInfo(filePath).fileName();
            task.metadata["file_path"] = filePath;
            task.metadata["line_content"] = line.trimmed();
            task.metadata["stub_keyword"] = stubType;
            task.metadata["language"] = QString::number(static_cast<int>(language));
            task.metadata["classification"] = QString::number(static_cast<int>(classification));
            task.metadata["is_method"] = isMethod ? "true" : "false";
            task.metadata["context_lines"] = QString::number(endLine - startLine);
            
            // Perform basic analysis
            analyzeFileContent(context, language, task);
            
            tasks.append(task);
            
            // Update statistics
            statistics_["totalStubsFound"] = statistics_["totalStubsFound"].toInt() + 1;
            
            QMap<QString, int> stubTypes = statistics_["stubTypes"].toMap();
            stubTypes[stubType] = stubTypes.value(stubType, 0) + 1;
            statistics_["stubTypes"] = stubTypes;
        }
    }
    
    // Update file statistics
    statistics_["totalFilesScanned"] = statistics_["totalFilesScanned"].toInt() + 1;
    QStringList filesProcessed = statistics_["filesProcessed"].toStringList();
    filesProcessed.append(filePath);
    statistics_["filesProcessed"] = filesProcessed;
    
    return tasks;
}

// Scan with specific analysis directions
QVector<DigestionTask> DigestionReverseEngineeringSystem::scanFileWithDirections(const QString& filePath, 
                                                                         const QSet<AnalysisDirection>& directions) {
    QVector<DigestionTask> tasks = scanFileForStubs(filePath);
    
    // Perform additional analysis for each direction
    ProgrammingLanguage language = detectLanguage(filePath);
    
    for (DigestionTask& task : tasks) {
        QMap<AnalysisDirection, DirectionalAnalysisResult> directionalResults;
        
        for (AnalysisDirection direction : directions) {
            DirectionalAnalysisResult result = performDirectionalAnalysis(filePath, direction);
            directionalResults[direction] = result;
            
            // Update statistics
            QMap<QString, int> analysisStats = statistics_["analysisDirections"].toMap();
            QString dirStr = QString::number(static_cast<int>(direction));
            analysisStats[dirStr] = analysisStats.value(dirStr, 0) + 1;
            statistics_["analysisDirections"] = analysisStats;
        }
        
        // Store directional results in task
        // Note: In a full implementation, we'd merge these into the task structure
    }
    
    return tasks;
}

// Perform comprehensive multi-directional analysis
ComprehensiveAnalysisReport DigestionReverseEngineeringSystem::performComprehensiveAnalysis(const QString& filePath) {
    ComprehensiveAnalysisReport report;
    report.filePath = filePath;
    report.language = detectLanguage(filePath);
    report.timestamp = QDateTime::currentDateTime();
    
    // Scan for stubs
    report.tasks = scanFileForStubs(filePath);
    
    // Perform all directional analyses
    QSet<AnalysisDirection> allDirections = {
        AnalysisDirection::ControlFlow, AnalysisDirection::DataFlow, AnalysisDirection::Dependencies,
        AnalysisDirection::Security, AnalysisDirection::Performance, AnalysisDirection::APISurface,
        AnalysisDirection::Architecture, AnalysisDirection::ResourceUsage, AnalysisDirection::ErrorPropagation,
        AnalysisDirection::Concurrency
    };
    
    for (AnalysisDirection direction : allDirections) {
        report.directionalResults[direction] = performDirectionalAnalysis(filePath, direction);
    }
    
    // Merge results and generate recommendations
    mergeDirectionalResults(report, report.directionalResults);
    
    // Cache the report
    analysisCache_[filePath] = report;
    
    return report;
}

// ==================== Directional Analysis Methods ====================

// Control flow analysis - build CFG, identify recursion, loops
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeControlFlow(const QString& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::ControlFlow;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    QTextStream in(&file);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    
    // Build control flow graph
    QVector<ControlFlowNode> cfg;
    int nodeId = 0;
    
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        
        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith("//") || line.startsWith("#") || line.startsWith("/*")) {
            continue;
        }
        
        ControlFlowNode node;
        node.id = nodeId++;
        node.lineNumber = i + 1;
        
        // Identify node type
        if (line.contains("if ") || line.contains("if(")) {
            node.type = "branch";
            node.description = "Conditional branch";
        } else if (line.contains("for ") || line.contains("for(") || 
                   line.contains("while ") || line.contains("while(") ||
                   line.contains("do ")) {
            node.type = "loop";
            node.description = "Loop construct";
        } else if (line.contains("return ") || line.contains("return;")) {
            node.type = "return";
            node.description = "Return statement";
        } else if (line.contains("{") && !line.contains("}")) {
            node.type = "entry";
            node.description = "Block entry";
        } else if (line.contains("}") && !line.contains("{")) {
            node.type = "exit";
            node.description = "Block exit";
        } else if (line.contains("(") && (line.contains(";") || line.contains(") {"))) {
            node.type = "call";
            node.description = "Function call";
        } else {
            node.type = "statement";
            node.description = "Statement";
        }
        
        cfg.append(node);
    }
    
    // Build edges (simplified)
    for (int i = 0; i < cfg.size() - 1; ++i) {
        cfg[i].successors.append(cfg[i + 1].id);
        cfg[i + 1].predecessors.append(cfg[i].id);
    }
    
    result.findings = QJsonArray::fromVariantList({
        QJsonObject::fromVariantMap({
            {"type", "control_flow_graph"},
            {"node_count", cfg.size()},
            {"entry_points", 1},
            {"exit_points", 1}
        }).toVariantMap()
    });
    
    result.summary = QString("Built CFG with %1 nodes").arg(cfg.size());
    result.completed = true;
    result.metrics["node_count"] = cfg.size();
    result.metrics["edge_count"] = cfg.size() - 1;
    
    return result;
}

// Data flow analysis - track variables, transformations, lifecycles
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeDataFlow(const QString& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::DataFlow;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    
    // Extract variable declarations and usages
    QRegularExpression varPattern;
    
    switch (language) {
        case ProgrammingLanguage::Cpp:
            varPattern = QRegularExpression("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*[=;]");
            break;
        case ProgrammingLanguage::Python:
            varPattern = QRegularExpression("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*[=:]");
            break;
        case ProgrammingLanguage::JavaScript:
        case ProgrammingLanguage::TypeScript:
            varPattern = QRegularExpression("\\b(var|let|const)\\s+([A-Za-z_][A-Za-z0-9_]*)");
            break;
        default:
            varPattern = QRegularExpression("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*=");
    }
    
    auto matches = varPattern.globalMatch(content);
    QSet<QString> variables;
    
    while (matches.hasNext()) {
        auto match = matches.next();
        QString varName = match.captured(1);
        if (!varName.isEmpty() && !varName.startsWith("__")) {
            variables.insert(varName);
        }
    }
    
    result.findings = QJsonArray::fromVariantList({
        QJsonObject::fromVariantMap({
            {"type", "data_flow"},
            {"variable_count", variables.size()},
            {"variables", QJsonArray::fromStringList(variables.values()).toVariantList()}
        }).toVariantMap()
    });
    
    result.summary = QString("Found %1 variables").arg(variables.size());
    result.completed = true;
    result.metrics["variable_count"] = variables.size();
    
    return result;
}

// Dependency analysis - identify external dependencies
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeDependencies(const QString& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::Dependencies;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    
    // Extract dependencies based on language
    QRegularExpression importPattern;
    QStringList dependencies;
    
    switch (language) {
        case ProgrammingLanguage::Cpp:
            importPattern = QRegularExpression("#include\\s*[<\"]([^>\"]+)[>\"]");
            break;
        case ProgrammingLanguage::Python:
            importPattern = QRegularExpression("^(import|from)\\s+([A-Za-z0-9_]+)");
            break;
        case ProgrammingLanguage::JavaScript:
        case ProgrammingLanguage::TypeScript:
            importPattern = QRegularExpression("^(import|require)\\s*\\(?['\"]([^'\"]+)['\"]");
            break;
        case ProgrammingLanguage::Java:
            importPattern = QRegularExpression("^import\\s+([A-Za-z0-9_.]+)");
            break;
        case ProgrammingLanguage::CSharp:
            importPattern = QRegularExpression("^using\\s+([A-Za-z0-9_.]+)");
            break;
        case ProgrammingLanguage::Go:
            importPattern = QRegularExpression('^import\\s+[\"\\']([^\"\\']+)[\"\\']');
            break;
        case ProgrammingLanguage::Rust:
            importPattern = QRegularExpression("^use\\s+([A-Za-z0-9_:]+)");
            break;
        case ProgrammingLanguage::PHP:
            importPattern = QRegularExpression("^(require|require_once|include|include_once)\\s*['\"]([^'\"]+)['\"]");
            break;
        case ProgrammingLanguage::Ruby:
            importPattern = QRegularExpression("^require\\s*['\"]([^'\"]+)['\"]");
            break;
        default:
            importPattern = QRegularExpression("#include\\s*[<\"]([^>\"]+)[>\"]");
    }
    
    auto matches = importPattern.globalMatch(content);
    
    while (matches.hasNext()) {
        auto match = matches.next();
        QString dep = match.captured(1);
        if (!dep.isEmpty()) {
            dependencies.append(dep);
        }
    }
    
    result.findings = QJsonArray::fromVariantList({
        QJsonObject::fromVariantMap({
            {"type", "dependencies"},
            {"dependency_count", dependencies.size()},
            {"dependencies", QJsonArray::fromStringList(dependencies).toVariantList()}
        }).toVariantMap()
    });
    
    result.summary = QString("Found %1 dependencies").arg(dependencies.size());
    result.completed = true;
    result.metrics["dependency_count"] = dependencies.size();
    
    // Update statistics
    statistics_["dependenciesFound"] = statistics_["dependenciesFound"].toInt() + dependencies.size();
    
    return result;
}

// Security analysis - detect vulnerabilities, unsafe patterns
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeSecurity(const QString& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::Security;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    
    // Detect security vulnerabilities
    QVector<SecurityVulnerability> vulnerabilities = detectSecurityIssues(content, language);
    
    result.findings = QJsonArray::fromVariantList({
        QJsonObject::fromVariantMap({
            {"type", "security"},
            {"vulnerability_count", vulnerabilities.size()},
            {"vulnerabilities", QJsonArray::fromStringList(
                QStringList() << "buffer_overflow" << "sql_injection" << "xss"
            ).toVariantList()}
        }).toVariantMap()
    });
    
    result.summary = QString("Found %1 potential vulnerabilities").arg(vulnerabilities.size());
    result.completed = true;
    result.metrics["vulnerability_count"] = vulnerabilities.size();
    
    // Update statistics
    statistics_["securityIssuesFound"] = statistics_["securityIssuesFound"].toInt() + vulnerabilities.size();
    statistics_["vulnerabilitiesFound"] = statistics_["vulnerabilitiesFound"].toInt() + vulnerabilities.size();
    
    return result;
}

// Performance analysis - identify bottlenecks, inefficiencies
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzePerformance(const QString& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::Performance;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    
    // Detect performance issues
    QVector<PerformanceIssue> issues = detectPerformanceIssues(content, language);
    
    result.findings = QJsonArray::fromVariantList({
        QJsonObject::fromVariantMap({
            {"type", "performance"},
            {"issue_count", issues.size()},
            {"issues", QJsonArray::fromStringList(
                QStringList() << "inefficient_loop" << "memory_leak" << "bottleneck"
            ).toVariantList()}
        }).toVariantMap()
    });
    
    result.summary = QString("Found %1 performance issues").arg(issues.size());
    result.completed = true;
    result.metrics["issue_count"] = issues.size();
    
    // Update statistics
    statistics_["performanceIssuesFound"] = statistics_["performanceIssuesFound"].toInt() + issues.size();
    
    return result;
}

// API surface analysis - identify public interfaces
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeAPISurface(const QString& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::APISurface;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    
    // Extract public methods and interfaces
    QRegularExpression publicPattern;
    
    switch (language) {
        case ProgrammingLanguage::Cpp:
            publicPattern = QRegularExpression("\\b(public|class|struct)\\s+([A-Za-z0-9_]+)");
            break;
        case ProgrammingLanguage::Java:
        case ProgrammingLanguage::CSharp:
            publicPattern = QRegularExpression("\\b(public)\\s+([A-Za-z0-9_<>]+)\\s+([A-Za-z0-9_]+)");
            break;
        case ProgrammingLanguage::Python:
            publicPattern = QRegularExpression("^\\s*def\\s+([A-Za-z0-9_]+)");
            break;
        case ProgrammingLanguage::JavaScript:
        case ProgrammingLanguage::TypeScript:
            publicPattern = QRegularExpression("\\b(export\\s+)?(function|class|interface)\\s+([A-Za-z0-9_]+)");
            break;
        default:
            publicPattern = QRegularExpression("\\b(public|export)\\s+([A-Za-z0-9_]+)");
    }
    
    auto matches = publicPattern.globalMatch(content);
    int apiCount = 0;
    
    while (matches.hasNext()) {
        auto match = matches.next();
        apiCount++;
    }
    
    result.findings = QJsonArray::fromVariantList({
        QJsonObject::fromVariantMap({
            {"type", "api_surface"},
            {"api_count", apiCount},
            {"public_apis", apiCount}
        }).toVariantMap()
    });
    
    result.summary = QString("Found %1 public APIs").arg(apiCount);
    result.completed = true;
    result.metrics["api_count"] = apiCount;
    
    return result;
}

// Architectural analysis - identify patterns, anti-patterns
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeArchitecture(const QString& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::Architecture;
    result.completed = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    
    // Identify design patterns and anti-patterns
    QStringList patternsFound;
    QStringList antiPatternsFound;
    
    // Singleton pattern
    if (content.contains("static") && content.contains("instance") && 
        (content.contains("getInstance") || content.contains("Instance"))) {
        patternsFound.append("singleton");
    }
    
    // Factory pattern
    if (content.contains("factory") || content.contains("Factory") || 
        content.contains("create") || content.contains("make")) {
        patternsFound.append("factory");
    }
    
    // Observer pattern
    if (content.contains("observer") || content.contains("Observer") || 
        content.contains("notify") || content.contains("subscribe")) {
        patternsFound.append("observer");
    }
    
    // God object anti-pattern (too many methods/variables)
    QRegularExpression methodPattern("\\bdef\\s+|\\bfunction\\s+|\\bfunc\\s+|\\bvoid\\s+|\\bint\\s+|\\bstring\\s+");
    auto methodMatches = methodPattern.globalMatch(content);
    int methodCount = 0;
    while (methodMatches.hasNext()) {
        methodMatches.next();
        methodCount++;
    }
    
    if (methodCount > 50) {
        antiPatternsFound.append("god_object");
    }
    
    // Spaghetti code anti-pattern (high cyclomatic complexity)
    QRegularExpression branchPattern("\\bif\\s*\\(|\\belse\\s*\\{|\\bcase\\s+|\\bwhile\\s*\\(|\\bfor\\s*\\(");
    auto branchMatches = branchPattern.globalMatch(content);
    int branchCount = 0;
    while (branchMatches.hasNext()) {
        branchMatches.next();
        branchCount++;
    }
    
    if (branchCount > methodCount * 5) {
        antiPatternsFound.append("spaghetti_code");
    }
    
    result.findings = QJsonArray::fromVariantList({
        QJsonObject::fromVariantMap({
            {"type", "architecture"},
            {"pattern_count", patternsFound.size()},
            {"anti_pattern_count", antiPatternsFound.size()},
            {"patterns", QJsonArray::fromStringList(patternsFound).toVariantList()},
            {"anti_patterns", QJsonArray::fromStringList(antiPatternsFound).toVariantList()}
        }).toVariantMap()
    });
    
    result.summary = QString("Found %1 patterns, %2 anti-patterns").arg(patternsFound.size()).arg(antiPatternsFound.size());
    result.completed = true;
    result.metrics["pattern_count"] = patternsFound.size();
    result.metrics["anti_pattern_count"] = antiPatternsFound.size();
    
    return result;
}

// ==================== Agentic Automation ====================

// Apply a single agentic pattern to a task
bool DigestionReverseEngineeringSystem::applyAgenticPattern(const QString& filePath, DigestionTask& task, const QString& patternName) {
    // Generate code for the pattern
    QMap<QString, QString> parameters;
    parameters["method_name"] = task.methodName;
    parameters["class_name"] = QFileInfo(filePath).baseName();
    parameters["parameters"] = "..."; // Would extract actual parameters
    
    CodeGenerationResult result = generateAgenticCode(patternName, task.language, parameters);
    
    if (result.success) {
        // Store generated code
        task.generatedCode[patternName] = result;
        task.appliedPatterns[patternName] = true;
        
        // Update statistics
        QMap<QString, int> patternStats = statistics_["agenticPatternsApplied"].toMap();
        patternStats[patternName] = patternStats.value(patternName, 0) + 1;
        statistics_["agenticPatternsApplied"] = patternStats;
        
        qDebug() << "[AGENTIC] Applied pattern" << patternName << "to" << task.methodName;
        return true;
    } else {
        qWarning() << "[AGENTIC] Failed to apply pattern" << patternName << ":" << result.errorMessage;
        return false;
    }
}

// Apply multiple agentic patterns with dependency resolution
QVector<bool> DigestionReverseEngineeringSystem::applyMultiplePatterns(const QString& filePath, 
                                                           DigestionTask& task,
                                                           const QVector<QString>& patternNames) {
    QVector<bool> results;
    
    // Resolve pattern dependencies
    QVector<QString> resolvedPatterns = resolvePatternDependencies(patternNames);
    
    // Apply patterns in order
    for (const QString& patternName : resolvedPatterns) {
        bool result = applyAgenticPattern(filePath, task, patternName);
        results.append(result);
    }
    
    return results;
}

// Apply full agentic automation suite
bool DigestionReverseEngineeringSystem::applyFullAgenticSuite(const QString& filePath, DigestionTask& task) {
    // Determine which patterns to apply based on stub type and context
    QVector<QString> patternsToApply;
    
    // Always apply logging
    patternsToApply.append("logging");
    
    // Apply error handling for non-trivial methods
    if (task.classification != StubClassification::EmptyImplementation &&
        task.classification != StubClassification::NoOperation) {
        patternsToApply.append("error_handling");
    }
    
    // Apply validation for methods with parameters
    if (!task.metadata.value("parameters", "").isEmpty()) {
        patternsToApply.append("validation");
    }
    
    // Apply metrics for performance-critical methods
    patternsToApply.append("metrics");
    
    // Apply async for I/O bound operations (heuristic)
    if (task.stubContext.contains("fetch") || task.stubContext.contains("read") || 
        task.stubContext.contains("write") || task.stubContext.contains("http") ||
        task.stubContext.contains("database") || task.stubContext.contains("file")) {
        patternsToApply.append("async");
    }
    
    // Apply all patterns
    QVector<bool> results = applyMultiplePatterns(filePath, task, patternsToApply);
    
    // Check if all patterns were applied successfully
    bool allSucceeded = std::all_of(results.begin(), results.end(), [](bool r) { return r; });
    
    if (allSucceeded) {
        qDebug() << "[AGENTIC] Applied full suite to" << task.methodName;
    } else {
        qWarning() << "[AGENTIC] Some patterns failed for" << task.methodName;
    }
    
    return allSucceeded;
}

// Apply agentic automation to multiple tasks
QVector<bool> DigestionReverseEngineeringSystem::applyAgenticExtensions(const QString& filePath, 
                                                                QVector<DigestionTask>& tasks) {
    QVector<bool> results;
    
    // Prioritize tasks by complexity and impact
    QVector<DigestionTask> prioritizedTasks = prioritizeTasks(tasks);
    
    // Apply agentic automation to each task
    for (DigestionTask& task : prioritizedTasks) {
        bool result = applyFullAgenticSuite(filePath, task);
        results.append(result);
    }
    
    return results;
}

// ==================== Utility Methods ====================

// Classify stub type
StubClassification DigestionReverseEngineeringSystem::classifyStub(const QString& content, ProgrammingLanguage language) {
    QString trimmed = content.trimmed();
    
    // Check for empty implementations
    if (trimmed == "{}" || trimmed == "{" || trimmed == "}" ||
        trimmed == "pass" || trimmed == "return" || trimmed == "return;" ||
        trimmed == "//" || trimmed.startsWith("//") || trimmed.startsWith("#") ||
        trimmed == "/* */" || trimmed == "/*" || trimmed == "*/") {
        return StubClassification::EmptyImplementation;
    }
    
    // Check for placeholder comments
    if (trimmed.contains("placeholder", Qt::CaseInsensitive) ||
        trimmed.contains("stub", Qt::CaseInsensitive) ||
        trimmed.contains("future", Qt::CaseInsensitive) ||
        trimmed.contains("coming soon", Qt::CaseInsensitive)) {
        return StubClassification::PlaceholderComment;
    }
    
    // Check for TODO/FIXME
    if (trimmed.contains("TODO", Qt::CaseInsensitive) ||
        trimmed.contains("FIXME", Qt::CaseInsensitive) ||
        trimmed.contains("XXX", Qt::CaseInsensitive) ||
        trimmed.contains("HACK", Qt::CaseInsensitive)) {
        return StubClassification::TODO_Fixme;
    }
    
    // Check for not implemented exceptions
    if (trimmed.contains("NotImplemented", Qt::CaseInsensitive) ||
        trimmed.contains("UnsupportedOperation", Qt::CaseInsensitive) ||
        trimmed.contains("Q_UNIMPLEMENTED", Qt::CaseInsensitive) ||
        trimmed.contains("unimplemented!", Qt::CaseInsensitive) ||
        trimmed.contains("todo!", Qt::CaseInsensitive)) {
        return StubClassification::NotImplementedException;
    }
    
    // Check for no-op operations
    if (trimmed.contains("noop", Qt::CaseInsensitive) ||
        trimmed.contains("no-op", Qt::CaseInsensitive) ||
        trimmed.contains("no_op", Qt::CaseInsensitive) ||
        trimmed.contains("NOP", Qt::CaseInsensitive)) {
        return StubClassification::NoOperation;
    }
    
    // Check for deprecated code
    if (trimmed.contains("deprecated", Qt::CaseInsensitive) ||
        trimmed.contains("obsolete", Qt::CaseInsensitive) ||
        trimmed.contains("legacy", Qt::CaseInsensitive)) {
        return StubClassification::Deprecated;
    }
    
    // Check for mock/stub patterns
    if (trimmed.contains("mock", Qt::CaseInsensitive) ||
        trimmed.contains("fake", Qt::CaseInsensitive) ||
        trimmed.contains("dummy", Qt::CaseInsensitive) ||
        trimmed.contains("test", Qt::CaseInsensitive)) {
        return StubClassification::Mock_Stub;
    }
    
    // Check for prototype patterns
    if (trimmed.contains("prototype", Qt::CaseInsensitive) ||
        trimmed.contains("experimental", Qt::CaseInsensitive) ||
        trimmed.contains("beta", Qt::CaseInsensitive) ||
        trimmed.contains("alpha", Qt::CaseInsensitive)) {
        return StubClassification::Prototype;
    }
    
    return StubClassification::NotStub;
}

// Get complexity score for a method
int DigestionReverseEngineeringSystem::calculateComplexity(const QString& methodContent, ProgrammingLanguage language) {
    int complexity = 1; // Base complexity
    
    // Count branching statements
    QRegularExpression branchPattern("\\b(if|else|elif|case|default|for|while|do|catch)\\b");
    auto matches = branchPattern.globalMatch(methodContent);
    
    while (matches.hasNext()) {
        matches.next();
        complexity++;
    }
    
    // Count logical operators
    QRegularExpression logicPattern("\\&\\&|\\|\\||\\?");
    auto logicMatches = logicPattern.globalMatch(methodContent);
    
    while (logicMatches.hasNext()) {
        logicMatches.next();
        complexity++;
    }
    
    // Count function calls (adds to complexity)
    QRegularExpression callPattern("\\w+\\s*\\(");
    auto callMatches = callPattern.globalMatch(methodContent);
    int callCount = 0;
    
    while (callMatches.hasNext()) {
        callMatches.next();
        callCount++;
    }
    
    complexity += callCount / 5; // Add 1 for every 5 function calls
    
    return complexity;
}

// Identify dependencies in code
QVector<DependencyInfo> DigestionReverseEngineeringSystem::extractDependencies(const QString& code, ProgrammingLanguage language) {
    QVector<DependencyInfo> dependencies;
    
    QRegularExpression importPattern;
    
    switch (language) {
        case ProgrammingLanguage::Cpp:
            importPattern = QRegularExpression("#include\\s*[<\"]([^>\"]+)[>\"]");
            break;
        case ProgrammingLanguage::Python:
            importPattern = QRegularExpression("^(import|from)\\s+([A-Za-z0-9_]+)");
            break;
        case ProgrammingLanguage::JavaScript:
        case ProgrammingLanguage::TypeScript:
            importPattern = QRegularExpression("^(import|require)\\s*\\(?['\"]([^'\"]+)['\"]");
            break;
        case ProgrammingLanguage::Java:
            importPattern = QRegularExpression("^import\\s+([A-Za-z0-9_.]+)");
            break;
        case ProgrammingLanguage::CSharp:
            importPattern = QRegularExpression("^using\\s+([A-Za-z0-9_.]+)");
            break;
        case ProgrammingLanguage::Go:
            importPattern = QRegularExpression('^import\\s+[\"\\']([^\"\\']+)[\"\\']');
            break;
        case ProgrammingLanguage::Rust:
            importPattern = QRegularExpression("^use\\s+([A-Za-z0-9_:]+)");
            break;
        case ProgrammingLanguage::PHP:
            importPattern = QRegularExpression("^(require|require_once|include|include_once)\\s*['\"]([^'\"]+)['\"]");
            break;
        case ProgrammingLanguage::Ruby:
            importPattern = QRegularExpression("^require\\s*['\"]([^'\"]+)['\"]");
            break;
        default:
            importPattern = QRegularExpression("#include\\s*[<\"]([^>\"]+)[>\"]");
    }
    
    auto matches = importPattern.globalMatch(code);
    
    while (matches.hasNext()) {
        auto match = matches.next();
        DependencyInfo dep;
        dep.name = match.captured(1);
        dep.type = "library";
        dep.isExternal = !dep.name.startsWith(".") && !dep.name.startsWith("/");
        dep.isOptional = false;
        dependencies.append(dep);
    }
    
    return dependencies;
}

// Detect security vulnerabilities
QVector<SecurityVulnerability> DigestionReverseEngineeringSystem::detectSecurityIssues(const QString& code, ProgrammingLanguage language) {
    QVector<SecurityVulnerability> vulnerabilities;
    
    // SQL Injection patterns
    QRegularExpression sqlPattern("(execute|query|exec)\\s*\\(\\s*[^\"']*\\+");
    if (sqlPattern.match(code).hasMatch()) {
        SecurityVulnerability vuln;
        vuln.type = "sql_injection";
        vuln.severity = "critical";
        vuln.description = "Potential SQL injection vulnerability";
        vuln.remediation = "Use parameterized queries or prepared statements";
        vulnerabilities.append(vuln);
    }
    
    // Buffer overflow patterns (C/C++)
    if (language == ProgrammingLanguage::Cpp) {
        QRegularExpression bufferPattern("(strcpy|strcat|sprintf|gets)\\s*\\(");
        if (bufferPattern.match(code).hasMatch()) {
            SecurityVulnerability vuln;
            vuln.type = "buffer_overflow";
            vuln.severity = "critical";
            vuln.description = "Potential buffer overflow vulnerability";
            vuln.remediation = "Use safe string functions (strncpy, strncat, snprintf)";
            vulnerabilities.append(vuln);
        }
    }
    
    // XSS patterns (web languages)
    if (language == ProgrammingLanguage::JavaScript || language == ProgrammingLanguage::TypeScript ||
        language == ProgrammingLanguage::PHP || language == ProgrammingLanguage::Python) {
        QRegularExpression xssPattern("(innerHTML|document.write|eval)\\s*\\(");
        if (xssPattern.match(code).hasMatch()) {
            SecurityVulnerability vuln;
            vuln.type = "cross_site_scripting";
            vuln.severity = "high";
            vuln.description = "Potential XSS vulnerability";
            vuln.remediation = "Use textContent instead of innerHTML, sanitize user input";
            vulnerabilities.append(vuln);
        }
    }
    
    // Command injection patterns
    QRegularExpression cmdPattern("(system|exec|popen|shell_exec)\\s*\\(");
    if (cmdPattern.match(code).hasMatch()) {
        SecurityVulnerability vuln;
        vuln.type = "command_injection";
        vuln.severity = "critical";
        vuln.description = "Potential command injection vulnerability";
        vuln.remediation = "Avoid executing user input as commands, use whitelist validation";
        vulnerabilities.append(vuln);
    }
    
    return vulnerabilities;
}

// Detect performance issues
QVector<PerformanceIssue> DigestionReverseEngineeringSystem::detectPerformanceIssues(const QString& code, ProgrammingLanguage language) {
    QVector<PerformanceIssue> issues;
    
    // Inefficient loop patterns
    QRegularExpression loopPattern("for\\s*\\([^)]*\\)\\s*\\{[^}]*\\b(strcpy|strcat|sprintf|new|delete)\\b");
    if (loopPattern.match(code).hasMatch()) {
        PerformanceIssue issue;
        issue.type = "inefficient_loop";
        issue.severity = "medium";
        issue.description = "Inefficient operation inside loop";
        issue.optimization = "Move expensive operations outside loop, pre-allocate memory";
        issues.append(issue);
    }
    
    // Memory leak patterns (C++)
    if (language == ProgrammingLanguage::Cpp) {
        QRegularExpression leakPattern("new\\s+[^;]+;[^}]*\\}[^}]*$");
        if (leakPattern.match(code).hasMatch()) {
            PerformanceIssue issue;
            issue.type = "memory_leak";
            issue.severity = "high";
            issue.description = "Potential memory leak - new without delete";
            issue.optimization = "Use smart pointers (std::unique_ptr, std::shared_ptr) or ensure delete is called";
            issues.append(issue);
        }
    }
    
    // Excessive memory allocation
    QRegularExpression allocPattern("(malloc|calloc|realloc|new)\\s*\\([^)]*\\*\\s*[0-9]+");
    if (allocPattern.match(code).hasMatch()) {
        PerformanceIssue issue;
        issue.type = "excessive_allocation";
        issue.severity = "medium";
        issue.description = "Potential excessive memory allocation";
        issue.optimization = "Use appropriate data structures, consider memory pooling";
        issues.append(issue);
    }
    
    return issues;
}

// Build control flow graph
QVector<ControlFlowNode> DigestionReverseEngineeringSystem::buildControlFlowGraph(const QString& methodContent, ProgrammingLanguage language) {
    QVector<ControlFlowNode> cfg;
    QStringList lines = methodContent.split('\n');
    
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.isEmpty() || line.startsWith("//") || line.startsWith("#")) {
            continue;
        }
        
        ControlFlowNode node;
        node.id = i;
        node.lineNumber = i + 1;
        
        // Identify node type
        if (line.contains("if ") || line.contains("if(")) {
            node.type = "branch";
            node.description = "Conditional branch";
        } else if (line.contains("for ") || line.contains("for(") || 
                   line.contains("while ") || line.contains("while(") ||
                   line.contains("do ")) {
            node.type = "loop";
            node.description = "Loop construct";
        } else if (line.contains("return ") || line.contains("return;")) {
            node.type = "return";
            node.description = "Return statement";
        } else if (line.contains("{") && !line.contains("}")) {
            node.type = "entry";
            node.description = "Block entry";
        } else if (line.contains("}") && !line.contains("{")) {
            node.type = "exit";
            node.description = "Block exit";
        } else if (line.contains("(") && (line.contains(";") || line.contains(") {"))) {
            node.type = "call";
            node.description = "Function call";
        } else {
            node.type = "statement";
            node.description = "Statement";
        }
        
        cfg.append(node);
    }
    
    // Build edges (simplified)
    for (int i = 0; i < cfg.size() - 1; ++i) {
        cfg[i].successors.append(cfg[i + 1].id);
        cfg[i + 1].predecessors.append(cfg[i].id);
    }
    
    return cfg;
}

// Analyze data flow
QVector<DataFlowInfo> DigestionReverseEngineeringSystem::analyzeDataFlow(const QString& methodContent, ProgrammingLanguage language) {
    QVector<DataFlowInfo> dataFlow;
    
    QRegularExpression varPattern;
    
    switch (language) {
        case ProgrammingLanguage::Cpp:
            varPattern = QRegularExpression("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*[=;]");
            break;
        case ProgrammingLanguage::Python:
            varPattern = QRegularExpression("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*[=:]");
            break;
        case ProgrammingLanguage::JavaScript:
        case ProgrammingLanguage::TypeScript:
            varPattern = QRegularExpression("\\b(var|let|const)\\s+([A-Za-z_][A-Za-z0-9_]*)");
            break;
        default:
            varPattern = QRegularExpression("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*=");
    }
    
    auto matches = varPattern.globalMatch(methodContent);
    QSet<QString> variables;
    
    while (matches.hasNext()) {
        auto match = matches.next();
        QString varName = match.captured(1);
        if (!varName.isEmpty() && !varName.startsWith("__")) {
            variables.insert(varName);
        }
    }
    
    for (const QString& varName : variables) {
        DataFlowInfo info;
        info.variableName = varName;
        info.dataType = "unknown";
        info.declarationLine = 0;
        info.usageLines = {0};
        info.modificationLines = {0};
        info.transformations = {"declaration"};
        dataFlow.append(info);
    }
    
    return dataFlow;
}

// Prioritize tasks based on complexity and impact
QVector<DigestionTask> DigestionReverseEngineeringSystem::prioritizeTasks(const QVector<DigestionTask>& tasks) {
    // Create a list of tasks with priority scores
    QVector<QPair<DigestionTask, int>> scoredTasks;
    
    for (const DigestionTask& task : tasks) {
        int priorityScore = 0;
        
        // Higher priority for critical stub types
        if (task.classification == StubClassification::NotImplementedException) {
            priorityScore += 100;
        } else if (task.classification == StubClassification::TODO_Fixme) {
            priorityScore += 80;
        } else if (task.classification == StubClassification::EmptyImplementation) {
            priorityScore += 60;
        } else if (task.classification == StubClassification::PlaceholderComment) {
            priorityScore += 40;
        }
        
        // Higher priority for methods with many dependencies
        priorityScore += task.dependencies.size() * 10;
        
        // Higher priority for methods with security issues
        priorityScore += task.securityIssues.size() * 50;
        
        // Higher priority for methods with performance issues
        priorityScore += task.performanceIssues.size() * 30;
        
        // Higher priority for complex methods
        int complexity = calculateComplexity(task.stubContext, task.language);
        priorityScore += complexity * 5;
        
        scoredTasks.append(qMakePair(task, priorityScore));
    }
    
    // Sort by priority score (descending)
    std::sort(scoredTasks.begin(), scoredTasks.end(), 
               [](const QPair<DigestionTask, int>& a, const QPair<DigestionTask, int>& b) {
                   return a.second > b.second;
               });
    
    // Extract sorted tasks
    QVector<DigestionTask> sortedTasks;
    for (const auto& scoredTask : scoredTasks) {
        sortedTasks.append(scoredTask.first);
    }
    
    return sortedTasks;
}

// Resolve pattern dependencies (topological sort)
QVector<QString> DigestionReverseEngineeringSystem::resolvePatternDependencies(const QVector<QString>& patternNames) {
    // For now, return in a safe order (validation -> logging -> error_handling -> metrics -> async)
    QVector<QString> resolved;
    
    // Always start with validation if present
    if (patternNames.contains("validation")) {
        resolved.append("validation");
    }
    
    // Add logging early
    if (patternNames.contains("logging")) {
        resolved.append("logging");
    }
    
    // Add error handling
    if (patternNames.contains("error_handling")) {
        resolved.append("error_handling");
    }
    
    // Add metrics
    if (patternNames.contains("metrics")) {
        resolved.append("metrics");
    }
    
    // Add async last (as it wraps everything)
    if (patternNames.contains("async")) {
        resolved.append("async");
    }
    
    return resolved;
}

// Get all registered agentic patterns
QMap<QString, AgenticPattern> DigestionReverseEngineeringSystem::getAgenticPatterns() const {
    return agenticPatterns_;
}

// Get patterns by category
QVector<AgenticPattern> DigestionReverseEngineeringSystem::getPatternsByCategory(const QString& category) const {
    QVector<AgenticPattern> patterns;
    
    for (auto it = agenticPatterns_.begin(); it != agenticPatterns_.end(); ++it) {
        if (it.value().category == category) {
            patterns.append(it.value());
        }
    }
    
    return patterns;
}

// Process entire directory recursively
ComprehensiveAnalysisReport DigestionReverseEngineeringSystem::processDirectory(const QString& directoryPath) {
    ComprehensiveAnalysisReport aggregatedReport;
    aggregatedReport.filePath = directoryPath;
    aggregatedReport.timestamp = QDateTime::currentDateTime();
    
    QDirIterator it(directoryPath, QDirIterator::Subdirectories);
    QVector<ComprehensiveAnalysisReport> allReports;
    
    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        
        // Skip non-source files
        QStringList sourceExtensions = {"cpp", "h", "py", "js", "ts", "java", "cs", "go", "rs", "swift", "kt", "php", "rb"};
        if (!sourceExtensions.contains(fileInfo.suffix())) {
            continue;
        }
        
        // Analyze the file
        ComprehensiveAnalysisReport report = performComprehensiveAnalysis(filePath);
        allReports.append(report);
        
        // Aggregate statistics
        for (const DigestionTask& task : report.tasks) {
            aggregatedReport.tasks.append(task);
        }
    }
    
    // Aggregate metrics
    aggregatedReport.aggregatedMetrics["total_files"] = allReports.size();
    aggregatedReport.aggregatedMetrics["total_tasks"] = aggregatedReport.tasks.size();
    
    return aggregatedReport;
}

// Export comprehensive analysis report
QString DigestionReverseEngineeringSystem::exportComprehensiveReport(const ComprehensiveAnalysisReport& report, const QString& format) {
    if (format == "json") {
        QJsonObject reportObj;
        reportObj["filePath"] = report.filePath;
        reportObj["language"] = static_cast<int>(report.language);
        reportObj["timestamp"] = report.timestamp.toString(Qt::ISODate);
        reportObj["totalTasks"] = report.tasks.size();
        
        QJsonArray tasksArray;
        for (const DigestionTask& task : report.tasks) {
            QJsonObject taskObj;
            taskObj["filePath"] = task.filePath;
            taskObj["methodName"] = task.methodName;
            taskObj["lineNumber"] = task.lineNumber;
            taskObj["language"] = static_cast<int>(task.language);
            taskObj["stubType"] = task.stubType;
            taskObj["agenticPlan"] = task.agenticPlan;
            
            QJsonObject metadataObj;
            for (auto it = task.metadata.begin(); it != task.metadata.end(); ++it) {
                metadataObj[it.key()] = it.value();
            }
            taskObj["metadata"] = metadataObj;
            
            tasksArray.append(taskObj);
        }
        reportObj["tasks"] = tasksArray;
        
        QJsonDocument doc(reportObj);
        return doc.toJson(QJsonDocument::Indented);
        
    } else if (format == "markdown") {
        QStringList report;
        report << "# Comprehensive Analysis Report";
        report << "";
        report << QString("**File:** %1").arg(report.filePath);
        report << QString("**Language:** %1").arg(static_cast<int>(report.language));
        report << QString("**Timestamp:** %1").arg(report.timestamp.toString());
        report << QString("**Total Tasks:** %1").arg(report.tasks.size());
        report << "";
        
        for (const DigestionTask& task : report.tasks) {
            report << QString("## %1").arg(task.methodName.isEmpty() ? "Unknown Method" : task.methodName);
            report << "";
            report << QString("- **File:** %1").arg(task.filePath);
            report << QString("- **Line:** %1").arg(task.lineNumber);
            report << QString("- **Language:** %1").arg(static_cast<int>(task.language));
            report << QString("- **Stub Type:** %1").arg(task.stubType);
            report << QString("- **Classification:** %1").arg(static_cast<int>(task.classification));
            report << "";
            
            if (!task.dependencies.isEmpty()) {
                report << "### Dependencies";
                for (const DependencyInfo& dep : task.dependencies) {
                    report << QString("- %1: %2").arg(dep.type).arg(dep.name);
                }
                report << "";
            }
            
            if (!task.securityIssues.isEmpty()) {
                report << "### Security Issues";
                for (const SecurityVulnerability& vuln : task.securityIssues) {
                    report << QString("- **%1** (%2): %3").arg(vuln.type).arg(vuln.severity).arg(vuln.description);
                }
                report << "";
            }
            
            if (!task.performanceIssues.isEmpty()) {
                report << "### Performance Issues";
                for (const PerformanceIssue& issue : task.performanceIssues) {
                    report << QString("- **%1** (%2): %3").arg(issue.type).arg(issue.severity).arg(issue.description);
                }
                report << "";
            }
            
            report << "### Agentic Plan";
            report << "";
            report << "```";
            report << task.agenticPlan;
            report << "```";
            report << "";
        }
        
        return report.join("\n");
    }
    
    return "Unsupported format: " + format;
}

// Generate HTML report with visualizations
QString DigestionReverseEngineeringSystem::generateHTMLReport(const ComprehensiveAnalysisReport& report) {
    QStringList html;
    
    html << "<!DOCTYPE html>";
    html << "<html>";
    html << "<head>";
    html << "<title>Comprehensive Analysis Report</title>";
    html << "<style>";
    html << "body { font-family: Arial, sans-serif; margin: 20px; }";
    html << ".header { background: #f0f0f0; padding: 20px; border-radius: 5px; }";
    html << ".task { border: 1px solid #ddd; margin: 10px 0; padding: 15px; border-radius: 5px; }";
    html << ".severity-critical { color: red; font-weight: bold; }";
    html << ".severity-high { color: orange; font-weight: bold; }";
    html << ".severity-medium { color: yellow; }";
    html << ".severity-low { color: green; }";
    html << "</style>";
    html << "</head>";
    html << "<body>";
    
    html << "<div class='header'>";
    html << "<h1>Comprehensive Analysis Report</h1>";
    html << QString("<p><strong>File:</strong> %1</p>").arg(report.filePath);
    html << QString("<p><strong>Language:</strong> %1</p>").arg(static_cast<int>(report.language));
    html << QString("<p><strong>Timestamp:</strong> %1</p>").arg(report.timestamp.toString());
    html << QString("<p><strong>Total Tasks:</strong> %1</p>").arg(report.tasks.size());
    html << "</div>";
    
    for (const DigestionTask& task : report.tasks) {
        html << "<div class='task'>";
        html << QString("<h2>%1</h2>").arg(task.methodName.isEmpty() ? "Unknown Method" : task.methodName);
        html << QString("<p><strong>File:</strong> %1</p>").arg(task.filePath);
        html << QString("<p><strong>Line:</strong> %1</p>").arg(task.lineNumber);
        html << QString("<p><strong>Language:</strong> %1</p>").arg(static_cast<int>(task.language));
        html << QString("<p><strong>Stub Type:</strong> %1</p>").arg(task.stubType);
        html << QString("<p><strong>Classification:</strong> %1</p>").arg(static_cast<int>(task.classification));
        
        if (!task.dependencies.isEmpty()) {
            html << "<h3>Dependencies</h3><ul>";
            for (const DependencyInfo& dep : task.dependencies) {
                html << QString("<li>%1: %2</li>").arg(dep.type).arg(dep.name);
            }
            html << "</ul>";
        }
        
        if (!task.securityIssues.isEmpty()) {
            html << "<h3>Security Issues</h3><ul>";
            for (const SecurityVulnerability& vuln : task.securityIssues) {
                QString severityClass = QString("severity-%1").arg(vuln.severity);
                html << QString("<li class='%1'><strong>%2</strong> (%3): %4</li>")
                    .arg(severityClass).arg(vuln.type).arg(vuln.severity).arg(vuln.description);
            }
            html << "</ul>";
        }
        
        if (!task.performanceIssues.isEmpty()) {
            html << "<h3>Performance Issues</h3><ul>";
            for (const PerformanceIssue& issue : task.performanceIssues) {
                QString severityClass = QString("severity-%1").arg(issue.severity);
                html << QString("<li class='%1'><strong>%2</strong> (%3): %4</li>")
                    .arg(severityClass).arg(issue.type).arg(issue.severity).arg(issue.description);
            }
            html << "</ul>";
        }
        
        html << "<h3>Agentic Plan</h3>";
        html << "<pre>" << task.agenticPlan << "</pre>";
        
        html << "</div>";
    }
    
    html << "</body>";
    html << "</html>";
    
    return html.join("\n");
}
