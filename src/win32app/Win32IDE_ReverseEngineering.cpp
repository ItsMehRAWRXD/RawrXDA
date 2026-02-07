// Win32IDE_ReverseEngineering.cpp - Reverse Engineering UI Integration
#include "Win32IDE.h"
#include "../reverse_engineering/RawrCodex.hpp"
#include "../reverse_engineering/RawrDumpBin.hpp"
#include "../reverse_engineering/RawrCompiler.hpp"
#include "../reverse_engineering/RawrReverseEngine.hpp"
#include <shlwapi.h>
#include <commdlg.h>
#include <sstream>

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

// Helper: Display results in output window
static void ShowOutput(HWND hwnd, const std::string& title, const std::string& content) {
    // Find or create output window
    HWND outputWnd = FindWindowExA(hwnd, NULL, "EDIT", NULL);
    if (!outputWnd) {
        MessageBoxA(hwnd, content.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    std::string fullText = "\r\n=== " + title + " ===\r\n" + content + "\r\n";
    
    int len = GetWindowTextLengthA(outputWnd);
    SendMessageA(outputWnd, EM_SETSEL, len, len);
    SendMessageA(outputWnd, EM_REPLACESEL, FALSE, (LPARAM)fullText.c_str());
    SendMessageA(outputWnd, EM_SCROLLCARET, 0, 0);
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
    
    ShowOutput(m_hwndMain, "Binary Analysis", ss.str());
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
    ShowOutput(m_hwndMain, "Disassembly", result);
}

// Menu handler for IDM_REVENG_DUMPBIN
void Win32IDE::handleReverseEngineeringDumpBin() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    std::string result = s_reDumpbin.DumpAll(s_reCurrentBinary);
    ShowOutput(m_hwndMain, "Binary Dump", result);
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
        ss << "✓ Compilation successful\n";
        ss << "  Object file: " << result.objectFile << "\n";
        ss << "  Time: " << result.compileTimeMs << " ms\n";
    } else {
        ss << "✗ Compilation failed\n";
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
    
    ShowOutput(m_hwndMain, "Compilation", ss.str());
}

// Menu handler for IDM_REVENG_COMPARE
void Win32IDE::handleReverseEngineeringCompare() {
    LOG_FUNCTION();
    
    std::string path1 = OpenBinaryDialog(m_hwndMain);
    if (path1.empty()) return;
    
    std::string path2 = OpenBinaryDialog(m_hwndMain);
    if (path2.empty()) return;
    
    std::string result = s_reDumpbin.CompareBinaries(path1, path2);
    ShowOutput(m_hwndMain, "Binary Comparison", result);
}

// Menu handler for IDM_REVENG_DETECT_VULNS
void Win32IDE::handleReverseEngineeringDetectVulns() {
    LOG_FUNCTION();
    
    if (s_reCurrentBinary.empty()) {
        MessageBoxA(m_hwndMain, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    std::string result = s_reDumpbin.DumpVulnerabilities(s_reCurrentBinary);
    ShowOutput(m_hwndMain, "Vulnerability Detection", result);
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
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DISASM, "&Disassemble\tCtrl+D");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DUMPBIN, "Dump &Binary\tCtrl+B");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_COMPILE, "&Compile Source\tCtrl+F7");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_CFG, "Control &Flow Graph");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_FUNCTIONS, "Recover F&unctions");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DEMANGLE, "De&mangle Symbols");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_SSA, "&SSA Lifting\tCtrl+Shift+S");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_RECURSIVE_DISASM, "R&ecursive Disassembly\tCtrl+Shift+R");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_COMPARE, "C&ompare Binaries");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DETECT_VULNS, "Detect &Vulnerabilities");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_EXPORT_IDA, "Export to &IDA Pro");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_EXPORT_GHIDRA, "Export to &Ghidra");
    
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
    ShowOutput(m_hwndMain, "Control Flow Graph", result);
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
    ShowOutput(m_hwndMain, "Function Recovery", result);
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
    ShowOutput(m_hwndMain, "Symbol Demangling", result);
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
    ShowOutput(m_hwndMain, "SSA Lifting", result);
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
    ShowOutput(m_hwndMain, "Recursive Descent Disassembly", result);
}
