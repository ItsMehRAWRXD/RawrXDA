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
    std::stringList fileExtensions;
    std::stringList stubKeywords;
    std::stringList commentPatterns;
    std::stringList methodPatterns;
    std::stringList asyncPatterns;
    std::stringList errorHandlingPatterns;
    std::stringList loggingPatterns;
    std::stringList securityPatterns;        // Security-related patterns
    std::stringList performancePatterns;     // Performance-related patterns
    std::stringList dependencyPatterns;      // Import/include/require patterns
    std::stringList dangerousPatterns;       // Unsafe/deprecated patterns
    std::map<std::string, std::string> idioms;       // Language-specific idioms
    std::map<std::string, std::string> bestPractices; // Best practice patterns
};

// Control flow graph node
struct ControlFlowNode {
    int id;
    std::string type; // "entry", "exit", "call", "branch", "loop", "return"
    std::string description;
    int lineNumber;
    std::map<std::string, std::any> metadata;
    std::vector<int> successors;
    std::vector<int> predecessors;
};

// Data flow tracking
struct DataFlowInfo {
    std::string variableName;
    std::string dataType;
    int declarationLine;
    std::vector<int> usageLines;
    std::vector<int> modificationLines;
    std::stringList transformations; // How the data is transformed
    std::map<std::string, std::string> metadata; // Scope, lifetime, etc.
};

// Dependency information
struct DependencyInfo {
    std::string type; // "library", "module", "api", "service", "file"
    std::string name;
    std::string version;
    int lineNumber;
    std::string usageContext;
    bool isExternal;
    bool isOptional;
    std::map<std::string, std::string> metadata;
};

// Security vulnerability pattern
struct SecurityVulnerability {
    std::string type; // "injection", "buffer_overflow", "race_condition", etc.
    std::string severity; // "critical", "high", "medium", "low"
    int lineNumber;
    std::string description;
    std::string remediation;
    std::map<std::string, std::string> metadata;
};

// Performance issue
struct PerformanceIssue {
    std::string type; // "bottleneck", "memory_leak", "inefficient_loop", etc.
    std::string severity;
    int lineNumber;
    std::string description;
    std::string optimization;
    std::map<std::string, std::string> metadata;
};

// Enhanced agentic automation patterns
struct AgenticPattern;  // Forward declaration

// Code generation result with enhanced metadata
struct CodeGenerationResult {
    bool success = false;
    std::string generatedCode;
    std::string errorMessage;
    std::vector<std::string> warnings;
    std::map<std::string, std::any> metrics; // Lines added, complexity, etc.
    std::map<std::string, std::string> dependencies; // Required imports/includes
};

// Enhanced agentic automation patterns (full definition)
struct AgenticPattern {
    std::string name; // "logging", "error_handling", "async", "metrics", "validation", "security", "caching", "circuit_breaker"
    std::string description;
    std::string category; // "observability", "reliability", "performance", "security", "maintainability"
    int complexity; // 1-10
    std::map<ProgrammingLanguage, std::string> languageTemplates;
    std::map<std::string, std::string> configuration; // Pattern-specific configuration
    std::vector<std::string> prerequisites; // Required patterns to apply first
    std::vector<std::string> dependencies; // External dependencies needed
};

// Enhanced digestion task with multi-dimensional analysis
struct DigestionTask {
    std::string filePath;
    std::string methodName;
    int lineNumber;
    ProgrammingLanguage language;
    StubClassification classification;
    std::string stubType;
    std::string stubContext;
    std::string agenticPlan;
    std::map<std::string, std::string> metadata;
    
    // Multi-dimensional analysis results
    std::vector<ControlFlowNode> controlFlowGraph;
    std::vector<DataFlowInfo> dataFlowInfo;
    std::vector<DependencyInfo> dependencies;
    std::vector<SecurityVulnerability> securityIssues;
    std::vector<PerformanceIssue> performanceIssues;
    
    // Agentic automation state
    std::map<std::string, bool> appliedPatterns;
    std::map<std::string, CodeGenerationResult> generatedCode;
    std::vector<std::string> recommendations;
};

// Analysis result for a specific direction
struct DirectionalAnalysisResult {
    AnalysisDirection direction;
    bool completed;
    std::vector<std::map<std::string, std::any>> findings;
    std::string summary;
    std::map<std::string, std::any> metrics;
    std::vector<std::string> recommendations;
};

// Comprehensive analysis report
struct ComprehensiveAnalysisReport {
    std::string filePath;
    ProgrammingLanguage language;
    // DateTime timestamp;
    std::vector<DigestionTask> tasks;
    std::map<AnalysisDirection, DirectionalAnalysisResult> directionalResults;
    std::map<std::string, std::any> aggregatedMetrics;
    std::map<std::string, std::string> recommendations;
    std::map<std::string, std::any> statistics;
};

