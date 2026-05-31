#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <optional>
#include <future>

namespace RawrXD {
namespace IDE {

// =============================================================================
// IDE FEATURES - Core IDE Services
// =============================================================================
// Provides: IntelliSense, Diagnostics, Navigation, Refactoring, Formatting
// Connects: LSP Server ↔ Editor ↔ Compiler
// =============================================================================

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

class IntelliSenseEngine;
class DiagnosticEngine;
class NavigationEngine;
class RefactoringEngine;
class FormattingEngine;
class SymbolIndex;
class DocumentModel;

// =============================================================================
// POSITION / RANGE (LSP-compatible)
// =============================================================================

struct Position {
    uint32_t line = 0;
    uint32_t character = 0;
    
    bool operator==(const Position& o) const { 
        return line == o.line && character == o.character; 
    }
    bool operator<(const Position& o) const {
        return line < o.line || (line == o.line && character < o.character);
    }
};

struct Range {
    Position start;
    Position end;
    
    bool contains(const Position& pos) const {
        return start <= pos && pos < end;
    }
    bool operator==(const Range& o) const {
        return start == o.start && end == o.end;
    }
};

// =============================================================================
// SYMBOL INFORMATION
// =============================================================================

enum class SymbolKind : uint32_t {
    Unknown = 0,
    File = 1, Module = 2, Namespace = 3, Package = 4,
    Class = 5, Method = 6, Property = 7, Field = 8,
    Constructor = 9, Enum = 10, Interface = 11, Function = 12,
    Variable = 13, Constant = 14, String = 15, Number = 16,
    Boolean = 17, Array = 18, Object = 19, Key = 20,
    Null = 21, EnumMember = 22, Struct = 23, Event = 24,
    Operator = 25, TypeParameter = 26
};

struct Symbol {
    std::string name;
    std::string detail;
    SymbolKind kind = SymbolKind::Unknown;
    Range range;
    Range selectionRange;
    std::string containerName;
    std::string uri;
    bool isDeprecated = false;
    bool isDefinition = false;
    std::string usr;  // Unified Symbol Resolution
    
    bool operator==(const Symbol& o) const { return usr == o.usr; }
};

// =============================================================================
// COMPLETION ITEM
// =============================================================================

enum class CompletionKind : uint32_t {
    Text = 1, Method = 2, Function = 3, Constructor = 4,
    Field = 5, Variable = 6, Class = 7, Interface = 8,
    Module = 9, Property = 10, Unit = 11, Value = 12,
    Enum = 13, Keyword = 14, Snippet = 15, Color = 16,
    File = 17, Reference = 18, Folder = 19, EnumMember = 20,
    Constant = 21, Struct = 22, Event = 23, Operator = 24,
    TypeParameter = 25
};

struct CompletionItem {
    std::string label;
    CompletionKind kind = CompletionKind::Text;
    std::string detail;
    std::string documentation;
    std::string sortText;
    std::string filterText;
    std::string insertText;
    bool isSnippet = false;
    bool isDeprecated = false;
    bool preselect = false;
    std::vector<std::string> commitCharacters;
    std::vector<std::string> additionalTextEdits;
};

// =============================================================================
// DIAGNOSTIC
// =============================================================================

enum class DiagnosticSeverity : uint32_t {
    Error = 1, Warning = 2, Information = 3, Hint = 4
};

struct Diagnostic {
    Range range;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string message;
    std::string source;
    uint32_t code = 0;
    std::vector<std::string> tags;
    std::vector<Diagnostic> relatedInformation;
};

// =============================================================================
// HOVER / SIGNATURE HELP
// =============================================================================

struct HoverInfo {
    std::string contents;
    std::optional<Range> range;
};

struct ParameterInfo {
    std::string label;
    std::string documentation;
};

struct SignatureInfo {
    std::string label;
    std::string documentation;
    std::vector<ParameterInfo> parameters;
    uint32_t activeParameter = 0;
};

struct SignatureHelp {
    std::vector<SignatureInfo> signatures;
    uint32_t activeSignature = 0;
    uint32_t activeParameter = 0;
};

// =============================================================================
// CODE ACTION / REFACTORING
// =============================================================================

enum class CodeActionKind : uint32_t {
    QuickFix = 1,
    Refactor = 2,
    RefactorExtract = 3,
    RefactorInline = 4,
    RefactorRewrite = 5,
    Source = 6,
    SourceOrganizeImports = 7,
    SourceFixAll = 8
};

struct TextEdit {
    Range range;
    std::string newText;
};

struct CodeAction {
    std::string title;
    CodeActionKind kind = CodeActionKind::QuickFix;
    std::vector<Diagnostic> diagnostics;
    bool isPreferred = false;
    std::vector<TextEdit> edits;
};

// =============================================================================
// DOCUMENT MODEL
// =============================================================================

class DocumentModel {
public:
    DocumentModel(const std::string& uri, const std::string& text);
    ~DocumentModel();
    
    // Text access
    const std::string& text() const { return text_; }
    std::string line(uint32_t lineNum) const;
    uint32_t lineCount() const { return static_cast<uint32_t>(lines_.size()); }
    
