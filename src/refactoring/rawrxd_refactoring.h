#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <optional>
#include <set>
#include <regex>
#include <chrono>

namespace RawrXD {
namespace Refactoring {

// =============================================================================
// REFACTORING TYPES
// =============================================================================

struct SourceLocation {
    std::string filePath;
    uint32_t line = 0;
    uint32_t column = 0;
    uint32_t endLine = 0;
    uint32_t endColumn = 0;
    
    bool contains(const SourceLocation& other) const {
        return filePath == other.filePath &&
               (line < other.line || 
                (line == other.line && column <= other.column)) &&
               (endLine > other.line ||
                (endLine == other.line && endColumn >= other.endColumn));
    }
    
    bool intersects(const SourceLocation& other) const {
        return filePath == other.filePath && contains(other);
    }
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
    std::string errorMessage;
    std::vector<TextEdit> edits;
    std::vector<std::string> affectedFiles;
    std::string preview;  // Preview of changes
    bool requiresConfirmation = true;  // Some refactorings need user confirmation
};

struct RefactoringContext {
    std::string filePath;
    uint32_t line = 0;
    uint32_t column = 0;
    std::string selectedText;
    std::unordered_map<std::string, std::string> fileContents;
    std::vector<std::string> projectIncludePaths;
    std::vector<std::string> compileFlags;
    bool previewOnly = false;
};

// =============================================================================
// REFACTORING ENGINE
// =============================================================================

class RefactoringEngine {
public:
    RefactoringEngine();
    ~RefactoringEngine();
    
    // Core refactorings
    RefactoringResult rename(const RefactoringContext& ctx, 
                             const std::string& newName);
    RefactoringResult extractMethod(const RefactoringContext& ctx,
                                    const std::string& methodName);
    RefactoringResult extractFunction(const RefactoringContext& ctx,
                                      const std::string& functionName);
    RefactoringResult extractVariable(const RefactoringContext& ctx,
                                      const std::string& variableName);
    RefactoringResult extractConstant(const RefactoringContext& ctx,
                                      const std::string& constantName);
    
    RefactoringResult inlineMethod(const RefactoringContext& ctx);
    RefactoringResult inlineVariable(const RefactoringContext& ctx);
    RefactoringResult inlineConstant(const RefactoringContext& ctx);
    RefactoringResult inlineFunction(const RefactoringContext& ctx);
    
    RefactoringResult moveMethod(const RefactoringContext& ctx,
                                  const std::string& targetClass);
    RefactoringResult moveClass(const RefactoringContext& ctx,
                                 const std::string& targetNamespace);
    RefactoringResult moveFunction(const RefactoringContext& ctx,
                                    const std::string& targetNamespace);
    RefactoringResult moveFile(const RefactoringContext& ctx,
                               const std::string& targetPath);
    
    RefactoringResult changeSignature(const RefactoringContext& ctx);
    RefactoringResult reorderParameters(const RefactoringContext& ctx,
                                        const std::vector<uint32_t>& newOrder);
    RefactoringResult addParameter(const RefactoringContext& ctx,
                                    const std::string& paramName,
                                    const std::string& paramType,
                                    const std::string& defaultValue);
    RefactoringResult removeParameter(const RefactoringContext& ctx,
                                       uint32_t index);
    
    RefactoringResult encapsulateField(const RefactoringContext& ctx);
    RefactoringResult makeMethodStatic(const RefactoringContext& ctx);
    RefactoringResult makeMethodConst(const RefactoringContext& ctx);
    RefactoringResult convertToAuto(const RefactoringContext& ctx);
    
    RefactoringResult introduceTypedef(const RefactoringContext& ctx,
                                       const std::string& typeName);
    RefactoringResult introduceNamespace(const RefactoringContext& ctx,
                                         const std::string& namespaceName);
    
    RefactoringResult splitDeclaration(const RefactoringContext& ctx);
    RefactoringResult mergeDeclarations(const RefactoringContext& ctx);
    
    RefactoringResult pullUpMember(const RefactoringContext& ctx);
    RefactoringResult pushDownMember(const RefactoringContext& ctx);
    
