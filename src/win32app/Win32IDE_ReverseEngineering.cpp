// Win32IDE_ReverseEngineering.cpp - Reverse Engineering UI Integration
#include "Win32IDE.h"
#include "../core/native_debugger_engine.h"
#include "../core/native_debugger_types.h"
#include "../reverse_engineering/RawrCodex.hpp"
#include "../reverse_engineering/RawrDumpBin.hpp"
#include "../reverse_engineering/RawrCompiler.hpp"
#include "../reverse_engineering/RawrReverseEngine.hpp"
#include <shlwapi.h>
#include <commdlg.h>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comdlg32.lib")

namespace RawrXD_RE_Internals {

// Global instances for RE analysis (prefixed to avoid collision with codex_ultimate.h's s_reCodex)
static RawrXD::ReverseEngineering::RawrCodex s_reCodex;
static RawrXD::ReverseEngineering::RawrDumpBin s_reDumpbin;
static RawrXD::ReverseEngineering::RawrCompiler s_reCompiler;
static RawrXD::ReverseEngineering::RawrReverseEngine s_reEngine;
static std::string s_reCurrentBinary;

// Helper: Open file dialog
static std::string OpenBinaryDialog(HWND hwnd) {
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Executable Files\0*.exe;*.dll;*.sys;*.obj;*.o;*.a;*.so\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Select Binary for Analysis";
    
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
    return "";
}

} // namespace RawrXD_RE_Internals

using namespace RawrXD_RE_Internals;

