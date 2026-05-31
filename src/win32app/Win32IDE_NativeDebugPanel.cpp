// =============================================================================
// Win32IDE_NativeDebugPanel.cpp — Phase 12: Native Debugger IDE Integration
// =============================================================================
// Command handlers and HTTP endpoint handlers for the native debugger engine.
// Integrates NativeDebuggerEngine into Win32IDE via 28 commands and 14 endpoints.
//
// IDM range: 5157 – 5184
// HTTP routes: /api/debug/* + /api/phase12/status
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <commctrl.h>
#include <commdlg.h>

#include "Win32IDE.h"
#include "../core/native_debugger_engine.h"
#include "../core/native_debugger_types.h"

#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>

using namespace RawrXD::Debugger;

// =============================================================================
//                         HTTP Helper (shared pattern)
// =============================================================================

static void sendHttpJson(SOCKET client, const std::string& json) {
    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n"
         << "Content-Type: application/json\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Content-Length: " << json.size() << "\r\n"
         << "\r\n"
         << json;
    std::string s = resp.str();
    send(client, s.c_str(), static_cast<int>(s.size()), 0);
}

static void sendHttpOk(SOCKET client, const std::string& message) {
    std::ostringstream json;
    json << "{\"ok\":true,\"message\":\"" << message << "\"}";
    sendHttpJson(client, json.str());
}

static void sendHttpError(SOCKET client, const std::string& message) {
    std::ostringstream json;
    json << "{\"ok\":false,\"error\":\"" << message << "\"}";
    std::string s = json.str();

    std::ostringstream resp;
    resp << "HTTP/1.1 400 Bad Request\r\n"
         << "Content-Type: application/json\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Content-Length: " << s.size() << "\r\n"
         << "\r\n"
         << s;
    std::string r = resp.str();
    send(client, r.c_str(), static_cast<int>(r.size()), 0);
}

static std::string wideToUtf8Local(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int needed = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }

    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), needed, nullptr,
                        nullptr);
    return out;
}

static bool readClipboardText(HWND owner, std::string& out, size_t maxLen = 1024) {
    out.clear();
    if (!IsClipboardFormatAvailable(CF_TEXT) || !OpenClipboard(owner)) {
        return false;
    }

    bool ok = false;
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData) {
        const char* pText = static_cast<const char*>(GlobalLock(hData));
        if (pText) {
            out.assign(pText);
            if (out.size() > maxLen) {
                out.resize(maxLen);
            }
            GlobalUnlock(hData);
            ok = !out.empty();
        }
    }

    CloseClipboard();
    return ok;
}

static bool tryParseUint32(const std::string& text, uint32_t& value) {
    if (text.empty()) {
        return false;
    }

    char* endPtr = nullptr;
    unsigned long parsed = strtoul(text.c_str(), &endPtr, 10);
    if (!endPtr || *endPtr != '\0' || parsed == 0 || parsed > 0xFFFFFFFFUL) {
        return false;
    }

    value = static_cast<uint32_t>(parsed);
    return true;
}

static bool getSelectedListViewText(HWND listView, int subItem, std::string& out) {
    out.clear();
    if (!listView) {
        return false;
    }

    int selected = ListView_GetNextItem(listView, -1, LVNI_SELECTED);
    if (selected < 0) {
        return false;
    }

    char buffer[512] = {};
    LVITEMA item = {};
    item.iSubItem = subItem;
    item.pszText = buffer;
    item.cchTextMax = static_cast<int>(sizeof(buffer));
    int copied = static_cast<int>(SendMessageA(listView, LVM_GETITEMTEXTA, static_cast<WPARAM>(selected),
                                               reinterpret_cast<LPARAM>(&item)));
    if (copied <= 0 || buffer[0] == '\0') {
        return false;
    }

    out.assign(buffer);
    return true;
}

static bool promptInputUtf8(Win32IDE* ide,
                            const wchar_t* title,
                            const wchar_t* prompt,
                            std::string& out,
                            size_t maxChars = 255) {
    out.clear();
    if (!ide || maxChars == 0) {
        return false;
    }

    std::vector<wchar_t> wbuf(maxChars + 1, L'\0');
    if (!ide->DialogBoxWithInput(title, prompt, wbuf.data(), wbuf.size())) {
        return false;
    }

    std::wstring ws(wbuf.data());
    size_t first = ws.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return false;
    }
    size_t last = ws.find_last_not_of(L" \t\r\n");
    ws = ws.substr(first, last - first + 1);

    out = wideToUtf8Local(ws);
    return !out.empty();
}

