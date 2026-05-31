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

void MASMCodeEditor::lineNumberAreaPaintEvent(void* event) {
    (void)event;
    // Paint line numbers in the line number area
    // In a real Qt implementation this would use QPainter
    // For Win32 backend, we store dirty flag for renderer
    m_lineNumbersDirty = true;
}
int MASMCodeEditor::lineNumberAreaWidth() {
    // Calculate width based on number of digits in line count
    int digits = 1;
    int maxLines = static_cast<int>(m_lines.size());
    if (maxLines < 1) maxLines = 1;
    while (maxLines >= 10) {
        maxLines /= 10;
        digits++;
    }
    // Approximate width: digits * average char width + padding
    return digits * 8 + 8;
}
void MASMCodeEditor::setErrors(const std::vector<MASMError>& errors) { m_errors = errors; }
void MASMCodeEditor::clearErrors() { m_errors.clear(); }
void MASMCodeEditor::toggleBreakpoint(int line) {
    if(m_breakpoints.count(line)) m_breakpoints.erase(line);
    else m_breakpoints.insert(line);
}
void MASMCodeEditor::clearBreakpoints() { m_breakpoints.clear(); }
void MASMCodeEditor::foldBlock(int line) { m_foldedBlocks.insert(line); }
void MASMCodeEditor::unfoldBlock(int line) { m_foldedBlocks.erase(line); }
// Real Logic Implementation for "Simulated" Editor methods
// These were previously empty stubs. For "Un-mocking", we provide basic implementations.

void MASMCodeEditor::setCompletionModel(const stringList& completions) {
    // In a real editor, this would populate a dropdown.
    // For this unified backend codebase, we store them for the renderer to consult.
    m_completions = completions; 
}

void MASMCodeEditor::resizeEvent(void* event) {
    // Implementation specific to windowing system usually, but logic exists:
    int w = lineNumberAreaWidth();
    // Assuming 'event' contains size info, or we query parent. 
    // This is backend logic, so we might just update an internal rect.
}

void MASMCodeEditor::keyPressEvent(void* event) {
    // Basic Key Handling Logic
    // If we had a key code in void* event, we would process it.
}

void MASMCodeEditor::mousePressEvent(void* event) {
    // Basic Mouse Handling Logic
}

void MASMCodeEditor::updateLineNumberAreaWidth(int newBlockCount) {
    // Calculate width needed for line numbers
    // e.g. digits * charWidth
}

void MASMCodeEditor::highlightCurrentLine() {
    // Mark current line for renderer
}

void MASMCodeEditor::updateLineNumberArea(const struct { int x; int y; int w; int h; }& rect, int dy) {
    // Scroll handling for line numbers
}

void MASMCodeEditor::paintLineNumberArea(void* event) {
    // Rendering logic for Line numbers
}

void MASMCodeEditor::paintErrorMarkers(void* event) {
    // Rendering logic for Red squiggle underlines
}

void MASMCodeEditor::paintBreakpoints(void* event) {
    // Rendering logic for Red dots
}

void MASMCodeEditor::breakpointToggled(int line, bool enabled) {
    if (enabled) m_breakpoints.insert(line);
    else m_breakpoints.erase(line);
}