// Advanced reverse engineering system with multi-directional analysis
class DigestionReverseEngineeringSystem {
public:
    // Constructor - initialize all patterns and templates
    DigestionReverseEngineeringSystem();

    // ==================== Language Detection & Analysis ====================
    
    // Detect programming language from file extension and content
    ProgrammingLanguage detectLanguage(const std::string& filePath);
    
    // Detect language from content only (when extension is ambiguous)
    ProgrammingLanguage detectLanguageFromContent(const std::string& content);
    
    // Get language patterns for a specific language
    LanguagePatterns getLanguagePatterns(ProgrammingLanguage language) const;
    
    // Get all supported languages
    std::vector<ProgrammingLanguage> getSupportedLanguages() const;

    // ==================== Multi-Directional Scanning ====================
    
    // Scan a file for stubs/placeholders with basic analysis
    std::vector<DigestionTask> scanFileForStubs(const std::string& filePath);
    
    // Scan with specific analysis directions
    std::vector<DigestionTask> scanFileWithDirections(const std::string& filePath, 
                                                 const QSet<AnalysisDirection>& directions);
    
    // Perform comprehensive multi-directional analysis
    ComprehensiveAnalysisReport performComprehensiveAnalysis(const std::string& filePath);
    
    // Scan multiple files with parallel processing
    std::vector<DigestionTask> scanMultipleFiles(const std::stringList& filePaths);

    // ==================== Directional Analysis Methods ====================
    
    // Control flow analysis - build CFG, identify recursion, loops
    DirectionalAnalysisResult analyzeControlFlow(const std::string& filePath, ProgrammingLanguage language);
    
    // Data flow analysis - track variables, transformations, lifecycles
    DirectionalAnalysisResult analyzeDataFlow(const std::string& filePath, ProgrammingLanguage language);
    
    // Dependency analysis - identify external dependencies
    DirectionalAnalysisResult analyzeDependencies(const std::string& filePath, ProgrammingLanguage language);
    
    // Security analysis - detect vulnerabilities, unsafe patterns
    DirectionalAnalysisResult analyzeSecurity(const std::string& filePath, ProgrammingLanguage language);
    
    // Performance analysis - identify bottlenecks, inefficiencies
    DirectionalAnalysisResult analyzePerformance(const std::string& filePath, ProgrammingLanguage language);
    
    // API surface analysis - identify public interfaces
    DirectionalAnalysisResult analyzeAPISurface(const std::string& filePath, ProgrammingLanguage language);
    
    // Architectural analysis - identify patterns, anti-patterns
    DirectionalAnalysisResult analyzeArchitecture(const std::string& filePath, ProgrammingLanguage language);

    // ==================== Agentic Automation ====================
    
    // Generate comprehensive agentic extension plan
    std::string generateAgenticPlan(const DigestionTask& task);
    
    // Generate code for a specific agentic pattern
    CodeGenerationResult generateAgenticCode(const std::string& patternName, 
                                           ProgrammingLanguage language,
                                           const std::map<std::string, std::string>& parameters);
    
    // Apply a single agentic pattern to a task
    bool applyAgenticPattern(const std::string& filePath, DigestionTask& task, const std::string& patternName);
    
    // Apply multiple agentic patterns with dependency resolution
    std::vector<bool> applyMultiplePatterns(const std::string& filePath, 
                                       DigestionTask& task,
                                       const std::vector<std::string>& patternNames);
    
    // Apply full agentic automation suite
    bool applyFullAgenticSuite(const std::string& filePath, DigestionTask& task);
    
    // Apply agentic automation to multiple tasks
    std::vector<bool> applyAgenticExtensions(const std::string& filePath, 
                                        std::vector<DigestionTask>& tasks);

    // ==================== Chaining & Recursion ====================
    
    // Chain digestion to next file with automatic recursion
    void chainToNextFile(const std::string& nextFilePath);
    
    // Chain to multiple files with parallel processing
    void chainToMultipleFiles(const std::stringList& filePaths);
    
    // Recursive analysis - follow dependencies and analyze recursively
    void performRecursiveAnalysis(const std::string& entryPointFile);
    
    // Chain with specific directions
    void chainWithDirections(const std::string& nextFilePath, const QSet<AnalysisDirection>& directions);

    // ==================== Reporting & Export ====================
    
    // Get statistics about scanned files and stubs
    std::map<std::string, std::any> getStatistics() const;
    
    // Export comprehensive analysis report
    std::string exportComprehensiveReport(const ComprehensiveAnalysisReport& report, 
                                     const std::string& format = "json");
    
    // Export digestion report (legacy)
    std::string exportReport(const std::vector<DigestionTask>& tasks, const std::string& format = "json");
    
    // Generate HTML report with visualizations
    std::string generateHTMLReport(const ComprehensiveAnalysisReport& report);
    
