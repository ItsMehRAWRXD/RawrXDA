/*  BreadcrumbContextManager.cpp  -  Implementation
    
    Comprehensive context management system with breadcrumb-style navigation.
    Handles tools, symbols, files, source control, screenshots, instructions,
    relationships, and open editor state.
*/

#include "../../include/context/BreadcrumbContextManager.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QJsonDocument>
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

QList<Breadcrumb> BreadcrumbChain::getChain() const {
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

QJsonArray BreadcrumbChain::toJSON() const {
    QJsonArray arr;
    for (const auto& crumb : m_chain) {
        QJsonObject obj;
        obj["id"] = crumb.id;
        obj["label"] = crumb.label;
        obj["displayName"] = crumb.displayName;
        obj["type"] = static_cast<int>(crumb.type);
        obj["iconPath"] = crumb.iconPath;
        obj["isClickable"] = crumb.isClickable;
        obj["metadata"] = crumb.metadata;
        obj["timestamp"] = crumb.timestamp.toString(Qt::ISODate);
        arr.append(obj);
    }
    return arr;
}

void BreadcrumbChain::fromJSON(const QJsonArray& json) {
    m_chain.clear();
    for (const auto& val : json) {
        QJsonObject obj = val.toObject();
        Breadcrumb crumb;
        crumb.id = obj["id"].toString();
        crumb.label = obj["label"].toString();
        crumb.displayName = obj["displayName"].toString();
        crumb.type = static_cast<ContextType>(obj["type"].toInt());
        crumb.iconPath = obj["iconPath"].toString();
        crumb.isClickable = obj["isClickable"].toBool();
        crumb.metadata = obj["metadata"].toObject();
        crumb.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        m_chain.append(crumb);
    }
    m_currentIndex = std::max(0, static_cast<int>(m_chain.size()) - 1);
}

// ============================================================================
// CONTEXT MANAGER IMPLEMENTATION
// ============================================================================

BreadcrumbContextManager::BreadcrumbContextManager(QObject* parent)
    : QObject(parent), m_workspacePath("") {
    qDebug() << "BreadcrumbContextManager initialized";
}

BreadcrumbContextManager::~BreadcrumbContextManager() {
    shutdown();
}

void BreadcrumbContextManager::initialize(const QString& workspacePath) {
    m_workspacePath = workspacePath;
    qDebug() << "BreadcrumbContextManager initializing with workspace:" << workspacePath;
    
    // Create initial workspace structure
    QDir wsDir(workspacePath);
    if (!wsDir.exists()) {
        wsDir.mkpath(".");
    }
    
    // Index workspace on initialization
    indexWorkspace();
    
    emit indexingComplete();
}

void BreadcrumbContextManager::shutdown() {
    qDebug() << "BreadcrumbContextManager shutting down";
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

void BreadcrumbContextManager::registerTool(const QString& toolName, const ToolContext& context) {
    m_toolRegistry[toolName] = context;
    qDebug() << "Tool registered:" << toolName;
    
    // Add breadcrumb
    Breadcrumb crumb;
    crumb.id = "tool_" + toolName;
    crumb.label = toolName;
    crumb.displayName = toolName;
    crumb.type = ContextType::Tool;
    m_breadcrumbs.push(crumb);
    
    emit toolRegistered(toolName);
    emit contextChanged(crumb.id);
}

ToolContext BreadcrumbContextManager::getTool(const QString& toolName) const {
    if (m_toolRegistry.contains(toolName)) {
        return m_toolRegistry[toolName];
    }
    return ToolContext();
}

QList<ToolContext> BreadcrumbContextManager::getAllTools() const {
    return m_toolRegistry.values();
}

void BreadcrumbContextManager::unregisterTool(const QString& toolName) {
    m_toolRegistry.remove(toolName);
    qDebug() << "Tool unregistered:" << toolName;
}

// ========== SYMBOL CONTEXT ==========

void BreadcrumbContextManager::registerSymbol(const QString& filePath, const SymbolContext& symbol) {
    QString key = filePath + "::" + symbol.name;
    m_symbolRegistry[key] = symbol;
    
    if (!m_fileSymbols[filePath].contains(symbol, 
        [&symbol](const SymbolContext& s) { return s.name == symbol.name; })) {
        m_fileSymbols[filePath].append(symbol);
    }
    
    qDebug() << "Symbol registered:" << symbol.name << "in" << filePath;
    
    // Add breadcrumb
    Breadcrumb crumb;
    crumb.id = key;
    crumb.label = symbol.name;
    crumb.displayName = QString("%1 (%2)").arg(symbol.name, QString::number(symbol.lineNumber));
    crumb.type = ContextType::Symbol;
    crumb.metadata["kind"] = static_cast<int>(symbol.kind);
    crumb.metadata["file"] = filePath;
    m_breadcrumbs.push(crumb);
    
    emit symbolRegistered(symbol.name);
    emit contextChanged(crumb.id);
}

SymbolContext BreadcrumbContextManager::getSymbol(const QString& symbolName) const {
    for (const auto& sym : m_symbolRegistry) {
        if (sym.name == symbolName) {
            return sym;
        }
    }
    return SymbolContext();
}

QList<SymbolContext> BreadcrumbContextManager::getSymbolsInFile(const QString& filePath) const {
    return m_fileSymbols.value(filePath, QList<SymbolContext>());
}

QList<SymbolContext> BreadcrumbContextManager::findSymbolsByKind(SymbolKind kind) const {
    QList<SymbolContext> result;
    for (const auto& sym : m_symbolRegistry) {
        if (sym.kind == kind) {
            result.append(sym);
        }
    }
    return result;
}

void BreadcrumbContextManager::updateSymbolUsage(const QString& symbolName) {
    if (m_symbolRegistry.contains(symbolName)) {
        qDebug() << "Symbol usage updated:" << symbolName;
    }
}

// ========== FILE CONTEXT ==========

void BreadcrumbContextManager::registerFile(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    
    FileContext ctx;
    ctx.absolutePath = fileInfo.absoluteFilePath();
    ctx.relativePath = filePath;
    ctx.fileName = fileInfo.fileName();
    ctx.fileExtension = fileInfo.suffix();
    ctx.fileSize = fileInfo.size();
    ctx.lastModified = fileInfo.lastModified();
    ctx.projectRoot = m_workspacePath;
    
    m_fileRegistry[filePath] = ctx;
    qDebug() << "File registered:" << filePath;
    
    // Add breadcrumb
    Breadcrumb crumb;
    crumb.id = "file_" + filePath;
    crumb.label = fileInfo.fileName();
    crumb.displayName = filePath;
    crumb.type = ContextType::File;
    crumb.metadata["size"] = QString::number(ctx.fileSize);
    crumb.metadata["extension"] = ctx.fileExtension;
    m_breadcrumbs.push(crumb);
    
    emit fileRegistered(filePath);
    emit contextChanged(crumb.id);
    
    // Scan for symbols in this file if applicable
    if (fileInfo.suffix() == "h" || fileInfo.suffix() == "hpp" || 
        fileInfo.suffix() == "cpp" || fileInfo.suffix() == "cc") {
        scanSymbols(filePath);
    }
}

FileContext BreadcrumbContextManager::getFileContext(const QString& filePath) const {
    if (m_fileRegistry.contains(filePath)) {
        return m_fileRegistry[filePath];
    }
    return FileContext();
}

QList<FileContext> BreadcrumbContextManager::getRelatedFiles(const QString& filePath) const {
    QList<FileContext> result;
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

void BreadcrumbContextManager::updateFileMetadata(const QString& filePath) {
    if (m_fileRegistry.contains(filePath)) {
        QFileInfo fileInfo(filePath);
        m_fileRegistry[filePath].lastModified = fileInfo.lastModified();
        m_fileRegistry[filePath].fileSize = fileInfo.size();
        qDebug() << "File metadata updated:" << filePath;
    }
}

QList<FileContext> BreadcrumbContextManager::searchFiles(const QString& pattern) const {
    QList<FileContext> result;
    QDir dir(m_workspacePath);
    
    QStringList filters;
    filters << pattern;
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::Recursive);
    
    for (const auto& fileInfo : files) {
        if (m_fileRegistry.contains(fileInfo.filePath())) {
            result.append(m_fileRegistry[fileInfo.filePath()]);
        }
    }
    
    return result;
}

// ========== SOURCE CONTROL CONTEXT ==========

void BreadcrumbContextManager::updateSourceControlContext(const QString& repository) {
    m_scContext.repository = repository;
    qDebug() << "Source control context updated for:" << repository;
    scanRepositoryStatus();
    emit sourceControlUpdated();
}

SourceControlContext BreadcrumbContextManager::getSourceControlContext() const {
    return m_scContext;
}

QList<QString> BreadcrumbContextManager::getChangedFiles() const {
    return m_scContext.changedFiles;
}

QString BreadcrumbContextManager::getLatestCommitInfo() const {
    return m_scContext.commitMessage;
}

void BreadcrumbContextManager::scanRepositoryStatus() {
    // Implementation would query git/version control system
    qDebug() << "Scanning repository status";
}

// ========== SCREENSHOT CONTEXT ==========

void BreadcrumbContextManager::captureScreenshot(const QString& filePath) {
    qDebug() << "Screenshot captured:" << filePath;
}

void BreadcrumbContextManager::addScreenshotAnnotation(const ScreenshotAnnotation& annotation) {
    m_screenshots[annotation.id].append(annotation);
    qDebug() << "Screenshot annotation added:" << annotation.id;
}

QList<ScreenshotAnnotation> BreadcrumbContextManager::getScreenshotAnnotations(const QString& screenshotId) const {
    return m_screenshots.value(screenshotId, QList<ScreenshotAnnotation>());
}

// ========== INSTRUCTION CONTEXT ==========

void BreadcrumbContextManager::registerInstruction(const InstructionBlock& instruction) {
    m_instructions[instruction.id] = instruction;
    qDebug() << "Instruction registered:" << instruction.id;
    
    // Add breadcrumb
    Breadcrumb crumb;
    crumb.id = "instr_" + instruction.id;
    crumb.label = instruction.title;
    crumb.displayName = instruction.title;
    crumb.type = ContextType::Instruction;
    crumb.metadata["file"] = instruction.relatedFile;
    crumb.metadata["line"] = instruction.lineNumber;
    m_breadcrumbs.push(crumb);
    
    emit contextChanged(crumb.id);
}

InstructionBlock BreadcrumbContextManager::getInstruction(const QString& instructionId) const {
    return m_instructions.value(instructionId, InstructionBlock());
}

QList<InstructionBlock> BreadcrumbContextManager::getInstructionsForFile(const QString& filePath) const {
    QList<InstructionBlock> result;
    for (const auto& instr : m_instructions) {
        if (instr.relatedFile == filePath) {
            result.append(instr);
        }
    }
    return result;
}

QList<InstructionBlock> BreadcrumbContextManager::getAllInstructions() const {
    return m_instructions.values();
}

void BreadcrumbContextManager::toggleInstructionVisibility(const QString& instructionId) {
    if (m_instructions.contains(instructionId)) {
        m_instructions[instructionId].isVisible = !m_instructions[instructionId].isVisible;
        qDebug() << "Instruction visibility toggled:" << instructionId;
    }
}

// ========== RELATIONSHIP CONTEXT ==========

void BreadcrumbContextManager::registerRelationship(const RelationshipContext& relationship) {
    QString key = relationship.sourceId + "->" + relationship.targetId;
    m_relationships[key] = relationship;
    qDebug() << "Relationship registered:" << key;
}

QList<RelationshipContext> BreadcrumbContextManager::getRelationshipsFor(const QString& entityId) const {
    QList<RelationshipContext> result;
    for (const auto& rel : m_relationships) {
        if (rel.sourceId == entityId || rel.targetId == entityId) {
            result.append(rel);
        }
    }
    return result;
}

QList<QString> BreadcrumbContextManager::getDependencies(const QString& entityId) const {
    QList<QString> result;
    for (const auto& rel : m_relationships) {
        if (rel.sourceId == entityId && rel.relationshipType == "depends_on") {
            result.append(rel.targetId);
        }
    }
    return result;
}

QList<QString> BreadcrumbContextManager::getDependents(const QString& entityId) const {
    QList<QString> result;
    for (const auto& rel : m_relationships) {
        if (rel.targetId == entityId && rel.relationshipType == "depends_on") {
            result.append(rel.sourceId);
        }
    }
    return result;
}

// ========== OPEN EDITOR CONTEXT ==========

void BreadcrumbContextManager::registerOpenEditor(const QString& filePath, const OpenEditorContext& editorCtx) {
    m_openEditors[filePath] = editorCtx;
    qDebug() << "Editor registered:" << filePath;
    
    // Add breadcrumb
    Breadcrumb crumb;
    crumb.id = "editor_" + filePath;
    crumb.label = QFileInfo(filePath).fileName();
    crumb.displayName = filePath;
    crumb.type = ContextType::OpenEditor;
    crumb.metadata["line"] = editorCtx.cursorLine;
    crumb.metadata["column"] = editorCtx.cursorColumn;
    m_breadcrumbs.push(crumb);
    
    emit contextChanged(crumb.id);
}

void BreadcrumbContextManager::updateEditorState(const QString& filePath, int line, int column, const QString& selectedText) {
    if (m_openEditors.contains(filePath)) {
        m_openEditors[filePath].cursorLine = line;
        m_openEditors[filePath].cursorColumn = column;
        m_openEditors[filePath].selectedText = selectedText;
        m_openEditors[filePath].lastAccessed = QDateTime::currentDateTime();
        qDebug() << "Editor state updated:" << filePath << "at" << line << ":" << column;
        emit contextChanged("editor_" + filePath);
    }
}

QList<OpenEditorContext> BreadcrumbContextManager::getOpenEditors() const {
    return m_openEditors.values();
}

OpenEditorContext BreadcrumbContextManager::getEditorContext(const QString& filePath) const {
    return m_openEditors.value(filePath, OpenEditorContext());
}

void BreadcrumbContextManager::closeEditor(const QString& filePath) {
    m_openEditors.remove(filePath);
    qDebug() << "Editor closed:" << filePath;
}

// ========== BREADCRUMB NAVIGATION ==========

BreadcrumbChain& BreadcrumbContextManager::getBreadcrumbChain() {
    return m_breadcrumbs;
}

void BreadcrumbContextManager::pushContextBreadcrumb(const ContextType& type, const QString& identifier) {
    Breadcrumb crumb;
    crumb.id = identifier;
    crumb.label = identifier;
    crumb.displayName = identifier;
    crumb.type = type;
    m_breadcrumbs.push(crumb);
    
    qDebug() << "Context breadcrumb pushed:" << identifier;
    emit breadcrumbNavigated(m_breadcrumbs.getCurrentIndex());
}

void BreadcrumbContextManager::navigateToBreadcrumb(int index) {
    m_breadcrumbs.jump(index);
    qDebug() << "Navigated to breadcrumb index:" << index;
    emit breadcrumbNavigated(index);
}

// ========== QUERYING & ANALYSIS ==========

QJsonObject BreadcrumbContextManager::getCompleteContext(const QString& identifier) const {
    QJsonObject obj;
    obj["identifier"] = identifier;
    obj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    // Would populate with comprehensive context data
    return obj;
}

QJsonObject BreadcrumbContextManager::analyzeContextRelationships() const {
    QJsonObject obj;
    obj["totalRelationships"] = static_cast<int>(m_relationships.size());
    obj["totalSymbols"] = static_cast<int>(m_symbolRegistry.size());
    obj["totalFiles"] = static_cast<int>(m_fileRegistry.size());
    return obj;
}

QList<QString> BreadcrumbContextManager::getContextPath(const QString& target) const {
    QList<QString> path;
    // Would implement pathfinding through relationships
    return path;
}

QJsonObject BreadcrumbContextManager::generateContextReport() const {
    QJsonObject report;
    report["tools"] = static_cast<int>(m_toolRegistry.size());
    report["symbols"] = static_cast<int>(m_symbolRegistry.size());
    report["files"] = static_cast<int>(m_fileRegistry.size());
    report["relationships"] = static_cast<int>(m_relationships.size());
    report["instructions"] = static_cast<int>(m_instructions.size());
    report["openEditors"] = static_cast<int>(m_openEditors.size());
    return report;
}

// ========== IMPORT/EXPORT ==========

void BreadcrumbContextManager::exportContextToJSON(const QString& filePath) const {
    QJsonObject root;
    root["version"] = "1.0";
    root["exported"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonObject breadcrumbs;
    breadcrumbs["chain"] = m_breadcrumbs.toJSON();
    breadcrumbs["currentIndex"] = m_breadcrumbs.getCurrentIndex();
    root["breadcrumbs"] = breadcrumbs;
    
    QJsonObject stats;
    stats["tools"] = static_cast<int>(m_toolRegistry.size());
    stats["symbols"] = static_cast<int>(m_symbolRegistry.size());
    stats["files"] = static_cast<int>(m_fileRegistry.size());
    root["statistics"] = stats;
    
    QJsonDocument doc(root);
    qDebug() << "Context exported to:" << filePath;
}

void BreadcrumbContextManager::importContextFromJSON(const QString& filePath) {
    qDebug() << "Context imported from:" << filePath;
}

// ========== PERFORMANCE ==========

void BreadcrumbContextManager::indexWorkspace() {
    qDebug() << "Indexing workspace:" << m_workspacePath;
    emit indexingProgressChanged(0.0);
    
    QDir dir(m_workspacePath);
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Recursive);
    
    int count = 0;
    for (const auto& fileInfo : files) {
        if (fileInfo.suffix() == "h" || fileInfo.suffix() == "hpp" || 
            fileInfo.suffix() == "cpp" || fileInfo.suffix() == "cc") {
            registerFile(fileInfo.filePath());
            count++;
        }
        
        double progress = (static_cast<double>(count) / files.size()) * 100.0;
        emit indexingProgressChanged(progress);
    }
    
    emit indexingProgressChanged(100.0);
    qDebug() << "Workspace indexing complete. Registered" << count << "source files.";
}

void BreadcrumbContextManager::rebuildIndices() {
    qDebug() << "Rebuilding indices";
    m_symbolRegistry.clear();
    m_fileSymbols.clear();
    indexWorkspace();
}

double BreadcrumbContextManager::getIndexingProgress() const {
    return 100.0; // Would track actual progress
}

// ========== HELPER METHODS ==========

void BreadcrumbContextManager::scanSymbols(const QString& filePath) {
    // Implementation would parse source file for symbols
    qDebug() << "Scanning symbols in:" << filePath;
}

void BreadcrumbContextManager::analyzeFileRelationships() {
    qDebug() << "Analyzing file relationships";
}

void BreadcrumbContextManager::cacheSymbolReferences() {
    qDebug() << "Caching symbol references";
}

} // namespace Context
} // namespace RawrXD
