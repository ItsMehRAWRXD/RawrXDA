#include "MASMCompilerWidget.h"
#include <sstream>
#include <algorithm>
#include <regex>

// ============================================================================
// Helper functions for string list serialization
// ============================================================================
static std::string joinList(const stringList& list) {
    std::stringstream ss;
    for(size_t i=0; i<list.size(); ++i) {
        if(i>0) ss << ";";
        ss << list[i];
    }
    return ss.str();
}

static stringList splitList(const std::string& s) {
    stringList list;
    if(s.empty()) return list;
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, ';')) {
        list.push_back(item);
    }
    return list;
}

// ============================================================================
// MASMProjectSettings Implementation
// ============================================================================

void MASMProjectSettings::save(std::map<std::string, std::string>& settings) const {
    settings["MASMProject.projectName"] = projectName;
    settings["MASMProject.projectPath"] = projectPath;
    settings["MASMProject.outputPath"] = outputPath;
    settings["MASMProject.mainFile"] = mainFile;
    settings["MASMProject.sourceFiles"] = joinList(sourceFiles);
    settings["MASMProject.includePaths"] = joinList(includePaths);
    settings["MASMProject.libraries"] = joinList(libraries);
    settings["MASMProject.targetArchitecture"] = targetArchitecture;
    settings["MASMProject.outputFormat"] = outputFormat;
    settings["MASMProject.optimizationLevel"] = std::to_string(optimizationLevel);
    settings["MASMProject.generateDebugInfo"] = generateDebugInfo ? "true" : "false";
    settings["MASMProject.warnings"] = warnings ? "true" : "false";
    settings["MASMProject.defines"] = joinList(defines);
}

void MASMProjectSettings::load(std::map<std::string, std::string>& settings) {
    if(settings.count("MASMProject.projectName")) projectName = settings["MASMProject.projectName"];
    if(settings.count("MASMProject.projectPath")) projectPath = settings["MASMProject.projectPath"];
    if(settings.count("MASMProject.outputPath")) outputPath = settings["MASMProject.outputPath"];
    if(settings.count("MASMProject.mainFile")) mainFile = settings["MASMProject.mainFile"];
    if(settings.count("MASMProject.sourceFiles")) sourceFiles = splitList(settings["MASMProject.sourceFiles"]);
    if(settings.count("MASMProject.includePaths")) includePaths = splitList(settings["MASMProject.includePaths"]);
    if(settings.count("MASMProject.libraries")) libraries = splitList(settings["MASMProject.libraries"]);
    if(settings.count("MASMProject.targetArchitecture")) targetArchitecture = settings["MASMProject.targetArchitecture"];
    
    if(settings.count("MASMProject.outputFormat")) outputFormat = settings["MASMProject.outputFormat"];
    
    if(settings.count("MASMProject.optimizationLevel")) optimizationLevel = std::stoi(settings["MASMProject.optimizationLevel"]);
    if(settings.count("MASMProject.generateDebugInfo")) generateDebugInfo = (settings["MASMProject.generateDebugInfo"] == "true");
    if(settings.count("MASMProject.warnings")) warnings = (settings["MASMProject.warnings"] == "true");
    if(settings.count("MASMProject.defines")) defines = splitList(settings["MASMProject.defines"]);
}

// ============================================================================
// MASMCodeEditor Stub Implementation
// ============================================================================

class MASMSyntaxHighlighter {
public:
    MASMSyntaxHighlighter(void* doc) {}
};

class MASMCodeEditor::LineNumberArea {
public:
    LineNumberArea(MASMCodeEditor* editor) {}
};

MASMCodeEditor::MASMCodeEditor(void* parent) {
    m_lineNumberArea = std::make_unique<LineNumberArea>(this);
    m_highlighter = std::make_unique<MASMSyntaxHighlighter>(nullptr);
}

MASMCodeEditor::~MASMCodeEditor() = default;

void MASMCodeEditor::lineNumberAreaPaintEvent(void* event) {}
int MASMCodeEditor::lineNumberAreaWidth() { return 0; }
void MASMCodeEditor::setErrors(const std::vector<MASMError>& errors) { m_errors = errors; }
void MASMCodeEditor::clearErrors() { m_errors.clear(); }
void MASMCodeEditor::toggleBreakpoint(int line) {
    if(m_breakpoints.count(line)) m_breakpoints.erase(line);
    else m_breakpoints.insert(line);
}
void MASMCodeEditor::clearBreakpoints() { m_breakpoints.clear(); }
void MASMCodeEditor::foldBlock(int line) { m_foldedBlocks.insert(line); }
void MASMCodeEditor::unfoldBlock(int line) { m_foldedBlocks.erase(line); }
void MASMCodeEditor::setCompletionModel(const stringList& completions) {}
void MASMCodeEditor::resizeEvent(void* event) {}
void MASMCodeEditor::keyPressEvent(void* event) {}
void MASMCodeEditor::mousePressEvent(void* event) {}
void MASMCodeEditor::updateLineNumberAreaWidth(int newBlockCount) {}
void MASMCodeEditor::highlightCurrentLine() {}
void MASMCodeEditor::updateLineNumberArea(const struct { int x; int y; int w; int h; }& rect, int dy) {}
void MASMCodeEditor::paintLineNumberArea(void* event) {}
void MASMCodeEditor::paintErrorMarkers(void* event) {}
void MASMCodeEditor::paintBreakpoints(void* event) {}
void MASMCodeEditor::breakpointToggled(int line, bool enabled) {}
void MASMCodeEditor::errorClicked(const MASMError& error) {}

