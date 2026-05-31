// =============================================================================
// include/rawrxd_refactoring.h - Refactoring & Analysis Engine
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

namespace RawrXD {
namespace Refactoring {

// =============================================================================
// REFACTORING TYPES
// =============================================================================

struct SourceLocation {
    std::string file;
    uint32_t line = 0;
    uint32_t column = 0;
    uint32_t endLine = 0;
    uint32_t endColumn = 0;
};

struct SourceRange {
    SourceLocation start;
    SourceLocation end;
};

struct TextEdit {
    SourceRange range;
    std::string newText;
    std::string reason;
};

struct RefactoringResult {
    bool success = false;
    std::string error;
    std::vector<TextEdit> edits;
    std::vector<std::string> affectedFiles;
    bool needsConfirmation = true;
};

struct RefactoringContext {
    std::string file;
    uint32_t line = 0;
    uint32_t column = 0;
    std::string selectedText;
    std::unordered_map<std::string, std::string> fileContents;
};

// =============================================================================
// REFACTORING ENGINE
// =============================================================================

class RefactoringEngine {
public:
    RefactoringEngine();
    ~RefactoringEngine();
    
    // Core refactorings
    RefactoringResult rename(const RefactoringContext& ctx, const std::string& newName);
    RefactoringResult extractMethod(const RefactoringContext& ctx, const std::string& name);
    RefactoringResult extractFunction(const RefactoringContext& ctx, const std::string& name);
    RefactoringResult extractVariable(const RefactoringContext& ctx, const std::string& name);
    RefactoringResult extractConstant(const RefactoringContext& ctx, const std::string& name);
    
    RefactoringResult inlineVariable(const RefactoringContext& ctx);
    RefactoringResult inlineFunction(const RefactoringContext& ctx);
    
    RefactoringResult moveMethod(const RefactoringContext& ctx, const std::string& targetClass);
    RefactoringResult moveClass(const RefactoringContext& ctx, const std::string& targetNs);
    
    RefactoringResult changeSignature(const RefactoringContext& ctx);
    RefactoringResult addParameter(const RefactoringContext& ctx, const std::string& name, 
                                    const std::string& type, const std::string& default_);
    RefactoringResult removeParameter(const RefactoringContext& ctx, uint32_t index);
    
    RefactoringResult encapsulateField(const RefactoringContext& ctx);
    RefactoringResult makeStatic(const RefactoringContext& ctx);
    RefactoringResult makeConst(const RefactoringContext& ctx);
    
    // Code organization
    RefactoringResult sortIncludes(const RefactoringContext& ctx);
    RefactoringResult removeUnusedIncludes(const RefactoringContext& ctx);
    RefactoringResult optimizeIncludes(const RefactoringContext& ctx);
    
    // Generation
    RefactoringResult generateGetter(const RefactoringContext& ctx);
    RefactoringResult generateSetter(const RefactoringContext& ctx);
    RefactoringResult generateGetterSetter(const RefactoringContext& ctx);
    RefactoringResult generateConstructor(const RefactoringContext& ctx);
    RefactoringResult generateDestructor(const RefactoringContext& ctx);
    RefactoringResult generateCopyConstructor(const RefactoringContext& ctx);
    RefactoringResult generateMoveConstructor(const RefactoringContext& ctx);
    RefactoringResult generateAssignment(const RefactoringContext& ctx);
    RefactoringResult generateComparison(const RefactoringContext& ctx);
    RefactoringResult generateSwap(const RefactoringContext& ctx);
    
    // Modernization
    RefactoringResult modernizeLoop(const RefactoringContext& ctx);
    RefactoringResult modernizeNullCheck(const RefactoringContext& ctx);
    RefactoringResult convertToAuto(const RefactoringContext& ctx);
    RefactoringResult convertToConstexpr(const RefactoringContext& ctx);
    
private:
    std::vector<SourceLocation> findReferences(const std::string& symbol,
                                                const std::string& file,
                                                const SourceLocation& loc);
    std::string indent(const std::string& code, uint32_t levels);
    uint32_t getIndentLevel(const std::string& source, uint32_t line);
    std::string deduceReturnType(const std::string& code);
    std::vector<std::pair<std::string, std::string>> deduceParams(const std::string& code);
};

// =============================================================================
// STATIC ANALYZER
// =============================================================================

struct AnalysisDiagnostic {
    std::string id;
    std::string message;
    std::string category;
    SourceLocation location;
    uint32_t severity = 1;
    std::string fixDescription;
    std::vector<TextEdit> fixes;
    bool hasAutoFix = false;
};

struct AnalysisResult {
    std::vector<AnalysisDiagnostic> diagnostics;
    std::unordered_map<std::string, uint32_t> stats;
};

struct CodeMetrics {
    uint32_t totalLines = 0;
    uint32_t codeLines = 0;
    uint32_t commentLines = 0;
    uint32_t blankLines = 0;
    