    // Position conversion
    Position positionAt(uint32_t offset) const;
    uint32_t offsetAt(const Position& pos) const;
    
    // Word extraction
    std::string wordAt(const Position& pos) const;
    Range wordRangeAt(const Position& pos) const;
    
    // Updates
    void update(const std::string& newText);
    void applyEdit(const TextEdit& edit);
    void applyEdits(const std::vector<TextEdit>& edits);
    
    // Version
    int32_t version() const { return version_; }
    void incrementVersion() { version_++; }
    
    // Language
    std::string languageId() const { return languageId_; }
    void setLanguageId(const std::string& id) { languageId_ = id; }
    
private:
    std::string uri_;
    std::string text_;
    std::vector<std::string> lines_;
    std::vector<uint32_t> lineOffsets_;
    int32_t version_ = 0;
    std::string languageId_ = "cpp";
    
    void rebuildLines();
};

// =============================================================================
// SYMBOL INDEX
// =============================================================================

class SymbolIndex {
public:
    SymbolIndex();
    ~SymbolIndex();
    
    // Indexing
    void addSymbol(const Symbol& symbol);
    void removeSymbol(const std::string& usr);
    void clearFile(const std::string& uri);
    void buildIndex(const std::string& uri, const std::string& text);
    
    // Queries
    std::vector<Symbol> findByName(const std::string& name) const;
    std::optional<Symbol> findByUsr(const std::string& usr) const;
    std::vector<Symbol> findByUri(const std::string& uri) const;
    std::optional<Symbol> findAtPosition(const std::string& uri, const Position& pos) const;
    std::vector<Symbol> findReferences(const std::string& usr) const;
    std::vector<Symbol> findInScope(const std::string& containerName) const;
    
    // Fuzzy search
    std::vector<Symbol> fuzzyFind(const std::string& query, uint32_t maxResults = 50) const;
    
    // Stats
    size_t symbolCount() const;
    size_t fileCount() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// =============================================================================
// INTELLISENSE ENGINE
// =============================================================================

class IntelliSenseEngine {
public:
    IntelliSenseEngine(SymbolIndex& index);
    ~IntelliSenseEngine();
    
    // Completion
    std::vector<CompletionItem> getCompletions(
        const DocumentModel& doc,
        const Position& pos,
        const std::string& triggerChar = ""
    );
    
    // Hover
    std::optional<HoverInfo> getHover(
        const DocumentModel& doc,
        const Position& pos
    );
    
    // Signature help
    std::optional<SignatureHelp> getSignatureHelp(
        const DocumentModel& doc,
        const Position& pos
    );
    
    // Context analysis
    std::string getContext(const DocumentModel& doc, const Position& pos) const;
    bool isInComment(const DocumentModel& doc, const Position& pos) const;
    bool isInString(const DocumentModel& doc, const Position& pos) const;
    
private:
    SymbolIndex& index_;
    
    std::vector<CompletionItem> getKeywordCompletions() const;
    std::vector<CompletionItem> getTypeCompletions() const;
    std::vector<CompletionItem> getMemberCompletions(
        const DocumentModel& doc, const Position& pos
    );
    std::vector<CompletionItem> getScopeCompletions(
        const DocumentModel& doc, const Position& pos
    );
};

// =============================================================================
// DIAGNOSTIC ENGINE
// =============================================================================

class DiagnosticEngine {
public:
    DiagnosticEngine();
    ~DiagnosticEngine();
    
    // Analysis
    std::vector<Diagnostic> analyze(const DocumentModel& doc);
    std::vector<Diagnostic> analyzeSyntax(const DocumentModel& doc);
    std::vector<Diagnostic> analyzeSemantics(const DocumentModel& doc);
    
    // Quick fixes
    std::vector<CodeAction> getQuickFixes(
        const DocumentModel& doc,
        const Diagnostic& diagnostic
    );
    
    // Configuration
    void setSeverity(const std::string& rule, DiagnosticSeverity severity);
    void enableRule(const std::string& rule);
    void disableRule(const std::string& rule);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    std::vector<Diagnostic> checkMissingSemicolons(const DocumentModel& doc);
    std::vector<Diagnostic> checkUnmatchedBraces(const DocumentModel& doc);
    std::vector<Diagnostic> checkUnusedVariables(const DocumentModel& doc);
    std::vector<Diagnostic> checkUndefinedSymbols(const DocumentModel& doc);
};

// =============================================================================
// NAVIGATION ENGINE
// =============================================================================

class NavigationEngine {
public:
    NavigationEngine(SymbolIndex& index);
    ~NavigationEngine();
    
    // Go to definition
    std::optional<Symbol> gotoDefinition(
        const DocumentModel& doc,
        const Position& pos
    );
    
    // Find references
    std::vector<Symbol> findReferences(
        const DocumentModel& doc,
        const Position& pos
    );
    
    // Document symbols (outline)
    std::vector<Symbol> getDocumentSymbols(const DocumentModel& doc);
    