    RefactoringResult extractInterface(const RefactoringContext& ctx,
                                       const std::string& interfaceName);
    RefactoringResult extractSuperclass(const RefactoringContext& ctx,
                                        const std::string& className);
    
    // Code organization
    RefactoringResult sortIncludes(const RefactoringContext& ctx);
    RefactoringResult removeUnusedIncludes(const RefactoringContext& ctx);
    RefactoringResult addInclude(const RefactoringContext& ctx,
                                 const std::string& include);
    RefactoringResult optimizeIncludes(const RefactoringContext& ctx);
    RefactoringResult addForwardDeclaration(const RefactoringContext& ctx,
                                            const std::string& symbol);
    
    // Code generation
    RefactoringResult generateGetter(const RefactoringContext& ctx);
    RefactoringResult generateSetter(const RefactoringContext& ctx);
    RefactoringResult generateGetterSetter(const RefactoringContext& ctx);
    RefactoringResult generateConstructor(const RefactoringContext& ctx);
    RefactoringResult generateDestructor(const RefactoringContext& ctx);
    RefactoringResult generateCopyConstructor(const RefactoringContext& ctx);
    RefactoringResult generateMoveConstructor(const RefactoringContext& ctx);
    RefactoringResult generateAssignmentOperator(const RefactoringContext& ctx);
    RefactoringResult generateComparisonOperators(const RefactoringContext& ctx);
    RefactoringResult generateStreamOperators(const RefactoringContext& ctx);
    RefactoringResult generateSwapFunction(const RefactoringContext& ctx);
    
    // Test generation
    RefactoringResult generateTestClass(const RefactoringContext& ctx);
    RefactoringResult generateTestMethod(const RefactoringContext& ctx);
    RefactoringResult generateMock(const RefactoringContext& ctx);
    RefactoringResult generateStubs(const RefactoringContext& ctx);
    
    // Modernization
    RefactoringResult modernizeLoop(const RefactoringContext& ctx);
    RefactoringResult modernizeNullCheck(const RefactoringContext& ctx);
    RefactoringResult modernizeStringLiteral(const RefactoringContext& ctx);
    RefactoringResult convertToConstexpr(const RefactoringContext& ctx);
    RefactoringResult convertToAuto(const RefactoringContext& ctx,
                                    const std::string& context);
    
private:
    // Internal helpers
    std::vector<SourceLocation> findReferences(const std::string& symbolName,
                                                const std::string& filePath,
                                                const std::string& content,
                                                const SourceLocation& location);
    bool isRenameSafe(const std::string& oldName, const std::string& newName);
    std::string extractCode(const std::string& source, const SourceRange& range);
    std::string indentCode(const std::string& code, uint32_t levels);
    uint32_t determineIndentation(const std::string& source, uint32_t line);
    bool isInsideFunction(const std::string& source, uint32_t line);
    bool isInsideClass(const std::string& source, uint32_t line);
    std::string determineReturnType(const std::string& code);
    std::vector<std::pair<std::string, std::string>> determineParameters(
        const std::string& code);
    std::vector<std::string> determineUsedVariables(const std::string& code,
                                                     const std::string& context);
};

// =============================================================================
// STATIC ANALYSIS ENGINE
// =============================================================================

struct AnalysisDiagnostic {
    std::string id;              // e.g., "RAW001"
    std::string message;
    std::string category;        // "performance", "security", "style", "bug"
    SourceLocation location;
    uint32_t severity = 1;       // 1=info, 2=warning, 3=error, 4=fatal
    std::string fixDescription;
    std::vector<TextEdit> suggestedFixes;
    bool hasAutoFix = false;
    std::vector<std::string> relatedLocations;
    std::string documentation;
};

struct AnalysisResult {
    std::vector<AnalysisDiagnostic> diagnostics;
    std::unordered_map<std::string, uint32_t> stats;  // per-category counts
    std::unordered_map<std::string, std::vector<SourceLocation>> findings;
};

struct CodeMetrics {
    // Complexity metrics
    uint32_t cyclomaticComplexity = 0;
    uint32_t cognitiveComplexity = 0;
    uint32_t nestingDepth = 0;
    uint32_t linesOfCode = 0;
    uint32_t linesOfComments = 0;
    uint32_t blankLines = 0;
    