    uint32_t cyclomaticComplexity = 0;
    uint32_t cognitiveComplexity = 0;
    uint32_t nestingDepth = 0;
    
    uint32_t functionCount = 0;
    uint32_t classCount = 0;
    uint32_t structCount = 0;
    uint32_t namespaceCount = 0;
    
    uint32_t todoCount = 0;
    uint32_t fixmeCount = 0;
    uint32_t magicNumberCount = 0;
    
    double maintainabilityIndex = 0.0;
    double commentRatio = 0.0;
    double avgFunctionLength = 0.0;
};

struct FunctionMetrics {
    std::string name;
    std::string signature;
    SourceLocation location;
    uint32_t cyclomaticComplexity = 0;
    uint32_t cognitiveComplexity = 0;
    uint32_t linesOfCode = 0;
    uint32_t parameterCount = 0;
    uint32_t nestingDepth = 0;
};

struct ClassMetrics {
    std::string name;
    SourceLocation location;
    uint32_t methodCount = 0;
    uint32_t fieldCount = 0;
    uint32_t publicMethods = 0;
    uint32_t privateMethods = 0;
    uint32_t baseClasses = 0;
    uint32_t derivedClasses = 0;
    uint32_t linesOfCode = 0;
    double couplingFactor = 0.0;
};

class StaticAnalyzer {
public:
    StaticAnalyzer();
    ~StaticAnalyzer();
    
    // Configuration
    void enableCheck(const std::string& id);
    void disableCheck(const std::string& id);
    void setSeverity(const std::string& id, uint32_t severity);
    void loadConfig(const std::string& path);
    
    // Analysis
    AnalysisResult analyzeFile(const std::string& path, const std::string& content = "");
    AnalysisResult analyzeProject(const std::string& path);
    
    // Metrics
    CodeMetrics computeMetrics(const std::string& path, const std::string& content = "");
    FunctionMetrics computeFunctionMetrics(const std::string& path, const SourceLocation& loc);
    ClassMetrics computeClassMetrics(const std::string& path, const SourceLocation& loc);
    
    // Specific analyses
    std::vector<std::string> findUnusedSymbols(const std::string& path);
    std::vector<std::string> findUnusedIncludes(const std::string& path);
    std::vector<std::pair<std::string, SourceLocation>> findMagicNumbers(const std::string& path);
    std::vector<SourceLocation> findPotentialNullDerefs(const std::string& path);
    std::vector<SourceLocation> findResourceLeaks(const std::string& path);
    std::vector<SourceLocation> findDataRaces(const std::string& path);
    
    // Code clone detection
    std::vector<std::pair<SourceLocation, SourceLocation>> findCodeClones(
        const std::string& path, uint32_t minLines = 6);
    
private:
    std::unordered_set<std::string> enabledChecks_;
    std::unordered_map<std::string, uint32_t> checkSeverities_;
    
    // Check implementations
    void checkMemorySafety(const std::string& content, AnalysisResult& result);
    void checkPerformance(const std::string& content, AnalysisResult& result);
    void checkSecurity(const std::string& content, AnalysisResult& result);
    void checkConcurrency(const std::string& content, AnalysisResult& result);
    void checkStyle(const std::string& content, AnalysisResult& result);
    void checkModernCpp(const std::string& content, AnalysisResult& result);
    void checkPotentialBugs(const std::string& content, AnalysisResult& result);
    
    // Metric calculations
    uint32_t calcCyclomaticComplexity(const std::string& code);
    uint32_t calcCognitiveComplexity(const std::string& code);
    uint32_t calcNestingDepth(const std::string& code);
};

// =============================================================================
// CODE FORMATTER
// =============================================================================

struct FormatStyle {
    enum class IndentStyle { Space, Tab };
    
