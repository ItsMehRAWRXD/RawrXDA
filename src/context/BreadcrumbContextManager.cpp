/*  BreadcrumbContextManager.cpp  -  Implementation
    
    Comprehensive context management system with breadcrumb-style navigation.
    Handles tools, symbols, files, source control, screenshots, instructions,
    relationships, and open editor state.
*/

#include "../../include/context/BreadcrumbContextManager.h"
#include <algorithm>

namespace RawrXD {
namespace Context {

// ============================================================================
// BREADCRUMB CHAIN IMPLEMENTATION
// ============================================================================

BreadcrumbChain::BreadcrumbChain() : m_currentIndex(-1) {}

BreadcrumbChain::~BreadcrumbChain() {}

void BreadcrumbChain::push(const Breadcrumb& crumb) {
    // Remove everything after current index if we've jumped backward
    if (m_currentIndex >= 0 && m_currentIndex < m_chain.size() - 1) {
        m_chain.erase(m_chain.begin() + m_currentIndex + 1, m_chain.end());
    }
    
    m_chain.append(crumb);
    m_currentIndex = m_chain.size() - 1;
}

void BreadcrumbChain::pop() {
    if (m_currentIndex > 0) {
        m_currentIndex--;
    }
}

void BreadcrumbChain::jump(int index) {
    if (index >= 0 && index < m_chain.size()) {
        m_currentIndex = index;
    }
}

void BreadcrumbChain::clear() {
    m_chain.clear();
    m_currentIndex = -1;
}

std::vector<Breadcrumb> BreadcrumbChain::getChain() const {
    return m_chain;
}

Breadcrumb BreadcrumbChain::getCurrentBreadcrumb() const {
    if (m_currentIndex >= 0 && m_currentIndex < m_chain.size()) {
        return m_chain[m_currentIndex];
    }
    return Breadcrumb();
}

int BreadcrumbChain::getCurrentIndex() const {
    return m_currentIndex;
}

int BreadcrumbChain::getChainLength() const {
    return m_chain.size();
}

void* BreadcrumbChain::toJSON() const {
    void* arr;
    for (const auto& crumb : m_chain) {
        void* obj;
        obj["id"] = crumb.id;
        obj["label"] = crumb.label;
        obj["displayName"] = crumb.displayName;
        obj["type"] = static_cast<int>(crumb.type);
        obj["iconPath"] = crumb.iconPath;
        obj["isClickable"] = crumb.isClickable;
        obj["metadata"] = crumb.metadata;
        obj["timestamp"] = crumb.timestamp.toString(ISODate);
        arr.append(obj);
    }
    return arr;
}

void BreadcrumbChain::fromJSON(const void*& json) {
    m_chain.clear();
    for (const auto& val : json) {
        void* obj = val.toObject();
        Breadcrumb crumb;
        crumb.id = obj["id"].toString();
        crumb.label = obj["label"].toString();
        crumb.displayName = obj["displayName"].toString();
        crumb.type = static_cast<ContextType>(obj["type"]);
        crumb.iconPath = obj["iconPath"].toString();
        crumb.isClickable = obj["isClickable"].toBool();
        crumb.metadata = obj["metadata"].toObject();
        crumb.timestamp = // DateTime::fromString(obj["timestamp"].toString(), ISODate);
        m_chain.append(crumb);
    }
    m_currentIndex = std::max(0, static_cast<int>(m_chain.size()) - 1);
}

// ============================================================================
// CONTEXT MANAGER IMPLEMENTATION
// ============================================================================

BreadcrumbContextManager::BreadcrumbContextManager()
    , m_workspacePath("") {
}

BreadcrumbContextManager::~BreadcrumbContextManager() {
    shutdown();
}

void BreadcrumbContextManager::initialize(const std::string& workspacePath) {
    m_workspacePath = workspacePath;
    
    // Create initial workspace structure
    // wsDir(workspacePath);
    if (!wsDir.exists()) {
        wsDir.mkpath(".");
    }
    
    // Index workspace on initialization
    indexWorkspace();
    
    indexingComplete();
}

void BreadcrumbContextManager::shutdown() {
    m_toolRegistry.clear();
    m_symbolRegistry.clear();
    m_fileRegistry.clear();
    m_relationships.clear();
    m_instructions.clear();
    m_openEditors.clear();
    m_screenshots.clear();
    m_breadcrumbs.clear();
}

// ========== TOOL CONTEXT ==========

void BreadcrumbContextManager::registerTool(const std::string& toolName, const ToolContext& context) {
    m_toolRegistry[toolName] = context;
    
    // Add breadcrumb
    Breadcrumb crumb;
    crumb.id = "tool_" + toolName;
    crumb.label = toolName;
    crumb.displayName = toolName;
    crumb.type = ContextType::Tool;
    m_breadcrumbs.push(crumb);
    
    toolRegistered(toolName);
    contextChanged(crumb.id);
}

ToolContext BreadcrumbContextManager::getTool(const std::string& toolName) const {
    if (m_toolRegistry.contains(toolName)) {
        return m_toolRegistry[toolName];
    }
    return ToolContext();
}

std::vector<ToolContext> BreadcrumbContextManager::getAllTools() const {
    return m_toolRegistry.values();
}

void BreadcrumbContextManager::unregisterTool(const std::string& toolName) {
    m_toolRegistry.remove(toolName);
}

// ========== SYMBOL CONTEXT ==========

void BreadcrumbContextManager::registerSymbol(const std::string& filePath, const SymbolContext& symbol) {
    std::string key = filePath + "::" + symbol.name;
    m_symbolRegistry[key] = symbol;
    
    if (!m_fileSymbols[filePath].contains(symbol, 
        [&symbol](const SymbolContext& s) { return s.name == symbol.name; })) {
        m_fileSymbols[filePath].append(symbol);
    }


    // Add breadcrumb
    Breadcrumb crumb;
    crumb.id = key;
    crumb.label = symbol.name;
    crumb.displayName = std::string("%1 (%2)"));
    crumb.type = ContextType::Symbol;
    crumb.metadata["kind"] = static_cast<int>(symbol.kind);
    crumb.metadata["file"] = filePath;
    m_breadcrumbs.push(crumb);
    