    // Size metrics
    uint32_t totalLines = 0;
    uint32_t functionCount = 0;
    uint32_t classCount = 0;
    uint32_t structCount = 0;
    uint32_t namespaceCount = 0;
    uint32_t enumCount = 0;
    uint32_t typedefCount = 0;
    uint32_t templateCount = 0;
    
    // Quality metrics
    uint32_t todoCount = 0;
    uint32_t fixmeCount = 0;
    uint32_t hackCount = 0;
    uint32_t magicNumberCount = 0;
    uint32_t longLineCount = 0;  // Lines > 120 chars
    
    // Maintainability
    double maintainabilityIndex = 0.0;
    double commentRatio = 0.0;
    double avgFunctionLength = 0.0;
    double avgParameterCount = 0.0;
    
    std::string toJson() const;
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
    uint32_t returnPathCount = 0;
    bool isRecursive = false;
    std::vector<std::string> calledFunctions;
    std::vector<std::string> callerFunctions;
};

struct ClassMetrics {
    std::string name;
    SourceLocation location;
    uint32_t methodCount = 0;
    uint32_t fieldCount = 0;
    uint32_t publicMethodCount = 0;
    uint32_t privateMethodCount = 0;
    uint32_t protectedMethodCount = 0;
    uint32_t baseClassCount = 0;
    uint32_t derivedClassCount = 0;
    uint32_t linesOfCode = 0;
    double couplingFactor = 0.0;
    double cohesionFactor = 0.0;
    bool hasVirtualMethods = false;
    bool hasVirtualDestructor = false;
    std::vector<std::string> dependencies;
};

class StaticAnalyzer {
public:
    StaticAnalyzer();
    ~StaticAnalyzer();
    
    // Configuration
    void enableCheck(const std::string& checkId);
    void disableCheck(const std::string& checkId);
    void setSeverity(const std::string& checkId, uint32_t severity);
    void setOption(const std::string& option, const std::string& value);
    void loadConfig(const std::string& configPath);
    
    // Analysis
    AnalysisResult analyzeFile(const std::string& filePath,
                               const std::string& content = "");
    AnalysisResult analyzeProject(const std::string& projectPath);
    AnalysisResult analyzeSelection(const std::string& filePath,
                                    const SourceRange& range);
    
    // Quick analysis (for typing feedback)
    AnalysisResult quickAnalyze(const std::string& filePath,
                                const std::string& content);
    
    // Metrics
    CodeMetrics computeMetrics(const std::string& filePath,
                               const std::string& content = "");
    FunctionMetrics computeFunctionMetrics(const std::string& filePath,
                                           const SourceLocation& location);
    ClassMetrics computeClassMetrics(const std::string& filePath,
                                     const SourceLocation& location);
    
    // Symbol analysis
    std::vector<std::string> findUnusedSymbols(const std::string& filePath);
    std::vector<std::string> findUnusedIncludes(const std::string& filePath);
    std::vector<std::pair<std::string, SourceLocation>> findMagicNumbers(
        const std::string& filePath);
    std::vector<SourceLocation> findPotentialNullDerefs(const std::string& filePath);
    std::vector<SourceLocation> findResourceLeaks(const std::string& filePath);
    std::vector<SourceLocation> findPotentialDataRaces(const std::string& filePath);
    
    // Pattern matching
    std::vector<SourceLocation> findPattern(const std::string& pattern,
                                             const std::string& content);
    std::vector<SourceLocation> findCodeClones(const std::string& filePath,
                                               uint32_t minLines = 6,
                                               uint32_t minTokens = 50);
    
    // Data flow analysis
    std::vector<std::string> getVariableValues(const std::string& varName,
                                               const SourceLocation& location);
    bool isVariableUsed(const std::string& varName,
                        const SourceRange& range);
    std::vector<SourceLocation> getVariableDefinitions(const std::string& varName,
                                                        const std::string& filePath);
    
private:
    // Check implementations
    void checkMemorySafety(const std::string& content, AnalysisResult& result);
    void checkPerformance(const std::string& content, AnalysisResult& result);
    void checkSecurity(const std::string& content, AnalysisResult& result);
    void checkConcurrency(const std::string& content, AnalysisResult& result);
    void checkStyle(const std::string& content, AnalysisResult& result);
    void checkModernCpp(const std::string& content, AnalysisResult& result);
    void checkPotentialBugs(const std::string& content, AnalysisResult& result);
    void checkDocumentation(const std::string& content, AnalysisResult& result);
    
