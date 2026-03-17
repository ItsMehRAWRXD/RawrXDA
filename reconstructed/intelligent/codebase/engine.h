// intelligent_codebase_engine.h - Deep Codebase Analysis Engine
#ifndef INTELLIGENT_CODEBASE_ENGINE_H
#define INTELLIGENT_CODEBASE_ENGINE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QRegularExpression>
#include <memory>

// Code symbol information
struct CodeSymbol {
    QString name;              // Symbol name
    QString type;              // Type: class, function, variable, etc.
    QString filePath;          // File containing this symbol
    int lineNumber;            // Line number
    QString signature;         // Full signature
    QStringList references;    // Where this symbol is referenced
    QStringList dependencies;  // Symbols this depends on
    QString documentation;     // Doc comments
    QJsonObject metadata;      // Additional metadata
};

// Dependency graph node
struct DependencyNode {
    QString symbolName;
    QString filePath;
    QStringList dependencies;
    QStringList dependents;
    int depth;                 // Depth in dependency tree
    bool isCyclic;             // Part of circular dependency
};

// Code pattern detected in codebase
struct CodePattern {
    QString patternId;         // Unique pattern identifier
    QString name;              // Pattern name
    QString description;       // What this pattern does
    QStringList examples;      // Example code snippets
    int frequency;             // How often pattern appears
    double confidence;         // Detection confidence (0-1)
    QStringList locations;     // Where pattern is found
    QJsonObject characteristics; // Pattern characteristics
};

// Refactoring opportunity
struct RefactoringOpportunity {
    QString opportunityId;
    QString type;              // extract_method, inline, move_class, etc.
    QString filePath;
    int startLine;
    int endLine;
    QString description;
    QString reasoning;
    double confidence;         // Confidence this is beneficial (0-1)
    QString suggestedCode;     // Suggested refactored code
    QStringList benefits;      // Expected benefits
    int estimatedEffort;       // Hours to implement
};

// Bug/issue detected
struct BugReport {
    QString bugId;
    QString severity;          // critical, high, medium, low
    QString type;              // null_pointer, memory_leak, race_condition, etc.
    QString filePath;
    int lineNumber;
    QString description;
    QString explanation;
    QString suggestedFix;
    QStringList affectedFunctions;
    double confidence;
};

// Code smell detected
struct CodeSmell {
    QString smellType;         // long_method, large_class, duplicate_code, etc.
    QString filePath;
    int startLine;
    int endLine;
    QString description;
    int severityScore;         // 1-10
    QStringList suggestedActions;
};

// Architecture pattern detected
struct ArchitecturePattern {
    QString patternName;       // MVC, Microservices, Layered, etc.
    double confidence;         // How confident we are (0-1)
    QStringList components;    // Components following this pattern
    QString description;
    QJsonObject structure;     // Detailed structure info
};

// Codebase statistics
struct CodebaseStats {
    int totalFiles;
    int totalLines;
    int totalFunctions;
    int totalClasses;
    QHash<QString, int> languageDistribution;
    QHash<QString, int> fileTypeDistribution;
    int cyclomaticComplexity;
    double testCoverage;
    int technicalDebtMinutes;
};

class IntelligentCodebaseEngine : public QObject {
    Q_OBJECT

public:
    explicit IntelligentCodebaseEngine(QObject* parent = nullptr);
    ~IntelligentCodebaseEngine();

    // Codebase analysis
    bool analyzeEntireCodebase(const QString& projectPath);
    bool analyzeFile(const QString& filePath);
    bool analyzeDirectory(const QString& directoryPath, bool recursive = true);
    
    // Symbol indexing and search
    QVector<CodeSymbol> findSymbol(const QString& symbolName) const;
    QVector<CodeSymbol> findSymbolsInFile(const QString& filePath) const;
    QVector<QString> findReferences(const QString& symbolName) const;
    DependencyNode getDependencyGraph(const QString& symbolName) const;
    QVector<DependencyNode> getFullDependencyGraph() const;
    
    // Pattern recognition
    QVector<CodePattern> detectPatterns() const;
    QVector<CodePattern> detectPatternsInFile(const QString& filePath) const;
    CodePattern findSimilarCode(const QString& codeSnippet) const;
    QVector<QString> suggestDesignPatterns(const QString& context) const;
    
    // Refactoring suggestions
    QVector<RefactoringOpportunity> discoverRefactoringOpportunities() const;
    QVector<RefactoringOpportunity> suggestRefactorings(const QString& filePath) const;
    QString generateRefactoredCode(const RefactoringOpportunity& opportunity) const;
    
    // Bug and issue detection
    QVector<BugReport> detectBugs() const;
    QVector<BugReport> detectBugsInFile(const QString& filePath) const;
    QVector<CodeSmell> detectCodeSmells() const;
    QVector<CodeSmell> detectCodeSmellsInFile(const QString& filePath) const;
    
