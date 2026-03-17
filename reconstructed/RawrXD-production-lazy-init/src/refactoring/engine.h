// Production-Grade Real-Time Refactoring Engine
// Enables safe, automated code transformation with comprehensive safety checks
#pragma once

#include <QString>
#include <QVector>
#include <QMap>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <functional>

// ========== CODE ANALYSIS STRUCTURES ==========

struct CodeMetric {
    QString metricName;
    double value;
    double threshold;
    bool isViolation;
    QString severity; // "info", "warning", "critical"
};

struct ComplexityAnalysis {
    QString fileName;
    QString functionName;
    int cyclomaticComplexity;
    int nestingDepth;
    int parameterCount;
    int lineCount;
    double cognitiveComplexity;
    bool shouldRefactor;
    QString refactoringReason;
};

struct CodePattern {
    QString patternName;
    QString patternRegex;
    QString replacement;
    QString description;
    bool isAutomated;
    QString category; // "performance", "readability", "safety", "maintainability"
};

struct RefactoringProposal {
    QString location; // file:line:column
    QString patternType;
    QString description;
    QString beforeCode;
    QString afterCode;
    double confidenceScore; // 0.0-1.0
    QString category;
    int estimatedTimeMinutes;
    bool isBreaking;
};

struct RefactoringRefactoringSymbolInfo {
    QString name;
    QString type; // "function", "class", "variable", "macro"
    QString file;
    int line;
    int column;
    QVector<QString> usages; // locations where symbol is used
    bool isPublic;
    QString documentation;

    bool operator==(const RefactoringRefactoringSymbolInfo& other) const {
        return name == other.name && type == other.type && file == other.file && 
               line == other.line && column == other.column;
    }
};

// ========== REFACTORING ENGINE ==========

class CodeAnalyzer : public QObject {
    Q_OBJECT

public:
    explicit CodeAnalyzer(QObject* parent = nullptr);
    ~CodeAnalyzer();

    // Complexity Analysis
    ComplexityAnalysis analyzeFunction(const QString& filePath, const QString& functionName);
    QVector<ComplexityAnalysis> analyzeFile(const QString& filePath);
    QVector<ComplexityAnalysis> analyzeProject(const QString& projectPath);

    // Code Metrics
    QVector<CodeMetric> calculateMetrics(const QString& filePath);
    CodeMetric calculateCyclomaticComplexity(const QString& functionBody);
    CodeMetric calculateNestingDepth(const QString& functionBody);
    CodeMetric calculateLinesOfCode(const QString& filePath);
    CodeMetric calculateDuplication(const QString& projectPath);

    // Pattern Detection
    QVector<RefactoringProposal> detectPatterns(const QString& filePath);
    QVector<RefactoringProposal> detectAntiPatterns(const QString& filePath);
    QVector<RefactoringProposal> detectDeadCode(const QString& filePath);

    // Symbol Analysis
    RefactoringSymbolInfo analyzeSymbol(const QString& symbolName, const QString& filePath);
    QVector<RefactoringSymbolInfo> findAllSymbols(const QString& projectPath);
    QVector<QString> findSymbolUsages(const QString& symbolName);
    bool canRenameSymbol(const QString& oldName, const QString& newName);

signals:
    void analysisStarted(QString filePath);
    void analysisProgress(int current, int total);
    void analysisCompleted(QString filePath);
    void complexityViolationFound(ComplexityAnalysis analysis);
    void patternDetected(RefactoringProposal proposal);

private:
    QVector<CodePattern> m_patterns;
    QMap<QString, RefactoringSymbolInfo> m_symbolCache;
    
    void loadDefaultPatterns();
    int calculateCyclomaticComplexityValue(const QString& code);
    int calculateNestingDepthValue(const QString& code);
};

// ========== AUTOMATED REFACTORING ==========

class AutomaticRefactoringEngine : public QObject {
    Q_OBJECT

public:
    explicit AutomaticRefactoringEngine(QObject* parent = nullptr);
    ~AutomaticRefactoringEngine();