    // Individual checks
    void checkNullPointer(const std::string& content, AnalysisResult& result);
    void checkMemoryLeak(const std::string& content, AnalysisResult& result);
    void checkDoubleFree(const std::string& content, AnalysisResult& result);
    void checkBufferOverflow(const std::string& content, AnalysisResult& result);
    void checkUseAfterMove(const std::string& content, AnalysisResult& result);
    void checkUninitializedVariable(const std::string& content, AnalysisResult& result);
    
    void checkInefficientString(const std::string& content, AnalysisResult& result);
    void checkInefficientContainer(const std::string& content, AnalysisResult& result);
    void checkUnnecessaryCopy(const std::string& content, AnalysisResult& result);
    void checkInefficientLoop(const std::string& content, AnalysisResult& result);
    void checkRedundantCondition(const std::string& content, AnalysisResult& result);
    
    void checkSQLInjection(const std::string& content, AnalysisResult& result);
    void checkFormatString(const std::string& content, AnalysisResult& result);
    void checkHardcodedCredentials(const std::string& content, AnalysisResult& result);
    void checkInsecureRandom(const std::string& content, AnalysisResult& result);
    void checkPathTraversal(const std::string& content, AnalysisResult& result);
    
    void checkDataRace(const std::string& content, AnalysisResult& result);
    void checkDeadlock(const std::string& content, AnalysisResult& result);
    void checkRaceCondition(const std::string& content, AnalysisResult& result);
    
    void checkNamingConvention(const std::string& content, AnalysisResult& result);
    void checkMagicNumbers(const std::string& content, AnalysisResult& result);
    void checkLongFunction(const std::string& content, AnalysisResult& result);
    void checkDeepNesting(const std::string& content, AnalysisResult& result);
    void checkMissingBreak(const std::string& content, AnalysisResult& result);
    
    void checkDeprecatedHeader(const std::string& content, AnalysisResult& result);
    void checkOldStyleCast(const std::string& content, AnalysisResult& result);
    void checkMissingOverride(const std::string& content, AnalysisResult& result);
    void checkMissingConstexpr(const std::string& content, AnalysisResult& result);
    void checkRangeBasedFor(const std::string& content, AnalysisResult& result);
    
    // Metric calculations
    uint32_t calculateCyclomaticComplexity(const std::string& code);
    uint32_t calculateCognitiveComplexity(const std::string& code);
    uint32_t calculateNestingDepth(const std::string& code);
    
    std::unordered_set<std::string> enabledChecks_;
    std::unordered_map<std::string, uint32_t> checkSeverities_;
    std::unordered_map<std::string, std::string> options_;
};

// =============================================================================
// CODE STYLE FORMATTER
// =============================================================================

struct FormatStyle {
    // Indentation
    enum class IndentStyle { Space, Tab };
    IndentStyle indentStyle = IndentStyle::Space;
    uint32_t indentWidth = 4;
    uint32_t tabWidth = 4;
    uint32_t continuationIndentWidth = 4;
    bool indentCaseLabels = true;
    bool indentCaseBlocks = false;
    bool indentGotoLabels = true;
    bool indentPPDirectives = false;
    bool indentExternAfterHash = false;
    bool indentWrappedFunctionNames = false;
    bool indentNamespace = true;
    
    // Alignment
    bool alignAfterOpenBracket = true;
    bool alignArrayOfStructures = true;
    bool alignConsecutiveAssignments = false;
    bool alignConsecutiveDeclarations = false;
    bool alignConsecutiveBitFields = false;
    bool alignEscapedNewlines = true;
    bool alignOperands = true;
    bool alignTrailingComments = true;
    
