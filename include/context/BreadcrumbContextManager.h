#pragma once
/*  BreadcrumbContextManager.h  -  Comprehensive Context with Breadcrumb Navigation (C++20, no Qt)
*/

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace RawrXD {
namespace Context {

class DrawingContext;
class BreadcrumbRenderer;

enum class ContextType {
    Tool, Symbol, File, SourceControl, Screenshot, Instruction, Relationship, OpenEditor,
};

enum class SymbolKind {
    Function, Class, Struct, Enum, Variable, Macro, Namespace, Interface, Module,
};

enum class SCStatus {
    Untracked, Modified, Staged, Committed, Pushed, Conflicted,
};

struct Breadcrumb {
    std::string id;
    std::string label;
    std::string displayName;
    ContextType type = ContextType::File;
    std::string iconPath;
    bool isClickable = true;
    std::string metadata;   // JSON-serialized (replaces QJsonObject)
    int64_t timestamp = 0;  // seconds since epoch (replaces QDateTime)
};

struct SymbolContext {
    std::string name;
    SymbolKind kind = SymbolKind::Function;
    std::string filePath;
    int lineNumber = 0;
    int columnNumber = 0;
    std::string signature;
    std::string documentation;
    std::vector<std::string> relatedSymbols;
    std::map<std::string, std::string> attributes;
    int scopeStart = 0;
    int scopeEnd = 0;
};

struct ToolContext {
    std::string toolName;
    std::string executablePath;
    std::string version;
    std::vector<std::string> arguments;
    std::map<std::string, std::string> environment;
    std::string workingDirectory;
    std::string description;
    bool isAvailable = false;
};

struct FileContext {
    std::string absolutePath;
    std::string relativePath;
    std::string fileName;
    std::string fileExtension;
    int64_t fileSize = 0;
    int64_t lastModified = 0;
    std::vector<std::string> relatedFiles;
    std::vector<SymbolContext> containedSymbols;
    std::string projectRoot;
};

struct SourceControlContext {
    std::string repository;
    std::string branch;
    std::string commitHash;
    std::string commitMessage;
    std::string author;
    int64_t commitDate = 0;
    SCStatus status = SCStatus::Untracked;
    std::vector<std::string> changedFiles;
    std::string diff;
    int addedLines = 0;
    int removedLines = 0;
};

struct ScreenshotAnnotation {
    std::string id;
    int x = 0, y = 0, width = 0, height = 0;
    std::string text;
    std::string highlightColor;
    int64_t captureTime = 0;
};

struct InstructionBlock {
    std::string id;
    std::string title;
    std::string content;
    std::vector<std::string> tags;
    std::string relatedFile;
    int lineNumber = 0;
    bool isVisible = true;
    std::string backgroundColor;
    std::string borderColor;
};

struct RelationshipContext {
    std::string sourceId;
    std::string targetId;
    std::string relationshipType;
    std::string description;
    std::string strength;
};

struct OpenEditorContext {
    std::string filePath;
    int cursorLine = 0;
    int cursorColumn = 0;
    int scrollPosition = 0;
    std::string selectedText;
    std::vector<int> breakpoints;
    bool isModified = false;
    int64_t lastAccessed = 0;
};

class BreadcrumbChain {
public:
    BreadcrumbChain();
    ~BreadcrumbChain();

    void push(const Breadcrumb& crumb);
    void pop();
    void jump(int index);
    void clear();

    std::vector<Breadcrumb> getChain() const;
    Breadcrumb getCurrentBreadcrumb() const;
    int getCurrentIndex() const;
    int getChainLength() const;

    std::string toJSON() const;
    void fromJSON(const std::string& json);

private:
    std::vector<Breadcrumb> m_chain;
    int m_currentIndex = 0;
};

class BreadcrumbContextManager {
public:
    BreadcrumbContextManager() = default;
    explicit BreadcrumbContextManager(void* /*parent*/) {}
    ~BreadcrumbContextManager();

    void initialize(const std::string& workspacePath);
    void shutdown();

    void registerTool(const std::string& toolName, const ToolContext& context);
    ToolContext getTool(const std::string& toolName) const;
    std::vector<ToolContext> getAllTools() const;
    void unregisterTool(const std::string& toolName);

    void registerSymbol(const std::string& filePath, const SymbolContext& symbol);
    SymbolContext getSymbol(const std::string& symbolName) const;
    std::vector<SymbolContext> getSymbolsInFile(const std::string& filePath) const;
    std::vector<SymbolContext> findSymbolsByKind(SymbolKind kind) const;
    void updateSymbolUsage(const std::string& symbolName);

    void registerFile(const std::string& filePath);
    FileContext getFileContext(const std::string& filePath) const;
    std::vector<FileContext> getRelatedFiles(const std::string& filePath) const;
    void updateFileMetadata(const std::string& filePath);
    std::vector<FileContext> searchFiles(const std::string& pattern) const;

    void updateSourceControlContext(const std::string& repository);
    SourceControlContext getSourceControlContext() const;
    std::vector<std::string> getChangedFiles() const;
    std::string getLatestCommitInfo() const;
    void scanRepositoryStatus();

    void captureScreenshot(const std::string& filePath);
    void addScreenshotAnnotation(const ScreenshotAnnotation& annotation);
    std::vector<ScreenshotAnnotation> getScreenshotAnnotations(const std::string& screenshotId) const;

    void registerInstruction(const InstructionBlock& instruction);
    InstructionBlock getInstruction(const std::string& instructionId) const;
    std::vector<InstructionBlock> getInstructionsForFile(const std::string& filePath) const;
    std::vector<InstructionBlock> getAllInstructions() const;
    void toggleInstructionVisibility(const std::string& instructionId);

    void registerRelationship(const RelationshipContext& relationship);
    std::vector<RelationshipContext> getRelationshipsFor(const std::string& entityId) const;
    std::vector<std::string> getDependencies(const std::string& entityId) const;
    std::vector<std::string> getDependents(const std::string& entityId) const;

    void registerOpenEditor(const std::string& filePath, const OpenEditorContext& editorCtx);
    void updateEditorState(const std::string& filePath, int line, int column, const std::string& selectedText = "");
    std::vector<OpenEditorContext> getOpenEditors() const;
    OpenEditorContext getEditorContext(const std::string& filePath) const;
    void closeEditor(const std::string& filePath);

    BreadcrumbChain& getBreadcrumbChain();
    void pushContextBreadcrumb(const ContextType& type, const std::string& identifier);
    void navigateToBreadcrumb(int index);

    std::string getCompleteContext(const std::string& identifier) const;
    std::string analyzeContextRelationships() const;
    std::vector<std::string> getContextPath(const std::string& target) const;
    std::string generateContextReport() const;

    void exportContextToJSON(const std::string& filePath) const;
    void importContextFromJSON(const std::string& filePath);

    void indexWorkspace();
    void rebuildIndices();
    double getIndexingProgress() const;

    void setOnContextChanged(std::function<void(const std::string&)> fn) { m_onContextChanged = std::move(fn); }
    void setOnToolRegistered(std::function<void(const std::string&)> fn) { m_onToolRegistered = std::move(fn); }
    void setOnSymbolRegistered(std::function<void(const std::string&)> fn) { m_onSymbolRegistered = std::move(fn); }
    void setOnFileRegistered(std::function<void(const std::string&)> fn) { m_onFileRegistered = std::move(fn); }

private:
    void scanSymbols(const std::string& filePath);
    void analyzeFileRelationships();
    void cacheSymbolReferences();

    std::map<std::string, ToolContext> m_toolRegistry;
    std::map<std::string, SymbolContext> m_symbolRegistry;
    std::map<std::string, FileContext> m_fileRegistry;
    std::map<std::string, std::vector<SymbolContext>> m_fileSymbols;
    std::map<std::string, RelationshipContext> m_relationships;
    std::map<std::string, InstructionBlock> m_instructions;
    std::map<std::string, OpenEditorContext> m_openEditors;
    std::map<std::string, std::vector<ScreenshotAnnotation>> m_screenshots;

    SourceControlContext m_scContext;
    BreadcrumbChain m_breadcrumbs;
    std::string m_workspacePath;

    std::function<void(const std::string&)> m_onContextChanged;
    std::function<void(const std::string&)> m_onToolRegistered;
    std::function<void(const std::string&)> m_onSymbolRegistered;
    std::function<void(const std::string&)> m_onFileRegistered;
};

} // namespace Context
} // namespace RawrXD
