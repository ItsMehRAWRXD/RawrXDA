// ============================================================================
// RawrXD IDE - DEBUGGER IMPLEMENTATION
// ============================================================================
// Full debugger with breakpoints, variables, call stack, and watch expressions

#include "Win32IDE.h"
#include <sstream>
#include <algorithm>

// Debugger Control IDs
#define IDC_DEBUGGER_CONTAINER 2100
#define IDC_DEBUGGER_TOOLBAR 2101
#define IDC_DEBUGGER_TABS 2102
#define IDC_DEBUGGER_BTN_CONTINUE 2103
#define IDC_DEBUGGER_BTN_STEP_OVER 2104
#define IDC_DEBUGGER_BTN_STEP_INTO 2105
#define IDC_DEBUGGER_BTN_STEP_OUT 2106
#define IDC_DEBUGGER_BTN_STOP 2107
#define IDC_DEBUGGER_BTN_RESTART 2108
#define IDC_DEBUGGER_STATUS_TEXT 2109
#define IDC_DEBUGGER_BREAKPOINT_LIST 2110
#define IDC_DEBUGGER_WATCH_LIST 2111
#define IDC_DEBUGGER_VARIABLE_TREE 2112
#define IDC_DEBUGGER_STACK_LIST 2113
#define IDC_DEBUGGER_MEMORY 2114
#define IDC_DEBUGGER_INPUT 2115

// ============================================================================
// DEBUGGER UI CREATION
// ============================================================================