    // Workspace symbols
    std::vector<Symbol> getWorkspaceSymbols(const std::string& query);
    
    // Document highlights
    std::vector<Range> getDocumentHighlights(
        const DocumentModel& doc,
        const Position& pos
    );
    
    // Peek
    std::optional<std::string> peekDefinition(
        const DocumentModel& doc,
        const Position& pos
    );
    
private:
    SymbolIndex& index_;
};

// =============================================================================
// REFACTORING ENGINE
// =============================================================================

class RefactoringEngine {
public:
    RefactoringEngine(SymbolIndex& index);
    ~RefactoringEngine();
    
    // Rename
    std::vector<TextEdit> rename(
        const DocumentModel& doc,
        const Position& pos,
        const std::string& newName
    );
    
    // Extract method
    std::vector<TextEdit> extractMethod(
        const DocumentModel& doc,
        const Range& range,
        const std::string& methodName
    );
    
    // Extract variable
    std::vector<TextEdit> extractVariable(
        const DocumentModel& doc,
        const Range& range,
        const std::string& varName
    );
    
    // Inline
    std::vector<TextEdit> inlineVariable(
        const DocumentModel& doc,
        const Position& pos
    );
    
    // Organize imports
    std::vector<TextEdit> organizeIncludes(const DocumentModel& doc);
    
private:
    SymbolIndex& index_;
};

// =============================================================================
// FORMATTING ENGINE
// =============================================================================

class FormattingEngine {
public:
    struct Options {
        uint32_t tabSize = 4;
        bool insertSpaces = true;
        bool trimTrailingWhitespace = true;
        bool insertFinalNewline = true;
        bool trimFinalNewlines = true;
        uint32_t maxLineLength = 120;
        bool alignConsecutiveAssignments = false;
        bool alignTrailingComments = false;
        bool breakBeforeBraces = true;
        bool allowShortFunctionsOnASingleLine = false;
    };
    
    FormattingEngine();
    ~FormattingEngine();
    
    // Format document
    std::vector<TextEdit> formatDocument(
        const DocumentModel& doc,
        const Options& options = {}
    );
    
    // Format range
    std::vector<TextEdit> formatRange(
        const DocumentModel& doc,
        const Range& range,
        const Options& options = {}
    );
    
    // Format on type
    std::vector<TextEdit> formatOnType(
        const DocumentModel& doc,
        const Position& pos,
        char ch,
        const Options& options = {}
    );
    
    // Set options
    void setOptions(const Options& options) { options_ = options; }
    
private:
    Options options_;
    
    std::string formatLine(const std::string& line, uint32_t indentLevel) const;
    uint32_t calculateIndent(const DocumentModel& doc, uint32_t lineNum) const;
    std::string trimTrailing(const std::string& line) const;
};

// =============================================================================
// IDE SERVICE MANAGER
// =============================================================================

class IDEServiceManager {
public:
    IDEServiceManager();
    ~IDEServiceManager();
    
    // Lifecycle
    bool initialize(const std::string& workspaceRoot);
    void shutdown();
    bool isInitialized() const { return initialized_; }
    
    // Document management
    void openDocument(const std::string& uri, const std::string& text, const std::string& languageId);
    void closeDocument(const std::string& uri);
    void updateDocument(const std::string& uri, const std::string& newText);
    void applyEdit(const std::string& uri, const TextEdit& edit);
    
    std::shared_ptr<DocumentModel> getDocument(const std::string& uri);
    bool hasDocument(const std::string& uri) const;
    std::vector<std::string> getOpenDocuments() const;
    
    // Services
    IntelliSenseEngine& intelliSense() { return *intelliSense_; }
    DiagnosticEngine& diagnostics() { return *diagnostics_; }
    NavigationEngine& navigation() { return *navigation_; }
    RefactoringEngine& refactoring() { return *refactoring_; }
    FormattingEngine& formatting() { return *formatting_; }
    SymbolIndex& symbolIndex() { return *symbolIndex_; }
    
    // Events
    using DiagnosticHandler = std::function<void(const std::string& uri, const std::vector<Diagnostic>&)>;
    void setDiagnosticHandler(DiagnosticHandler handler) { diagnosticHandler_ = handler; }
    
    void publishDiagnostics(const std::string& uri);
    
private:
    bool initialized_ = false;
    std::string workspaceRoot_;
    
    std::unordered_map<std::string, std::shared_ptr<DocumentModel>> documents_;
    std::mutex documentsMutex_;
    
    std::unique_ptr<SymbolIndex> symbolIndex_;
    std::unique_ptr<IntelliSenseEngine> intelliSense_;
    std::unique_ptr<DiagnosticEngine> diagnostics_;
    std::unique_ptr<NavigationEngine> navigation_;
    std::unique_ptr<RefactoringEngine> refactoring_;
    std::unique_ptr<FormattingEngine> formatting_;
    
    DiagnosticHandler diagnosticHandler_;
    
    void onDocumentChanged(const std::string& uri);
};

} // namespace IDE
} // namespace RawrXD