// =============================================================================
//                    Phase 12 Lifecycle
// =============================================================================

void Win32IDE::initPhase12() {
    OutputDebugStringA("[Phase12] Initializing Native Debugger Engine...\n");

    DebugConfig config;
    config.breakOnEntry         = true;
    config.autoLoadSymbols      = true;
    config.enableSourceStepping = true;
    config.maxEventHistory      = 10000;

    // Default Microsoft symbol server path
    config.symbolPath = "srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols";

    DebugResult r = NativeDebuggerEngine::Instance().initialize(config);
    if (!r.success) {
        std::string msg = "[Phase12] WARNING: Native debugger init failed: ";
        msg += r.detail;
        msg += "\n";
        OutputDebugStringA(msg.c_str());
    }

    m_phase12Initialized = true;
    OutputDebugStringA("[Phase12] Native Debugger Engine initialized.\n");
}

void Win32IDE::shutdownPhase12() {
    if (!m_phase12Initialized) return;

    OutputDebugStringA("[Phase12] Shutting down Native Debugger Engine...\n");
    NativeDebuggerEngine::Instance().shutdown();
    m_phase12Initialized = false;
    OutputDebugStringA("[Phase12] Native Debugger Engine shut down.\n");
}

// =============================================================================
//                    Command Handlers — Session Control
// =============================================================================