// Menu handler for IDM_REVENG_ANALYZE
void Win32IDE::handleReverseEngineeringAnalyze() {
    LOG_FUNCTION();
    
    std::string path = OpenBinaryDialog(m_hwndMain);
    if (path.empty()) return;
    
    if (!s_reCodex.LoadBinary(path)) {
        MessageBoxA(m_hwndMain, "Failed to load binary", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    s_reCurrentBinary = path;

    std::stringstream ss;
    ss << "Binary: " << path << "\n\n";
    
    auto sections = s_reCodex.GetSections();
    ss << "Sections: " << sections.size() << "\n";
    for (const auto& sec : sections) {
        ss << "  " << sec.name 
           << " (VA: 0x" << std::hex << sec.virtualAddress 
           << ", Size: 0x" << sec.virtualSize << ")\n" << std::dec;
    }
    
    auto imports = s_reCodex.GetImports();
    ss << "\nImports: " << imports.size() << "\n";
    for (size_t i = 0; i < std::min<size_t>(10, imports.size()); ++i) {
        ss << "  " << imports[i].moduleName << "!" << imports[i].functionName << "\n";
    }
    if (imports.size() > 10) ss << "  ... and " << (imports.size() - 10) << " more\n";
    
    auto exports = s_reCodex.GetExports();
    ss << "\nExports: " << exports.size() << "\n";
    for (size_t i = 0; i < std::min<size_t>(10, exports.size()); ++i) {
        ss << "  " << exports[i].name << " @ 0x" << std::hex << exports[i].address << "\n" << std::dec;
    }
    if (exports.size() > 10) ss << "  ... and " << (exports.size() - 10) << " more\n";
    
    auto strings = s_reCodex.ExtractStrings(8);
    ss << "\nStrings (>= 8 chars): " << strings.size() << "\n";
    for (size_t i = 0; i < std::min<size_t>(10, strings.size()); ++i) {
        ss << "  " << strings[i].substr(0, 60);
        if (strings[i].length() > 60) ss << "...";
        ss << "\n";
    }
    if (strings.size() > 10) ss << "  ... and " << (strings.size() - 10) << " more\n";
    
    appendToOutput("\n=== Binary Analysis ===\n\n" + ss.str(), "Output", OutputSeverity::Info);
}

void Win32IDE::setCurrentBinaryForReverseEngineering(const std::string& path) {
    s_reCurrentBinary = path;
}

void Win32IDE::handleReverseEngineeringSetBinaryFromActive() {
    if (m_currentFile.empty()) {
        MessageBoxA(m_hwndMain, "No active document. Open an .exe, .dll, or .obj file first.", "RE", MB_OK | MB_ICONINFORMATION);
        return;
    }
    const std::string& path = m_currentFile;
    size_t dot = path.find_last_of(".\\/");
    if (dot == std::string::npos || path[dot] != '.') {
        MessageBoxA(m_hwndMain, "Active document is not a binary. Open an .exe, .dll, or .obj file.", "RE", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::string ext = path.substr(dot + 1);
    if (ext != "exe" && ext != "dll" && ext != "obj" && ext != "o" && ext != "so") {
        MessageBoxA(m_hwndMain, "Active document is not a binary. Open an .exe, .dll, or .obj file.", "RE", MB_OK | MB_ICONINFORMATION);
        return;
    }
    s_reCurrentBinary = path;
    appendToOutput("[RE] Current binary set to: " + path + " (Disassemble, DumpBin, CFG, etc. will use this.)\n", "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringSetBinaryFromDebugTarget() {
    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    if (!engine.isInitialized()) {
        MessageBoxA(m_hwndMain, "Debugger not initialized. Launch or attach to a target first.", "RE", MB_OK | MB_ICONINFORMATION);
        return;
    }
    const std::string& path = engine.getTargetPath();
    if (path.empty()) {
        MessageBoxA(m_hwndMain, "No debug target path. Launch or attach to a process first.", "RE", MB_OK | MB_ICONINFORMATION);
        return;
    }
    setCurrentBinaryForReverseEngineering(path);
    appendToOutput("[RE] Binary set from debug target: " + path + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringSetBinaryFromBuildOutput() {
    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Executables\0*.exe;*.dll\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Select built binary (e.g. build\\Debug\\*.exe)";
    if (!GetOpenFileNameA(&ofn)) return;
    setCurrentBinaryForReverseEngineering(path);
    appendToOutput("[RE] Binary set from build output: " + std::string(path) + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringDisassembleAtRIP() {
    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    if (!engine.isInitialized()) {
        MessageBoxA(m_hwndMain, "Debugger not initialized. Launch and break the target first.", "RE", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (engine.getState() != RawrXD::Debugger::DebugSessionState::Broken) {
        MessageBoxA(m_hwndMain, "Target must be paused (breakpoint or pause). Use Break or set a breakpoint.", "RE", MB_OK | MB_ICONINFORMATION);
        return;
    }
    RawrXD::Debugger::RegisterSnapshot snap;
    RawrXD::Debugger::DebugResult r = engine.captureRegisters(snap);
    if (!r.success) {
        appendToOutput(std::string("[RE] Failed to capture registers: ") + r.detail + "\n", "Output", OutputSeverity::Error);
        return;
    }
    uint64_t rip = snap.rip;
    std::vector<RawrXD::Debugger::DisassembledInstruction> instructions;
    r = engine.disassembleAt(rip, 30, instructions);
    if (!r.success) {
        appendToOutput(std::string("[RE] Disassembly failed: ") + r.detail + "\n", "Output", OutputSeverity::Error);
        return;
    }
    const std::string& targetPath = engine.getTargetPath();
    if (!targetPath.empty())
        setCurrentBinaryForReverseEngineering(targetPath);
    std::ostringstream ss;
    ss << "\n=== Disassembly at RIP 0x" << std::hex << rip << " ===\n\n";
    for (const auto& inst : instructions) {
        ss << "  0x" << std::hex << inst.address << "  " << inst.fullText;
        if (inst.isCurrentIP) ss << "  <-- RIP";
        ss << "\n";
    }
    ss << "\n";
    appendToOutput(ss.str(), "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_DISASM
void Win32IDE::handleReverseEngineeringDisassemble() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    // Simple dialog for address input (you can enhance this with a proper dialog)
    char addrStr[64] = "0x1000";
    // For production, create a proper dialog box
    
    uint64_t addr = 0x1000;
    size_t count = 50;
    
    std::string result = s_reDumpbin.DumpDisassembly(s_reCurrentBinary, addr, count);
    appendToOutput("\n=== Disassembly ===\n\n" + result, "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_DUMPBIN
void Win32IDE::handleReverseEngineeringDumpBin() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    std::string result = s_reDumpbin.DumpAll(s_reCurrentBinary);
    appendToOutput("\n=== Binary Dump ===\n\n" + result, "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_COMPILE
void Win32IDE::handleReverseEngineeringCompile() {
    LOG_FUNCTION();
    
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Source Files\0*.c;*.cpp;*.asm\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Select Source File to Compile";
    
    if (!GetOpenFileNameA(&ofn)) return;
    
    RawrXD::ReverseEngineering::CompilerOptions opts;
    opts.optimizationLevel = 2;
    opts.targetArch = "x64";
    opts.debugInfo = true;
    
    s_reCompiler.SetOptions(opts);
    
    std::stringstream ss;
    ss << "Compiling: " << filename << "\n\n";
    
    auto result = s_reCompiler.CompileSource(filename);
    
    if (result.success) {
        ss << "Compilation successful\n";
        ss << "  Object file: " << result.objectFile << "\n";
        ss << "  Time: " << result.compileTimeMs << " ms\n";
        setCurrentBinaryForReverseEngineering(result.objectFile);
        ss << "  RE binary set to object file (Disassemble/DumpBin/CFG available).\n";
    } else {
        ss << "Compilation failed\n";
        for (const auto& err : result.errors) {
            ss << "  Error: " << err << "\n";
        }
    }
    
    if (!result.warnings.empty()) {
        ss << "\nWarnings:\n";
        for (const auto& warn : result.warnings) {
            ss << "  " << warn << "\n";
        }
    }
    
    appendToOutput("\n=== Compilation ===\n\n" + ss.str(), "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_COMPARE
void Win32IDE::handleReverseEngineeringCompare() {
    LOG_FUNCTION();
    
    std::string path1 = OpenBinaryDialog(m_hwndMain);
    if (path1.empty()) return;
    
    std::string path2 = OpenBinaryDialog(m_hwndMain);
    if (path2.empty()) return;
    
    std::string result = s_reDumpbin.CompareBinaries(path1, path2);
    appendToOutput("\n=== Binary Comparison ===\n\n" + result, "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_DETECT_VULNS
void Win32IDE::handleReverseEngineeringDetectVulns() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    std::string result = s_reDumpbin.DumpVulnerabilities(s_reCurrentBinary);
    appendToOutput("\n=== Vulnerability Detection ===\n\n" + result, "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_EXPORT_IDA
void Win32IDE::handleReverseEngineeringExportIDA() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Python Scripts\0*.py\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Export IDA Script";
    ofn.lpstrDefExt = "py";
    
    if (!GetSaveFileNameA(&ofn)) return;
    
    std::string script = s_reCodex.ExportToIDA();
    
    FILE* f = fopen(filename, "w");
    if (f) {
        fwrite(script.c_str(), 1, script.length(), f);
        fclose(f);
        MessageBoxA(m_hwndMain, "IDA script exported successfully", "Success", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxA(m_hwndMain, "Failed to save script", "Error", MB_OK | MB_ICONERROR);
    }
}

// Menu handler for IDM_REVENG_EXPORT_GHIDRA
void Win32IDE::handleReverseEngineeringExportGhidra() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Python Scripts\0*.py\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Export Ghidra Script";
    ofn.lpstrDefExt = "py";
    
    if (!GetSaveFileNameA(&ofn)) return;
    
    std::string script = s_reCodex.ExportToGhidra();
    
    FILE* f = fopen(filename, "w");
    if (f) {
        fwrite(script.c_str(), 1, script.length(), f);
        fclose(f);
        MessageBoxA(m_hwndMain, "Ghidra script exported successfully", "Success", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxA(m_hwndMain, "Failed to save script", "Error", MB_OK | MB_ICONERROR);
    }
}

// Create Reverse Engineering menu
HMENU Win32IDE::createReverseEngineeringMenu() {
    HMENU menu = CreateMenu();
    
    AppendMenuA(menu, MF_STRING, IDM_REVENG_ANALYZE, "&Analyze Binary\tCtrl+R");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_SET_BINARY_FROM_ACTIVE, "Set binary from &active document");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_SET_BINARY_FROM_DEBUG_TARGET, "Set binary from &debug target");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_SET_BINARY_FROM_BUILD_OUTPUT, "Set binary from &build output...");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DISASM_AT_RIP, "Disassemble at current &RIP (debugger)");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DISASM, "&Disassemble\tCtrl+D");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DUMPBIN, "Dump &Binary\tCtrl+B");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DECOMPILER_VIEW, "Decompiler &View\tCtrl+Shift+D");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_COMPILE, "&Compile Source\tCtrl+F7");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_CFG, "Control &Flow Graph");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_FUNCTIONS, "Recover F&unctions");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DEMANGLE, "De&mangle Symbols");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_SSA, "&SSA Lifting\tCtrl+Shift+S");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_RECURSIVE_DISASM, "R&ecursive Disassembly\tCtrl+Shift+R");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_TYPE_RECOVERY, "&Type Recovery\tCtrl+Shift+T");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DATA_FLOW, "Data &Flow Analysis");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_COMPARE, "C&ompare Binaries");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DETECT_VULNS, "Detect &Vulnerabilities");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_EXPORT_IDA, "Export to &IDA Pro");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_EXPORT_GHIDRA, "Export to &Ghidra");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_LICENSE_INFO, "&License Info");
    
    return menu;
}