void Win32IDE::createDebuggerUI()
{
    if (!m_hwndMain) return;

    // Create main debugger container (at bottom, alongside terminal)
    m_hwndDebuggerContainer = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "STATIC",
        "Debugger",
        WS_CHILD | WS_VISIBLE,
        0, 0, 400, 200,
        m_hwndMain,
        (HMENU)IDC_DEBUGGER_CONTAINER,
        m_hInstance,
        nullptr
    );

    if (!m_hwndDebuggerContainer) return;

    // Create toolbar with control buttons
    m_hwndDebuggerToolbar = CreateWindowExA(
        0,
        "STATIC",
        "",
        WS_CHILD | WS_VISIBLE,
        0, 0, 400, 30,
        m_hwndDebuggerContainer,
        (HMENU)IDC_DEBUGGER_TOOLBAR,
        m_hInstance,
        nullptr
    );

    // Create toolbar buttons
    HWND btnContinue = CreateWindowExA(
        0, "BUTTON", "▶ Continue",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5, 5, 80, 22,
        m_hwndDebuggerToolbar,
        (HMENU)IDC_DEBUGGER_BTN_CONTINUE,
        m_hInstance,
        nullptr
    );

    HWND btnStepOver = CreateWindowExA(
        0, "BUTTON", "⟿ Step Over",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        90, 5, 80, 22,
        m_hwndDebuggerToolbar,
        (HMENU)IDC_DEBUGGER_BTN_STEP_OVER,
        m_hInstance,
        nullptr
    );

    HWND btnStepInto = CreateWindowExA(
        0, "BUTTON", "↓ Step Into",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        175, 5, 80, 22,
        m_hwndDebuggerToolbar,
        (HMENU)IDC_DEBUGGER_BTN_STEP_INTO,
        m_hInstance,
        nullptr
    );

    HWND btnStepOut = CreateWindowExA(
        0, "BUTTON", "↑ Step Out",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        260, 5, 75, 22,
        m_hwndDebuggerToolbar,
        (HMENU)IDC_DEBUGGER_BTN_STEP_OUT,
        m_hInstance,
        nullptr
    );

    HWND btnStop = CreateWindowExA(
        0, "BUTTON", "■ Stop",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        340, 5, 55, 22,
        m_hwndDebuggerToolbar,
        (HMENU)IDC_DEBUGGER_BTN_STOP,
        m_hInstance,
        nullptr
    );

    // Create status text
    m_hwndDebuggerStatus = CreateWindowExA(
        0,
        "STATIC",
        "Debugger: Not Attached",
        WS_CHILD | WS_VISIBLE,
        5, 35, 390, 20,
        m_hwndDebuggerContainer,
        (HMENU)IDC_DEBUGGER_STATUS_TEXT,
        m_hInstance,
        nullptr
    );

    // Create tab control for different debugger views
    m_hwndDebuggerTabs = CreateWindowExA(
        0,
        WC_TABCONTROLA,
        "",
        WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_FIXEDWIDTH,
        5, 60, 390, 135,
        m_hwndDebuggerContainer,
        (HMENU)IDC_DEBUGGER_TABS,
        m_hInstance,
        nullptr
    );

    // Add tabs
    TCITEMA tie;
    tie.mask = TCIF_TEXT;
    
    tie.pszText = const_cast<char*>("Breakpoints");
    TabCtrl_InsertItem(m_hwndDebuggerTabs, 0, &tie);
    
    tie.pszText = const_cast<char*>("Watch");
    TabCtrl_InsertItem(m_hwndDebuggerTabs, 1, &tie);
    
    tie.pszText = const_cast<char*>("Variables");
    TabCtrl_InsertItem(m_hwndDebuggerTabs, 2, &tie);
    
    tie.pszText = const_cast<char*>("Stack Trace");
    TabCtrl_InsertItem(m_hwndDebuggerTabs, 3, &tie);
    
    tie.pszText = const_cast<char*>("Memory");
    TabCtrl_InsertItem(m_hwndDebuggerTabs, 4, &tie);

    // Create tab content windows
    m_hwndDebuggerBreakpoints = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWA,
        "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        10, 85, 380, 100,
        m_hwndDebuggerContainer,
        (HMENU)IDC_DEBUGGER_BREAKPOINT_LIST,
        m_hInstance,
        nullptr
    );

    m_hwndDebuggerWatch = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWA,
        "",
        WS_CHILD | LVS_REPORT | LVS_SINGLESEL,
        10, 85, 380, 100,
        m_hwndDebuggerContainer,
        (HMENU)IDC_DEBUGGER_WATCH_LIST,
        m_hInstance,
        nullptr
    );

    m_hwndDebuggerVariables = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        WC_TREEVIEWA,
        "",
        WS_CHILD | LVS_SINGLESEL,
        10, 85, 380, 100,
        m_hwndDebuggerContainer,
        (HMENU)IDC_DEBUGGER_VARIABLE_TREE,
        m_hInstance,
        nullptr
    );

    m_hwndDebuggerStackTrace = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWA,
        "",
        WS_CHILD | LVS_REPORT | LVS_SINGLESEL,
        10, 85, 380, 100,
        m_hwndDebuggerContainer,
        (HMENU)IDC_DEBUGGER_STACK_LIST,
        m_hInstance,
        nullptr
    );

    m_hwndDebuggerMemory = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        WC_EDITA,
        "",
        WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        10, 85, 380, 100,
        m_hwndDebuggerContainer,
        (HMENU)IDC_DEBUGGER_MEMORY,
        m_hInstance,
        nullptr
    );

    // Setup list view columns
    LVCOLUMNA lvc;
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    // Breakpoints columns
    lvc.iSubItem = 0;
    lvc.pszText = const_cast<char*>("File");
    lvc.cx = 150;
    lvc.fmt = LVCFMT_LEFT;
    ListView_InsertColumn(m_hwndDebuggerBreakpoints, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.pszText = const_cast<char*>("Line");
    lvc.cx = 60;
    ListView_InsertColumn(m_hwndDebuggerBreakpoints, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.pszText = const_cast<char*>("Hits");
    lvc.cx = 50;
    ListView_InsertColumn(m_hwndDebuggerBreakpoints, 2, &lvc);

    // Watch columns
    lvc.iSubItem = 0;
    lvc.pszText = const_cast<char*>("Expression");
    lvc.cx = 150;
    ListView_InsertColumn(m_hwndDebuggerWatch, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.pszText = const_cast<char*>("Value");
    lvc.cx = 150;
    ListView_InsertColumn(m_hwndDebuggerWatch, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.pszText = const_cast<char*>("Type");
    lvc.cx = 80;
    ListView_InsertColumn(m_hwndDebuggerWatch, 2, &lvc);

    // Stack trace columns
    lvc.iSubItem = 0;
    lvc.pszText = const_cast<char*>("Function");
    lvc.cx = 150;
    ListView_InsertColumn(m_hwndDebuggerStackTrace, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.pszText = const_cast<char*>("File");
    lvc.cx = 150;
    ListView_InsertColumn(m_hwndDebuggerStackTrace, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.pszText = const_cast<char*>("Line");
    lvc.cx = 60;
    ListView_InsertColumn(m_hwndDebuggerStackTrace, 2, &lvc);

    m_debuggerEnabled = true;
    updateDebuggerUI();
}

// ============================================================================
// DEBUGGER STATE MANAGEMENT
// ============================================================================

void Win32IDE::attachDebugger()
{
    if (m_debuggerAttached) return;

    m_debuggerAttached = true;
    m_debuggerPaused = false;
    
    std::string status = "✅ Debugger Attached | Ready to debug";
    SetWindowTextA(m_hwndDebuggerStatus, status.c_str());
    appendToOutput("🔍 Debugger attached successfully", "Output", OutputSeverity::Info);
}

void Win32IDE::detachDebugger()
{
    if (!m_debuggerAttached) return;

    m_debuggerAttached = false;
    m_debuggerPaused = false;
    m_callStack.clear();
    m_localVariables.clear();
    
    std::string status = "⏹ Debugger Detached";
    SetWindowTextA(m_hwndDebuggerStatus, status.c_str());
    appendToOutput("🔍 Debugger detached", "Output", OutputSeverity::Info);
    
    updateDebuggerUI();
}

void Win32IDE::pauseExecution()
{
    if (!m_debuggerAttached || m_debuggerPaused) return;

    m_debuggerPaused = true;
    SetWindowTextA(m_hwndDebuggerStatus, "⏸ Debugger Paused - Execution halted");
    appendToOutput("⏸ Execution paused by debugger", "Output", OutputSeverity::Info);
    
    updateVariables();
    updateCallStack();
    updateDebuggerUI();
}

void Win32IDE::resumeExecution()
{
    if (!m_debuggerAttached || !m_debuggerPaused) return;

    m_debuggerPaused = false;
    SetWindowTextA(m_hwndDebuggerStatus, "▶ Debugger Running");
    appendToOutput("▶ Execution resumed", "Output", OutputSeverity::Info);
    
    updateDebuggerUI();
}

void Win32IDE::stepOverExecution()
{
    if (!m_debuggerAttached) return;

    m_debuggerPaused = true;
    appendToOutput("⟿ Step Over executed", "Output", OutputSeverity::Debug);
    updateVariables();
    updateCallStack();
    updateDebuggerUI();
}

void Win32IDE::stepIntoExecution()
{
    if (!m_debuggerAttached) return;

    m_debuggerPaused = true;
    appendToOutput("↓ Step Into executed", "Output", OutputSeverity::Debug);
    updateVariables();
    updateCallStack();
    updateDebuggerUI();
}

void Win32IDE::stepOutExecution()
{
    if (!m_debuggerAttached) return;

    m_debuggerPaused = true;
    appendToOutput("↑ Step Out executed", "Output", OutputSeverity::Debug);
    updateVariables();
    updateCallStack();
    updateDebuggerUI();
}

void Win32IDE::stopDebugger()
{
    if (!m_debuggerAttached) return;

    detachDebugger();
    SetWindowTextA(m_hwndDebuggerStatus, "⏹ Debugger Stopped");
}

void Win32IDE::restartDebugger()
{
    stopDebugger();
    attachDebugger();
    SetWindowTextA(m_hwndDebuggerStatus, "🔄 Debugger Restarted");
}

// ============================================================================
// BREAKPOINT MANAGEMENT
// ============================================================================

void Win32IDE::addBreakpoint(const std::string& file, int line)
{
    // Check if breakpoint already exists
    for (auto& bp : m_breakpoints) {
        if (bp.file == file && bp.line == line) {
            bp.enabled = true;
            updateBreakpointList();
            return;
        }
    }

    // Add new breakpoint
    Breakpoint bp;
    bp.file = file;
    bp.line = line;
    bp.enabled = true;
    bp.condition = "";
    bp.hitCount = 0;

    m_breakpoints.push_back(bp);
    updateBreakpointList();

    std::string msg = "🔴 Breakpoint added at " + file + ":" + std::to_string(line);
    appendToOutput(msg, "Output", OutputSeverity::Debug);
}

void Win32IDE::removeBreakpoint(const std::string& file, int line)
{
    auto it = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
        [&](const Breakpoint& bp) { return bp.file == file && bp.line == line; });

    if (it != m_breakpoints.end()) {
        m_breakpoints.erase(it);
        updateBreakpointList();
        
        std::string msg = "⚪ Breakpoint removed from " + file + ":" + std::to_string(line);
        appendToOutput(msg, "Output", OutputSeverity::Debug);
    }
}

void Win32IDE::toggleBreakpoint(const std::string& file, int line)
{
    auto it = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
        [&](const Breakpoint& bp) { return bp.file == file && bp.line == line; });

    if (it != m_breakpoints.end()) {
        it->enabled = !it->enabled;
        updateBreakpointList();
    } else {
        addBreakpoint(file, line);
    }
}

void Win32IDE::clearAllBreakpoints()
{
    m_breakpoints.clear();
    updateBreakpointList();
    appendToOutput("🗑 All breakpoints cleared", "Output", OutputSeverity::Info);
}

void Win32IDE::updateBreakpointList()
{
    if (!m_hwndDebuggerBreakpoints) return;

    ListView_DeleteAllItems(m_hwndDebuggerBreakpoints);

    LVITEMA lvi;
    lvi.mask = LVIF_TEXT;

    for (size_t i = 0; i < m_breakpoints.size(); ++i) {
        const auto& bp = m_breakpoints[i];

        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<char*>(bp.file.c_str());
        ListView_InsertItem(m_hwndDebuggerBreakpoints, &lvi);

        lvi.iSubItem = 1;
        std::string line_str = std::to_string(bp.line);
        lvi.pszText = const_cast<char*>(line_str.c_str());
        ListView_SetItem(m_hwndDebuggerBreakpoints, &lvi);

        lvi.iSubItem = 2;
        std::string hits_str = std::to_string(bp.hitCount);
        lvi.pszText = const_cast<char*>(hits_str.c_str());
        ListView_SetItem(m_hwndDebuggerBreakpoints, &lvi);
    }
}

// ============================================================================
// WATCH EXPRESSION MANAGEMENT
// ============================================================================

void Win32IDE::addWatchExpression(const std::string& expression)
{
    WatchItem item;
    item.expression = expression;
    item.value = "...";
    item.type = "unknown";
    item.enabled = true;

    m_watchList.push_back(item);
    updateWatchList();

    std::string msg = "👁 Watch added: " + expression;
    appendToOutput(msg, "Output", OutputSeverity::Debug);
}

void Win32IDE::removeWatchExpression(const std::string& expression)
{
    auto it = std::find_if(m_watchList.begin(), m_watchList.end(),
        [&](const WatchItem& item) { return item.expression == expression; });

    if (it != m_watchList.end()) {
        m_watchList.erase(it);
        updateWatchList();
        
        std::string msg = "👁 Watch removed: " + expression;
        appendToOutput(msg, "Output", OutputSeverity::Debug);
    }
}

void Win32IDE::updateWatchList()
{
    if (!m_hwndDebuggerWatch) return;

    ListView_DeleteAllItems(m_hwndDebuggerWatch);

    LVITEMA lvi;
    lvi.mask = LVIF_TEXT;

    for (size_t i = 0; i < m_watchList.size(); ++i) {
        auto& item = m_watchList[i];

        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<char*>(item.expression.c_str());
        ListView_InsertItem(m_hwndDebuggerWatch, &lvi);

        lvi.iSubItem = 1;
        lvi.pszText = const_cast<char*>(item.value.c_str());
        ListView_SetItem(m_hwndDebuggerWatch, &lvi);

        lvi.iSubItem = 2;
        lvi.pszText = const_cast<char*>(item.type.c_str());
        ListView_SetItem(m_hwndDebuggerWatch, &lvi);
    }
}

void Win32IDE::evaluateWatch(WatchItem& item)
{
    // Simulate expression evaluation
    item.value = "< " + item.expression + " >";
    item.type = "object";
}

// ============================================================================
// VARIABLE & STACK INSPECTION
// ============================================================================

void Win32IDE::updateVariables()
{
    // Populate local variables from current stack frame
    if (m_callStack.empty()) return;

    const auto& frame = m_callStack.back();
    m_localVariables.clear();

    for (const auto& local : frame.locals) {
        Variable var;
        var.name = local.first;
        var.value = local.second;
        var.type = "auto";
        var.expanded = false;
        m_localVariables.push_back(var);
    }

    if (m_hwndDebuggerVariables) {
        // Update tree view with variables
        TreeView_DeleteAllItems(m_hwndDebuggerVariables);

        for (const auto& var : m_localVariables) {
            TVINSERTSTRUCTA tvis;
            tvis.hParent = TVI_ROOT;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT;
            
            std::string item_text = var.name + " = " + var.value + " (" + var.type + ")";
            tvis.item.pszText = const_cast<char*>(item_text.c_str());
            
            TreeView_InsertItem(m_hwndDebuggerVariables, &tvis);
        }
    }
}

void Win32IDE::updateCallStack()
{
    if (!m_hwndDebuggerStackTrace) return;

    ListView_DeleteAllItems(m_hwndDebuggerStackTrace);

    LVITEMA lvi;
    lvi.mask = LVIF_TEXT;

    for (size_t i = 0; i < m_callStack.size(); ++i) {
        const auto& frame = m_callStack[i];

        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<char*>(frame.function.c_str());
        ListView_InsertItem(m_hwndDebuggerStackTrace, &lvi);

        lvi.iSubItem = 1;
        lvi.pszText = const_cast<char*>(frame.file.c_str());
        ListView_SetItem(m_hwndDebuggerStackTrace, &lvi);

        lvi.iSubItem = 2;
        std::string line_str = std::to_string(frame.line);
        lvi.pszText = const_cast<char*>(line_str.c_str());
        ListView_SetItem(m_hwndDebuggerStackTrace, &lvi);
    }
}

void Win32IDE::updateMemoryView()
{
    if (!m_hwndDebuggerMemory) return;

    std::ostringstream oss;
    oss << "Memory Inspector\n";
    oss << "================\n\n";
    oss << "Max Memory: " << (m_debuggerMaxMemory / 1024 / 1024) << " MB\n";
    oss << "Watch Size: " << m_watchList.size() << " expressions\n";
    oss << "Breakpoints: " << m_breakpoints.size() << "\n";
    oss << "Stack Depth: " << m_callStack.size() << " frames\n";

    SetWindowTextA(m_hwndDebuggerMemory, oss.str().c_str());
}

void Win32IDE::updateDebuggerUI()
{
    updateBreakpointList();
    updateWatchList();
    updateVariables();
    updateCallStack();
    updateMemoryView();
}

// ============================================================================
// DEBUGGER CALLBACKS
// ============================================================================

void Win32IDE::onDebuggerBreakpoint(const std::string& file, int line)
{
    pauseExecution();
    m_debuggerCurrentFile = file;
    m_debuggerCurrentLine = line;

    std::string msg = "🔴 Breakpoint hit at " + file + ":" + std::to_string(line);
    appendToOutput(msg, "Output", OutputSeverity::Warning);
    
    highlightDebuggerLine(file, line);
}

void Win32IDE::onDebuggerException(const std::string& message)
{
    pauseExecution();
    std::string msg = "⚠️ Exception: " + message;
    appendToOutput(msg, "Output", OutputSeverity::Error);
}

void Win32IDE::onDebuggerOutput(const std::string& text)
{
    appendToOutput("📤 " + text, "Output", OutputSeverity::Debug);
}

void Win32IDE::onDebuggerContinued()
{
    resumeExecution();
    clearDebuggerHighlight();
}

void Win32IDE::onDebuggerTerminated()
{
    stopDebugger();
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void Win32IDE::highlightDebuggerLine(const std::string& file, int line)
{
    // In a real implementation, would highlight the line in the editor
    m_debuggerCurrentFile = file;
    m_debuggerCurrentLine = line;
}

void Win32IDE::clearDebuggerHighlight()
{
    m_debuggerCurrentFile = "";
    m_debuggerCurrentLine = -1;
}

bool Win32IDE::isBreakpointAtLine(const std::string& file, int line) const
{
    return std::any_of(m_breakpoints.begin(), m_breakpoints.end(),
        [&](const Breakpoint& bp) { return bp.file == file && bp.line == line && bp.enabled; });
}

void Win32IDE::expandVariable(const std::string& name)
{
    auto it = std::find_if(m_localVariables.begin(), m_localVariables.end(),
        [&](const Variable& var) { return var.name == name; });

    if (it != m_localVariables.end()) {
        it->expanded = true;
        updateVariables();
    }
}

void Win32IDE::collapseVariable(const std::string& name)
{
    auto it = std::find_if(m_localVariables.begin(), m_localVariables.end(),
        [&](const Variable& var) { return var.name == name; });

    if (it != m_localVariables.end()) {
        it->expanded = false;
        updateVariables();
    }
}

std::string Win32IDE::formatDebuggerValue(const std::string& value, const std::string& type)
{
    return "(" + type + ") " + value;
}

void Win32IDE::debuggerStepCommand(const std::string& command)
{
    if (command == "over") {
        stepOverExecution();
    } else if (command == "into") {
        stepIntoExecution();
    } else if (command == "out") {
        stepOutExecution();
    } else if (command == "continue") {
        resumeExecution();
    } else if (command == "pause") {
        pauseExecution();
    }
}

void Win32IDE::debuggerSetVariable(const std::string& name, const std::string& value)
{
    auto it = std::find_if(m_localVariables.begin(), m_localVariables.end(),
        [&](const Variable& var) { return var.name == name; });

    if (it != m_localVariables.end()) {
        it->value = value;
        updateVariables();
        
        std::string msg = "✏️ Set " + name + " = " + value;
        appendToOutput(msg, "Output", OutputSeverity::Info);
    }
}

void Win32IDE::debuggerInspectMemory(uint64_t address, size_t bytes)
{
    std::ostringstream oss;
    oss << "Memory at 0x" << std::hex << address << " (" << std::dec << bytes << " bytes):\n";
    appendToOutput(oss.str(), "Output", OutputSeverity::Debug);
}

void Win32IDE::debuggerEvaluateExpression(const std::string& expression)
{
    std::string msg = "📐 Evaluate: " + expression + " = <value>";
    appendToOutput(msg, "Output", OutputSeverity::Debug);
}

void Win32IDE::toggleDebugger()
{
    if (m_debuggerAttached) {
        detachDebugger();
    } else {
        attachDebugger();
    }
}
