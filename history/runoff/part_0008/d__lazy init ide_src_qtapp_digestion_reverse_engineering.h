// digestion_reverse_engineering.h
// Advanced Multi-Language Agentic System for Full Reverse Engineering and Autonomous Code Generation
// Created: 2026-01-24
// Enhanced: 2026-01-24 (Multi-Directional Reverse Engineering & Advanced Agentic Automation)
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

#pragma once
#include <QString>
#include <QVector>
#include <QMap>
#include <QSet>
#include <QSharedPointer>

// Supported programming languages (expanded)
enum class ProgrammingLanguage {
    Unknown,
    Cpp,
    CSharp,
    Python,
    JavaScript,
    TypeScript,
    Java,
    Go,
    Rust,
    Swift,
    Kotlin,
    PHP,
    Ruby,
    ObjectiveC,
    Assembly,
    SQL,
    HTML_CSS,
    YAML_JSON,
    Shell_Bash,
    PowerShell,
    Markdown,
    XML
};

// Analysis directions for reverse engineering
enum class AnalysisDirection {
    ControlFlow,      // Function calls, recursion, loops, branching
    DataFlow,         // Variable lifecycles, transformations, state changes
    Dependencies,     // External libraries, APIs, imports, coupling
    Security,         // Vulnerabilities, unsafe code, injection points
    Performance,      // Bottlenecks, memory leaks, inefficient patterns
    APISurface,       // Public interfaces, endpoints, contracts
    Architecture,     // Design patterns, anti-patterns, modularity
    ResourceUsage,    // Memory, CPU, I/O, network usage patterns
    ErrorPropagation, // Exception paths, error handling coverage
    Concurrency       // Thread safety, race conditions, deadlocks
};

// Stub classification types
enum class StubClassification {
    NotStub,
    EmptyImplementation,
    PlaceholderComment,
    TODO_Fixme,
    NotImplementedException,
    NoOperation,
    FutureExtension,
    Deprecated,
    Mock_Stub,
    Prototype
};

// Language-specific patterns with enhanced metadata
struct LanguagePatterns {
    ProgrammingLanguage language;
    QStringList fileExtensions;
    QStringList stubKeywords;
    QStringList commentPatterns;
    QStringList methodPatterns;
    QStringList asyncPatterns;
    QStringList errorHandlingPatterns;
    QStringList loggingPatterns;
    QStringList securityPatterns;        // Security-related patterns
    QStringList performancePatterns;     // Performance-related patterns
    QStringList dependencyPatterns;      // Import/include/require patterns
    QStringList dangerousPatterns;       // Unsafe/deprecated patterns
    QMap<QString, QString> idioms;       // Language-specific idioms
    QMap<QString, QString> bestPractices; // Best practice patterns
};

// Control flow graph node
struct ControlFlowNode {
    int id;
    QString type; // "entry", "exit", "call", "branch", "loop", "return"
    QString description;
    int lineNumber;
    QMap<QString, QVariant> metadata;
    QVector<int> successors;
    QVector<int> predecessors;
};

// Data flow tracking
struct DataFlowInfo {
    QString variableName;
    QString dataType;
    int declarationLine;
    QVector<int> usageLines;
    QVector<int> modificationLines;
    QStringList transformations; // How the data is transformed
    QMap<QString, QString> metadata; // Scope, lifetime, etc.
};

// Dependency information
struct DependencyInfo {
    QString type; // "library", "module", "api", "service", "file"
    QString name;
    QString version;
    int lineNumber;
    QString usageContext;
    bool isExternal;
    bool isOptional;
    QMap<QString, QString> metadata;
};

// Security vulnerability pattern
struct SecurityVulnerability {
    QString type; // "injection", "buffer_overflow", "race_condition", etc.
    QString severity; // "critical", "high", "medium", "low"
    int lineNumber;
    QString description;
    QString remediation;
    QMap<QString, QString> metadata;
};

// Performance issue
struct PerformanceIssue {
    QString type; // "bottleneck", "memory_leak", "inefficient_loop", etc.
    QString severity;
    int lineNumber;
    QString description;
    QString optimization;
    QMap<QString, QString> metadata;
};