    IndentStyle indentStyle = IndentStyle::Space;
    uint32_t indentWidth = 4;
    uint32_t tabWidth = 4;
    uint32_t columnLimit = 120;
    
    bool insertSpaceAfterKeywords = true;
    bool insertSpaceAroundOperators = true;
    bool insertSpaceBeforeParentheses = false;
    bool placeBracesOnNewLine = false;
    bool sortIncludes = true;
    bool alignConsecutiveDecls = false;
    bool alignConsecutiveAssigns = false;
    
    static FormatStyle LLVM();
    static FormatStyle Google();
    static FormatStyle Microsoft();
    static FormatStyle Chromium();
};

class CodeFormatter {
public:
    CodeFormatter();
    explicit CodeFormatter(const FormatStyle& style);
    
    std::string format(const std::string& code);
    std::string formatRange(const std::string& code, uint32_t startLine, uint32_t endLine);
    std::vector<TextEdit> formatEdits(const std::string& code);
    
    void setStyle(const FormatStyle& style);
    FormatStyle getStyle() const;
    bool loadStyleFile(const std::string& path);
    
private:
    FormatStyle style_;
    
    void formatIndentation(std::string& code);
    void formatSpacing(std::string& code);
    void formatBraces(std::string& code);
    void sortIncludes(std::string& code);
    void alignDeclarations(std::string& code);
};

// =============================================================================
// CODE NAVIGATOR
// =============================================================================

struct CallGraphNode {
    std::string function;
    std::string signature;
    SourceLocation location;
    std::vector<CallGraphNode*> callees;
    std::vector<CallGraphNode*> callers;
    uint32_t callCount = 0;
    bool isRecursive = false;
};

struct InheritanceNode {
    std::string className;
    SourceLocation location;
    std::vector<InheritanceNode*> bases;
    std::vector<InheritanceNode*> derived;
    std::vector<std::string> methods;
    std::vector<std::string> fields;
};

class CodeNavigator {
public:
    CodeNavigator();
    ~CodeNavigator();
    
    // Call hierarchy
    CallGraphNode* getCallGraph(const std::string& file);
    std::vector<CallGraphNode*> getCallers(const std::string& function);
    std::vector<CallGraphNode*> getCallees(const std::string& function);
    std::vector<std::string> findRecursiveCalls();
    std::vector<std::string> findDeadCode();
    
    // Inheritance hierarchy
    InheritanceNode* getInheritanceTree(const std::string& file);
    std::vector<InheritanceNode*> getBases(const std::string& className);
    std::vector<InheritanceNode*> getDerived(const std::string& className);
    std::vector<std::string> findVirtualOverrides(const std::string& className,
                                                   const std::string& method);
    
private:
    std::unordered_map<std::string, std::unique_ptr<CallGraphNode>> callGraph_;
    std::unordered_map<std::string, std::unique_ptr<InheritanceNode>> inheritanceTree_;
    
    void buildCallGraph(const std::string& file);
    void buildInheritanceTree(const std::string& file);
};

// =============================================================================
// METRICS DASHBOARD
// =============================================================================

struct ProjectMetrics {
    uint32_t totalFiles = 0;
    uint32_t totalLines = 0;
    uint32_t totalFunctions = 0;
    uint32_t totalClasses = 0;
    
    double avgComplexity = 0.0;
    double testCoverage = 0.0;
    uint32_t codeSmells = 0;
    uint32_t bugs = 0;
    uint32_t vulnerabilities = 0;
    uint32_t technicalDebt = 0;  // Minutes
    
    std::vector<std::pair<std::string, uint32_t>> mostComplexFiles;
    std::vector<std::pair<std::string, uint32_t>> mostComplexFunctions;
    std::vector<std::pair<std::string, uint32_t>> mostChangedFiles;
};

class MetricsDashboard {
public:
    MetricsDashboard();
    ~MetricsDashboard();
    
    ProjectMetrics computeProjectMetrics(const std::string& path);
    CodeMetrics computeFileMetrics(const std::string& path);
    FunctionMetrics computeFunctionMetrics(const std::string& path, const SourceLocation& loc);
    
    void updateMetrics();
    void exportToJSON(const std::string& path);
    void exportToHTML(const std::string& path);
    
private:
    ProjectMetrics projectMetrics_;
    std::unordered_map<std::string, CodeMetrics> fileMetrics_;
};

} // namespace Refactoring
} // namespace RawrXD
