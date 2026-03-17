#include "MASMCompilerWidget.h"
#include <sstream>
#include <algorithm>
#include <regex>
#include <windows.h>
#include <iostream>

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
// Real Implementation of MASMCompilerWidget class methods
// ============================================================================

MASMCompilerWidget::MASMCompilerWidget(void* parent) : m_isCompiling(false), m_isRunning(false), m_isDebugging(false) {
    m_projectExplorer = std::make_unique<MASMProjectExplorer>(this);
    m_editor = std::make_unique<MASMCodeEditor>(this);
    m_symbolBrowser = std::make_unique<MASMSymbolBrowser>(this);
    m_debugger = std::make_unique<MASMDebugger>(this);
    m_buildOutput = std::make_unique<MASMBuildOutput>(this);
}

MASMCompilerWidget::~MASMCompilerWidget() {}

static bool ExecuteCmd(const std::string& cmd, std::string& output) {
    HANDLE hPipeRead, hPipeWrite;
    SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    
    if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0)) return false;
    SetHandleInformation(hPipeRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hPipeWrite;
    si.hStdError = hPipeWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    char cmdBuf[1024];
    strncpy(cmdBuf, cmd.c_str(), sizeof(cmdBuf));

    if (!CreateProcessA(NULL, cmdBuf, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hPipeWrite);
        CloseHandle(hPipeRead);
        return false;
    }

    CloseHandle(hPipeWrite); // Close write end, keep read end

    char buffer[128];
    DWORD bytesRead;
    while (ReadFile(hPipeRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hPipeRead);
    
    return exitCode == 0;
}

void MASMCompilerWidget::build() {
    compilationStarted();
    std::string src = m_project.mainFile.empty() ? "main.asm" : m_project.mainFile;
    std::string obj = src.substr(0, src.find_last_of('.')) + ".obj";
    std::string exe = src.substr(0, src.find_last_of('.')) + ".exe";
    
    std::string log;
    
    // 1. Compile
    std::string mlCmd = "ml64.exe /c /Zd /Zi " + src;
    if(m_buildOutput) m_buildOutput->appendMessage("Compiling: " + mlCmd);
    
    if (!ExecuteCmd(mlCmd, log)) {
        if(m_buildOutput) m_buildOutput->appendError(MASMError(src, 0, 0, "error", "ML64 failed:\n" + log));
        compilationFinished(false);
        return;
    }
    if(m_buildOutput) m_buildOutput->appendMessage(log.empty() ? "Compile OK" : log);

    // 2. Link
    std::string linkCmd = "link.exe /SUBSYSTEM:CONSOLE /DEBUG /ENTRY:main /OUT:" + exe + " " + obj + " kernel32.lib user32.lib";
    if(m_buildOutput) m_buildOutput->appendMessage("Linking: " + linkCmd);
    
    log.clear();
    if (!ExecuteCmd(linkCmd, log)) {
        if(m_buildOutput) m_buildOutput->appendError(MASMError(src, 0, 0, "error", "LINK failed:\n" + log));
        compilationFinished(false);
        return;
    }
    if(m_buildOutput) m_buildOutput->appendMessage(log.empty() ? "Link OK" : log);

    compilationFinished(true);
}

void MASMCompilerWidget::rebuild() {
    clean();
    build();
}

void MASMCompilerWidget::clean() {
    std::string base = m_project.mainFile.substr(0, m_project.mainFile.find_last_of('.'));
    DeleteFileA((base + ".obj").c_str());
    DeleteFileA((base + ".exe").c_str());
    DeleteFileA((base + ".pdb").c_str());
    DeleteFileA((base + ".ilk").c_str());
    if(m_buildOutput) m_buildOutput->appendMessage("Cleaned project files.");
}

void MASMCompilerWidget::run() {
    std::string exe = m_project.mainFile.substr(0, m_project.mainFile.find_last_of('.')) + ".exe";
    executionStarted();
    
    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.lpVerb = "open";
    sei.lpFile = exe.c_str();
    sei.nShow = SW_SHOW;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    
    if (ShellExecuteExA(&sei)) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
    } else {
        if(m_buildOutput) m_buildOutput->appendError(MASMError(exe, 0, 0, "error", "Failed to launch executable"));
    }
    
    executionFinished(0);
}

void MASMCompilerWidget::debug() {
    // Basic debugger launch - could be evolved to use Win32 Debug API
    if(m_buildOutput) m_buildOutput->appendMessage("Launching debugger (VS JIT)...");
    std::string exe = m_project.mainFile.substr(0, m_project.mainFile.find_last_of('.')) + ".exe";
    std::string cmd = "vsjitdebugger.exe -p " + std::to_string(GetCurrentProcessId()); // Placeholder logic
    // Real logic would be CreateProcess with DEBUG_PROCESS flag
    // For now, functional run:
    run();
}

void MASMCompilerWidget::stop() {
    // Terminate process logic would go here
    m_isRunning = false;
}

void MASMCompilerWidget::compileFile(const std::string& sourceFile, const std::string& outputFile) {
    // Just a wrapper for build in this simple version
    m_project.mainFile = sourceFile;
    build();
}
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