// Enhanced agentic automation patterns
struct AgenticPattern;  // Forward declaration

// Code generation result with enhanced metadata
struct CodeGenerationResult {
    bool success = false;
    QString generatedCode;
    QString errorMessage;
    QVector<QString> warnings;
    QMap<QString, QVariant> metrics; // Lines added, complexity, etc.
    QMap<QString, QString> dependencies; // Required imports/includes
};

// Enhanced agentic automation patterns (full definition)
struct AgenticPattern {
    QString name; // "logging", "error_handling", "async", "metrics", "validation", "security", "caching", "circuit_breaker"
    QString description;
    QString category; // "observability", "reliability", "performance", "security", "maintainability"
    int complexity; // 1-10
    QMap<ProgrammingLanguage, QString> languageTemplates;
    QMap<QString, QString> configuration; // Pattern-specific configuration
    QVector<QString> prerequisites; // Required patterns to apply first
    QVector<QString> dependencies; // External dependencies needed
};

// Enhanced digestion task with multi-dimensional analysis
struct DigestionTask {
    QString filePath;
    QString methodName;
    int lineNumber;
    ProgrammingLanguage language;
    StubClassification classification;
    QString stubType;
    QString stubContext;
    QString agenticPlan;
    QMap<QString, QString> metadata;
    
    // Multi-dimensional analysis results
    QVector<ControlFlowNode> controlFlowGraph;
    QVector<DataFlowInfo> dataFlowInfo;
    QVector<DependencyInfo> dependencies;
    QVector<SecurityVulnerability> securityIssues;
    QVector<PerformanceIssue> performanceIssues;
    
    // Agentic automation state
    QMap<QString, bool> appliedPatterns;
    QMap<QString, CodeGenerationResult> generatedCode;
    QVector<QString> recommendations;
};

// Analysis result for a specific direction
struct DirectionalAnalysisResult {
    AnalysisDirection direction;
    bool completed;
    QVector<QMap<QString, QVariant>> findings;
    QString summary;
    QMap<QString, QVariant> metrics;
    QVector<QString> recommendations;
};

// Comprehensive analysis report
struct ComprehensiveAnalysisReport {
    QString filePath;
    ProgrammingLanguage language;
    QDateTime timestamp;
    QVector<DigestionTask> tasks;
    QMap<AnalysisDirection, DirectionalAnalysisResult> directionalResults;
    QMap<QString, QVariant> aggregatedMetrics;
    QMap<QString, QString> recommendations;
    QMap<QString, QVariant> statistics;
};

// Advanced reverse engineering system with multi-directional analysis
class DigestionReverseEngineeringSystem {
public:
    // Constructor - initialize all patterns and templates
    DigestionReverseEngineeringSystem();

    // ==================== Language Detection & Analysis ====================
    
    // Detect programming language from file extension and content
    ProgrammingLanguage detectLanguage(const QString& filePath);
    
    // Detect language from content only (when extension is ambiguous)
    ProgrammingLanguage detectLanguageFromContent(const QString& content);
    
    // Get language patterns for a specific language
    LanguagePatterns getLanguagePatterns(ProgrammingLanguage language) const;
    
    // Get all supported languages
    QVector<ProgrammingLanguage> getSupportedLanguages() const;

    // ==================== Multi-Directional Scanning ====================
    
    // Scan a file for stubs/placeholders with basic analysis
    QVector<DigestionTask> scanFileForStubs(const QString& filePath);
    
    // Scan with specific analysis directions
    QVector<DigestionTask> scanFileWithDirections(const QString& filePath, 
                                                 const QSet<AnalysisDirection>& directions);
    
    // Perform comprehensive multi-directional analysis
    ComprehensiveAnalysisReport performComprehensiveAnalysis(const QString& filePath);
    
    // Scan multiple files with parallel processing
    QVector<DigestionTask> scanMultipleFiles(const QStringList& filePaths);