    // Architecture analysis
    ArchitecturePattern detectArchitecturePattern() const;
    QVector<QString> identifyLayers() const;
    QJsonObject analyzeModularity() const;
    double calculateCohesion(const QString& className) const;
    double calculateCoupling(const QString& className) const;
    
    // Code metrics
    CodebaseStats getCodebaseStatistics() const;
    int calculateCyclomaticComplexity(const QString& functionCode) const;
    int calculateLinesOfCode(const QString& filePath) const;
    double estimateTestCoverage() const;
    int calculateTechnicalDebt() const;
    
    // Code understanding
    QString explainCode(const QString& codeSnippet, const QString& language) const;
    QStringList suggestImprovements(const QString& codeSnippet) const;
    QString generateDocumentation(const QString& functionCode) const;
    
    // Multi-language support
    void setLanguage(const QString& language);
    QString getLanguage() const;
    QStringList getSupportedLanguages() const;
    
    // Configuration
    void setBatchSize(int size);
    void setAnalysisDepth(int depth); // 1=shallow, 3=deep
    void enableRealTimeAnalysis(bool enable);
    void setExcludedPaths(const QStringList& paths);

signals:
    void analysisStarted(const QString& path);
    void analysisProgress(int percentage);
    void analysisComplete(const QString& path);
    void bugsFound(int count);
    void refactoringOpportunitiesFound(int count);
    void patternsDetected(int count);
    void errorOccurred(const QString& error);

private:
    // Core analysis functions
    void parseFile(const QString& filePath);
    void buildSymbolIndex(const QString& filePath);
    void buildDependencyGraph();
    void detectCircularDependencies();
    
    // Language-specific parsers
    void parseCppFile(const QString& filePath);
    void parsePythonFile(const QString& filePath);
    void parseJavaScriptFile(const QString& filePath);
    void parseJavaFile(const QString& filePath);
    void parseRustFile(const QString& filePath);
    void parseGoFile(const QString& filePath);
    
    // Pattern detection algorithms
    QVector<CodePattern> detectCommonPatterns(const QString& code) const;
    QVector<CodePattern> detectDesignPatterns(const QString& code) const;
    bool matchesPattern(const QString& code, const QString& patternRegex) const;
    
    // Refactoring detection
    RefactoringOpportunity detectLongMethod(const QString& functionCode, 
                                           const QString& filePath, int line) const;
    RefactoringOpportunity detectDuplicateCode(const QString& code) const;
    RefactoringOpportunity detectLargeClass(const QString& className) const;
    
    // Bug detection algorithms
    BugReport detectNullPointerDereference(const QString& code, 
                                          const QString& filePath, int line) const;
    BugReport detectMemoryLeak(const QString& code, 
                              const QString& filePath, int line) const;
    BugReport detectRaceCondition(const QString& code,
                                 const QString& filePath, int line) const;
    BugReport detectInfiniteLoop(const QString& code,
                                const QString& filePath, int line) const;
    
    // Code smell detection
    CodeSmell detectLongParameterList(const QString& functionCode,
                                     const QString& filePath, int line) const;
    CodeSmell detectGodClass(const QString& className) const;
    CodeSmell detectFeatureEnvy(const QString& methodCode) const;
    
    // Helper functions
    QString extractFunctionName(const QString& signature) const;
    QStringList extractParameters(const QString& signature) const;
    QString extractReturnType(const QString& signature) const;
    int countCodeLines(const QString& code) const;
    bool isTestFile(const QString& filePath) const;
    QString normalizeCode(const QString& code) const;
    
    // Data members
    QString currentLanguage;
    QString projectRootPath;
    QHash<QString, QVector<CodeSymbol>> symbolIndex;
    QHash<QString, DependencyNode> dependencyGraph;
    QVector<CodePattern> detectedPatterns;
    QVector<RefactoringOpportunity> refactoringOpportunities;
    QVector<BugReport> detectedBugs;
    QVector<CodeSmell> detectedSmells;
    CodebaseStats statistics;
    
    int batchSize;
    int analysisDepth;
    bool realTimeAnalysisEnabled;
    QStringList excludedPaths;
    
    // Regular expressions for pattern matching
    QHash<QString, QRegularExpression> patternRegexes;
    
    // Configuration
    static constexpr int DEFAULT_BATCH_SIZE = 100;
    static constexpr int DEFAULT_ANALYSIS_DEPTH = 2;
    static constexpr int MAX_METHOD_LINES = 50;
    static constexpr int MAX_CLASS_METHODS = 20;
    static constexpr int MAX_PARAMETERS = 5;
};

#endif // INTELLIGENT_CODEBASE_ENGINE_H