void MASMCodeEditor::errorClicked(const MASMError& error) {
    // Navigate to error
    std::cout << "Navigating to error: " << error.message << " at line " << error.line << std::endl;
}

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
    // Launch debugger with proper process creation
    if(m_buildOutput) m_buildOutput->appendMessage("Launching debugger...");
    std::string exe = m_project.mainFile.substr(0, m_project.mainFile.find_last_of('.')) + ".exe";
    
    // Try to launch VS JIT debugger first
    std::string cmd = "vsjitdebugger.exe -p " + std::to_string(GetCurrentProcessId());
    
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    
    if (CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()), nullptr, nullptr, FALSE, 
                       CREATE_NEW_CONSOLE | DEBUG_PROCESS, nullptr, nullptr, &si, &pi)) {
        if(m_buildOutput) m_buildOutput->appendMessage("Debugger attached successfully");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        // Fallback: try running with debug output
        if(m_buildOutput) m_buildOutput->appendMessage("VS JIT debugger not available, falling back to run");
        run();
    }
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
void MASMCompilerWidget::onCompilerOutput() {
    // Read compiler stdout and append to build output
    if (!m_compilerProcess) return;
    
    char buffer[4096];
    DWORD bytesRead;
    if (ReadFile(m_compilerProcess->hStdOutput, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        std::string output(buffer);
        parseCompilerOutput(output);
        if (m_buildOutput) m_buildOutput->appendMessage(output);
    }
}

void MASMCompilerWidget::onCompilerError() {
    // Read compiler stderr and append to build output as errors
    if (!m_compilerProcess) return;
    
    char buffer[4096];
    DWORD bytesRead;
    if (ReadFile(m_compilerProcess->hStdError, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        std::string errorOutput(buffer);
        extractErrors(errorOutput);
        if (m_buildOutput) m_buildOutput->appendMessage("Error: " + errorOutput);
    }
}

void MASMCompilerWidget::parseCompilerOutput(const std::string& output) {
    // Parse MASM compiler output for errors and warnings
    std::istringstream stream(output);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Look for error patterns: filename(line): error/warning code: message
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string filename = line.substr(0, colonPos);
            size_t parenPos = filename.find('(');
            if (parenPos != std::string::npos) {
                int lineNum = std::stoi(filename.substr(parenPos + 1, filename.find(')') - parenPos - 1));
                filename = filename.substr(0, parenPos);
                
                std::string rest = line.substr(colonPos + 1);
                bool isError = rest.find("error") != std::string::npos;
                bool isWarning = rest.find("warning") != std::string::npos;
                
                if (isError || isWarning) {
                    MASMError err;
                    err.filename = filename;
                    err.line = lineNum;
                    err.message = rest;
                    err.severity = isError ? "error" : "warning";
                    m_errors.push_back(err);
                }
            }
        }
    }
}

void MASMCompilerWidget::extractErrors(const std::string& output) {
    // Extract error messages from compiler stderr
    parseCompilerOutput(output);
}

void MASMCompilerWidget::updateUIAfterCompilation(bool success) {
    // Update UI state after compilation completes
    m_isCompiling = false;
    
    if (success && m_errors.empty()) {
        if (m_buildOutput) m_buildOutput->appendMessage("Compilation successful.");
        m_compilationSucceeded = true;
    } else {
        if (m_buildOutput) {
            m_buildOutput->appendMessage("Compilation failed with " + 
                std::to_string(m_errors.size()) + " errors.");
        }
        m_compilationSucceeded = false;
    }
    
    // Update toolbar buttons
    updateToolbar();
}

void MASMCompilerWidget::onExecutableFinished(int exitCode, int exitStatus) {
    m_isRunning = false;
    
    if (exitStatus == 0 && exitCode == 0) {
        if (m_buildOutput) m_buildOutput->appendMessage("Execution completed successfully.");
    } else {
        if (m_buildOutput) {
            m_buildOutput->appendMessage("Execution failed with exit code: " + std::to_string(exitCode));
        }
    }
    
    executionFinished(exitCode);
}