// ============================================================================
// Stubs for MASMCompilerWidget class methods
// ============================================================================

MASMCompilerWidget::MASMCompilerWidget(void* parent) : m_isCompiling(false), m_isRunning(false), m_isDebugging(false) {
    m_projectExplorer = std::make_unique<MASMProjectExplorer>(this);
    m_editor = std::make_unique<MASMCodeEditor>(this);
    m_symbolBrowser = std::make_unique<MASMSymbolBrowser>(this);
    m_debugger = std::make_unique<MASMDebugger>(this);
    m_buildOutput = std::make_unique<MASMBuildOutput>(this);
}

MASMCompilerWidget::~MASMCompilerWidget() {}

void MASMCompilerWidget::build() {}
void MASMCompilerWidget::rebuild() {}
void MASMCompilerWidget::clean() {}
void MASMCompilerWidget::run() {}
void MASMCompilerWidget::debug() {}
void MASMCompilerWidget::stop() {}
void MASMCompilerWidget::compileFile(const std::string& sourceFile, const std::string& outputFile) {}
std::string MASMCompilerWidget::getCompilerExecutable() const { return "masm.exe"; }
stringList MASMCompilerWidget::getCompilerArguments(const std::string& sourceFile, const std::string& outputFile) const { return {}; }
void MASMCompilerWidget::onCompilerFinished(int exitCode, int exitStatus) {} 
void MASMCompilerWidget::onCompilerOutput() {}
void MASMCompilerWidget::onCompilerError() {}
void MASMCompilerWidget::parseCompilerOutput(const std::string& output) {}
void MASMCompilerWidget::extractErrors(const std::string& output) {}
void MASMCompilerWidget::updateUIAfterCompilation(bool success) {}
void MASMCompilerWidget::onExecutableFinished(int exitCode, int exitStatus) {}
void MASMCompilerWidget::onExecutableOutput() {}
void MASMCompilerWidget::onExecutableError() {}
void MASMCompilerWidget::onErrorClicked(const MASMError& error) {}
void MASMCompilerWidget::onSymbolSelected(const MASMSymbol& symbol) {}
void MASMCompilerWidget::onBreakpointToggled(int line, bool enabled) {}
void MASMCompilerWidget::saveFile() {}
void MASMCompilerWidget::openFile(const std::string& filePath) {}
void MASMCompilerWidget::setupUI() {}
void MASMCompilerWidget::setupToolbar() {}
void MASMCompilerWidget::connectSignals() {}
void MASMCompilerWidget::compilationStarted() {}
void MASMCompilerWidget::compilationFinished(bool success) {}
void MASMCompilerWidget::executionStarted() {}
void MASMCompilerWidget::executionFinished(int exitCode) {}

// ============================================================================
// Stubs for Helper Classes
// ============================================================================

MASMProjectExplorer::MASMProjectExplorer(void* parent) {}
void MASMProjectExplorer::setProject(const MASMProjectSettings& project) {}
void MASMProjectExplorer::refresh() {}
void MASMProjectExplorer::populateTree() {}
void MASMProjectExplorer::onTreeItemDoubleClicked(void* item, int column) {}
void MASMProjectExplorer::onTreeContextMenu(const struct { int x; int y; }& pos) {}
void MASMProjectExplorer::addContextMenuActions(void* menu, void* item) {}
void MASMProjectExplorer::fileOpened(const std::string& filePath) {}

MASMSymbolBrowser::MASMSymbolBrowser(void* parent) {}
void MASMSymbolBrowser::setSymbols(const std::vector<MASMSymbol>& symbols) {}
void MASMSymbolBrowser::clear() {}
void MASMSymbolBrowser::filter(const std::string& text) {}
void MASMSymbolBrowser::populateTree() {}
void MASMSymbolBrowser::onSymbolClicked(void* item, int column) {}
void MASMSymbolBrowser::onFilterChanged(const std::string& text) {}

MASMDebugger::MASMDebugger(void* parent) {}
void MASMDebugger::startDebugging(const std::string& executablePath) {}
void MASMDebugger::stopDebugging() {}
void MASMDebugger::stepOver() {}
void MASMDebugger::stepInto() {}
void MASMDebugger::stepOut() {}
void MASMDebugger::continueExecution() {}
void MASMDebugger::pause() {}
void MASMDebugger::setBreakpoints(const std::unordered_set<int>& breakpoints) {}
void MASMDebugger::onDebuggerOutput() {}
void MASMDebugger::onDebuggerError() {}
void MASMDebugger::updateRegisters() {}
void MASMDebugger::updateStack() {}
void MASMDebugger::updateDisassembly() {}
void MASMDebugger::debuggerStarted() {}
void MASMDebugger::debuggerStopped() {}

MASMBuildOutput::MASMBuildOutput(void* parent) {}
void MASMBuildOutput::clear() {}
void MASMBuildOutput::appendMessage(const std::string& message) {}
void MASMBuildOutput::appendError(const MASMError& error) {}
void MASMBuildOutput::appendStage(const std::string& stage) {}
void MASMBuildOutput::setStats(const MASMCompilationStats& stats) {}
void MASMBuildOutput::formatErrorMessage(const MASMError& error, std::string& output) {}
void MASMBuildOutput::onOutputDoubleClicked() {}
void MASMBuildOutput::errorDoubleClicked(const MASMError& error) {}