    // Line breaking
    uint32_t columnLimit = 120;
    bool allowAllParametersOnNextLine = true;
    bool allowShortBlocksOnASingleLine = false;
    bool allowShortCaseLabelsOnASingleLine = false;
    bool allowShortFunctionsOnASingleLine = true;
    bool allowShortIfStatementsOnASingleLine = false;
    bool allowShortLoopsOnASingleLine = false;
    bool allowShortLambdasOnASingleLine = true;
    bool alwaysBreakAfterReturnType = false;
    bool alwaysBreakBeforeMultilineStrings = false;
    bool alwaysBreakTemplateDeclarations = true;
    bool breakBeforeBinaryOperators = false;
    bool breakBeforeBraces = false;  // Attach
    bool breakBeforeConceptDeclarations = true;
    bool breakBeforeTernaryOperators = true;
    bool breakConstructorInitializers = false;
    bool breakInheritance = false;
    bool breakStringLiterals = true;
    
    // Spacing
    bool spaceAfterCStyleCast = false;
    bool spaceAfterLogicalNot = false;
    bool spaceAfterTemplateKeyword = true;
    bool spaceAroundPointerOperators = true;
    bool spaceBeforeAssignmentOperators = true;
    bool spaceBeforeCpp11BracedList = false;
    bool spaceBeforeCtorInitializerColon = true;
    bool spaceBeforeInheritanceColon = true;
    bool spaceBeforeParens = true;  // ControlStatements
    bool spaceBeforeRangeBasedForLoopColon = true;
    bool spaceBeforeSquareBrackets = false;
    bool spaceBetweenEmptyParentheses = false;
    bool spaceInEmptyBlock = false;
    bool spaceInEmptyParentheses = false;
    bool spaceInSquareBrackets = false;
    bool spacesBeforeTrailingComments = 1;
    bool spacesInAngles = false;
    bool spacesInCStyleCastParentheses = false;
    bool spacesInContainerLiterals = true;
    bool spacesInParentheses = false;
    bool spacesInSquareBrackets = false;
    
    // Braces
    enum class BraceBreakingStyle { 
        Attach, 
        Linux, 
        Stroustrup, 
        Allman, 
        GNU, 
        Weber, 
        Whitesmiths 
    };
    BraceBreakingStyle breakBeforeBraces_ = BraceBreakingStyle::Attach;
    
    // Include sorting
    enum class IncludeSortingStyle { 
        Never, 
        CaseSensitive, 
        CaseInsensitive 
    };
    IncludeSortingStyle includeSorting = IncludeSortingStyle::CaseSensitive;
    std::vector<std::string> includeCategories;
    std::string includeBlockStyle = "Preserve";
    
    // Other
    bool fixNamespaceComments = true;
    bool maxEmptyLinesToKeep = 1;
    bool keepEmptyLinesAtTheStartOfBlocks = false;
    bool reflowComments = true;
    bool sortIncludes = true;
    bool sortUsingDeclarations = true;
    
    // Presets
    static FormatStyle LLVM();
    static FormatStyle Google();
    static FormatStyle Chromium();
    static FormatStyle Mozilla();
    static FormatStyle WebKit();
    static FormatStyle Microsoft();
    static FormatStyle ClangFormat();
    static FormatStyle fromFile(const std::string& path);
};

class CodeFormatter {
public:
    CodeFormatter();
    explicit CodeFormatter(const FormatStyle& style);
    ~CodeFormatter();
    
    // Format
    std::string format(const std::string& source);
    std::string formatRange(const std::string& source,
                            const SourceRange& range);
    std::vector<TextEdit> formatEdits(const std::string& source);
    
    // Style management
    void setStyle(const FormatStyle& style) { style_ = style; }
    FormatStyle getStyle() const { return style_; }
    bool loadStyleFile(const std::string& path);
    
private:
    FormatStyle style_;
    
    // Formatting passes
    void formatIndentation(std::string& source);
    void formatSpacing(std::string& source);
    void formatBraces(std::string& source);
    void formatLineBreaks(std::string& source);
    void sortIncludes(std::string& source);
    void alignDeclarations(std::string& source);
    void formatComments(std::string& source);
    void formatNamespaceComments(std::string& source);
    
