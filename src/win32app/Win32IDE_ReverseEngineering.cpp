// Win32IDE_ReverseEngineering.cpp - Reverse Engineering UI Integration
#include "Win32IDE.h"
#include "../reverse_engineering/RawrCodex.hpp"
#include "../reverse_engineering/RawrDumpBin.hpp"
#include "../reverse_engineering/RawrCompiler.hpp"
#include <shlwapi.h>
#include <commdlg.h>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comdlg32.lib")

namespace RawrXD {

// Global instances
static ReverseEngineering::RawrCodex g_codex;
static ReverseEngineering::RawrDumpBin g_dumpbin;
static ReverseEngineering::RawrCompiler g_compiler;
static std::string g_currentBinary;

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

// Menu handler for IDM_REVENG_ANALYZE
void Win32IDE::handleReverseEngineeringAnalyze() {
    LOG_FUNCTION();
    
    std::string path = OpenBinaryDialog(m_hwnd);
    if (path.empty()) return;
    
    if (!g_codex.LoadBinary(path)) {
        MessageBoxA(m_hwnd, "Failed to load binary", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    g_currentBinary = path;
    
    std::stringstream ss;
    ss << "Binary: " << path << "\n\n";
    
    auto sections = g_codex.GetSections();
    ss << "Sections: " << sections.size() << "\n";
    for (const auto& sec : sections) {
        ss << "  " << sec.name 
           << " (VA: 0x" << std::hex << sec.virtualAddress 
           << ", Size: 0x" << sec.size << ")\n" << std::dec;
    }
    
    auto imports = g_codex.GetImports();
    ss << "\nImports: " << imports.size() << "\n";
    for (size_t i = 0; i < std::min<size_t>(10, imports.size()); ++i) {
        ss << "  " << imports[i].dllName << "!" << imports[i].functionName << "\n";
    }
    if (imports.size() > 10) ss << "  ... and " << (imports.size() - 10) << " more\n";
    
    auto exports = g_codex.GetExports();
    ss << "\nExports: " << exports.size() << "\n";
    for (size_t i = 0; i < std::min<size_t>(10, exports.size()); ++i) {
        ss << "  " << exports[i].name << " @ 0x" << std::hex << exports[i].address << "\n" << std::dec;
    }
    if (exports.size() > 10) ss << "  ... and " << (exports.size() - 10) << " more\n";
    
    auto strings = g_codex.ExtractStrings(8);
    ss << "\nStrings (>= 8 chars): " << strings.size() << "\n";
    for (size_t i = 0; i < std::min<size_t>(10, strings.size()); ++i) {
        ss << "  0x" << std::hex << strings[i].offset << ": " 
           << std::dec << strings[i].value.substr(0, 60);
        if (strings[i].value.length() > 60) ss << "...";
        ss << "\n";
    }
    if (strings.size() > 10) ss << "  ... and " << (strings.size() - 10) << " more\n";
    
    ShowOutput(m_hwnd, "Binary Analysis", ss.str());
}

// Menu handler for IDM_REVENG_DISASM
void Win32IDE::handleReverseEngineeringDisasm() {
    LOG_FUNCTION();
    
    if (g_currentBinary.empty()) {
        MessageBoxA(m_hwnd, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    // Simple dialog for address input (you can enhance this with a proper dialog)
    char addrStr[64] = "0x1000";
    // For production, create a proper dialog box
    
    uint64_t addr = 0x1000;
    size_t count = 50;
    
    std::string result = g_dumpbin.DumpDisassembly(g_currentBinary, addr, count);
    ShowOutput(m_hwnd, "Disassembly", result);
}

// Menu handler for IDM_REVENG_DUMPBIN
void Win32IDE::handleReverseEngineeringDumpBin() {
    LOG_FUNCTION();
    
    if (g_currentBinary.empty()) {
        MessageBoxA(m_hwnd, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    std::string result = g_dumpbin.DumpAll(g_currentBinary);
    ShowOutput(m_hwnd, "Binary Dump", result);
}

// Menu handler for IDM_REVENG_COMPILE
void Win32IDE::handleReverseEngineeringCompile() {
    LOG_FUNCTION();
    
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = "Source Files\0*.c;*.cpp;*.asm\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Select Source File to Compile";
    
    if (!GetOpenFileNameA(&ofn)) return;
    
    ReverseEngineering::CompilerOptions opts;
    opts.optimizationLevel = 2;
    opts.targetArch = "x64";
    opts.debug = true;
    
    g_compiler.SetOptions(opts);
    
    std::stringstream ss;
    ss << "Compiling: " << filename << "\n\n";
    
    auto result = g_compiler.CompileSource(filename);
    
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
    
    ShowOutput(m_hwnd, "Compilation", ss.str());
}

// Menu handler for IDM_REVENG_COMPARE
void Win32IDE::handleReverseEngineeringCompare() {
    LOG_FUNCTION();
    
    std::string path1 = OpenBinaryDialog(m_hwnd);
    if (path1.empty()) return;
    
    std::string path2 = OpenBinaryDialog(m_hwnd);
    if (path2.empty()) return;
    
    std::string result = g_dumpbin.CompareBinaries(path1, path2);
    ShowOutput(m_hwnd, "Binary Comparison", result);
}

// Menu handler for IDM_REVENG_DETECT_VULNS
void Win32IDE::handleReverseEngineeringDetectVulns() {
    LOG_FUNCTION();
    
    if (g_currentBinary.empty()) {
        MessageBoxA(m_hwnd, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    std::string result = g_dumpbin.DumpVulnerabilities(g_currentBinary);
    ShowOutput(m_hwnd, "Vulnerability Detection", result);
}

// Menu handler for IDM_REVENG_EXPORT_IDA
void Win32IDE::handleReverseEngineeringExportIDA() {
    LOG_FUNCTION();
    
    if (g_currentBinary.empty()) {
        MessageBoxA(m_hwnd, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = "Python Scripts\0*.py\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Export IDA Script";
    ofn.lpstrDefExt = "py";
    
    if (!GetSaveFileNameA(&ofn)) return;
    
    std::string script = g_codex.ExportToIDA();
    
    FILE* f = fopen(filename, "w");
    if (f) {
        fwrite(script.c_str(), 1, script.length(), f);
        fclose(f);
        MessageBoxA(m_hwnd, "IDA script exported successfully", "Success", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxA(m_hwnd, "Failed to save script", "Error", MB_OK | MB_ICONERROR);
    }
}

// Menu handler for IDM_REVENG_EXPORT_GHIDRA
void Win32IDE::handleReverseEngineeringExportGhidra() {
    LOG_FUNCTION();
    
    if (g_currentBinary.empty()) {
        MessageBoxA(m_hwnd, "No binary loaded. Use 'Analyze Binary' first.", "Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = "Python Scripts\0*.py\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Export Ghidra Script";
    ofn.lpstrDefExt = "py";
    
    if (!GetSaveFileNameA(&ofn)) return;
    
    std::string script = g_codex.ExportToGhidra();
    
    FILE* f = fopen(filename, "w");
    if (f) {
        fwrite(script.c_str(), 1, script.length(), f);
        fclose(f);
        MessageBoxA(m_hwnd, "Ghidra script exported successfully", "Success", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxA(m_hwnd, "Failed to save script", "Error", MB_OK | MB_ICONERROR);
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
    AppendMenuA(menu, MF_STRING, IDM_REVENG_COMPARE, "C&ompare Binaries");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DETECT_VULNS, "Detect &Vulnerabilities");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_EXPORT_IDA, "Export to &IDA Pro");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_EXPORT_GHIDRA, "Export to &Ghidra");
    
    return menu;
}

} // namespace RawrXD