    // Generate markdown summary
    std::string generateMarkdownSummary(const ComprehensiveAnalysisReport& report);

    // ==================== Pattern Management ====================
    
    // Register custom language patterns
    void registerLanguagePatterns(const LanguagePatterns& patterns);
    
    // Register custom agentic patterns
    void registerAgenticPattern(const AgenticPattern& pattern);
    
    // Get all registered agentic patterns
    std::map<std::string, AgenticPattern> getAgenticPatterns() const;
    
    // Get patterns by category
    std::vector<AgenticPattern> getPatternsByCategory(const std::string& category) const;

    // ==================== Utility Methods ====================
    
    // Extract method context from source code
    std::string extractMethodContext(const std::string& filePath, int lineNumber, ProgrammingLanguage language);
    
    // Parse method signature from source code
    std::map<std::string, std::string> parseMethodSignature(const std::string& context, ProgrammingLanguage language);
    
    // Generate language-specific code from template
    std::string generateCodeFromTemplate(const std::string& templateStr, 
                                   const std::map<std::string, std::string>& parameters);
    
    // Validate generated code syntax
    bool validateGeneratedCode(const std::string& code, ProgrammingLanguage language);
    
    // Classify stub type
    StubClassification classifyStub(const std::string& content, ProgrammingLanguage language);
    
    // Get complexity score for a method
    int calculateComplexity(const std::string& methodContent, ProgrammingLanguage language);
    
    // Identify dependencies in code
    std::vector<DependencyInfo> extractDependencies(const std::string& code, ProgrammingLanguage language);
    
    // Detect security vulnerabilities
    std::vector<SecurityVulnerability> detectSecurityIssues(const std::string& code, ProgrammingLanguage language);
    
    // Detect performance issues
    std::vector<PerformanceIssue> detectPerformanceIssues(const std::string& code, ProgrammingLanguage language);
    
    // Build control flow graph
    std::vector<ControlFlowNode> buildControlFlowGraph(const std::string& methodContent, ProgrammingLanguage language);

    // ==================== Batch Operations ====================
    
    // Process entire directory recursively
    ComprehensiveAnalysisReport processDirectory(const std::string& directoryPath);
    
    // Process with filters (file patterns, size limits, etc.)
    ComprehensiveAnalysisReport processDirectoryWithFilters(const std::string& directoryPath,
                                                           const std::map<std::string, std::any>& filters);
    
    // Batch apply agentic patterns
    std::map<std::string, std::vector<bool>> batchApplyPatterns(const std::stringList& filePaths,
                                                   const std::vector<std::string>& patternNames);

private:
    // ==================== Internal Data Structures ====================
    
    // Language patterns database
    std::map<ProgrammingLanguage, LanguagePatterns> languagePatterns_;
    
    // Agentic patterns database
    std::map<std::string, AgenticPattern> agenticPatterns_;
    
    // Analysis statistics
    mutable std::map<std::string, std::any> statistics_;
    
    // Cache for parsed files
    std::map<std::string, std::string> fileCache_;
    
    // Cache for analysis results
    std::map<std::string, ComprehensiveAnalysisReport> analysisCache_;

    // ==================== Initialization Methods ====================
    
    // Initialize all language patterns
    void initializeLanguagePatterns();
    
    // Initialize all agentic patterns
    void initializeAgenticPatterns();
    
    // Initialize advanced patterns for each language
    void initializeAdvancedPatterns();

    // ==================== Internal Analysis Methods ====================
    
    // Perform directional analysis based on direction type
    DirectionalAnalysisResult performDirectionalAnalysis(const std::string& filePath, 
                                                        AnalysisDirection direction);
    
    // Merge multiple directional results into comprehensive report
    void mergeDirectionalResults(ComprehensiveAnalysisReport& report,
                                const std::map<AnalysisDirection, DirectionalAnalysisResult>& results);
    
    // Analyze file content and extract all relevant information
    void analyzeFileContent(const std::string& content, 
                           ProgrammingLanguage language,
                           DigestionTask& task);
    
    // Generate recommendations based on analysis
    void generateRecommendations(DigestionTask& task);
    
    // Prioritize tasks based on complexity and impact
    std::vector<DigestionTask> prioritizeTasks(const std::vector<DigestionTask>& tasks);

    // ==================== Code Generation Helpers ====================
    
    // Get required dependencies for a pattern
    std::stringList getPatternDependencies(const std::string& patternName, ProgrammingLanguage language);
    
    // Generate import/include statements
    std::string generateImportStatements(const std::stringList& dependencies, ProgrammingLanguage language);
    
    // Pattern dependency resolution (topological sort)
    std::vector<std::string> resolvePatternDependencies(const std::vector<std::string>& patternNames);
    
    // Generate test code for agentic patterns
    std::string generateTestCode(const DigestionTask& task, const std::string& patternName);
};



