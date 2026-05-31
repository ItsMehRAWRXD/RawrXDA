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
    statistics_["filesProcessed"] = std::stringList();
    statistics_["stubTypes"] = std::map<std::string, int>();
    statistics_["languagesDetected"] = std::map<std::string, int>();
    statistics_["analysisDirections"] = std::map<std::string, int>();
    statistics_["agenticPatternsApplied"] = std::map<std::string, int>();
    statistics_["securityIssuesFound"] = 0;
    statistics_["performanceIssuesFound"] = 0;
    statistics_["dependenciesFound"] = 0;
    statistics_["vulnerabilitiesFound"] = 0;
}

// ==================== Language Detection & Analysis ====================

// Detect programming language from file extension and content
// [SECURITY] [VALIDATION] Language detection is critical for regex accuracy
// [FAILURE_POINT] Ambiguous extensions (like .h) can lead to incorrect language detection
ProgrammingLanguage DigestionReverseEngineeringSystem::detectLanguage(const std::string& filePath) {
    // Info fileInfo(filePath);
    std::string extension = fileInfo.suffix().toLower();

    // Note: extension-based detection is fast but not complete. For extensionless
    // files (e.g., scripts with shebangs) or misnamed sources, the content-based
    // fallback below is the intended coverage path.
    
    // Map file extensions to programming languages (expanded)
    static std::map<std::string, ProgrammingLanguage> extensionMap = {
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
    // [FLOW] Content-based fallback is heuristic and may misclassify mixed files
    if (lang == ProgrammingLanguage::Unknown || extension == "h" || extension == "config") {
        // File operation removed;
        // [FAILURE_POINT] File I/O can fail or be slow on large/networked files
        if (file.open(std::iostream::ReadOnly | std::iostream::Text)) {
            std::stringstream in(&file);
            // [DATA_FLOW] Sample only a prefix to keep detection fast
            std::string content = in.read(1000); // Read first 1000 characters
            file.close();
            
            lang = detectLanguageFromContent(content);
        }
    }
    
    // Update statistics
    // [OBSERVABILITY] Track language distribution for later reporting
    if (lang != ProgrammingLanguage::Unknown) {
        std::map<std::string, int> languages = statistics_["languagesDetected"].toMap();
        std::string langStr = std::string::number(static_cast<int>(lang));
        languages[langStr] = languages.value(langStr, 0) + 1;
        statistics_["languagesDetected"] = languages;
    }
    
    return lang;
}

// Detect language from content only (when extension is ambiguous)
ProgrammingLanguage DigestionReverseEngineeringSystem::detectLanguageFromContent(const std::string& content) {
    // Look for language-specific patterns in the content
    std::string trimmed = content.trimmed();
    // [SECURITY] Heuristic matching can yield false positives for generated/minified code
    
    // C++ patterns
    if (trimmed.contains("#include") || trimmed.contains("using namespace") || 
        trimmed.contains("std::") || trimmed.contains("class ") || trimmed.contains("public:") ||
        trimmed.contains("") || trimmed.contains("Qt")) {
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
std::vector<ProgrammingLanguage> DigestionReverseEngineeringSystem::getSupportedLanguages() const {
    std::vector<ProgrammingLanguage> languages;
    for (auto it = languagePatterns_.begin(); it != languagePatterns_.end(); ++it) {
        languages.append(it.key());
    }
    return languages;
}

// ==================== Multi-Directional Scanning ====================

// Scan a file for stubs/placeholders with basic analysis
// [SECURITY] [FAILURE_POINT] File I/O failure or large file sizes can cause system hang
// [VALIDATION] Validates stub keywords against language-specific patterns
std::vector<DigestionTask> DigestionReverseEngineeringSystem::scanFileForStubs(const std::string& filePath) {
    std::vector<DigestionTask> tasks;
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        return tasks;
    }
    
    std::stringstream in(&file);
    int lineNum = 0;
    std::stringList allLines;
    
    // Read all lines first
    // [FAILURE_POINT] Reading very large files fully can increase memory pressure
    while (!in.atEnd()) {
        allLines.append(in.readLine());
    }
    
    // Update statistics
    statistics_["totalLinesAnalyzed"] = statistics_["totalLinesAnalyzed"] + allLines.size();
    
    // Detect language
    ProgrammingLanguage language = detectLanguage(filePath);
    if (language == ProgrammingLanguage::Unknown) {
        return tasks;
    }
    
    // Get language patterns
    LanguagePatterns patterns = getLanguagePatterns(language);
    if (patterns.stubKeywords.empty()) {
        return tasks;
    }
    
    // Build regex pattern for stub detection
    // [DATA_FLOW] Compile once per file to keep per-line checks cheap
    std::string stubPatternStr = "\\b(" + patterns.stubKeywords.join("|") + ")\\b";
    std::regex stubPattern(stubPatternStr, std::regex::CaseInsensitiveOption);
    
    // Method detection patterns
    std::vector<std::regex> methodPatterns;
    for (const std::string& patternStr : patterns.methodPatterns) {
        methodPatterns.append(std::regex(patternStr));
    }
    
    // Scan for stubs
    for (int i = 0; i < allLines.size(); ++i) {
        std::string line = allLines[i];
        ++lineNum;
        
        // Skip empty lines and comments (for performance)
        // [SECURITY] Comment detection is shallow; embedded stubs in block comments may be missed
        std::string trimmedLine = line.trimmed();
        if (trimmedLine.empty() || patterns.commentPatterns.contains(trimmedLine.left(2))) {
            continue;
        }
        
        // Look for method definitions
        // [FAILURE_POINT] Regex-based method detection can mis-handle complex signatures
        bool isMethod = false;
        std::string methodName;
        for (const std::regex& methodPattern : methodPatterns) {
            auto match = methodPattern.match(line);
            if (match.hasMatch()) {
                isMethod = true;
                methodName = match"";
                statistics_["totalMethodsFound"] = statistics_["totalMethodsFound"] + 1;
                break;
            }
        }
        
        // Look for stubs
        if (stubPattern.match(line).hasMatch() || isMethod) {
            // Extract method context (10 lines before and after)
            // [DATA_FLOW] Context window feeds analysis and agentic planning
            int startLine = qMax(0, i - 10);
            int endLine = qMin(allLines.size(), i + 11);
            
            std::stringList contextLines;
            for (int j = startLine; j < endLine; ++j) {
                contextLines.append(allLines[j]);
            }
            
            std::string context = contextLines.join("\n");
            
            // Determine stub type
            // [FLOW] Method stubs are classified differently from inline markers
            std::string stubType = "unknown";
            if (isMethod) {
                // Check if method is empty or has placeholder
                bool hasImplementation = false;
                for (int j = i + 1; j < qMin(allLines.size(), i + 20); ++j) {
                    std::string nextLine = allLines[j].trimmed();
                    if (nextLine == "{" || nextLine.startsWith("//") || nextLine.startsWith("#")) {
                        continue;
                    }
                    if (nextLine == "}" || nextLine == "end" || nextLine.startsWith("def ") || nextLine.startsWith("func ")) {
                        break;
                    }
                    if (!nextLine.empty() && !patterns.commentPatterns.contains(nextLine.left(2))) {
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
                for (const std::string& keyword : patterns.stubKeywords) {
                    if (line.contains(keyword, CaseInsensitive)) {
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
            task.metadata["file_name"] = // FileInfo: filePath).fileName();
            task.metadata["file_path"] = filePath;
            task.metadata["line_content"] = line.trimmed();
            task.metadata["stub_keyword"] = stubType;
            task.metadata["language"] = std::string::number(static_cast<int>(language));
            task.metadata["classification"] = std::string::number(static_cast<int>(classification));
            task.metadata["is_method"] = isMethod ? "true" : "false";
            task.metadata["context_lines"] = std::string::number(endLine - startLine);
            
            // Perform basic analysis
            // [SECURITY] Analysis is static/regex-driven and may miss obfuscated patterns
            analyzeFileContent(context, language, task);
            
            tasks.append(task);
            
            // Update statistics
            statistics_["totalStubsFound"] = statistics_["totalStubsFound"] + 1;
            
            std::map<std::string, int> stubTypes = statistics_["stubTypes"].toMap();
            stubTypes[stubType] = stubTypes.value(stubType, 0) + 1;
            statistics_["stubTypes"] = stubTypes;
        }
    }
    
    // Update file statistics
    statistics_["totalFilesScanned"] = statistics_["totalFilesScanned"] + 1;
    std::stringList filesProcessed = statistics_["filesProcessed"].toStringList();
    filesProcessed.append(filePath);
    statistics_["filesProcessed"] = filesProcessed;
    
    return tasks;
}

// Scan with specific analysis directions
std::vector<DigestionTask> DigestionReverseEngineeringSystem::scanFileWithDirections(const std::string& filePath, 
                                                                         const std::set<AnalysisDirection>& directions) {
    std::vector<DigestionTask> tasks = scanFileForStubs(filePath);
    
    // Perform additional analysis for each direction
    ProgrammingLanguage language = detectLanguage(filePath);
    
    for (DigestionTask& task : tasks) {
        // [FLOW] Each task accumulates directional analysis results independently
        std::map<AnalysisDirection, DirectionalAnalysisResult> directionalResults;
        
        for (AnalysisDirection direction : directions) {
            DirectionalAnalysisResult result = performDirectionalAnalysis(filePath, direction);
            directionalResults[direction] = result;
            
            // Update statistics
            std::map<std::string, int> analysisStats = statistics_["analysisDirections"].toMap();
            std::string dirStr = std::string::number(static_cast<int>(direction));
            analysisStats[dirStr] = analysisStats.value(dirStr, 0) + 1;
            statistics_["analysisDirections"] = analysisStats;
        }
        
        // Store directional results in task
        // Note: In a full implementation, we'd merge these into the task structure
    }
    
    return tasks;
}

// Perform comprehensive multi-directional analysis
// [SECURITY] [VALIDATION] Aggregates multiple analysis streams
// [FAILURE_POINT] Failure in any direction should be isolated to prevent total report failure
ComprehensiveAnalysisReport DigestionReverseEngineeringSystem::performComprehensiveAnalysis(const std::string& filePath) {
    ComprehensiveAnalysisReport report;
    report.filePath = filePath;
    report.language = detectLanguage(filePath);
    report.timestamp = // DateTime::currentDateTime();
    
    // Scan for stubs
    report.tasks = scanFileForStubs(filePath);
    
    // Perform all directional analyses
    std::set<AnalysisDirection> allDirections = {
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
    // [DATA_FLOW] Cache enables reuse but can grow with large directories
    analysisCache_[filePath] = report;
    
    return report;
}

// ==================== Directional Analysis Methods ====================

// Control flow analysis - build CFG, identify recursion, loops
// [SECURITY] [VALIDATION] [SCHEMA] Builds a control flow graph (CFG)
// [FAILURE_POINT] Deep recursion or extreme branching can exceed CFG node limits
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeControlFlow(const std::string& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::ControlFlow;
    result.completed = false;
    
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    std::stringstream in(&file);
    std::stringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    
    // Build control flow graph
    // [FLOW] This is a simplified CFG (linear edges only)
    std::vector<ControlFlowNode> cfg;
    int nodeId = 0;
    
    for (int i = 0; i < lines.size(); ++i) {
        std::string line = lines[i].trimmed();
        
        // Skip empty lines and comments
        if (line.empty() || line.startsWith("//") || line.startsWith("#") || line.startsWith("/*")) {
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
    // [FAILURE_POINT] No explicit branching edges; results are approximate
    for (int i = 0; i < cfg.size() - 1; ++i) {
        cfg[i].successors.append(cfg[i + 1].id);
        cfg[i + 1].predecessors.append(cfg[i].id);
    }
    
    result.findings = nlohmann::json::fromVariantList({
        nlohmann::json::fromVariantMap({
            {"type", "control_flow_graph"},
            {"node_count", cfg.size()},
            {"entry_points", 1},
            {"exit_points", 1}
        }).toVariantMap()
    });
    
    result.summary = std::string("Built CFG with %1 nodes"));
    result.completed = true;
    result.metrics["node_count"] = cfg.size();
    result.metrics["edge_count"] = cfg.size() - 1;
    
    return result;
}

// Data flow analysis - track variables, transformations, lifecycles
// [SECURITY] [FAILURE_POINT] Regex-based variable extraction is susceptible to obfuscation
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeDataFlow(const std::string& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::DataFlow;
    result.completed = false;
    
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    std::stringstream in(&file);
    std::string content = in.readAll();
    
    // Extract variable declarations and usages
    // [SECURITY] Regex extraction may miss shadowing or destructuring patterns
    std::regex varPattern;
    
    switch (language) {
        case ProgrammingLanguage::Cpp:
            varPattern = std::regex("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*[=;]");
            break;
        case ProgrammingLanguage::Python:
            varPattern = std::regex("\\b([A-Za-z_][A-ZaZ0-9_]*)\\s*[=:]");
            break;
        case ProgrammingLanguage::JavaScript:
        case ProgrammingLanguage::TypeScript:
            varPattern = std::regex("\\b(var|let|const)\\s+([A-Za-z_][A-ZaZ0-9_]*)");
            break;
        default:
            varPattern = std::regex("\\b([A-Za-z_][A-ZaZ0-9_]*)\\s*=");
    }
    
    auto matches = varPattern;
    std::set<std::string> variables;
    
    while (matchesfalse) {
        auto match = matches;
        std::string varName = match"";
        if (!varName.empty() && !varName.startsWith("__")) {
            variables.insert(varName);
        }
    }
    
    result.findings = nlohmann::json::fromVariantList({
        nlohmann::json::fromVariantMap({
            {"type", "data_flow"},
            {"variable_count", variables.size()},
            {"variables", nlohmann::json::fromStringList(variables.values()).toVariantList()}
        }).toVariantMap()
    });
    
    result.summary = std::string("Found %1 variables"));
    result.completed = true;
    result.metrics["variable_count"] = variables.size();
    
    return result;
}

// Dependency analysis - identify external dependencies
// [SECURITY] [VALIDATION] Checks for external/optional libraries
// [FAILURE_POINT] Missing or malformed manifest files can lead to missed dependencies
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeDependencies(const std::string& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::Dependencies;
    result.completed = false;
    
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    std::stringstream in(&file);
    std::string content = in.readAll();
    
    // Extract dependencies based on language
    // [FAILURE_POINT] Line-anchored patterns may miss inline/aliased imports
    std::regex importPattern;
    std::stringList dependencies;
    
    switch (language) {
        case ProgrammingLanguage::Cpp:
            importPattern = std::regex("#include\\s*[<\"]([^>\"]+)[>\"]");
            break;
        case ProgrammingLanguage::Python:
            importPattern = std::regex("^(import|from)\\s+([A-Za-z0-9_]+)");
            break;
        case ProgrammingLanguage::JavaScript:
        case ProgrammingLanguage::TypeScript:
            importPattern = std::regex("^(import|require)\\s*\\(?['\"]([^'\"]+)['\"]");
            break;
        case ProgrammingLanguage::Java:
            importPattern = std::regex("^import\\s+([A-Za-z0-9_.]+)");
            break;
        case ProgrammingLanguage::CSharp:
            importPattern = std::regex("^using\\s+([A-Za-z0-9_.]+)");
            break;
        case ProgrammingLanguage::Go:
            importPattern = std::regex('^import\\s+[\"\\']([^\"\\']+)[\"\\']');
            break;
        case ProgrammingLanguage::Rust:
            importPattern = std::regex("^use\\s+([A-Za-z0-9_:]+)");
            break;
        case ProgrammingLanguage::PHP:
            importPattern = std::regex("^(require|require_once|include|include_once)\\s*['\"]([^'\"]+)['\"]");
            break;
        case ProgrammingLanguage::Ruby:
            importPattern = std::regex("^require\\s*['\"]([^'\"]+)['\"]");
            break;
        default:
            importPattern = std::regex("#include\\s*[<\"]([^>\"]+)[>\"]");
    }
    
    auto matches = importPattern;
    
    while (matchesfalse) {
        auto match = matches;
        std::string dep = match"";
        if (!dep.empty()) {
            dependencies.append(dep);
        }
    }
    
    result.findings = nlohmann::json::fromVariantList({
        nlohmann::json::fromVariantMap({
            {"type", "dependencies"},
            {"dependency_count", dependencies.size()},
            {"dependencies", nlohmann::json::fromStringList(dependencies).toVariantList()}
        }).toVariantMap()
    });
    
    result.summary = std::string("Found %1 dependencies"));
    result.completed = true;
    result.metrics["dependency_count"] = dependencies.size();
    
    // Update statistics
    statistics_["dependenciesFound"] = statistics_["dependenciesFound"] + dependencies.size();
    
    return result;
}

// Security analysis - detect vulnerabilities, unsafe patterns
// [SECURITY] [VALIDATION] [FAILURE_POINT] Uses regex for static analysis (false positives/negatives)
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeSecurity(const std::string& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::Security;
    result.completed = false;
    
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    std::stringstream in(&file);
    std::string content = in.readAll();
    
    // Detect security vulnerabilities
    // [SECURITY] Heuristic matching can over/under-report; treat as advisory
    std::vector<SecurityVulnerability> vulnerabilities = detectSecurityIssues(content, language);
    
    result.findings = nlohmann::json::fromVariantList({
        nlohmann::json::fromVariantMap({
            {"type", "security"},
            {"vulnerability_count", vulnerabilities.size()},
            {"vulnerabilities", nlohmann::json::fromStringList(
                std::stringList() << "buffer_overflow" << "sql_injection" << "xss"
            ).toVariantList()}
        }).toVariantMap()
    });
    
    result.summary = std::string("Found %1 potential vulnerabilities"));
    result.completed = true;
    result.metrics["vulnerability_count"] = vulnerabilities.size();
    
    // Update statistics
    statistics_["securityIssuesFound"] = statistics_["securityIssuesFound"] + vulnerabilities.size();
    statistics_["vulnerabilitiesFound"] = statistics_["vulnerabilitiesFound"] + vulnerabilities.size();
    
    return result;
}

// Performance analysis - identify bottlenecks, inefficiencies
// [SECURITY] [FAILURE_POINT] Heuristic-based leak detection may miss complex lifecycles
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzePerformance(const std::string& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::Performance;
    result.completed = false;
    
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    std::stringstream in(&file);
    std::string content = in.readAll();
    
    // Detect performance issues
    // [FAILURE_POINT] Heuristics do not model runtime behavior or allocations precisely
    std::vector<PerformanceIssue> issues = detectPerformanceIssues(content, language);
    
    result.findings = nlohmann::json::fromVariantList({
        nlohmann::json::fromVariantMap({
            {"type", "performance"},
            {"issue_count", issues.size()},
            {"issues", nlohmann::json::fromStringList(
                std::stringList() << "inefficient_loop" << "memory_leak" << "bottleneck"
            ).toVariantList()}
        }).toVariantMap()
    });
    
    result.summary = std::string("Found %1 performance issues"));
    result.completed = true;
    result.metrics["issue_count"] = issues.size();
    
    // Update statistics
    statistics_["performanceIssuesFound"] = statistics_["performanceIssuesFound"] + issues.size();
    
    return result;
}

// API surface analysis - identify public interfaces
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeAPISurface(const std::string& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::APISurface;
    result.completed = false;
    
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    std::stringstream in(&file);
    std::string content = in.readAll();
    
    // Extract public methods and interfaces
    std::regex publicPattern;
    
    switch (language) {
        case ProgrammingLanguage::Cpp:
            publicPattern = std::regex("\\b(public|class|struct)\\s+([A-Za-z0-9_]+)");
            break;
        case ProgrammingLanguage::Java:
        case ProgrammingLanguage::CSharp:
            publicPattern = std::regex("\\b(public)\\s+([A-Za-z0-9_<>]+)\\s+([A-Za-z0-9_]+)");
            break;
        case ProgrammingLanguage::Python:
            publicPattern = std::regex("^\\s*def\\s+([A-Za-z0-9_]+)");
            break;
        case ProgrammingLanguage::JavaScript:
        case ProgrammingLanguage::TypeScript:
            publicPattern = std::regex("\\b(export\\s+)?(function|class|interface)\\s+([A-Za-z0-9_]+)");
            break;
        default:
            publicPattern = std::regex("\\b(public|export)\\s+([A-Za-z0-9_]+)");
    }
    
    auto matches = publicPattern;
    int apiCount = 0;
    
    while (matchesfalse) {
        auto match = matches;
        apiCount++;
    }
    
    result.findings = nlohmann::json::fromVariantList({
        nlohmann::json::fromVariantMap({
            {"type", "api_surface"},
            {"api_count", apiCount},
            {"public_apis", apiCount}
        }).toVariantMap()
    });
    
    result.summary = std::string("Found %1 public APIs");
    result.completed = true;
    result.metrics["api_count"] = apiCount;
    
    return result;
}

// Architectural analysis - identify patterns, anti-patterns
// [SECURITY] [VALIDATION] [SCHEMA] Detects god objects and spaghetti code
DirectionalAnalysisResult DigestionReverseEngineeringSystem::analyzeArchitecture(const std::string& filePath, ProgrammingLanguage language) {
    DirectionalAnalysisResult result;
    result.direction = AnalysisDirection::Architecture;
    result.completed = false;
    
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        result.summary = "Failed to open file";
        return result;
    }
    
    std::stringstream in(&file);
    std::string content = in.readAll();
    
    // Identify design patterns and anti-patterns
    std::stringList patternsFound;
    std::stringList antiPatternsFound;
    
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
    std::regex methodPattern("\\bdef\\s+|\\bfunction\\s+|\\bfunc\\s+|\\bvoid\\s+|\\bint\\s+|\\bstring\\s+");
    auto methodMatches = methodPattern;
    int methodCount = 0;
    while (methodMatchesfalse) {
        methodMatches;
        methodCount++;
    }
    
    if (methodCount > 50) {
        antiPatternsFound.append("god_object");
    }
    
    // Spaghetti code anti-pattern (high cyclomatic complexity)
    std::regex branchPattern("\\bif\\s*\\(|\\belse\\s*\\{|\\bcase\\s+|\\bwhile\\s*\\(|\\bfor\\s*\\(");
    auto branchMatches = branchPattern;
    int branchCount = 0;
    while (branchMatchesfalse) {
        branchMatches;
        branchCount++;
    }
    
    if (branchCount > methodCount * 5) {
        antiPatternsFound.append("spaghetti_code");
    }
    
    result.findings = nlohmann::json::fromVariantList({
        nlohmann::json::fromVariantMap({
            {"type", "architecture"},
            {"pattern_count", patternsFound.size()},
            {"anti_pattern_count", antiPatternsFound.size()},
            {"patterns", nlohmann::json::fromStringList(patternsFound).toVariantList()},
            {"anti_patterns", nlohmann::json::fromStringList(antiPatternsFound).toVariantList()}
        }).toVariantMap()
    });
    
    result.summary = std::string("Found %1 patterns, %2 anti-patterns")));
    result.completed = true;
    result.metrics["pattern_count"] = patternsFound.size();
    result.metrics["anti_pattern_count"] = antiPatternsFound.size();
    
    return result;
}

// ==================== Agentic Automation ====================

// Apply a single agentic pattern to a task
bool DigestionReverseEngineeringSystem::applyAgenticPattern(const std::string& filePath, DigestionTask& task, const std::string& patternName) {
    // Generate code for the pattern
    std::map<std::string, std::string> parameters;
    parameters["method_name"] = task.methodName;
    parameters["class_name"] = // FileInfo: filePath).baseName();
    parameters["parameters"] = "..."; // Would extract actual parameters
    
    CodeGenerationResult result = generateAgenticCode(patternName, task.language, parameters);
    
    if (result.success) {
        // Store generated code
        task.generatedCode[patternName] = result;
        task.appliedPatterns[patternName] = true;
        
        // Update statistics
        std::map<std::string, int> patternStats = statistics_["agenticPatternsApplied"].toMap();
        patternStats[patternName] = patternStats.value(patternName, 0) + 1;
        statistics_["agenticPatternsApplied"] = patternStats;
        
        return true;
    } else {
        return false;
    }
}

// Apply multiple agentic patterns with dependency resolution
std::vector<bool> DigestionReverseEngineeringSystem::applyMultiplePatterns(const std::string& filePath, 
                                                           DigestionTask& task,
                                                           const std::vector<std::string>& patternNames) {
    std::vector<bool> results;
    
    // Resolve pattern dependencies
    std::vector<std::string> resolvedPatterns = resolvePatternDependencies(patternNames);
    
    // Apply patterns in order
    for (const std::string& patternName : resolvedPatterns) {
        bool result = applyAgenticPattern(filePath, task, patternName);
        results.append(result);
    }
    
    return results;
}

// Apply full agentic automation suite
bool DigestionReverseEngineeringSystem::applyFullAgenticSuite(const std::string& filePath, DigestionTask& task) {
    // Determine which patterns to apply based on stub type and context
    std::vector<std::string> patternsToApply;
    
    // Always apply logging
    patternsToApply.append("logging");
    
    // Apply error handling for non-trivial methods
    if (task.classification != StubClassification::EmptyImplementation &&
        task.classification != StubClassification::NoOperation) {
        patternsToApply.append("error_handling");
    }
    
    // Apply validation for methods with parameters
    if (!task.metadata.value("parameters", "").empty()) {
        patternsToApply.append("validation");
    }
    
    // Apply metrics for performance-critical methods
    patternsToApply.append("metrics");
    
    // Apply async for I/O bound operations (heuristic)
    // [FLOW] Heuristic checks context keywords to decide async suitability
    if (task.stubContext.contains("fetch") || task.stubContext.contains("read") || 
        task.stubContext.contains("write") || task.stubContext.contains("http") ||
        task.stubContext.contains("database") || task.stubContext.contains("file")) {
        patternsToApply.append("async");
    }
    
    // Apply all patterns
    std::vector<bool> results = applyMultiplePatterns(filePath, task, patternsToApply);
    
    // Check if all patterns were applied successfully
    bool allSucceeded = std::all_of(results.begin(), results.end(), [](bool r) { return r; });
    
    if (allSucceeded) {
    } else {
    }
    
    return allSucceeded;
}

// Apply agentic automation to multiple tasks
std::vector<bool> DigestionReverseEngineeringSystem::applyAgenticExtensions(const std::string& filePath, 
                                                                std::vector<DigestionTask>& tasks) {
    std::vector<bool> results;
    
    // Prioritize tasks by complexity and impact
    std::vector<DigestionTask> prioritizedTasks = prioritizeTasks(tasks);
    
    // Apply agentic automation to each task
    for (DigestionTask& task : prioritizedTasks) {
        bool result = applyFullAgenticSuite(filePath, task);
        results.append(result);
    }
    
    return results;
}

// ==================== Utility Methods ====================

// Classify stub type
StubClassification DigestionReverseEngineeringSystem::classifyStub(const std::string& content, ProgrammingLanguage language) {
    std::string trimmed = content.trimmed();
    // [FLOW] Classification order matters; earlier checks short-circuit later ones
    
    // Check for empty implementations
    if (trimmed == "{}" || trimmed == "{" || trimmed == "}" ||
        trimmed == "pass" || trimmed == "return" || trimmed == "return;" ||
        trimmed == "//" || trimmed.startsWith("//") || trimmed.startsWith("#") ||
        trimmed == "/* */" || trimmed == "/*" || trimmed == "*/") {
        return StubClassification::EmptyImplementation;
    }
    
    // Check for placeholder comments
    if (trimmed.contains("placeholder", CaseInsensitive) ||
        trimmed.contains("stub", CaseInsensitive) ||
        trimmed.contains("future", CaseInsensitive) ||
        trimmed.contains("coming soon", CaseInsensitive)) {
        return StubClassification::PlaceholderComment;
    }
    
    // Check for TODO/FIXME
    if (trimmed.contains("TODO", CaseInsensitive) ||
        trimmed.contains("FIXME", CaseInsensitive) ||
        trimmed.contains("XXX", CaseInsensitive) ||
        trimmed.contains("HACK", CaseInsensitive)) {
        return StubClassification::TODO_Fixme;
    }
    
    // Check for not implemented exceptions
    if (trimmed.contains("NotImplemented", CaseInsensitive) ||
        trimmed.contains("UnsupportedOperation", CaseInsensitive) ||
        trimmed.contains("", CaseInsensitive) ||
        trimmed.contains("unimplemented!", CaseInsensitive) ||
        trimmed.contains("todo!", CaseInsensitive)) {
        return StubClassification::NotImplementedException;
    }
    
    // Check for no-op operations
    if (trimmed.contains("noop", CaseInsensitive) ||
        trimmed.contains("no-op", CaseInsensitive) ||
        trimmed.contains("no_op", CaseInsensitive) ||
        trimmed.contains("NOP", CaseInsensitive)) {
        return StubClassification::NoOperation;
    }
    
    // Check for deprecated code
    if (trimmed.contains("deprecated", CaseInsensitive) ||
        trimmed.contains("obsolete", CaseInsensitive) ||
        trimmed.contains("legacy", CaseInsensitive)) {
        return StubClassification::Deprecated;
    }
    
    // Check for mock/stub patterns
    if (trimmed.contains("mock", CaseInsensitive) ||
        trimmed.contains("fake", CaseInsensitive) ||
        trimmed.contains("dummy", CaseInsensitive) ||
        trimmed.contains("test", CaseInsensitive)) {
        return StubClassification::Mock_Stub;
    }
    
    // Check for prototype patterns
    if (trimmed.contains("prototype", CaseInsensitive) ||
        trimmed.contains("experimental", CaseInsensitive) ||
        trimmed.contains("beta", CaseInsensitive) ||
        trimmed.contains("alpha", CaseInsensitive)) {
        return StubClassification::Prototype;
    }
    
    return StubClassification::NotStub;
}

// Get complexity score for a method
int DigestionReverseEngineeringSystem::calculateComplexity(const std::string& methodContent, ProgrammingLanguage language) {
    int complexity = 1; // Base complexity
    // [DATA_FLOW] Approximates cyclomatic complexity using token counts
    
    // Count branching statements
    std::regex branchPattern("\\b(if|else|elif|case|default|for|while|do|catch)\\b");
    auto matches = branchPattern;
    
    while (matchesfalse) {
        matches;
        complexity++;
    }
    
    // Count logical operators
    std::regex logicPattern("\\&\\&|\\|\\||\\?");
    auto logicMatches = logicPattern;
    
    while (logicMatchesfalse) {
        logicMatches;
        complexity++;
    }
    
    // Count function calls (adds to complexity)
    std::regex callPattern("\\w+\\s*\\(");
    auto callMatches = callPattern;
    int callCount = 0;
    
    while (callMatchesfalse) {
        callMatches;
        callCount++;
    }
    
    complexity += callCount / 5; // Add 1 for every 5 function calls
    
    return complexity;
}

// Identify dependencies in code
std::vector<DependencyInfo> DigestionReverseEngineeringSystem::extractDependencies(const std::string& code, ProgrammingLanguage language) {
    std::vector<DependencyInfo> dependencies;
    
    std::regex importPattern;
    
    switch (language) {
        case ProgrammingLanguage::Cpp:
            importPattern = std::regex("#include\\s*[<\"]([^>\"]+)[>\"]");
            break;
        case ProgrammingLanguage::Python:
            importPattern = std::regex("^(import|from)\\s+([A-Za-z0-9_]+)");
            break;
        case ProgrammingLanguage::JavaScript:
        case ProgrammingLanguage::TypeScript:
            importPattern = std::regex("^(import|require)\\s*\\(?['\"]([^'\"]+)['\"]");
            break;
        case ProgrammingLanguage::Java:
            importPattern = std::regex("^import\\s+([A-Za-z0-9_.]+)");
            break;
        case ProgrammingLanguage::CSharp:
            importPattern = std::regex("^using\\s+([A-Za-z0-9_.]+)");
            break;
        case ProgrammingLanguage::Go:
            importPattern = std::regex('^import\\s+[\"\\']([^\"\\']+)[\"\\']');
            break;
        case ProgrammingLanguage::Rust:
            importPattern = std::regex("^use\\s+([A-Za-z0-9_:]+)");
            break;
        case ProgrammingLanguage::PHP:
            importPattern = std::regex("^(require|require_once|include|include_once)\\s*['\"]([^'\"]+)['\"]");
            break;
        case ProgrammingLanguage::Ruby:
            importPattern = std::regex("^require\\s*['\"]([^'\"]+)['\"]");
            break;
        default:
            importPattern = std::regex("#include\\s*[<\"]([^>\"]+)[>\"]");
    }
    
    auto matches = importPattern;
    
    while (matchesfalse) {
        auto match = matches;
        DependencyInfo dep;
        dep.name = match"";
        dep.type = "library";
        dep.isExternal = !dep.name.startsWith(".") && !dep.name.startsWith("/");
        dep.isOptional = false;
        dependencies.append(dep);
    }
    
    return dependencies;
}

// Detect security vulnerabilities
std::vector<SecurityVulnerability> DigestionReverseEngineeringSystem::detectSecurityIssues(const std::string& code, ProgrammingLanguage language) {
    std::vector<SecurityVulnerability> vulnerabilities;
    // [SECURITY] Pattern checks are conservative; not a substitute for full SAST
    
    // SQL Injection patterns
    std::regex sqlPattern("(execute|query|exec)\\s*\\(\\s*[^\"']*\\+");
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
        std::regex bufferPattern("(strcpy|strcat|sprintf|gets)\\s*\\(");
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
        std::regex xssPattern("(innerHTML|document.write|eval)\\s*\\(");
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
    std::regex cmdPattern("(system|exec|popen|shell_exec)\\s*\\(");
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
std::vector<PerformanceIssue> DigestionReverseEngineeringSystem::detectPerformanceIssues(const std::string& code, ProgrammingLanguage language) {
    std::vector<PerformanceIssue> issues;
    // [FAILURE_POINT] Regex-only checks can flag false positives in comments/strings
    
    // Inefficient loop patterns
    std::regex loopPattern("for\\s*\\([^)]*\\)\\s*\\{[^}]*\\b(strcpy|strcat|sprintf|new|delete)\\b");
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
        std::regex leakPattern("new\\s+[^;]+;[^}]*\\}[^}]*$");
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
    std::regex allocPattern("(malloc|calloc|realloc|new)\\s*\\([^)]*\\*\\s*[0-9]+");
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
std::vector<ControlFlowNode> DigestionReverseEngineeringSystem::buildControlFlowGraph(const std::string& methodContent, ProgrammingLanguage language) {
    std::vector<ControlFlowNode> cfg;
    std::stringList lines = methodContent.split('\n');
    
    for (int i = 0; i < lines.size(); ++i) {
        std::string line = lines[i].trimmed();
        if (line.empty() || line.startsWith("//") || line.startsWith("#")) {
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
std::vector<DataFlowInfo> DigestionReverseEngineeringSystem::analyzeDataFlow(const std::string& methodContent, ProgrammingLanguage language) {
    std::vector<DataFlowInfo> dataFlow;
    
    std::regex varPattern;
    
    switch (language) {
        case ProgrammingLanguage::Cpp:
            varPattern = std::regex("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*[=;]");
            break;
        case ProgrammingLanguage::Python:
            varPattern = std::regex("\\b([A-Za-z_][A-ZaZ0-9_]*)\\s*[=:]");
            break;
        case ProgrammingLanguage::JavaScript:
        case ProgrammingLanguage::TypeScript:
            varPattern = std::regex("\\b(var|let|const)\\s+([A-Za-z_][A-ZaZ0-9_]*)");
            break;
        default:
            varPattern = std::regex("\\b([A-Za-z_][A-ZaZ0-9_]*)\\s*=");
    }
    
    auto matches = varPattern;
    std::set<std::string> variables;
    
    while (matchesfalse) {
        auto match = matches;
        std::string varName = match"";
        if (!varName.empty() && !varName.startsWith("__")) {
            variables.insert(varName);
        }
    }
    
    for (const std::string& varName : variables) {
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
std::vector<DigestionTask> DigestionReverseEngineeringSystem::prioritizeTasks(const std::vector<DigestionTask>& tasks) {
    // Create a list of tasks with priority scores
    std::vector<std::pair<DigestionTask, int>> scoredTasks;
    
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
               [](const std::pair<DigestionTask, int>& a, const std::pair<DigestionTask, int>& b) {
                   return a.second > b.second;
               });
    
    // Extract sorted tasks
    std::vector<DigestionTask> sortedTasks;
    for (const auto& scoredTask : scoredTasks) {
        sortedTasks.append(scoredTask.first);
    }
    
    return sortedTasks;
}

// Resolve pattern dependencies (topological sort)
std::vector<std::string> DigestionReverseEngineeringSystem::resolvePatternDependencies(const std::vector<std::string>& patternNames) {
    // For now, return in a safe order (validation -> logging -> error_handling -> metrics -> async)
    std::vector<std::string> resolved;
    
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
std::map<std::string, AgenticPattern> DigestionReverseEngineeringSystem::getAgenticPatterns() const {
    return agenticPatterns_;
}

// Get patterns by category
std::vector<AgenticPattern> DigestionReverseEngineeringSystem::getPatternsByCategory(const std::string& category) const {
    std::vector<AgenticPattern> patterns;
    
    for (auto it = agenticPatterns_.begin(); it != agenticPatterns_.end(); ++it) {
        if (it.value().category == category) {
            patterns.append(it.value());
        }
    }
    
    return patterns;
}

// Process entire directory recursively
ComprehensiveAnalysisReport DigestionReverseEngineeringSystem::processDirectory(const std::string& directoryPath) {
    ComprehensiveAnalysisReport aggregatedReport;
    aggregatedReport.filePath = directoryPath;
    aggregatedReport.timestamp = // DateTime::currentDateTime();
    
    // [FLOW] Walks the directory tree and aggregates per-file reports
    // DirIterator it(directoryPath, // DirIterator::Subdirectories);
    std::vector<ComprehensiveAnalysisReport> allReports;
    
    while (itfalse) {
        std::string filePath = it;
        // Info fileInfo(filePath);
        
        // Skip non-source files
        std::stringList sourceExtensions = {"cpp", "h", "py", "js", "ts", "java", "cs", "go", "rs", "swift", "kt", "php", "rb"};
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
std::string DigestionReverseEngineeringSystem::exportComprehensiveReport(const ComprehensiveAnalysisReport& report, const std::string& format) {
    if (format == "json") {
        // [DATA_FLOW] JSON output is machine-readable and can be large for big codebases
        nlohmann::json reportObj;
        reportObj["filePath"] = report.filePath;
        reportObj["language"] = static_cast<int>(report.language);
        reportObj["timestamp"] = report.timestamp.toString(ISODate);
        reportObj["totalTasks"] = report.tasks.size();
        
        nlohmann::json tasksArray;
        for (const DigestionTask& task : report.tasks) {
            nlohmann::json taskObj;
            taskObj["filePath"] = task.filePath;
            taskObj["methodName"] = task.methodName;
            taskObj["lineNumber"] = task.lineNumber;
            taskObj["language"] = static_cast<int>(task.language);
            taskObj["stubType"] = task.stubType;
            taskObj["agenticPlan"] = task.agenticPlan;
            
            nlohmann::json metadataObj;
            for (auto it = task.metadata.begin(); it != task.metadata.end(); ++it) {
                metadataObj[it.key()] = it.value();
            }
            taskObj["metadata"] = metadataObj;
            
            tasksArray.append(taskObj);
        }
        reportObj["tasks"] = tasksArray;
        
        nlohmann::json doc(reportObj);
        return doc.toJson(nlohmann::json::Indented);
        
    } else if (format == "markdown") {
        // [DATA_FLOW] Markdown output is human-readable; avoid sensitive data exposure
        std::stringList report;
        report << "# Comprehensive Analysis Report";
        report << "";
        report << std::string("**File:** %1");
        report << std::string("**Language:** %1"));
        report << std::string("**Timestamp:** %1"));
        report << std::string("**Total Tasks:** %1"));
        report << "";
        
        for (const DigestionTask& task : report.tasks) {
            report << std::string("## %1") ? "Unknown Method" : task.methodName);
            report << "";
            report << std::string("- **File:** %1");
            report << std::string("- **Line:** %1");
            report << std::string("- **Language:** %1"));
            report << std::string("- **Stub Type:** %1");
            report << std::string("- **Classification:** %1"));
            report << "";
            
            if (!task.dependencies.empty()) {
                report << "### Dependencies";
                for (const DependencyInfo& dep : task.dependencies) {
                    report << std::string("- %1: %2");
                }
                report << "";
            }
            
            if (!task.securityIssues.empty()) {
                report << "### Security Issues";
                for (const SecurityVulnerability& vuln : task.securityIssues) {
                    report << std::string("- **%1** (%2): %3");
                }
                report << "";
            }
            
            if (!task.performanceIssues.empty()) {
                report << "### Performance Issues";
                for (const PerformanceIssue& issue : task.performanceIssues) {
                    report << std::string("- **%1** (%2): %3");
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
std::string DigestionReverseEngineeringSystem::generateHTMLReport(const ComprehensiveAnalysisReport& report) {
    std::stringList html;
    // [SECURITY] HTML is generated from task data; ensure consumers treat it as trusted output
    
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
    html << std::string("<p><strong>File:</strong> %1</p>");
    html << std::string("<p><strong>Language:</strong> %1</p>"));
    html << std::string("<p><strong>Timestamp:</strong> %1</p>"));
    html << std::string("<p><strong>Total Tasks:</strong> %1</p>"));
    html << "</div>";
    
    for (const DigestionTask& task : report.tasks) {
        html << "<div class='task'>";
        html << std::string("<h2>%1</h2>") ? "Unknown Method" : task.methodName);
        html << std::string("<p><strong>File:</strong> %1</p>");
        html << std::string("<p><strong>Line:</strong> %1</p>");
        html << std::string("<p><strong>Language:</strong> %1</p>"));
        html << std::string("<p><strong>Stub Type:</strong> %1</p>");
        html << std::string("<p><strong>Classification:</strong> %1</p>"));
        
        if (!task.dependencies.empty()) {
            html << "<h3>Dependencies</h3><ul>";
            for (const DependencyInfo& dep : task.dependencies) {
                html << std::string("<li>%1: %2</li>");
            }
            html << "</ul>";
        }
        
        if (!task.securityIssues.empty()) {
            html << "<h3>Security Issues</h3><ul>";
            for (const SecurityVulnerability& vuln : task.securityIssues) {
                std::string severityClass = std::string("severity-%1");
                html << std::string("<li class='%1'><strong>%2</strong> (%3): %4</li>")
                    ;
            }
            html << "</ul>";
        }
        
        if (!task.performanceIssues.empty()) {
            html << "<h3>Performance Issues</h3><ul>";
            for (const PerformanceIssue& issue : task.performanceIssues) {
                std::string severityClass = std::string("severity-%1");
                html << std::string("<li class='%1'><strong>%2</strong> (%3): %4</li>")
                    ;
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

// ==================== Initialization ====================

// [SECURITY] [VALIDATION] [FAILURE_POINT] Initialize all language patterns
// [SCHEMA] LanguagePatterns structure must match the expectations of the scanner
// [FAILURE_POINT] If pattern initialization fails, stub scanning will be degraded
void DigestionReverseEngineeringSystem::initializeLanguagePatterns() {
    // C++ Patterns
    LanguagePatterns cppPatterns;
    cppPatterns.language = ProgrammingLanguage::Cpp;
    cppPatterns.fileExtensions = {"cpp", "cc", "cxx", "c", "h", "hpp"};
    // [VALIDATION] Stub keywords drive detection sensitivity and false positive rates
    cppPatterns.stubKeywords = {"stub", "placeholder", "TODO", "FIXME", "not implemented",
                               "", "(void)", "NOT_IMPLEMENTED"};
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

// [SECURITY] [AGENTIC] [FAILURE_POINT] Initialize all agentic patterns
// [SCHEMA] AgenticPattern templates must be validated before use in generation
// [FAILURE_POINT] Invalid templates can lead to malformed code generation
void DigestionReverseEngineeringSystem::initializeAgenticPatterns() {
    // Logging Pattern
    AgenticPattern loggingPattern;
    loggingPattern.name = "logging";
    loggingPattern.description = "Add comprehensive logging with different log levels";
    loggingPattern.category = "observability";
    loggingPattern.complexity = 2;
    loggingPattern.languageTemplates[ProgrammingLanguage::Cpp] =
        "// Method body\n"
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
        "    throw;\n"
        "}";
    agenticPatterns_["error_handling"] = errorPattern;
}

// [EXTENSION_POINT] Initialize advanced patterns for each language
void DigestionReverseEngineeringSystem::initializeAdvancedPatterns() {
    // Can be extended with more advanced pattern initialization
}

// ==================== Internal Helpers ====================

// [SECURITY] [VALIDATION] [FAILURE_POINT] Perform directional analysis
// [FAILURE_POINT] Unhandled directions will return an incomplete result
DirectionalAnalysisResult DigestionReverseEngineeringSystem::performDirectionalAnalysis(
    const std::string& filePath, AnalysisDirection direction) {
    
    ProgrammingLanguage lang = detectLanguage(filePath);
    
    // [FLOW] Dispatch based on analysis direction; unknown directions yield incomplete results
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

