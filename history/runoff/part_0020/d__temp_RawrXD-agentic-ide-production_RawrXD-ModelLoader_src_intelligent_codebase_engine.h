#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileSystemWatcher>
#include <memory>

// Enterprise-grade symbol information
struct SymbolInfo {
    QString name;
    QString type; // "function", "class", "variable", "namespace"
    QString filePath;
    int lineNumber;
    QString signature;
    QString returnType;
    QVector<QString> parameters;
    bool isConst = false;
    bool isStatic = false;
    QVector<QString> baseClasses;
    QJsonObject metadata;
};

// Dependency relationship
struct DependencyInfo {
    QString fromSymbol;
    QString toSymbol;
    QString dependencyType; // "import", "inheritance", "usage"
    QString filePath;
    int lineNumber;
    double confidence;
};

// Architecture pattern detection
struct ArchitecturePattern {
    QString patternType; // "MVC", "Microservice", "Layered", "Monolith"
    double confidenceScore;
    QJsonArray evidence;
    QVector<QString> affectedFiles;
    QJsonObject characteristics;
};

// Code complexity metrics
struct CodeComplexity {
    double cyclomaticComplexity = 0.0;
    double cognitiveComplexity = 0.0;
    int linesOfCode = 0;
    int numberOfFunctions = 0;
    int numberOfClasses = 0;
    int numberOfDependencies = 0;
    double duplicationPercentage = 0.0;
    QJsonArray complexityHotspots;
};

// Refactoring suggestions
struct RefactoringOpportunity {
    QString type; // "extract_method", "inline_function", "move_class", "rename"
    QString description;
    QString filePath;
    int startLine;
    int endLine;
    double confidence;
    double estimatedImpact;
    QJsonObject implementationHints;
};

// Bug detection
struct BugReport {
    QString bugType; // "null_pointer", "memory_leak", "infinite_loop", "race_condition"
    QString severity; // "low", "medium", "high", "critical"
    QString description;
    QString filePath;
    int lineNumber;
    double confidence;
    QVector<QString> evidence;
    QJsonObject suggestedFix;
};

// Optimization suggestions
struct Optimization {
    QString optimizationType; // "performance", "memory", "readability", "security"
    QString description;
    QString filePath;
    int lineNumber;
    double potentialImprovement;
    double confidence;
    QJsonObject currentImplementation;
    QJsonObject optimizedImplementation;
};

/**
 * @brief Enterprise Intelligent Codebase Analysis Engine
 * 
 * Provides deep code understanding, symbol indexing, dependency analysis,
 * architecture pattern detection, and intelligent refactoring suggestions.
 */
class IntelligentCodebaseEngine : public QObject {
    Q_OBJECT
    
private:
    QHash<QString, SymbolInfo> symbolIndex;
    QHash<QString, QVector<DependencyInfo>> dependencyGraph;
    QHash<QString, ArchitecturePattern> architecturePatterns;
    QHash<QString, QVector<SymbolInfo>> fileSymbols;
    QHash<QString, QVector<QString>> callGraph;
    
    QVector<RefactoringOpportunity> refactoringOpportunities;
    QVector<BugReport> bugReports;
    QVector<Optimization> optimizations;
    CodeComplexity complexityAnalysis;
    
    QFileSystemWatcher* fileWatcher;
    
    // Configuration
    bool enableRealTimeIndexing = true;
    bool enableDeepAnalysis = true;
    bool enablePatternRecognition = true;
    int maxAnalysisDepth = 3;
    double minimumConfidence = 0.7;
    int batchSize = 100;
    
public:
    explicit IntelligentCodebaseEngine(QObject* parent = nullptr);
    ~IntelligentCodebaseEngine();
    
    // Core analysis
    bool analyzeEntireCodebase(const QString& projectPath);
    bool analyzeFile(const QString& filePath);
    bool analyzeFunction(const QString& functionName, const QString& filePath);
    bool analyzeSymbol(const QString& symbolName);
    