    symbolRegistered(symbol.name);
    contextChanged(crumb.id);
}

SymbolContext BreadcrumbContextManager::getSymbol(const std::string& symbolName) const {
    for (const auto& sym : m_symbolRegistry) {
        if (sym.name == symbolName) {
            return sym;
        }
    }
    return SymbolContext();
}

std::vector<SymbolContext> BreadcrumbContextManager::getSymbolsInFile(const std::string& filePath) const {
    return m_fileSymbols.value(filePath, std::vector<SymbolContext>());
}

std::vector<SymbolContext> BreadcrumbContextManager::findSymbolsByKind(SymbolKind kind) const {
    std::vector<SymbolContext> result;
    for (const auto& sym : m_symbolRegistry) {
        if (sym.kind == kind) {
            result.append(sym);
        }
    }
    return result;
}

void BreadcrumbContextManager::updateSymbolUsage(const std::string& symbolName) {
    if (m_symbolRegistry.contains(symbolName)) {
    }
}

// ========== FILE CONTEXT ==========

void BreadcrumbContextManager::registerFile(const std::string& filePath) {
    // Info fileInfo(filePath);
    
    FileContext ctx;
    ctx.absolutePath = fileInfo.string();
    ctx.relativePath = filePath;
    ctx.fileName = fileInfo.fileName();
    ctx.fileExtension = fileInfo.suffix();
    ctx.fileSize = fileInfo.size();
    ctx.lastModified = fileInfo.lastModified();
    ctx.projectRoot = m_workspacePath;
    
    m_fileRegistry[filePath] = ctx;
    
    // Add breadcrumb
    Breadcrumb crumb;
    crumb.id = "file_" + filePath;
    crumb.label = fileInfo.fileName();
    crumb.displayName = filePath;
    crumb.type = ContextType::File;
    crumb.metadata["size"] = std::string::number(ctx.fileSize);
    crumb.metadata["extension"] = ctx.fileExtension;
    m_breadcrumbs.push(crumb);
    
    fileRegistered(filePath);
    contextChanged(crumb.id);
    
    // Scan for symbols in this file if applicable
    if (fileInfo.suffix() == "h" || fileInfo.suffix() == "hpp" || 
        fileInfo.suffix() == "cpp" || fileInfo.suffix() == "cc") {
        scanSymbols(filePath);
    }
}

FileContext BreadcrumbContextManager::getFileContext(const std::string& filePath) const {
    if (m_fileRegistry.contains(filePath)) {
        return m_fileRegistry[filePath];
    }
    return FileContext();
}

std::vector<FileContext> BreadcrumbContextManager::getRelatedFiles(const std::string& filePath) const {
    std::vector<FileContext> result;
    if (m_fileRegistry.contains(filePath)) {
        const auto& file = m_fileRegistry[filePath];
        for (const auto& relFile : file.relatedFiles) {
            if (m_fileRegistry.contains(relFile)) {
                result.append(m_fileRegistry[relFile]);
            }
        }
    }
    return result;
}

void BreadcrumbContextManager::updateFileMetadata(const std::string& filePath) {
    if (m_fileRegistry.contains(filePath)) {
        // Info fileInfo(filePath);
        m_fileRegistry[filePath].lastModified = fileInfo.lastModified();
        m_fileRegistry[filePath].fileSize = fileInfo.size();
    }
}

std::vector<FileContext> BreadcrumbContextManager::searchFiles(const std::string& pattern) const {
    std::vector<FileContext> result;
    // dir(m_workspacePath);
    
    std::stringList filters;
    filters << pattern;
    std::vector<std::string> files = dir// Dir listing;
    
    for (const auto& fileInfo : files) {
        if (m_fileRegistry.contains(fileInfo.filePath())) {
            result.append(m_fileRegistry[fileInfo.filePath()]);
        }
    }
    
    return result;
}

// ========== SOURCE CONTROL CONTEXT ==========

void BreadcrumbContextManager::updateSourceControlContext(const std::string& repository) {
    m_scContext.repository = repository;
    scanRepositoryStatus();
    sourceControlUpdated();
}

SourceControlContext BreadcrumbContextManager::getSourceControlContext() const {
    return m_scContext;
}

std::vector<std::string> BreadcrumbContextManager::getChangedFiles() const {
    return m_scContext.changedFiles;
}

std::string BreadcrumbContextManager::getLatestCommitInfo() const {
    return m_scContext.commitMessage;
}

void BreadcrumbContextManager::scanRepositoryStatus() {
    // Implementation would query git/version control system
}

// ========== SCREENSHOT CONTEXT ==========

void BreadcrumbContextManager::captureScreenshot(const std::string& filePath) {
}

void BreadcrumbContextManager::addScreenshotAnnotation(const ScreenshotAnnotation& annotation) {
    m_screenshots[annotation.id].append(annotation);
}

std::vector<ScreenshotAnnotation> BreadcrumbContextManager::getScreenshotAnnotations(const std::string& screenshotId) const {
    return m_screenshots.value(screenshotId, std::vector<ScreenshotAnnotation>());
}

// ========== INSTRUCTION CONTEXT ==========

void BreadcrumbContextManager::registerInstruction(const InstructionBlock& instruction) {
    m_instructions[instruction.id] = instruction;
    
    // Add breadcrumb
    Breadcrumb crumb;
    crumb.id = "instr_" + instruction.id;
    crumb.label = instruction.title;
    crumb.displayName = instruction.title;
    crumb.type = ContextType::Instruction;
    crumb.metadata["file"] = instruction.relatedFile;
    crumb.metadata["line"] = instruction.lineNumber;
    m_breadcrumbs.push(crumb);
    
    contextChanged(crumb.id);
}

InstructionBlock BreadcrumbContextManager::getInstruction(const std::string& instructionId) const {
    return m_instructions.value(instructionId, InstructionBlock());
}

std::vector<InstructionBlock> BreadcrumbContextManager::getInstructionsForFile(const std::string& filePath) const {
    std::vector<InstructionBlock> result;
    for (const auto& instr : m_instructions) {
        if (instr.relatedFile == filePath) {
            result.append(instr);
        }
    }
    return result;
}

std::vector<InstructionBlock> BreadcrumbContextManager::getAllInstructions() const {
    return m_instructions.values();
}

void BreadcrumbContextManager::toggleInstructionVisibility(const std::string& instructionId) {
    if (m_instructions.contains(instructionId)) {
        m_instructions[instructionId].isVisible = !m_instructions[instructionId].isVisible;
    }
}

// ========== RELATIONSHIP CONTEXT ==========

void BreadcrumbContextManager::registerRelationship(const RelationshipContext& relationship) {
    std::string key = relationship.sourceId + "->" + relationship.targetId;
    m_relationships[key] = relationship;
}

std::vector<RelationshipContext> BreadcrumbContextManager::getRelationshipsFor(const std::string& entityId) const {
    std::vector<RelationshipContext> result;
    for (const auto& rel : m_relationships) {
        if (rel.sourceId == entityId || rel.targetId == entityId) {
            result.append(rel);
        }
    }
    return result;
}

std::vector<std::string> BreadcrumbContextManager::getDependencies(const std::string& entityId) const {
    std::vector<std::string> result;
    for (const auto& rel : m_relationships) {
        if (rel.sourceId == entityId && rel.relationshipType == "depends_on") {
            result.append(rel.targetId);
        }
    }
    return result;
}

std::vector<std::string> BreadcrumbContextManager::getDependents(const std::string& entityId) const {
    std::vector<std::string> result;
    for (const auto& rel : m_relationships) {
        if (rel.targetId == entityId && rel.relationshipType == "depends_on") {
            result.append(rel.sourceId);
        }
    }
    return result;
}

// ========== OPEN EDITOR CONTEXT ==========

void BreadcrumbContextManager::registerOpenEditor(const std::string& filePath, const OpenEditorContext& editorCtx) {
    m_openEditors[filePath] = editorCtx;
    
    // Add breadcrumb
    Breadcrumb crumb;
    crumb.id = "editor_" + filePath;
    crumb.label = // FileInfo: filePath).fileName();
    crumb.displayName = filePath;
    crumb.type = ContextType::OpenEditor;
    crumb.metadata["line"] = editorCtx.cursorLine;
    crumb.metadata["column"] = editorCtx.cursorColumn;
    m_breadcrumbs.push(crumb);
    
    contextChanged(crumb.id);
}

void BreadcrumbContextManager::updateEditorState(const std::string& filePath, int line, int column, const std::string& selectedText) {
    if (m_openEditors.contains(filePath)) {
        m_openEditors[filePath].cursorLine = line;
        m_openEditors[filePath].cursorColumn = column;
        m_openEditors[filePath].selectedText = selectedText;
        m_openEditors[filePath].lastAccessed = // DateTime::currentDateTime();
        contextChanged("editor_" + filePath);
    }
}

std::vector<OpenEditorContext> BreadcrumbContextManager::getOpenEditors() const {
    return m_openEditors.values();
}

OpenEditorContext BreadcrumbContextManager::getEditorContext(const std::string& filePath) const {
    return m_openEditors.value(filePath, OpenEditorContext());
}

void BreadcrumbContextManager::closeEditor(const std::string& filePath) {
    m_openEditors.remove(filePath);
}

// ========== BREADCRUMB NAVIGATION ==========

BreadcrumbChain& BreadcrumbContextManager::getBreadcrumbChain() {
    return m_breadcrumbs;
}

void BreadcrumbContextManager::pushContextBreadcrumb(const ContextType& type, const std::string& identifier) {
    Breadcrumb crumb;
    crumb.id = identifier;
    crumb.label = identifier;
    crumb.displayName = identifier;
    crumb.type = type;
    m_breadcrumbs.push(crumb);
    
    breadcrumbNavigated(m_breadcrumbs.getCurrentIndex());
}

void BreadcrumbContextManager::navigateToBreadcrumb(int index) {
    m_breadcrumbs.jump(index);
    breadcrumbNavigated(index);
}

// ========== QUERYING & ANALYSIS ==========

void* BreadcrumbContextManager::getCompleteContext(const std::string& identifier) const {
    void* obj;
    obj["identifier"] = identifier;
    obj["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    // Would populate with comprehensive context data
    return obj;
}

void* BreadcrumbContextManager::analyzeContextRelationships() const {
    void* obj;
    obj["totalRelationships"] = static_cast<int>(m_relationships.size());
    obj["totalSymbols"] = static_cast<int>(m_symbolRegistry.size());
    obj["totalFiles"] = static_cast<int>(m_fileRegistry.size());
    return obj;
}

std::vector<std::string> BreadcrumbContextManager::getContextPath(const std::string& target) const {
    std::vector<std::string> path;
    // Would implement pathfinding through relationships
    return path;
}

void* BreadcrumbContextManager::generateContextReport() const {
    void* report;
    report["tools"] = static_cast<int>(m_toolRegistry.size());
    report["symbols"] = static_cast<int>(m_symbolRegistry.size());
    report["files"] = static_cast<int>(m_fileRegistry.size());
    report["relationships"] = static_cast<int>(m_relationships.size());
    report["instructions"] = static_cast<int>(m_instructions.size());
    report["openEditors"] = static_cast<int>(m_openEditors.size());
    return report;
}

// ========== IMPORT/EXPORT ==========

void BreadcrumbContextManager::exportContextToJSON(const std::string& filePath) const {
    void* root;
    root["version"] = "1.0";
    root["exported"] = // DateTime::currentDateTime().toString(ISODate);
    
    void* breadcrumbs;
    breadcrumbs["chain"] = m_breadcrumbs.toJSON();
    breadcrumbs["currentIndex"] = m_breadcrumbs.getCurrentIndex();
    root["breadcrumbs"] = breadcrumbs;
    
    void* stats;
    stats["tools"] = static_cast<int>(m_toolRegistry.size());
    stats["symbols"] = static_cast<int>(m_symbolRegistry.size());
    stats["files"] = static_cast<int>(m_fileRegistry.size());
    root["statistics"] = stats;
    
    void* doc(root);
}

void BreadcrumbContextManager::importContextFromJSON(const std::string& filePath) {
}

// ========== PERFORMANCE ==========

void BreadcrumbContextManager::indexWorkspace() {
    indexingProgressChanged(0.0);
    
    // dir(m_workspacePath);
    std::vector<std::string> files = dir// Dir listing;
    
    int count = 0;
    for (const auto& fileInfo : files) {
        if (fileInfo.suffix() == "h" || fileInfo.suffix() == "hpp" || 
            fileInfo.suffix() == "cpp" || fileInfo.suffix() == "cc") {
            registerFile(fileInfo.filePath());
            count++;
        }
        
        double progress = (static_cast<double>(count) / files.size()) * 100.0;
        indexingProgressChanged(progress);
    }
    
    indexingProgressChanged(100.0);
}

void BreadcrumbContextManager::rebuildIndices() {
    m_symbolRegistry.clear();
    m_fileSymbols.clear();
    indexWorkspace();
}

double BreadcrumbContextManager::getIndexingProgress() const {
    return 100.0; // Would track actual progress
}

// ========== HELPER METHODS ==========

void BreadcrumbContextManager::scanSymbols(const std::string& filePath) {
    // Implementation would parse source file for symbols
}

void BreadcrumbContextManager::analyzeFileRelationships() {
}

void BreadcrumbContextManager::cacheSymbolReferences() {
}

} // namespace Context
} // namespace RawrXD