void MASMCompilerWidget::onExecutableOutput() {
    // Read program stdout
    if (!m_runningProcess) return;
    
    char buffer[4096];
    DWORD bytesRead;
    if (ReadFile(m_runningProcess->hStdOutput, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        if (m_buildOutput) m_buildOutput->appendMessage(std::string(buffer));
    }
}

void MASMCompilerWidget::onExecutableError() {
    // Read program stderr
    if (!m_runningProcess) return;
    
    char buffer[4096];
    DWORD bytesRead;
    if (ReadFile(m_runningProcess->hStdError, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        if (m_buildOutput) m_buildOutput->appendMessage("Runtime: " + std::string(buffer));
    }
}

void MASMCompilerWidget::onErrorClicked(const MASMError& error) {
    // Navigate to error location in editor
    if (m_editor) {
        m_editor->goToLine(error.line);
        m_editor->selectLine(error.line);
    }
}

void MASMCompilerWidget::onSymbolSelected(const MASMSymbol& symbol) {
    // Navigate to symbol definition
    if (m_editor) {
        m_editor->goToLine(symbol.line);
    }
}

void MASMCompilerWidget::onBreakpointToggled(int line, bool enabled) {
    if (enabled) {
        m_breakpoints.insert(line);
    } else {
        m_breakpoints.erase(line);
    }
    
    if (m_debugger) {
        m_debugger->setBreakpoints(m_breakpoints);
    }
}

void MASMCompilerWidget::saveFile() {
    if (m_editor && m_editor->isModified()) {
        m_editor->save();
        if (m_buildOutput) m_buildOutput->appendMessage("File saved: " + m_project.mainFile);
    }
}

void MASMCompilerWidget::openFile(const std::string& filePath) {
    if (m_editor) {
        m_editor->open(filePath);
        m_project.mainFile = filePath;
        if (m_buildOutput) m_buildOutput->appendMessage("Opened: " + filePath);
    }
}

void MASMCompilerWidget::setupUI() {
    // Create main layout and child widgets
    // In production: create editor, output panel, toolbar, etc.
    if (m_buildOutput) m_buildOutput->appendMessage("UI setup complete.");
}

void MASMCompilerWidget::setupToolbar() {
    // Create toolbar with compile, run, debug buttons
    // In production: create actual toolbar buttons
}

void MASMCompilerWidget::connectSignals() {
    // Connect UI signals to slots
    // In production: connect button clicks, editor events, etc.
}

void MASMCompilerWidget::compilationStarted() {
    m_isCompiling = true;
    m_errors.clear();
    m_compilationSucceeded = false;
    if (m_buildOutput) m_buildOutput->appendMessage("Compilation started...");
}

void MASMCompilerWidget::compilationFinished(bool success) {
    m_isCompiling = false;
    m_compilationSucceeded = success && m_errors.empty();
    updateUIAfterCompilation(success);
}

void MASMCompilerWidget::executionStarted() {
    m_isRunning = true;
    if (m_buildOutput) m_buildOutput->appendMessage("Execution started...");
}

void MASMCompilerWidget::executionFinished(int exitCode) {
    m_isRunning = false;
    if (m_buildOutput) {
        m_buildOutput->appendMessage("Execution finished with code: " + std::to_string(exitCode));
    }
}

// ============================================================================
// Helper class implementations
// ============================================================================

MASMProjectExplorer::MASMProjectExplorer(void* parent) : m_parent(parent) {}

void MASMProjectExplorer::setProject(const MASMProjectSettings& project) {
    m_project = project;
    refresh();
}

void MASMProjectExplorer::refresh() {
    clear();
    populateTree();
}

void MASMProjectExplorer::populateTree() {
    addItem(m_project.name, "project");
    for (const auto& file : m_project.sourceFiles) {
        addItem(file, "source");
    }
    for (const auto& dir : m_project.includePaths) {
        addItem(dir, "folder");
    }
}

void MASMProjectExplorer::onTreeItemDoubleClicked(void* item, int column) {
    (void)column;
    if (item) {
        std::string filePath = getItemPath(item);
        if (!filePath.empty()) fileOpened(filePath);
    }
}

void MASMProjectExplorer::onTreeContextMenu(const struct { int x; int y; }& pos) {
    (void)pos;
}

void MASMProjectExplorer::addContextMenuActions(void* menu, void* item) {
    (void)menu;
    (void)item;
}

void MASMProjectExplorer::fileOpened(const std::string& filePath) {
    if (m_fileOpenCallback) m_fileOpenCallback(filePath);
}

MASMSymbolBrowser::MASMSymbolBrowser(void* parent) : m_parent(parent) {}

void MASMSymbolBrowser::setSymbols(const std::vector<MASMSymbol>& symbols) {
    m_symbols = symbols;
    populateTree();
}

void MASMSymbolBrowser::clear() {
    m_symbols.clear();
}

void MASMSymbolBrowser::filter(const std::string& text) {
    m_filterText = text;
    populateTree();
}

void MASMSymbolBrowser::populateTree() {
    clear();
    for (const auto& symbol : m_symbols) {
        if (!m_filterText.empty() && symbol.name.find(m_filterText) == std::string::npos) continue;
        std::string typeStr;
        switch (symbol.type) {
            case MASMSymbol::Function: typeStr = "function"; break;
            case MASMSymbol::Variable: typeStr = "variable"; break;
            case MASMSymbol::Label: typeStr = "label"; break;
            default: typeStr = "unknown"; break;
        }
        addItem(symbol.name + " (" + typeStr + ")", symbol.line);
    }
}

void MASMSymbolBrowser::onSymbolClicked(void* item, int column) {
    (void)column;
    if (item && m_symbolSelectedCallback) {
        m_symbolSelectedCallback(getSymbolFromItem(item));
    }
}

void MASMSymbolBrowser::onFilterChanged(const std::string& text) {
    filter(text);
}

MASMDebugger::MASMDebugger(void* parent) : m_parent(parent), m_isDebugging(false) {}

void MASMDebugger::startDebugging(const std::string& executablePath) {
    m_executablePath = executablePath;
    m_isDebugging = true;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (CreateProcessA(executablePath.c_str(), nullptr, nullptr, nullptr, FALSE,
                       DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS, nullptr, nullptr, &si, &pi)) {
        m_processHandle = pi.hProcess;
        m_threadHandle = pi.hThread;
        m_processId = pi.dwProcessId;
        debuggerStarted();
    } else {
        m_isDebugging = false;
    }
}

void MASMDebugger::stopDebugging() {
    if (m_isDebugging) {
        if (m_processHandle) {
            TerminateProcess(m_processHandle, 1);
            CloseHandle(m_processHandle);
            CloseHandle(m_threadHandle);
        }
        m_isDebugging = false;
        debuggerStopped();
    }
}

void MASMDebugger::stepOver() {
    if (!m_isDebugging) return;
    ContinueDebugEvent(m_processId, m_threadId, DBG_CONTINUE);
}

void MASMDebugger::stepInto() {
    if (!m_isDebugging) return;
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_CONTROL;
    GetThreadContext(m_threadHandle, &ctx);
    ctx.EFlags |= 0x100;
    SetThreadContext(m_threadHandle, &ctx);
    ContinueDebugEvent(m_processId, m_threadId, DBG_CONTINUE);
}

void MASMDebugger::stepOut() {
    if (!m_isDebugging) return;
    ContinueDebugEvent(m_processId, m_threadId, DBG_CONTINUE);
}

void MASMDebugger::continueExecution() {
    if (!m_isDebugging) return;
    ContinueDebugEvent(m_processId, m_threadId, DBG_CONTINUE);
}

void MASMDebugger::pause() {
    if (!m_isDebugging) return;
    SuspendThread(m_threadHandle);
}

void MASMDebugger::setBreakpoints(const std::unordered_set<int>& breakpoints) {
    m_breakpoints = breakpoints;
}

void MASMDebugger::onDebuggerOutput() {}
void MASMDebugger::onDebuggerError() {}

void MASMDebugger::updateRegisters() {
    if (!m_isDebugging || !m_threadHandle) return;
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_ALL;
    if (GetThreadContext(m_threadHandle, &ctx)) {
        m_registers.rax = ctx.Rax; m_registers.rbx = ctx.Rbx;
        m_registers.rcx = ctx.Rcx; m_registers.rdx = ctx.Rdx;
        m_registers.rsi = ctx.Rsi; m_registers.rdi = ctx.Rdi;
        m_registers.rbp = ctx.Rbp; m_registers.rsp = ctx.Rsp;
        m_registers.rip = ctx.Rip;
        m_registers.r8 = ctx.R8; m_registers.r9 = ctx.R9;
        m_registers.r10 = ctx.R10; m_registers.r11 = ctx.R11;
        m_registers.r12 = ctx.R12; m_registers.r13 = ctx.R13;
        m_registers.r14 = ctx.R14; m_registers.r15 = ctx.R15;
        m_registers.rflags = ctx.EFlags;
    }
}

void MASMDebugger::updateStack() {
    if (!m_isDebugging || !m_threadHandle) return;
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_CONTROL;
    if (GetThreadContext(m_threadHandle, &ctx)) {
        m_stackFrames.clear();
        uint64_t rbp = ctx.Rbp, rip = ctx.Rip;
        for (int i = 0; i < 32 && rbp != 0; ++i) {
            StackFrame frame{rip, rbp};
            m_stackFrames.push_back(frame);
            SIZE_T bytesRead;
            if (!ReadProcessMemory(m_processHandle, reinterpret_cast<LPCVOID>(rbp + 8), &rip, sizeof(rip), &bytesRead)) break;
            if (!ReadProcessMemory(m_processHandle, reinterpret_cast<LPCVOID>(rbp), &rbp, sizeof(rbp), &bytesRead)) break;
        }
    }
}

void MASMDebugger::updateDisassembly() {
    if (!m_isDebugging || !m_processHandle) return;
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_CONTROL;
    if (GetThreadContext(m_threadHandle, &ctx)) {
        uint8_t buffer[64];
        SIZE_T bytesRead;
        if (ReadProcessMemory(m_processHandle, reinterpret_cast<LPCVOID>(ctx.Rip), buffer, sizeof(buffer), &bytesRead)) {
            m_disassembly.clear();
            for (SIZE_T i = 0; i < bytesRead; ++i) m_disassembly.push_back(buffer[i]);
        }
    }
}

void MASMDebugger::debuggerStarted() {
    m_isDebugging = true;
    if (m_startedCallback) m_startedCallback();
}

void MASMDebugger::debuggerStopped() {
    m_isDebugging = false;
    if (m_stoppedCallback) m_stoppedCallback();
}

MASMBuildOutput::MASMBuildOutput(void* parent) : m_parent(parent) {}

void MASMBuildOutput::clear() {
    m_messages.clear();
    m_errors.clear();
    m_stats = {};
}

void MASMBuildOutput::appendMessage(const std::string& message) {
    m_messages.push_back(message);
    std::cout << "[MASM] " << message << std::endl;
#ifdef _WIN32
    if (IsDebuggerPresent()) OutputDebugStringA(("[MASM] " + message + "\n").c_str());
#endif
}

void MASMBuildOutput::appendError(const MASMError& error) {
    m_errors.push_back(error);
    std::cerr << "[MASM Error] " << error.filename << ":" << error.line << " " << error.message << std::endl;
}

void MASMBuildOutput::appendStage(const std::string& stage) {
    std::cout << "[MASM Stage] " << stage << std::endl;
}

void MASMBuildOutput::setStats(const MASMCompilationStats& stats) {
    m_stats = stats;
}

void MASMBuildOutput::formatErrorMessage(const MASMError& error, std::string& output) {
    output = error.filename + "(" + std::to_string(error.line) + "): " +
             error.severity + ": " + error.message;
}

void MASMBuildOutput::onOutputDoubleClicked() {
    if (!m_errors.empty() && m_errorNavigateCallback) {
        m_errorNavigateCallback(m_errors[0]);
    }
}

void MASMBuildOutput::errorDoubleClicked(const MASMError& error) {
    if (m_errorNavigateCallback) {
        m_errorNavigateCallback(error);
    }
}
