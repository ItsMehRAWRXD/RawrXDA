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
#include <vector>
#include <algorithm>
#include <cwchar>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comdlg32.lib")

namespace RawrXD_RE_Internals {

// Global instances for RE analysis (prefixed to avoid collision with codex_ultimate.h's s_reCodex)
static RawrXD::ReverseEngineering::RawrCodex s_reCodex;
static RawrXD::ReverseEngineering::RawrDumpBin s_reDumpbin;
static RawrXD::ReverseEngineering::RawrCompiler s_reCompiler;
static RawrXD::ReverseEngineering::RawrReverseEngine s_reEngine;
static std::string s_reCurrentBinary;

static bool TryGetPEImageBaseAndSize(const std::string& path, uint64_t& imageBase, uint32_t& sizeOfImage) {
    imageBase = 0;
    sizeOfImage = 0;

    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap) {
        CloseHandle(hFile);
        return false;
    }

    const uint8_t* base = (const uint8_t*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    bool ok = false;
    __try {
        if (*(const uint16_t*)(base + 0x00) != 0x5A4D) __leave; // MZ
        uint32_t e_lfanew = *(const uint32_t*)(base + 0x3C);
        const uint8_t* nt = base + e_lfanew;
        if (*(const uint32_t*)(nt + 0x00) != 0x00004550) __leave; // PE\0\0
        uint16_t optSize = *(const uint16_t*)(nt + 0x14);
        const uint8_t* opt = nt + 0x18;
        if (optSize < 0x40) __leave;
        uint16_t magic = *(const uint16_t*)(opt + 0x00);
        if (magic == 0x20B) {
            imageBase = *(const uint64_t*)(opt + 0x18);
            sizeOfImage = *(const uint32_t*)(opt + 0x38);
            ok = (imageBase != 0 && sizeOfImage != 0);
        } else if (magic == 0x10B) {
            imageBase = *(const uint32_t*)(opt + 0x1C);
            sizeOfImage = *(const uint32_t*)(opt + 0x38);
            ok = (imageBase != 0 && sizeOfImage != 0);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return ok;
}

static uint64_t TryGetPEEntryPointRVA(const std::string& path) {
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap) {
        CloseHandle(hFile);
        return 0;
    }

    const uint8_t* base = (const uint8_t*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return 0;
    }

    uint64_t epRva = 0;
    __try {
        if (*(const uint16_t*)(base + 0x00) != 0x5A4D) __leave; // MZ
        uint32_t e_lfanew = *(const uint32_t*)(base + 0x3C);
        const uint8_t* nt = base + e_lfanew;
        if (*(const uint32_t*)(nt + 0x00) != 0x00004550) __leave; // PE\0\0
        uint16_t optSize = *(const uint16_t*)(nt + 0x14);
        const uint8_t* opt = nt + 0x18;
        if (optSize < 0x18) __leave;
        uint16_t magic = *(const uint16_t*)(opt + 0x00);
        if (magic != 0x10B && magic != 0x20B) __leave;
        epRva = *(const uint32_t*)(opt + 0x10); // AddressOfEntryPoint
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        epRva = 0;
    }

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return epRva;
}

static bool ParseHexU64W(const wchar_t* s, uint64_t& out) {
    out = 0;
    if (!s) return false;
    while (*s == L' ' || *s == L'\t' || *s == L'\r' || *s == L'\n') ++s;
    if (*s == 0) return false;
    wchar_t* end = nullptr;
    unsigned long long v = wcstoull(s, &end, 0);
    if (end == s) return false;
    out = (uint64_t)v;
    return true;
}

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

    // Prefer the real PE entrypoint RVA when available (more reliable than symbols).
    entryAddr = TryGetPEEntryPointRVA(s_reCurrentBinary);

    // Advanced mode: hold SHIFT when invoking the decompiler view to jump to any function.
    // Accepts VA (ImageBase+RVA) or plain RVA. We normalize VA->RVA when possible.
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        wchar_t buf[64] = {};
        if (!DialogBoxWithInput(L"Decompiler View Target",
                                L"Function address (hex). VA or RVA. Blank = entrypoint.",
                                buf, _countof(buf))) {
            return;
        }
        uint64_t userAddr = 0;
        if (ParseHexU64W(buf, userAddr)) {
            uint64_t imageBase = 0;
            uint32_t sizeOfImage = 0;
            if (TryGetPEImageBaseAndSize(s_reCurrentBinary, imageBase, sizeOfImage) &&
                userAddr >= imageBase && userAddr < (imageBase + (uint64_t)sizeOfImage)) {
                entryAddr = userAddr - imageBase; // VA -> RVA
            } else {
                entryAddr = userAddr; // treat as RVA (or non-PE absolute addressing)
            }
        }
    }