    // Token-based formatting
    struct Token {
        std::string text;
        uint32_t line = 0;
        uint32_t column = 0;
        enum Type { 
            Keyword, Identifier, Number, String, Char,
            Operator, Punctuation, Comment, Whitespace, Newline, EOF_
        } type;
    };
    
    std::vector<Token> tokenize(const std::string& source);
    std::string detokenize(const std::vector<Token>& tokens);
    
    // Utility
    std::string getIndentation(uint32_t level);
    uint32_t getIndentationLevel(const std::string& line);
    bool isNamespace(const std::string& line);
    bool isPreprocessorDirective(const std::string& line);
    bool isAccessSpecifier(const std::string& token);
    std::vector<std::string> splitLines(const std::string& source);
    std::string joinLines(const std::vector<std::string>& lines);
};

// =============================================================================
// CODE NAVIGATION
// =============================================================================

struct CallGraphNode {
    std::string functionName;
    std::string signature;
    SourceLocation location;
    std::vector<CallGraphNode*> callees;
    std::vector<CallGraphNode*> callers;
    uint32_t callCount = 0;
    uint32_t complexity = 0;
    bool isRecursive = false;
    bool isVirtual = false;
    bool isTemplate = false;
};

struct InheritanceNode {
    std::string className;
    SourceLocation location;
    std::vector<InheritanceNode*> baseClasses;
    std::vector<InheritanceNode*> derivedClasses;
    std::vector<std::string> methods;
    std::vector<std::string> fields;
    uint32_t depth = 0;
    bool isAbstract = false;
    bool isInterface = false;
};

struct DependencyNode {
    std::string fileName;
    std::vector<DependencyNode*> includes;
    std::vector<DependencyNode*> includedBy;
    uint32_t dependencyCount = 0;
    bool isCyclic = false;
};

class CodeNavigator {
public:
    CodeNavigator();
    ~CodeNavigator();
    
    // Call hierarchy
    CallGraphNode* getCallGraph(const std::string& filePath);
    CallGraphNode* findFunction(const std::string& name);
    std::vector<CallGraphNode*> getCallers(const std::string& functionName);
    std::vector<CallGraphNode*> getCallees(const std::string& functionName);
    std::vector<std::string> findRecursiveCalls();
    std::vector<std::string> findDeadCode();
    
    // Inheritance hierarchy
    InheritanceNode* getInheritanceTree(const std::string& filePath);
    InheritanceNode* findClass(const std::string& name);
    std::vector<InheritanceNode*> getBaseClasses(const std::string& className);
    std::vector<InheritanceNode*> getDerivedClasses(const std::string& className);
    std::vector<std::string> findVirtualMethodOverrides(
        const std::string& className,
        const std::string& methodName);
    std::vector<std::string> findUnusedVirtualMethods();
    
    // Include graph
    DependencyNode* getIncludeGraph(const std::string& filePath);
    std::vector<std::string> getIncludeChain(const std::string& fromFile,
                                              const std::string& toFile);
    std::vector<std::string> findIncludeCycles();
    std::vector<std::string> findMissingIncludes(const std::string& filePath);
    std::vector<std::string> findUnusedIncludes(const std::string& filePath);
    
    // Symbol navigation
    std::vector<SourceLocation> findAllSymbols(const std::string& name);
    std::vector<SourceLocation> findSymbolDeclarations(const std::string& name);
    std::vector<SourceLocation> findSymbolDefinitions(const std::string& name);
    std::vector<SourceLocation> findSymbolUsages(const std::string& name);
    
    // File navigation
    std::vector<std::string> findHeaderForSource(const std::string& sourcePath);
    std::vector<std::string> findSourceForHeader(const std::string& headerPath);
    std::vector<std::string> findMatchingFiles(const std::string& pattern);
    std::vector<std::string> findRelatedFiles(const std::string& filePath);
    
private:
    std::unordered_map<std::string, std::unique_ptr<CallGraphNode>> callGraph_;
    std::unordered_map<std::string, std::unique_ptr<InheritanceNode>> inheritanceTree_;
    std::unordered_map<std::string, std::unique_ptr<DependencyNode>> includeGraph_;
    