    // ==================== Directional Analysis Methods ====================
    
    // Control flow analysis - build CFG, identify recursion, loops
    DirectionalAnalysisResult analyzeControlFlow(const QString& filePath, ProgrammingLanguage language);
    
    // Data flow analysis - track variables, transformations, lifecycles
    DirectionalAnalysisResult analyzeDataFlow(const QString& filePath, ProgrammingLanguage language);
    
    // Dependency analysis - identify external dependencies
    DirectionalAnalysisResult analyzeDependencies(const QString& filePath, ProgrammingLanguage language);
    
    // Security analysis - detect vulnerabilities, unsafe patterns
    DirectionalAnalysisResult analyzeSecurity(const QString& filePath, ProgrammingLanguage language);
    
    // Performance analysis - identify bottlenecks, inefficiencies
    DirectionalAnalysisResult analyzePerformance(const QString& filePath, ProgrammingLanguage language);
    
    // API surface analysis - identify public interfaces
    DirectionalAnalysisResult analyzeAPISurface(const QString& filePath, ProgrammingLanguage language);
    
    // Architectural analysis - identify patterns, anti-patterns
    DirectionalAnalysisResult analyzeArchitecture(const QString& filePath, ProgrammingLanguage language);

    // ==================== Agentic Automation ====================
    
    // Generate comprehensive agentic extension plan
    QString generateAgenticPlan(const DigestionTask& task);
    
    // Generate code for a specific agentic pattern
    CodeGenerationResult generateAgenticCode(const QString& patternName, 
                                           ProgrammingLanguage language,
                                           const QMap<QString, QString>& parameters);
    
    // Apply a single agentic pattern to a task
    bool applyAgenticPattern(const QString& filePath, DigestionTask& task, const QString& patternName);
    
    // Apply multiple agentic patterns with dependency resolution
    QVector<bool> applyMultiplePatterns(const QString& filePath, 
                                       DigestionTask& task,
                                       const QVector<QString>& patternNames);
    
    // Apply full agentic automation suite
    bool applyFullAgenticSuite(const QString& filePath, DigestionTask& task);
    
    // Apply agentic automation to multiple tasks
    QVector<bool> applyAgenticExtensions(const QString& filePath, 
                                        QVector<DigestionTask>& tasks);

    // ==================== Chaining & Recursion ====================
    
    // Chain digestion to next file with automatic recursion
    void chainToNextFile(const QString& nextFilePath);
    
    // Chain to multiple files with parallel processing
    void chainToMultipleFiles(const QStringList& filePaths);
    
    // Recursive analysis - follow dependencies and analyze recursively
    void performRecursiveAnalysis(const QString& entryPointFile);
    
    // Chain with specific directions
    void chainWithDirections(const QString& nextFilePath, const QSet<AnalysisDirection>& directions);

    // ==================== Reporting & Export ====================
    
    // Get statistics about scanned files and stubs
    QMap<QString, QVariant> getStatistics() const;
    
    // Export comprehensive analysis report
    QString exportComprehensiveReport(const ComprehensiveAnalysisReport& report, 
                                     const QString& format = "json");
    
    // Export digestion report (legacy)
    QString exportReport(const QVector<DigestionTask>& tasks, const QString& format = "json");
    
    // Generate HTML report with visualizations
    QString generateHTMLReport(const ComprehensiveAnalysisReport& report);
    
    // Generate markdown summary
    QString generateMarkdownSummary(const ComprehensiveAnalysisReport& report);

    // ==================== Pattern Management ====================
    
    // Register custom language patterns
    void registerLanguagePatterns(const LanguagePatterns& patterns);
    
    // Register custom agentic patterns
    void registerAgenticPattern(const AgenticPattern& pattern);
    
    // Get all registered agentic patterns
    QMap<QString, AgenticPattern> getAgenticPatterns() const;
    
    // Get patterns by category
    QVector<AgenticPattern> getPatternsByCategory(const QString& category) const;

    // ==================== Utility Methods ====================
    
    // Extract method context from source code
    QString extractMethodContext(const QString& filePath, int lineNumber, ProgrammingLanguage language);
    