    for (const auto& sym : symbols) {
        if (sym.name == "_start" || sym.name == "main" || sym.name == "WinMain" ||
            sym.name == "_main" || sym.name == "wmain" || sym.name == "wWinMain" ||
            sym.name == "entry" || sym.name == "EntryPoint") {
            if (entryAddr == 0) entryAddr = sym.address;
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
            // Attempt type recovery to produce typed SSA locals.
            // If type recovery fails, we still emit readable pseudocode with defaults.
            std::vector<std::string> varTypes;
            std::vector<uint32_t> varTypeConfidence;
            varTypes.resize(ssaResult.totalVars, "uint64_t");
            varTypeConfidence.resize(ssaResult.totalVars, 0);
            {
                auto typeResult = s_reCodex.RecoverTypes(entryAddr);
                if (typeResult.success) {
                    for (const auto& ti : typeResult.types) {
                        if (ti.ssaVarId >= ssaResult.totalVars) continue;

                        std::string t = ti.typeName.empty() ? "uint64_t" : ti.typeName;
                        // Normalize some internal tags into C-ish types.
                        switch (ti.baseType) {
                            case RawrXD::ReverseEngineering::RecoveredType::Pointer:
                            case RawrXD::ReverseEngineering::RecoveredType::DataPtr:
                            case RawrXD::ReverseEngineering::RecoveredType::StructPtr:
                            case RawrXD::ReverseEngineering::RecoveredType::ArrayPtr:
                            case RawrXD::ReverseEngineering::RecoveredType::StringPtr:
                                t = "void*";
                                break;
                            case RawrXD::ReverseEngineering::RecoveredType::CodePtr:
                                t = "void(*)()";
                                break;
                            default:
                                break;
                        }

                        // Keep widths sane if the inference didn't set it.
                        if (ti.typeWidth == 4 && t == "uint64_t") t = "uint32_t";
                        if (ti.typeWidth == 2 && t == "uint64_t") t = "uint16_t";
                        if (ti.typeWidth == 1 && t == "uint64_t") t = "uint8_t";

                        varTypes[ti.ssaVarId] = t;
                        varTypeConfidence[ti.ssaVarId] = static_cast<uint32_t>(ti.confidence);
                    }
                }
            }

            // Generate C-like pseudocode from SSA
            oss << "// @0x" << std::hex << entryAddr << "\n";
            oss << "int __cdecl entry_function(void) {\n";

            // Declare SSA variables
            int maxDecl = (int)ssaResult.totalVars;
            if (maxDecl > 256) maxDecl = 256; // avoid generating absurdly large local lists
            for (int v = 0; v < maxDecl; ++v) {
                oss << "    " << varTypes[v] << " v" << std::dec << v << ";";
                if (varTypeConfidence[v] != 0) {
                    oss << "  // SSA v" << v << ", conf=" << varTypeConfidence[v] << "%";
                } else {
                    oss << "  // SSA v" << v;
                }
                oss << "\n";
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
                    oss << "v" << std::dec << si.dstVarId << " = ";
                }

                // Generate C-like expression
                switch (si.op) {
                case RawrXD::ReverseEngineering::SSAOpType::Assign:
                    if (si.src1VarId >= 0)
                        oss << "v" << si.src1VarId;
                    else
                        oss << "0";
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Add:
                    oss << "v" << si.src1VarId << " + v" << si.src2VarId;
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Sub:
                    oss << "v" << si.src1VarId << " - v" << si.src2VarId;
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Load:
                    if (si.src1VarId >= 0)
                        oss << "*(uint64_t*)v" << si.src1VarId;
                    else
                        oss << "*(uint64_t*)0x0";
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Store:
                    if (si.src1VarId >= 0 && si.src2VarId >= 0)
                        oss << "*(uint64_t*)v" << si.src1VarId << " = v" << si.src2VarId;
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Call:
                    oss << "func_0x" << std::hex << si.callTarget << std::dec << "(";
                    if (si.src1VarId >= 0) oss << "v" << si.src1VarId;
                    oss << ")";
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Ret:
                    oss << "return";
                    if (si.src1VarId >= 0) oss << " v" << si.src1VarId;
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Cmp:
                    oss << "v" << si.src1VarId << " == v" << si.src2VarId;
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Branch:
                    oss << "if (v" << si.src1VarId << ") goto 0x"
                        << std::hex << si.branchTarget << std::dec;
                    break;
                case RawrXD::ReverseEngineering::SSAOpType::Lea:
                    oss << "&v" << si.src1VarId;
                    break;
                default:
                    oss << opName << "(";
                    if (si.src1VarId >= 0) oss << "v" << si.src1VarId;
                    if (si.src2VarId >= 0) oss << ", v" << si.src2VarId;
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