void Win32IDE::cmdDbgLaunch() {
    auto& engine = NativeDebuggerEngine::Instance();
    registerDebuggerEngineCallbacks();

    char exePath[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwndMain;
    ofn.lpstrFilter  = "Executables\0*.exe\0All Files\0*.*\0";
    ofn.lpstrFile    = exePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle   = "Select Executable to Debug";

    if (!GetOpenFileNameA(&ofn)) return;

    DebugResult r = engine.launchProcess(exePath);
    if (r.success) {
        setCurrentBinaryForReverseEngineering(exePath);
        syncDebuggerSessionStateFromEngine();
        appendToOutput("[Debug] Launched: " + std::string(exePath) + "\n");
        appendToOutput("[RE] Binary set for analysis (Reverse Engineering menu: Disassemble, DumpBin, CFG, etc.)\n", "Output", OutputSeverity::Info);
    } else {
        syncDebuggerSessionStateFromEngine();
        appendToOutput("[Debug] Launch failed: " + std::string(r.detail) + "\n");
    }
}

void Win32IDE::cmdDbgAttach() {
    auto& engine = NativeDebuggerEngine::Instance();
    registerDebuggerEngineCallbacks();

    // Input dialog for PID
    wchar_t pidBuf[32] = {};
    if (!DialogBoxWithInput(L"Attach to Process", L"Enter Process ID (PID):", pidBuf, 32))
        return;

    uint32_t pid = static_cast<uint32_t>(_wtoi(pidBuf));
    if (pid == 0) {
        appendToOutput("[Debug] Invalid PID.\n");
        return;
    }

    DebugResult r = engine.attachToProcess(pid);
    if (r.success) {
        syncDebuggerSessionStateFromEngine();
        std::ostringstream msg;
        msg << "[Debug] Attached to PID " << pid << "\n";
        appendToOutput(msg.str());
        const std::string& path = engine.getTargetPath();
        if (!path.empty()) {
            setCurrentBinaryForReverseEngineering(path);
            appendToOutput("[RE] Binary set from debug target (Reverse Engineering menu).\n", "Output", OutputSeverity::Info);
        }
    } else {
        syncDebuggerSessionStateFromEngine();
        appendToOutput("[Debug] Attach failed: " + std::string(r.detail) + "\n");
    }
}

void Win32IDE::cmdDbgDetach() {
    syncDebuggerSessionStateFromEngine();
    if (m_debuggerAttached) {
        detachDebugger();
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().detach();
    appendToOutput("[Debug] " + std::string(r.detail) + "\n");
    syncDebuggerSessionStateFromEngine();
}

// =============================================================================
//                    Command Handlers — Execution Control
// =============================================================================

void Win32IDE::cmdDbgGo() {
    syncDebuggerSessionStateFromEngine();
    if (m_debuggerAttached && m_debuggerPaused) {
        resumeExecution();
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().go();
    appendToOutput("[Debug] " + std::string(r.detail) + "\n");
    syncDebuggerSessionStateFromEngine();
}

void Win32IDE::cmdDbgStepOver() {
    syncDebuggerSessionStateFromEngine();
    if (m_debuggerAttached) {
        stepOverExecution();
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().stepOver();
    appendToOutput("[Debug] " + std::string(r.detail) + "\n");
    syncDebuggerSessionStateFromEngine();
}

void Win32IDE::cmdDbgStepInto() {
    syncDebuggerSessionStateFromEngine();
    if (m_debuggerAttached) {
        stepIntoExecution();
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().stepInto();
    appendToOutput("[Debug] " + std::string(r.detail) + "\n");
    syncDebuggerSessionStateFromEngine();
}

void Win32IDE::cmdDbgStepOut() {
    syncDebuggerSessionStateFromEngine();
    if (m_debuggerAttached) {
        stepOutExecution();
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().stepOut();
    appendToOutput("[Debug] " + std::string(r.detail) + "\n");
    syncDebuggerSessionStateFromEngine();
}

void Win32IDE::cmdDbgBreak() {
    syncDebuggerSessionStateFromEngine();
    if (m_debuggerAttached && !m_debuggerPaused) {
        pauseExecution();
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().breakExecution();
    appendToOutput("[Debug] " + std::string(r.detail) + "\n");
    syncDebuggerSessionStateFromEngine();
}

void Win32IDE::cmdDbgKill() {
    syncDebuggerSessionStateFromEngine();
    if (m_debuggerAttached) {
        stopDebugger();
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().terminateTarget();
    appendToOutput("[Debug] " + std::string(r.detail) + "\n");
    syncDebuggerSessionStateFromEngine();
}

// =============================================================================
//                    Command Handlers — Breakpoints
// =============================================================================

void Win32IDE::cmdDbgAddBP() {
    // Prompt first for production UX; clipboard is fallback only.
    std::string rawInput;
    if (!promptInputUtf8(this,
                         L"Add Breakpoint",
                         L"Enter address (e.g. 0x7FF6A0001000) or symbol name:",
                         rawInput,
                         255)) {
        if (!readClipboardText(m_hwndMain, rawInput, 255)) {
            appendToOutput("[Debug] Breakpoint input cancelled.\n");
            return;
        }
    }

    uint64_t addr = 0;
    if (rawInput.size() > 2 && rawInput[0] == '0' && (rawInput[1] == 'x' || rawInput[1] == 'X')) {
        addr = strtoull(rawInput.c_str() + 2, nullptr, 16);
    } else if (!rawInput.empty()) {
        // Try as symbol name
        DebugResult r = NativeDebuggerEngine::Instance().addBreakpointBySymbol(rawInput);
        appendToOutput("[Debug] " + std::string(r.detail) + "\n");
        return;
    }

    if (addr == 0) {
        appendToOutput("[Debug] Invalid address. Copy hex address to clipboard.\n");
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().addBreakpoint(addr);
    std::ostringstream msg;
    msg << "[Debug] Breakpoint at 0x" << std::hex << addr << ": " << r.detail << "\n";
    appendToOutput(msg.str());
}

void Win32IDE::cmdDbgRemoveBP() {
    std::string selectedFile;
    std::string selectedLine;
    if (getSelectedListViewText(m_hwndDebuggerBreakpoints, 0, selectedFile) &&
        getSelectedListViewText(m_hwndDebuggerBreakpoints, 1, selectedLine)) {
        int line = atoi(selectedLine.c_str());
        if (!selectedFile.empty() && line > 0) {
            removeBreakpoint(selectedFile, line);
            return;
        }
    }

    // Fallback: remove by breakpoint ID from clipboard
    std::string rawId;
    if (!readClipboardText(m_hwndMain, rawId, 63)) {
        appendToOutput("[Debug] Select a breakpoint row or copy numeric breakpoint ID to clipboard.\n");
        return;
    }

    uint32_t id = 0;
    if (!tryParseUint32(rawId, id)) {
        appendToOutput("[Debug] Invalid breakpoint ID in clipboard.\n");
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().removeBreakpoint(id);
    appendToOutput("[Debug] " + std::string(r.detail) + "\n");
}

void Win32IDE::cmdDbgEnableBP() {
    std::string selectedFile;
    std::string selectedLine;
    if (getSelectedListViewText(m_hwndDebuggerBreakpoints, 0, selectedFile) &&
        getSelectedListViewText(m_hwndDebuggerBreakpoints, 1, selectedLine)) {
        int line = atoi(selectedLine.c_str());
        if (!selectedFile.empty() && line > 0) {
            setBreakpoint(selectedFile, line);
            return;
        }
    }

    // Fallback: enable by breakpoint ID from clipboard
    std::string rawId;
    if (!readClipboardText(m_hwndMain, rawId, 63)) {
        appendToOutput("[Debug] Select a breakpoint row or copy numeric breakpoint ID to clipboard.\n");
        return;
    }

    uint32_t id = 0;
    if (!tryParseUint32(rawId, id)) {
        appendToOutput("[Debug] Invalid breakpoint ID in clipboard.\n");
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().enableBreakpoint(id, true);
    appendToOutput("[Debug] " + std::string(r.detail) + "\n");
}

void Win32IDE::cmdDbgClearBPs() {
    clearAllBreakpoints();
}

void Win32IDE::cmdDbgListBPs() {
    std::string json = NativeDebuggerEngine::Instance().toJsonBreakpoints();
    appendToOutput("[Debug] Breakpoints:\n" + json + "\n");
}

// =============================================================================
//                    Command Handlers — Watches
// =============================================================================

void Win32IDE::cmdDbgAddWatch() {
    std::string expression;
    if (!promptInputUtf8(this, L"Add Watch", L"Enter watch expression:", expression, 255)) {
        if (!readClipboardText(m_hwndMain, expression, 255) || expression.empty()) {
            appendToOutput("[Debug] Add watch cancelled.\n");
            return;
        }
    }

    addWatchExpression(expression);
}

void Win32IDE::cmdDbgRemoveWatch() {
    std::string selectedExpression;
    if (getSelectedListViewText(m_hwndDebuggerWatch, 0, selectedExpression) && !selectedExpression.empty()) {
        removeWatchExpression(selectedExpression);
        return;
    }

    std::string rawInput;
    if (!readClipboardText(m_hwndMain, rawInput, 255) || rawInput.empty()) {
        appendToOutput("[Debug] Copy watch expression or numeric watch ID to clipboard first.\n");
        return;
    }

    auto& engine = NativeDebuggerEngine::Instance();
    char* endPtr = nullptr;
    unsigned long watchId = strtoul(rawInput.c_str(), &endPtr, 10);
    if (endPtr && *endPtr == '\0' && watchId > 0) {
        const auto& watches = engine.getWatches();
        for (const auto& watch : watches) {
            if (watch.id == static_cast<uint32_t>(watchId)) {
                removeWatchExpression(watch.expression);
                return;
            }
        }

        DebugResult r = engine.removeWatch(static_cast<uint32_t>(watchId));
        appendToOutput("[Debug] " + std::string(r.detail) + "\n");
        return;
    }

    removeWatchExpression(rawInput);
}

// =============================================================================
//                    Command Handlers — Inspection
// =============================================================================

void Win32IDE::cmdDbgRegisters() {
    RegisterSnapshot snap;
    DebugResult r = NativeDebuggerEngine::Instance().captureRegisters(snap);
    if (!r.success) {
        appendToOutput("[Debug] " + std::string(r.detail) + "\n");
        return;
    }
    std::string formatted = NativeDebuggerEngine::Instance().formatRegisters(snap);
    appendToOutput("[Debug] Registers:\n" + formatted);
}

void Win32IDE::cmdDbgStack() {
    std::vector<NativeStackFrame> frames;
    DebugResult r = NativeDebuggerEngine::Instance().walkStack(frames);
    if (!r.success) {
        appendToOutput("[Debug] " + std::string(r.detail) + "\n");
        return;
    }
    std::string formatted = NativeDebuggerEngine::Instance().formatStackTrace(frames);
    appendToOutput("[Debug] Call Stack:\n" + formatted);
}

void Win32IDE::cmdDbgMemory() {
    std::string input;
    if (!promptInputUtf8(this,
                         L"Inspect Memory",
                         L"Enter address (hex, e.g. 0x7FF6A0001000):",
                         input,
                         63)) {
        if (!readClipboardText(m_hwndMain, input, 63)) {
            appendToOutput("[Debug] Memory inspection cancelled.\n");
            return;
        }
    }

    uint64_t addr = 0;
    if (input.size() > 2 && input[0] == '0' && (input[1] == 'x' || input[1] == 'X')) {
        addr = strtoull(input.c_str() + 2, nullptr, 16);
    } else {
        addr = strtoull(input.c_str(), nullptr, 16);
    }

    if (addr == 0) {
        appendToOutput("[Debug] Invalid memory address.\n");
        return;
    }

    debuggerInspectMemory(addr, 256);
}

void Win32IDE::cmdDbgDisasm() {
    // Prompt for address first; fallback to clipboard or current RIP.
    uint64_t addr = 0;

    std::string input;
    if (promptInputUtf8(this,
                        L"Disassemble",
                        L"Enter start address (blank to use current RIP):",
                        input,
                        63)) {
        if (!input.empty()) {
            if (input.size() > 2 && input[0] == '0' && (input[1] == 'x' || input[1] == 'X')) {
                addr = strtoull(input.c_str() + 2, nullptr, 16);
            } else {
                addr = strtoull(input.c_str(), nullptr, 16);
            }
        }
    } else {
        std::string clip;
        if (readClipboardText(m_hwndMain, clip, 63) && !clip.empty()) {
            if (clip.size() > 2 && clip[0] == '0' && (clip[1] == 'x' || clip[1] == 'X')) {
                addr = strtoull(clip.c_str() + 2, nullptr, 16);
            } else {
                addr = strtoull(clip.c_str(), nullptr, 16);
            }
        }
    }

    if (addr == 0) {
        // Use current RIP when no address provided or parse failed.
        RegisterSnapshot snap;
        NativeDebuggerEngine::Instance().captureRegisters(snap);
        addr = snap.rip;
        if (addr == 0) {
            appendToOutput("[Debug] No address for disassembly.\n");
            return;
        } else {
            std::ostringstream hint;
            hint << "[Debug] Disassembly using current RIP: 0x" << std::hex << addr << "\n";
            appendToOutput(hint.str());
        }
    }

    std::vector<DisassembledInstruction> insts;
    DebugResult r = NativeDebuggerEngine::Instance().disassembleAt(addr, 32, insts);
    if (!r.success) {
        appendToOutput("[Debug] " + std::string(r.detail) + "\n");
        return;
    }

    std::ostringstream oss;
    for (const auto& inst : insts) {
        oss << "  0x" << std::hex << inst.address << "  " << inst.mnemonic;
        if (!inst.operands.empty()) oss << " " << inst.operands;
        if (inst.hasBreakpoint) oss << " [BP]";
        if (!inst.symbol.empty()) oss << "  ; " << inst.symbol;
        oss << "\n";
    }
    appendToOutput("[Debug] Disassembly:\n" + oss.str());
}

void Win32IDE::cmdDbgModules() {
    std::vector<DebugModule> modules;
    DebugResult r = NativeDebuggerEngine::Instance().enumerateModules(modules);
    if (!r.success) {
        appendToOutput("[Debug] " + std::string(r.detail) + "\n");
        return;
    }

    std::ostringstream oss;
    oss << "[Debug] Loaded modules (" << modules.size() << "):\n";
    for (const auto& mod : modules) {
        oss << "  0x" << std::hex << mod.baseAddress << "  "
            << std::dec << mod.size << " bytes  "
            << (mod.symbolsLoaded ? "[SYM]" : "[---]") << "  "
            << mod.name << "\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdDbgThreads() {
    std::vector<DebugThread> threads;
    DebugResult r = NativeDebuggerEngine::Instance().enumerateThreads(threads);
    if (!r.success) {
        appendToOutput("[Debug] " + std::string(r.detail) + "\n");
        return;
    }

    std::ostringstream oss;
    oss << "[Debug] Threads (" << threads.size() << "):\n";
    for (const auto& t : threads) {
        oss << "  ID=" << t.threadId;
        if (t.isCurrent) oss << " [CURRENT]";
        if (t.isSuspended) oss << " [SUSPENDED]";
        if (!t.name.empty()) oss << " (" << t.name << ")";
        oss << "\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdDbgSwitchThread() {
    std::string input;
    if (!promptInputUtf8(this, L"Switch Thread", L"Enter thread ID (decimal):", input, 63)) {
        if (!readClipboardText(m_hwndMain, input, 63)) {
            appendToOutput("[Debug] Thread switch cancelled.\n");
            return;
        }
    }

    uint32_t tid = static_cast<uint32_t>(atoi(input.c_str()));
    if (tid == 0) {
        appendToOutput("[Debug] Invalid thread ID.\n");
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().switchThread(tid);
    appendToOutput("[Debug] " + std::string(r.detail) + "\n");
    if (r.success) {
        syncDebuggerSessionStateFromEngine();
        updateVariables();
        updateCallStack();
        updateMemoryView();
    }
}

void Win32IDE::cmdDbgEvaluate() {
    std::string expression;
    if (!promptInputUtf8(this, L"Evaluate Expression", L"Enter expression to evaluate:", expression, 255)) {
        if (!readClipboardText(m_hwndMain, expression, 255)) {
            appendToOutput("[Debug] Evaluate cancelled.\n");
            return;
        }
    }

    if (expression.empty()) {
        appendToOutput("[Debug] Expression is empty.\n");
        return;
    }

    debuggerEvaluateExpression(expression);
}

void Win32IDE::cmdDbgSetRegister() {
    std::string input;
    if (!promptInputUtf8(this,
                         L"Set Register",
                         L"Enter register assignment (e.g. rax=0x1234):",
                         input,
                         127)) {
        if (!readClipboardText(m_hwndMain, input, 127)) {
            appendToOutput("[Debug] Set register cancelled.\n");
            return;
        }
    }

    // Parse "regname=value"
    size_t eqPos = input.find('=');
    if (eqPos == std::string::npos) {
        appendToOutput("[Debug] Format: regname=value (e.g. rax=0x1234)\n");
        return;
    }
    std::string regName = input.substr(0, eqPos);
    std::string valStr = input.substr(eqPos + 1);

    uint64_t value = 0;
    if (valStr.size() > 2 && valStr[0] == '0' && (valStr[1] == 'x' || valStr[1] == 'X')) {
        value = strtoull(valStr.c_str() + 2, nullptr, 16);
    } else {
        value = strtoull(valStr.c_str(), nullptr, 10);
    }

    DebugResult r = NativeDebuggerEngine::Instance().setRegister(regName, value);
    appendToOutput("[Debug] " + std::string(r.detail) + "\n");
}

void Win32IDE::cmdDbgSearchMemory() {
    std::string hexStr;
    if (!promptInputUtf8(this,
                         L"Search Memory",
                         L"Enter hex byte pattern (e.g. CC9090CC):",
                         hexStr,
                         255)) {
        if (!readClipboardText(m_hwndMain, hexStr, 255)) {
            appendToOutput("[Debug] Memory search cancelled.\n");
            return;
        }
    }

    if (hexStr.empty()) {
        appendToOutput("[Debug] Empty memory search pattern.\n");
        return;
    }

    // Parse hex string into bytes
    std::vector<uint8_t> pattern;
    for (size_t i = 0; i + 1 < hexStr.size(); i += 2) {
        char byte[3] = { hexStr[i], hexStr[i + 1], 0 };
        pattern.push_back(static_cast<uint8_t>(strtoul(byte, nullptr, 16)));
    }

    if (pattern.empty()) {
        appendToOutput("[Debug] Invalid hex pattern.\n");
        return;
    }

    // Search from base of first module
    std::vector<DebugModule> mods;
    NativeDebuggerEngine::Instance().enumerateModules(mods);
    uint64_t searchBase = (mods.empty()) ? 0x10000 : mods[0].baseAddress;
    uint64_t searchSize = (mods.empty()) ? 0x7FFFFFFF : mods[0].size;

    std::vector<uint64_t> matches;
    DebugResult r = NativeDebuggerEngine::Instance().searchMemory(
        searchBase, searchSize, pattern.data(), static_cast<uint32_t>(pattern.size()), matches);

    if (!r.success || matches.empty()) {
        appendToOutput("[Debug] Pattern not found.\n");
        return;
    }

    std::ostringstream oss;
    oss << "[Debug] Found " << matches.size() << " match(es):\n";
    for (auto addr : matches) {
        oss << "  0x" << std::hex << addr << "\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdDbgSymbolPath() {
    std::string symbolPath;
    if (!promptInputUtf8(this,
                         L"Set Symbol Path",
                         L"Enter symbol path (e.g. srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols):",
                         symbolPath,
                         511)) {
        if (!readClipboardText(m_hwndMain, symbolPath, 511)) {
            appendToOutput("[Debug] Symbol path update cancelled.\n");
            return;
        }
    }

    if (symbolPath.empty()) {
        appendToOutput("[Debug] Symbol path is empty.\n");
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().setSymbolPath(symbolPath);
    appendToOutput("[Debug] " + std::string(r.detail) + "\n");
}

void Win32IDE::cmdDbgStatus() {
    std::string json = NativeDebuggerEngine::Instance().toJsonStatus();
    appendToOutput("[Debug] Status:\n" + json + "\n");
}

// =============================================================================
//                    HTTP Endpoint Handlers
// =============================================================================

void Win32IDE::handleDbgStatusEndpoint(SOCKET client) {
    sendHttpJson(client, NativeDebuggerEngine::Instance().toJsonStatus());
}

void Win32IDE::handleDbgBreakpointsEndpoint(SOCKET client) {
    sendHttpJson(client, NativeDebuggerEngine::Instance().toJsonBreakpoints());
}

void Win32IDE::handleDbgRegistersEndpoint(SOCKET client) {
    sendHttpJson(client, NativeDebuggerEngine::Instance().toJsonRegisters());
}

void Win32IDE::handleDbgStackEndpoint(SOCKET client) {
    sendHttpJson(client, NativeDebuggerEngine::Instance().toJsonStack());
}

void Win32IDE::handleDbgMemoryEndpoint(SOCKET client, const std::string& body) {
    // Parse body for address and size: {"address":"0x...", "size":256}
    uint64_t addr = 0;
    uint64_t size = 256;

    // Simple parser (no json library dependency for HTTP handler)
    size_t addrPos = body.find("\"address\"");
    if (addrPos != std::string::npos) {
        size_t colonPos = body.find(':', addrPos);
        size_t quoteStart = body.find('"', colonPos + 1);
        size_t quoteEnd = body.find('"', quoteStart + 1);
        if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            std::string addrStr = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            if (addrStr.size() > 2 && addrStr[0] == '0' && addrStr[1] == 'x') {
                addr = strtoull(addrStr.c_str() + 2, nullptr, 16);
            }
        }
    }

    size_t sizePos = body.find("\"size\"");
    if (sizePos != std::string::npos) {
        size_t colonPos = body.find(':', sizePos);
        if (colonPos != std::string::npos) {
            size = strtoull(body.c_str() + colonPos + 1, nullptr, 10);
        }
    }

    if (addr == 0) {
        sendHttpError(client, "Missing or invalid address");
        return;
    }

    sendHttpJson(client, NativeDebuggerEngine::Instance().toJsonMemory(addr, size));
}

void Win32IDE::handleDbgDisasmEndpoint(SOCKET client, const std::string& body) {
    uint64_t addr = 0;
    uint32_t lines = 32;

    size_t addrPos = body.find("\"address\"");
    if (addrPos != std::string::npos) {
        size_t colonPos = body.find(':', addrPos);
        size_t quoteStart = body.find('"', colonPos + 1);
        size_t quoteEnd = body.find('"', quoteStart + 1);
        if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            std::string addrStr = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            if (addrStr.size() > 2 && addrStr[0] == '0' && addrStr[1] == 'x') {
                addr = strtoull(addrStr.c_str() + 2, nullptr, 16);
            }
        }
    }

    size_t linesPos = body.find("\"lines\"");
    if (linesPos != std::string::npos) {
        size_t colonPos = body.find(':', linesPos);
        if (colonPos != std::string::npos) {
            lines = static_cast<uint32_t>(atoi(body.c_str() + colonPos + 1));
        }
    }

    if (addr == 0) {
        // Use current RIP
        RegisterSnapshot snap;
        NativeDebuggerEngine::Instance().captureRegisters(snap);
        addr = snap.rip;
    }

    sendHttpJson(client, NativeDebuggerEngine::Instance().toJsonDisassembly(addr, lines));
}

void Win32IDE::handleDbgModulesEndpoint(SOCKET client) {
    sendHttpJson(client, NativeDebuggerEngine::Instance().toJsonModules());
}

void Win32IDE::handleDbgThreadsEndpoint(SOCKET client) {
    sendHttpJson(client, NativeDebuggerEngine::Instance().toJsonThreads());
}

void Win32IDE::handleDbgEventsEndpoint(SOCKET client) {
    sendHttpJson(client, NativeDebuggerEngine::Instance().toJsonEvents(100));
}

void Win32IDE::handleDbgWatchesEndpoint(SOCKET client) {
    sendHttpJson(client, NativeDebuggerEngine::Instance().toJsonWatches());
}

void Win32IDE::handleDbgLaunchEndpoint(SOCKET client, const std::string& body) {
    // Parse {"path":"...", "args":"...", "workingDir":"..."}
    std::string path, args, workDir;

    size_t pathPos = body.find("\"path\"");
    if (pathPos != std::string::npos) {
        size_t colonPos = body.find(':', pathPos);
        size_t qStart = body.find('"', colonPos + 1);
        size_t qEnd = body.find('"', qStart + 1);
        if (qStart != std::string::npos && qEnd != std::string::npos) {
            path = body.substr(qStart + 1, qEnd - qStart - 1);
        }
    }

    size_t argsPos = body.find("\"args\"");
    if (argsPos != std::string::npos) {
        size_t colonPos = body.find(':', argsPos);
        size_t qStart = body.find('"', colonPos + 1);
        size_t qEnd = body.find('"', qStart + 1);
        if (qStart != std::string::npos && qEnd != std::string::npos) {
            args = body.substr(qStart + 1, qEnd - qStart - 1);
        }
    }

    size_t wdPos = body.find("\"workingDir\"");
    if (wdPos != std::string::npos) {
        size_t colonPos = body.find(':', wdPos);
        size_t qStart = body.find('"', colonPos + 1);
        size_t qEnd = body.find('"', qStart + 1);
        if (qStart != std::string::npos && qEnd != std::string::npos) {
            workDir = body.substr(qStart + 1, qEnd - qStart - 1);
        }
    }

    if (path.empty()) {
        sendHttpError(client, "Missing 'path' field");
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().launchProcess(path, args, workDir);
    if (r.success) {
        setCurrentBinaryForReverseEngineering(path);
        sendHttpOk(client, r.detail);
    } else {
        sendHttpError(client, r.detail);
    }
}

void Win32IDE::handleDbgAttachEndpoint(SOCKET client, const std::string& body) {
    // Parse {"pid":1234}
    uint32_t pid = 0;
    size_t pidPos = body.find("\"pid\"");
    if (pidPos != std::string::npos) {
        size_t colonPos = body.find(':', pidPos);
        if (colonPos != std::string::npos) {
            pid = static_cast<uint32_t>(atoi(body.c_str() + colonPos + 1));
        }
    }

    if (pid == 0) {
        sendHttpError(client, "Missing or invalid 'pid' field");
        return;
    }

    DebugResult r = NativeDebuggerEngine::Instance().attachToProcess(pid);
    if (r.success) {
        const std::string& path = NativeDebuggerEngine::Instance().getTargetPath();
        if (!path.empty())
            setCurrentBinaryForReverseEngineering(path);
        sendHttpOk(client, r.detail);
    } else {
        sendHttpError(client, r.detail);
    }
}

void Win32IDE::handleDbgGoEndpoint(SOCKET client) {
    DebugResult r = NativeDebuggerEngine::Instance().go();
    if (r.success) {
        sendHttpOk(client, r.detail);
    } else {
        sendHttpError(client, r.detail);
    }
}

void Win32IDE::handlePhase12StatusEndpoint(SOCKET client) {
    std::ostringstream j;
    j << "{";
    j << "\"phase\":12,";
    j << "\"name\":\"Native Debugger Engine\",";
    j << "\"initialized\":" << (m_phase12Initialized ? "true" : "false") << ",";
    j << "\"engine\":" << NativeDebuggerEngine::Instance().toJsonStatus();
    j << "}";
    sendHttpJson(client, j.str());
}

void Win32IDE::handleReSetBinaryEndpoint(SOCKET client, const std::string& body) {
    // Parse {"path":"C:\\...\\file.exe"} — set as current RE binary for Disassemble/DumpBin/CFG etc.
    std::string path;
    size_t pathPos = body.find("\"path\"");
    if (pathPos != std::string::npos) {
        size_t colonPos = body.find(':', pathPos);
        size_t qStart = body.find('"', colonPos + 1);
        size_t qEnd = body.find('"', qStart + 1);
        if (qStart != std::string::npos && qEnd != std::string::npos)
            path = body.substr(qStart + 1, qEnd - qStart - 1);
    }
    if (path.empty()) {
        sendHttpError(client, "Missing 'path' field");
        return;
    }
    setCurrentBinaryForReverseEngineering(path);
    appendToOutput("[RE] Binary set via API: " + path + " (Reverse Engineering menu)\n", "Output", OutputSeverity::Info);
    sendHttpOk(client, "RE binary set to " + path);
}

#endif // _WIN32