    // Parse method signature from source code
    QMap<QString, QString> parseMethodSignature(const QString& context, ProgrammingLanguage language);
    
    // Generate language-specific code from template
    QString generateCodeFromTemplate(const QString& templateStr, 
                                   const QMap<QString, QString>& parameters);
    
    // Validate generated code syntax
    bool validateGeneratedCode(const QString& code, ProgrammingLanguage language);
    
    // Classify stub type
    StubClassification classifyStub(const QString& content, ProgrammingLanguage language);
    
    // Get complexity score for a method
    int calculateComplexity(const QString& methodContent, ProgrammingLanguage language);
    
    // Identify dependencies in code
    QVector<DependencyInfo> extractDependencies(const QString& code, ProgrammingLanguage language);
    
    // Detect security vulnerabilities
    QVector<SecurityVulnerability> detectSecurityIssues(const QString& code, ProgrammingLanguage language);
    
    // Detect performance issues
    QVector<PerformanceIssue> detectPerformanceIssues(const QString& code, ProgrammingLanguage language);
    
    // Build control flow graph
    QVector<ControlFlowNode> buildControlFlowGraph(const QString& methodContent, ProgrammingLanguage language);

    // ==================== Batch Operations ====================
    
    // Process entire directory recursively
    ComprehensiveAnalysisReport processDirectory(const QString& directoryPath);
    
    // Process with filters (file patterns, size limits, etc.)
    ComprehensiveAnalysisReport processDirectoryWithFilters(const QString& directoryPath,
                                                           const QMap<QString, QVariant>& filters);
    
    // Batch apply agentic patterns
    QMap<QString, QVector<bool>> batchApplyPatterns(const QStringList& filePaths,
                                                   const QVector<QString>& patternNames);

private:
    // ==================== Internal Data Structures ====================
    
    // Language patterns database
    QMap<ProgrammingLanguage, LanguagePatterns> languagePatterns_;
    
    // Agentic patterns database
    QMap<QString, AgenticPattern> agenticPatterns_;
    
    // Analysis statistics
    mutable QMap<QString, QVariant> statistics_;
    
    // Cache for parsed files
    QMap<QString, QString> fileCache_;
    
    // Cache for analysis results
    QMap<QString, ComprehensiveAnalysisReport> analysisCache_;

    // ==================== Initialization Methods ====================
    
    // Initialize all language patterns
    void initializeLanguagePatterns();
    
    // Initialize all agentic patterns
    void initializeAgenticPatterns();
    
    // Initialize advanced patterns for each language
    void initializeAdvancedPatterns();

    // ==================== Internal Analysis Methods ====================
    
    // Perform directional analysis based on direction type
    DirectionalAnalysisResult performDirectionalAnalysis(const QString& filePath, 
                                                        AnalysisDirection direction);
    
    // Merge multiple directional results into comprehensive report
    void mergeDirectionalResults(ComprehensiveAnalysisReport& report,
                                const QMap<AnalysisDirection, DirectionalAnalysisResult>& results);
    
    // Analyze file content and extract all relevant information
    void analyzeFileContent(const QString& content, 
                           ProgrammingLanguage language,
                           DigestionTask& task);
    
    // Generate recommendations based on analysis
    void generateRecommendations(DigestionTask& task);
    
    // Prioritize tasks based on complexity and impact
    QVector<DigestionTask> prioritizeTasks(const QVector<DigestionTask>& tasks);

    // ==================== Code Generation Helpers ====================
    
    // Get required dependencies for a pattern
    QStringList getPatternDependencies(const QString& patternName, ProgrammingLanguage language);
    
    // Generate import/include statements
    QString generateImportStatements(const QStringList& dependencies, ProgrammingLanguage language);
    
    // Pattern dependency resolution (topological sort)
    QVector<QString> resolvePatternDependencies(const QVector<QString>& patternNames);
    
    // Generate test code for agentic patterns
    QString generateTestCode(const DigestionTask& task, const QString& patternName);
};