// Menu handler for IDM_REVENG_CFG
void Win32IDE::handleReverseEngineeringCFG() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    // Get entry point from the codex analysis
    auto symbols = s_reCodex.GetSymbols();
    uint64_t entryAddr = 0;
    for (const auto& sym : symbols) {
        if (sym.name == "_start" || sym.name == "main" || sym.name == "WinMain" ||
            sym.name == "_main" || sym.name == "wmain" || sym.name == "wWinMain" ||
            sym.name == "entry" || sym.name == "EntryPoint") {
            entryAddr = sym.address;
            break;
        }
    }
    if (entryAddr == 0 && !symbols.empty()) {
        entryAddr = symbols[0].address; // fallback to first symbol
    }
    
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string result = s_reEngine.AnalyzeCFG(entryAddr);
    appendToOutput("\n=== Control Flow Graph ===\n\n" + result, "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_FUNCTIONS
void Win32IDE::handleReverseEngineeringFunctions() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string result = s_reEngine.RecoverFunctions();
    appendToOutput("\n=== Function Recovery ===\n\n" + result, "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_DEMANGLE
void Win32IDE::handleReverseEngineeringDemangle() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string result = s_reEngine.DemangleAll();
    appendToOutput("\n=== Symbol Demangling ===\n\n" + result, "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_SSA
void Win32IDE::handleReverseEngineeringSSA() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    // Determine function entry point for SSA lifting
    auto symbols = s_reCodex.GetSymbols();
    uint64_t entryAddr = 0;
    for (const auto& sym : symbols) {
        if (sym.name == "_start" || sym.name == "main" || sym.name == "WinMain" ||
            sym.name == "_main" || sym.name == "wmain" || sym.name == "wWinMain" ||
            sym.name == "entry" || sym.name == "EntryPoint") {
            entryAddr = sym.address;
            break;
        }
    }
    if (entryAddr == 0 && !symbols.empty()) {
        entryAddr = symbols[0].address;
    }
    
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string result = s_reEngine.LiftSSA(entryAddr);
    appendToOutput("\n=== SSA Lifting ===\n\n" + result, "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_RECURSIVE_DISASM
void Win32IDE::handleReverseEngineeringRecursiveDisasm() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    // Determine entry point for recursive descent
    auto symbols = s_reCodex.GetSymbols();
    uint64_t entryAddr = 0;
    for (const auto& sym : symbols) {
        if (sym.name == "_start" || sym.name == "main" || sym.name == "WinMain" ||
            sym.name == "_main" || sym.name == "wmain" || sym.name == "wWinMain" ||
            sym.name == "entry" || sym.name == "EntryPoint") {
            entryAddr = sym.address;
            break;
        }
    }
    if (entryAddr == 0 && !symbols.empty()) {
        entryAddr = symbols[0].address;
    }
    
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string result = s_reEngine.RecursiveDisassemble(entryAddr);
    appendToOutput("\n=== Recursive Descent Disassembly ===\n\n" + result, "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_TYPE_RECOVERY
void Win32IDE::handleReverseEngineeringTypeRecovery() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    auto symbols = s_reCodex.GetSymbols();
    uint64_t entryAddr = 0;
    for (const auto& sym : symbols) {
        if (sym.name == "_start" || sym.name == "main" || sym.name == "WinMain" ||
            sym.name == "_main" || sym.name == "wmain" || sym.name == "wWinMain" ||
            sym.name == "entry" || sym.name == "EntryPoint") {
            entryAddr = sym.address;
            break;
        }
    }
    if (entryAddr == 0 && !symbols.empty()) {
        entryAddr = symbols[0].address;
    }
    
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string result = s_reEngine.RecoverTypes(entryAddr);
    appendToOutput("\n=== Type Recovery ===\n\n" + result, "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_DATA_FLOW
void Win32IDE::handleReverseEngineeringDataFlow() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    auto symbols = s_reCodex.GetSymbols();
    uint64_t entryAddr = 0;
    for (const auto& sym : symbols) {
        if (sym.name == "_start" || sym.name == "main" || sym.name == "WinMain" ||
            sym.name == "_main" || sym.name == "wmain" || sym.name == "wWinMain" ||
            sym.name == "entry" || sym.name == "EntryPoint") {
            entryAddr = sym.address;
            break;
        }
    }
    if (entryAddr == 0 && !symbols.empty()) {
        entryAddr = symbols[0].address;
    }
    
    // RecoverTypes includes def-use chain output
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string result = s_reEngine.RecoverTypes(entryAddr);
    appendToOutput("\n=== Data Flow Analysis ===\n\n" + result, "Output", OutputSeverity::Info);
}

// Menu handler for IDM_REVENG_LICENSE_INFO
void Win32IDE::handleReverseEngineeringLicenseInfo() {
    LOG_FUNCTION();
    
    s_reEngine.LoadBinary(s_reCurrentBinary.empty() ? "" : s_reCurrentBinary);
    std::string result = s_reEngine.GetLicenseInfo();
    appendToOutput("\n=== License Info ===\n\n" + result, "Output", OutputSeverity::Info);
}

// ============================================================================
// Menu handler for IDM_REVENG_DECOMPILER_VIEW
// Opens the Direct2D split decompiler/disassembly view with:
//   - Syntax-colored pseudocode (left pane)
//   - Address-tagged disassembly (right pane)
//   - Synchronized selection between panes
//   - Right-click variable rename with SSA propagation
// ============================================================================
void Win32IDE::handleReverseEngineeringDecompilerView() {
    LOG_FUNCTION();

    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }

    // Load the binary into the full engine
    s_reEngine.LoadBinary(s_reCurrentBinary);

    // Determine entry point for analysis
    auto symbols = s_reCodex.GetSymbols();
    uint64_t entryAddr = 0;
    for (const auto& sym : symbols) {
        if (sym.name == "_start" || sym.name == "main" || sym.name == "WinMain" ||
            sym.name == "_main" || sym.name == "wmain" || sym.name == "wWinMain" ||
            sym.name == "entry" || sym.name == "EntryPoint") {
            entryAddr = sym.address;
            break;
        }
    }
    if (entryAddr == 0 && !symbols.empty()) {
        entryAddr = symbols[0].address;
    }

    // Generate decompiled pseudocode via type recovery + SSA lifting
    // This produces C-like output with address annotations
    std::string decompCode;
    {
        std::ostringstream oss;
        oss << "// Decompiled from: " << s_reCurrentBinary << "\n";
        oss << "// Entry point: 0x" << std::hex << entryAddr << "\n\n";

        // Attempt SSA lifting to generate pseudocode
        auto ssaResult = s_reCodex.LiftToSSA(entryAddr);
        if (ssaResult.success) {
            // Generate C-like pseudocode from SSA
            oss << "// @0x" << std::hex << entryAddr << "\n";
            oss << "int __cdecl entry_function(void) {\n";

            // Declare SSA variables
            for (int v = 0; v < (int)ssaResult.totalVars && v < 32; ++v) {
                oss << "    int var_rax_" << std::dec << v << ";  // SSA v" << v << "\n";
            }
            oss << "\n";

            // Emit pseudocode from SSA instructions
            static const char* ssaOpNames[] = {
                "assign", "add", "sub", "mul", "div", "and", "or", "xor",
                "shl", "shr", "sar", "not", "neg", "load", "store", "call",
                "ret", "cmp", "test", "branch", "jmp", "phi", "lea", "unknown"
            };
            uint32_t currentBB = UINT32_MAX;

            for (const auto& si : ssaResult.instructions) {
                if (si.isDeadCode) continue;

                if (si.bbIndex != currentBB) {
                    currentBB = si.bbIndex;
                    oss << "\n    // --- Block " << std::dec << currentBB << " ---\n";
                }

                uint32_t opIdx = static_cast<uint32_t>(si.op);
                const char* opName = (opIdx < 24) ? ssaOpNames[opIdx] : "unknown";

                oss << "    ";
                // Emit address annotation
                oss << "// @0x" << std::hex << si.origAddress << "\n    ";

                if (si.dstVarId >= 0) {
                    oss << "var_rax_" << std::dec << si.dstVarId << " = ";
                }

                // Generate C-like expression
                switch (si.op) {
                case RawrXD::ReverseEngineering::SSAOpType::Assign:
                    if (si.src1VarId >= 0)
                        oss << "var_rax_" << si.src1VarId;
                    else
                        oss << "0";
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Add:
                    oss << "var_rax_" << si.src1VarId << " + var_rax_" << si.src2VarId;
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Sub:
                    oss << "var_rax_" << si.src1VarId << " - var_rax_" << si.src2VarId;
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Load:
                    if (si.src1VarId >= 0)
                        oss << "*(int*)var_rax_" << si.src1VarId;
                    else
                        oss << "*(int*)0x0";
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Store:
                    if (si.src1VarId >= 0 && si.src2VarId >= 0)
                        oss << "*(int*)var_rax_" << si.src1VarId << " = var_rax_" << si.src2VarId;
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Call:
                    oss << "func_0x" << std::hex << si.callTarget << std::dec << "(";
                    if (si.src1VarId >= 0) oss << "var_rax_" << si.src1VarId;
                    oss << ")";
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Ret:
                    oss << "return";
                    if (si.src1VarId >= 0) oss << " var_rax_" << si.src1VarId;
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Cmp:
                    oss << "var_rax_" << si.src1VarId << " == var_rax_" << si.src2VarId;
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Branch:
                    oss << "if (var_rax_" << si.src1VarId << ") goto 0x"
                        << std::hex << si.branchTarget << std::dec;
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Lea:
                    oss << "&var_rax_" << si.src1VarId;
                    break;
                default:
                    oss << opName << "(";
                    if (si.src1VarId >= 0) oss << "var_rax_" << si.src1VarId;
                    if (si.src2VarId >= 0) oss << ", var_rax_" << si.src2VarId;
                    oss << ")";
                    break;
                }
                oss << ";  // " << opName << "\n";
            }

            oss << "\n    return 0;\n";
            oss << "}\n";
        } else {
            // Fallback: generate minimal pseudocode from the disassembly
            oss << "// SSA lifting unavailable — generating basic pseudocode\n\n";
            oss << "// @0x" << std::hex << entryAddr << "\n";
            oss << "void entry_function(void) {\n";
            oss << "    // Binary analysis in progress...\n";
            oss << "    // Load binary with 'Analyze Binary' for full decompilation\n";
            oss << "}\n";
        }

        decompCode = oss.str();
    }

    // Generate disassembly output
    std::string disasmText = s_reEngine.RecursiveDisassemble(entryAddr);

    // Extract just the binary filename for display
    std::string displayName = s_reCurrentBinary;
    size_t lastSlash = displayName.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        displayName = displayName.substr(lastSlash + 1);
    }

    // Show the Direct2D decompiler view
    showDecompilerView(decompCode, disasmText, displayName);
}