    void buildCallGraph(const std::string& filePath);
    void buildInheritanceTree(const std::string& filePath);
    void buildIncludeGraph(const std::string& filePath);
};

// =============================================================================
// CODE METRICS DASHBOARD
// =============================================================================

struct ProjectMetrics {
    // Summary
    uint32_t totalFiles = 0;
    uint32_t totalLinesOfCode = 0;
    uint32_t totalFunctions = 0;
    uint32_t totalClasses = 0;
    uint32_t totalComments = 0;
    
    // Quality
    double averageComplexity = 0.0;
    double testCoverage = 0.0;
    uint32_t codeSmells = 0;
    uint32_t bugs = 0;
    uint32_t vulnerabilities = 0;
    uint32_t technicalDebt = 0;  // In minutes
    
    // Technical debt breakdown
    std::unordered_map<std::string, uint32_t> debtByCategory;
    std::unordered_map<std::string, uint32_t> debtByFile;
    
    // Hotspots
    std::vector<std::pair<std::string, uint32_t>> mostComplexFiles;
    std::vector<std::pair<std::string, uint32_t>> mostComplexFunctions;
    std::vector<std::pair<std::string, uint32_t>> mostChangedFiles;
    std::vector<std::pair<std::string, uint32_t>> duplicateCode;
    
    // Trends (if historical data available)
    std::vector<std::pair<std::string, double>> complexityTrend;
    std::vector<std::pair<std::string, double>> coverageTrend;
    std::vector<std::pair<std::string, uint32_t>> debtTrend;
    
    std::string toJson() const;
};

class MetricsDashboard {
public:
    MetricsDashboard();
    ~MetricsDashboard();
    
    // Compute metrics
    ProjectMetrics computeProjectMetrics(const std::string& projectPath);
    CodeMetrics computeFileMetrics(const std::string& filePath);
    FunctionMetrics computeFunctionMetrics(const std::string& filePath,
                                           const SourceLocation& loc);
    
    // Aggregate
    void updateMetrics();
    void resetMetrics();
    
    // Query
    ProjectMetrics getProjectMetrics() const { return projectMetrics_; }
    CodeMetrics getFileMetrics(const std::string& filePath) const;
    FunctionMetrics getFunctionMetrics(const std::string& qualifiedName) const;
    
    // Export
    void exportToJSON(const std::string& outputPath);
    void exportToHTML(const std::string& outputPath);
    void exportToCSV(const std::string& outputPath);
    
    // Historical tracking
    void saveBaseline();
    void compareWithBaseline();
    std::vector<std::pair<std::string, std::string>> getChangesFromBaseline();
    
private:
    ProjectMetrics projectMetrics_;
    std::unordered_map<std::string, CodeMetrics> fileMetrics_;
    std::unordered_map<std::string, FunctionMetrics> functionMetrics_;
    std::unordered_map<std::string, ProjectMetrics> history_;
    
    std::string baselinePath_;
    std::chrono::system_clock::time_point lastUpdate_;
};

// =============================================================================
// QUICK ACTIONS
// =============================================================================

struct QuickAction {
    std::string id;
    std::string title;
    std::string description;
    std::string category;  // "refactoring", "quickfix", "source_action"
    std::string kind;
    std::function<RefactoringResult()> execute;
    bool isPreferred = false;
    bool isDisabled = false;
    std::string disabledReason;
};

class QuickActionProvider {
public:
    QuickActionProvider();
    ~QuickActionProvider();
    
    // Get available actions
    std::vector<QuickAction> getActions(const RefactoringContext& ctx);
    std::vector<QuickAction> getRefactorings(const RefactoringContext& ctx);
    std::vector<QuickAction> getQuickFixes(const RefactoringContext& ctx,
                                            const std::vector<AnalysisDiagnostic>& diags);
    std::vector<QuickAction> getSourceActions(const RefactoringContext& ctx);
    
    // Execute action
    RefactoringResult executeAction(const std::string& actionId,
                                    const RefactoringContext& ctx);
    
private:
    std::unordered_map<std::string, QuickAction> registeredActions_;
    
    void registerBuiltInActions();
};

} // namespace Refactoring
} // namespace RawrXD