    // Safe Refactoring Operations
    bool applyRefactoring(const RefactoringProposal& proposal, const QString& filePath);
    QVector<QString> validateRefactoring(const QString& filePath, const RefactoringProposal& proposal);
    bool rollbackRefactoring(const QString& filePath);

    // Refactoring Operations
    bool extractMethod(const QString& filePath, int startLine, int endLine, const QString& methodName);
    bool renameSymbol(const QString& symbolName, const QString& newName, const QString& scope);
    bool removeDeadCode(const QString& filePath);
    bool simplifyComplexConditions(const QString& filePath);
    bool extractConstants(const QString& filePath);
    bool mergeClasses(const QString& class1, const QString& class2, const QString& resultFile);
    bool splitClass(const QString& sourceClass, const QVector<QString>& newClasses);
    bool inlineFunction(const QString& functionName, int maxInlineSize);
    bool introduceParameterObject(const QString& functionName);
    bool replaceArrayWithObject(const QString& filePath, const QString& arrayName);

    // Batch Operations
    int refactorProject(const QString& projectPath, const QVector<RefactoringProposal>& proposals);
    int refactorByCategory(const QString& projectPath, const QString& category);

signals:
    void refactoringStarted(RefactoringProposal proposal);
    void refactoringProgress(int current, int total);
    void refactoringCompleted(RefactoringProposal proposal, bool success);
    void refactoringFailed(RefactoringProposal proposal, QString error);
    void validationError(QString location, QString message);

private:
    struct RefactoringContext {
        QString filePath;
        QString originalContent;
        QMap<QString, QString> symbolMappings;
        QVector<QString> appliedChanges;
        int version;
    };

    QMap<QString, RefactoringContext> m_contexts;
    int m_maxInlineSize;

    bool validateSafetyConstraints(const RefactoringProposal& proposal);
    bool updateAllReferences(const QString& oldName, const QString& newName);
};

// ========== INTELLIGENT REFACTORING COORDINATOR ==========

class RefactoringCoordinator : public QObject {
    Q_OBJECT

public:
    explicit RefactoringCoordinator(QObject* parent = nullptr);
    ~RefactoringCoordinator();

    void initialize(const QString& projectPath);
    
    // Comprehensive Analysis & Refactoring
    QJsonObject analyzeCodeQuality();
    QJsonArray generateRefactoringPlan();
    int executeRefactoringPlan(const QJsonArray& plan);
    
    // Priority-based Refactoring
    int refactorByPriority(const QString& priority); // "critical", "high", "medium", "low"
    int refactorComplexFunctions(int maxComplexity = 10);
    int refactorLargeFiles(int maxLines = 300);
    int refactorDuplicateCode();
    
    // Iterative Improvement
    QJsonObject getRefactoringMetrics();
    bool improveCodeMetricsIteratively();

signals:
    void qualityReportGenerated(QJsonObject report);
    void refactoringPlanCreated(QJsonArray plan);
    void refactoringExecuted(int filesChanged, int linesChanged);
    void metricsImproved(QString metric, double oldValue, double newValue);

private:
    std::unique_ptr<CodeAnalyzer> m_analyzer;
    std::unique_ptr<AutomaticRefactoringEngine> m_refactoringEngine;
    QString m_projectPath;
    
    QVector<ComplexityAnalysis> m_analysis;
    QVector<RefactoringProposal> m_proposals;
};

// ========== HELPERS ==========

class RefactoringUtils {
public:
    static QString extractFunctionBody(const QString& code, const QString& functionName);
    static int countCyclomaticComplexity(const QString& code);
    static QVector<QString> findAllFunctions(const QString& filePath);
    static bool isValidCppIdentifier(const QString& name);
    static QString generateUniqueName(const QString& baseName, const QVector<QString>& existingNames);
    static QVector<int> findCodeDuplication(const QString& filePath, int minLines = 5);
};