    // Intelligent recommendations
    QVector<RefactoringOpportunity> discoverRefactoringOpportunities();
    QVector<BugReport> detectBugs();
    QVector<Optimization> suggestOptimizations();
    ArchitecturePattern detectArchitecturePattern();
    CodeComplexity analyzeComplexity();
    
    // Symbol intelligence
    SymbolInfo getSymbolInfo(const QString& symbolName);
    QVector<SymbolInfo> getSymbolsInFile(const QString& filePath);
    QVector<DependencyInfo> getSymbolDependencies(const QString& symbolName);
    
    // Dependency intelligence
    QJsonArray getFileDependencies(const QString& filePath);
    QJsonObject getDependencyGraph();
    QVector<QString> getCircularDependencies();
    
    // Real-time analysis
    bool startRealTimeAnalysis(const QString& projectPath);
    bool stopRealTimeAnalysis();
    bool updateAnalysis(const QString& filePath);
    
    // Quality metrics
    double calculateCodeQualityScore();
    double calculateMaintainabilityIndex();
    QJsonObject generateQualityReport();
    
signals:
    void analysisStarted(const QString& projectPath);
    void analysisProgress(int percentage, const QString& currentFile);
    void analysisCompleted(const QJsonObject& results);
    void refactoringOpportunitiesFound(const QVector<RefactoringOpportunity>& opportunities);
    void bugsDetected(const QVector<BugReport>& bugs);
    void optimizationsSuggested(const QVector<Optimization>& optimizations);
    void architecturePatternDetected(const ArchitecturePattern& pattern);
    void realTimeAnalysisUpdated(const QString& filePath);
    void codeQualityScoreUpdated(double score);
    
private:
    QStringList getAllSourceFiles(const QString& projectPath);
    void processBatchRelationships(const QStringList& files);
    void performGlobalAnalysis();
    QJsonObject generateAnalysisResults();
    
    QString detectLanguage(const QString& filePath);
    QString detectFileType(const QString& filePath);
    
    bool analyzeCppFile(const QString& filePath, const QString& content);
    bool analyzePythonFile(const QString& filePath, const QString& content);
    bool analyzeGenericFile(const QString& filePath, const QString& content);
    
    bool analyzeCallGraph(const QString& filePath, const QString& content);
    bool analyzeDependencies(const QString& filePath, const QString& content);
    
    QVector<QString> parseParameters(const QString& paramString);
    double calculateFunctionComplexity(const SymbolInfo& function);
    
    void discoverExtractMethodOpportunities();
    void discoverInlineFunctionOpportunities();
    void discoverMoveClassOpportunities();
    void discoverRenameOpportunities();
    
    void detectNullPointerBugs();
    void detectMemoryLeakBugs();
    void detectInfiniteLoopBugs();
    void detectRaceConditionBugs();
    
    void suggestPerformanceOptimizations();
    void suggestMemoryOptimizations();
    void suggestReadabilityOptimizations();
    void suggestSecurityOptimizations();
    
    bool checkForNullChecks(const SymbolInfo& symbol);
    bool checkForMemoryAllocation(const SymbolInfo& symbol);
    bool checkForMemoryDeallocation(const SymbolInfo& symbol);
    bool checkForLoopsWithoutBreak(const SymbolInfo& symbol);
    bool checkForSharedStateAccess(const SymbolInfo& symbol);
    bool checkForSynchronization(const SymbolInfo& symbol);
    bool checkForNestedLoops(const SymbolInfo& symbol);
    bool checkForLargeObjectCreation(const SymbolInfo& symbol);
    bool checkForInputValidation(const SymbolInfo& symbol);
    bool checkForBufferOverflowRisk(const SymbolInfo& symbol);
    
    void removeFileAnalysis(const QString& filePath);
    void updateDependentAnalyses(const QString& filePath);
    
    int estimateLinesOfCode(const SymbolInfo& symbol);
    double calculateHalsteadVolume();
};
