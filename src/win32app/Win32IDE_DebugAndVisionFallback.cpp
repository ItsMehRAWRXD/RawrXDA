#if defined(_WIN32) && !defined(_MSC_VER)

#include "Win32IDE.h"
#include "../reverse_engineering/RawrCodex.hpp"
#include "../reverse_engineering/RawrCompiler.hpp"
#include "../reverse_engineering/RawrDumpBin.hpp"
#include "../reverse_engineering/RawrReverseEngine.hpp"

#include <commdlg.h>
#include <winsock2.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using RawrXD::ReverseEngineering::RawrCodex;
using RawrXD::ReverseEngineering::RawrCompiler;
using RawrXD::ReverseEngineering::RawrDumpBin;
using RawrXD::ReverseEngineering::RawrReverseEngine;

static RawrCodex s_reCodex;
static RawrDumpBin s_reDumpbin;
static RawrCompiler s_reCompiler;
static RawrReverseEngine s_reEngine;
static std::string s_reCurrentBinary;

bool hasSupportedBinaryExt(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return false;
    }
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return ext == "exe" || ext == "dll" || ext == "obj" || ext == "o" || ext == "so" || ext == "sys";
}

std::string openBinaryDialog(HWND hwndOwner, const char* title) {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.lpstrFilter = "Executable Files\0*.exe;*.dll;*.sys;*.obj;*.o;*.so\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = title;
    if (!GetOpenFileNameA(&ofn)) {
        return {};
    }
    return std::string(filename);
}

uint64_t resolveEntryAddress() {
    auto symbols = s_reCodex.GetSymbols();
    for (const auto& sym : symbols) {
        if (sym.name == "_start" || sym.name == "main" || sym.name == "WinMain" ||
            sym.name == "_main" || sym.name == "wmain" || sym.name == "wWinMain" ||
            sym.name == "entry" || sym.name == "EntryPoint") {
            return sym.address;
        }
    }
    if (!symbols.empty()) {
        return symbols[0].address;
    }
    return 0x1000;
}

bool ensureBinaryLoaded(HWND hwndOwner) {
    if (!s_reCurrentBinary.empty()) {
        return true;
    }
    std::string path = openBinaryDialog(hwndOwner, "Select Binary for Analysis");
    if (path.empty()) {
        return false;
    }
    if (!s_reCodex.LoadBinary(path)) {
        MessageBoxA(hwndOwner, "Failed to load binary", "RE", MB_OK | MB_ICONERROR);
        return false;
    }
    s_reCurrentBinary = path;
    return true;
}

void sendFallbackJson(SOCKET client, const char* endpoint) {
    if (client == static_cast<SOCKET>(-1)) {
        return;
    }
    std::ostringstream body;
    body << "{\"ok\":false,\"endpoint\":\"" << endpoint
         << "\",\"error\":\"disabled in MinGW build lane\"}";
    const std::string payload = body.str();

    std::ostringstream resp;
    resp << "HTTP/1.1 501 Not Implemented\r\n"
         << "Content-Type: application/json\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Content-Length: " << payload.size() << "\r\n"
         << "\r\n"
         << payload;
    const std::string wire = resp.str();
    send(client, wire.c_str(), static_cast<int>(wire.size()), 0);
}

}  // namespace

void Win32IDE::updateMinimap() {}

void Win32IDE::toggleMinimap() {
    m_settings.minimapEnabled = !m_settings.minimapEnabled;
}

void Win32IDE::showVisionEncoder() {
    appendToOutput("[Vision] Vision encoder window is disabled in MinGW lane.\n");
}

void Win32IDE::toggleBreakpoint(const std::string& /*file*/, int /*line*/) {
    appendToOutput("[Debug] Breakpoint toggling requires native debugger lane.\n");
}

HMENU Win32IDE::createReverseEngineeringMenu() {
    HMENU menu = CreateMenu();
    AppendMenuA(menu, MF_STRING, IDM_REVENG_ANALYZE, "&Analyze Binary\tCtrl+R");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_SET_BINARY_FROM_ACTIVE, "Set binary from &active document");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_SET_BINARY_FROM_BUILD_OUTPUT, "Set binary from &build output...");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DISASM_AT_RIP, "Disassemble around entrypoint");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DISASM, "&Disassemble\tCtrl+D");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DUMPBIN, "Dump &Binary\tCtrl+B");
    AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_CFG, "Control &Flow Graph");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_FUNCTIONS, "Recover F&unctions");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DEMANGLE, "De&mangle Symbols");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_SSA, "&SSA Lifting");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_RECURSIVE_DISASM, "R&ecursive Disassembly");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_TYPE_RECOVERY, "&Type Recovery");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DATA_FLOW, "Data &Flow Analysis");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DECOMPILER_VIEW, "Decompiler &View");
    AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(menu, MF_STRING, IDM_REVENG_COMPARE, "C&ompare Binaries");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_DETECT_VULNS, "Detect &Vulnerabilities");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_EXPORT_IDA, "Export to &IDA Pro");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_EXPORT_GHIDRA, "Export to &Ghidra");
    AppendMenuA(menu, MF_STRING, IDM_REVENG_LICENSE_INFO, "&License Info");
    return menu;
}

void Win32IDE::updateMemoryView() {}

void Win32IDE::setCurrentBinaryForReverseEngineering(const std::string& path) {
    s_reCurrentBinary = path;
}

void Win32IDE::debuggerEvaluateExpression(const std::string& /*expression*/) {
    appendToOutput("[Debug] Expression evaluator requires native debugger lane.\n");
}

void Win32IDE::handleReverseEngineeringAnalyze() {
    std::string path = openBinaryDialog(m_hwndMain, "Select Binary for Analysis");
    if (path.empty()) {
        return;
    }
    if (!s_reCodex.LoadBinary(path)) {
        MessageBoxA(m_hwndMain, "Failed to load binary", "RE", MB_OK | MB_ICONERROR);
        return;
    }
    s_reCurrentBinary = path;
    auto sections = s_reCodex.GetSections();
    auto imports = s_reCodex.GetImports();
    auto exports = s_reCodex.GetExports();
    std::ostringstream ss;
    ss << "\n=== Binary Analysis ===\n"
       << "Binary: " << path << "\n"
       << "Sections: " << sections.size() << "\n"
       << "Imports: " << imports.size() << "\n"
       << "Exports: " << exports.size() << "\n\n";
    appendToOutput(ss.str(), "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringDisassemble() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    uint64_t entry = resolveEntryAddress();
    std::string disasm = s_reDumpbin.DumpDisassembly(s_reCurrentBinary, entry, 64);
    appendToOutput("\n=== Disassembly ===\n\n" + disasm, "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringDumpBin() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    std::string dump = s_reDumpbin.DumpAll(s_reCurrentBinary);
    appendToOutput("\n=== DumpBin ===\n\n" + dump, "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringCompile() {
    char sourcePath[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Source Files\0*.c;*.cpp;*.asm\0All Files\0*.*\0";
    ofn.lpstrFile = sourcePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) {
        return;
    }
    RawrXD::ReverseEngineering::CompilerOptions opts;
    opts.optimizationLevel = 2;
    opts.targetArch = "x64";
    opts.debugInfo = true;
    s_reCompiler.SetOptions(opts);
    auto result = s_reCompiler.CompileSource(sourcePath);
    std::ostringstream ss;
    ss << "\n=== Compile Source ===\n";
    ss << "Input: " << sourcePath << "\n";
    ss << (result.success ? "Status: success\n" : "Status: failed\n");
    if (result.success && !result.objectFile.empty()) {
        s_reCurrentBinary = result.objectFile;
        ss << "Object: " << result.objectFile << "\n";
    }
    for (const auto& w : result.warnings) {
        ss << "Warning: " << w << "\n";
    }
    for (const auto& e : result.errors) {
        ss << "Error: " << e << "\n";
    }
    appendToOutput(ss.str(), "Output", result.success ? OutputSeverity::Info : OutputSeverity::Error);
}

void Win32IDE::handleReverseEngineeringCompare() {
    std::string left = openBinaryDialog(m_hwndMain, "Select Left Binary");
    if (left.empty()) return;
    std::string right = openBinaryDialog(m_hwndMain, "Select Right Binary");
    if (right.empty()) return;
    std::string diff = s_reDumpbin.CompareBinaries(left, right);
    appendToOutput("\n=== Binary Compare ===\n\n" + diff, "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringDetectVulns() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    std::string vulns = s_reDumpbin.DumpVulnerabilities(s_reCurrentBinary);
    appendToOutput("\n=== Vulnerability Detection ===\n\n" + vulns, "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringExportIDA() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    char outputPath[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Python Scripts\0*.py\0All Files\0*.*\0";
    ofn.lpstrFile = outputPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "py";
    if (!GetSaveFileNameA(&ofn)) {
        return;
    }
    std::ofstream out(outputPath, std::ios::binary);
    out << s_reCodex.ExportToIDA();
    appendToOutput("[RE] Exported IDA script: " + std::string(outputPath) + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringExportGhidra() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    char outputPath[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Python Scripts\0*.py\0All Files\0*.*\0";
    ofn.lpstrFile = outputPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "py";
    if (!GetSaveFileNameA(&ofn)) {
        return;
    }
    std::ofstream out(outputPath, std::ios::binary);
    out << s_reCodex.ExportToGhidra();
    appendToOutput("[RE] Exported Ghidra script: " + std::string(outputPath) + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringCFG() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string cfg = s_reEngine.AnalyzeCFG(resolveEntryAddress());
    appendToOutput("\n=== CFG ===\n\n" + cfg, "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringFunctions() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string functions = s_reEngine.RecoverFunctions();
    appendToOutput("\n=== Function Recovery ===\n\n" + functions, "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringDemangle() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string demangled = s_reEngine.DemangleAll();
    appendToOutput("\n=== Symbol Demangle ===\n\n" + demangled, "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringSSA() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string ssa = s_reEngine.LiftSSA(resolveEntryAddress());
    appendToOutput("\n=== SSA Lifting ===\n\n" + ssa, "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringRecursiveDisasm() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string recursive = s_reEngine.RecursiveDisassemble(resolveEntryAddress());
    appendToOutput("\n=== Recursive Disassembly ===\n\n" + recursive, "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringTypeRecovery() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string types = s_reEngine.RecoverTypes(resolveEntryAddress());
    appendToOutput("\n=== Type Recovery ===\n\n" + types, "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringDataFlow() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string flow = s_reEngine.RecoverTypes(resolveEntryAddress());
    appendToOutput("\n=== Data Flow Analysis ===\n\n" + flow, "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringLicenseInfo() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string info = s_reEngine.GetLicenseInfo();
    appendToOutput("\n=== Reverse Engineering License Info ===\n\n" + info, "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringDecompilerView() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    const uint64_t entry = resolveEntryAddress();
    s_reEngine.LoadBinary(s_reCurrentBinary);
    std::string pseudo = s_reEngine.RecoverTypes(entry);
    std::string disasm = s_reDumpbin.DumpDisassembly(s_reCurrentBinary, entry, 80);
    showDecompilerView(pseudo, disasm, s_reCurrentBinary);
}

void Win32IDE::handleReverseEngineeringSetBinaryFromActive() {
    if (m_currentFile.empty() || !hasSupportedBinaryExt(m_currentFile)) {
        MessageBoxA(m_hwndMain, "Active file is not a supported binary.", "RE", MB_OK | MB_ICONINFORMATION);
        return;
    }
    s_reCurrentBinary = m_currentFile;
    s_reCodex.LoadBinary(s_reCurrentBinary);
    appendToOutput("[RE] Binary set from active document: " + s_reCurrentBinary + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringSetBinaryFromDebugTarget() {
    if (!s_reCurrentBinary.empty()) {
        appendToOutput("[RE] Using current selected binary: " + s_reCurrentBinary + "\n", "Output", OutputSeverity::Info);
        return;
    }
    handleReverseEngineeringSetBinaryFromActive();
}

void Win32IDE::handleReverseEngineeringSetBinaryFromBuildOutput() {
    std::string path = openBinaryDialog(m_hwndMain, "Select Build Output Binary");
    if (path.empty()) {
        return;
    }
    s_reCurrentBinary = path;
    s_reCodex.LoadBinary(s_reCurrentBinary);
    appendToOutput("[RE] Binary set from build output: " + s_reCurrentBinary + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::handleReverseEngineeringDisassembleAtRIP() {
    if (!ensureBinaryLoaded(m_hwndMain)) {
        return;
    }
    const uint64_t entry = resolveEntryAddress();
    std::string disasm = s_reDumpbin.DumpDisassembly(s_reCurrentBinary, entry, 40);
    std::ostringstream out;
    out << "\n=== Disassembly at entrypoint 0x" << std::hex << entry << " ===\n\n" << disasm;
    appendToOutput(out.str(), "Output", OutputSeverity::Info);
}

void Win32IDE::handleReSetBinaryEndpoint(SOCKET client, const std::string& /*body*/) {
    sendFallbackJson(client, "re/set-binary");
}

void Win32IDE::handleDbgStatusEndpoint(SOCKET client) { sendFallbackJson(client, "debug/status"); }
void Win32IDE::handleDbgBreakpointsEndpoint(SOCKET client) { sendFallbackJson(client, "debug/breakpoints"); }
void Win32IDE::handleDbgRegistersEndpoint(SOCKET client) { sendFallbackJson(client, "debug/registers"); }
void Win32IDE::handleDbgStackEndpoint(SOCKET client) { sendFallbackJson(client, "debug/stack"); }
void Win32IDE::handleDbgMemoryEndpoint(SOCKET client, const std::string& /*body*/) { sendFallbackJson(client, "debug/memory"); }
void Win32IDE::handleDbgDisasmEndpoint(SOCKET client, const std::string& /*body*/) { sendFallbackJson(client, "debug/disasm"); }
void Win32IDE::handleDbgModulesEndpoint(SOCKET client) { sendFallbackJson(client, "debug/modules"); }
void Win32IDE::handleDbgThreadsEndpoint(SOCKET client) { sendFallbackJson(client, "debug/threads"); }
void Win32IDE::handleDbgEventsEndpoint(SOCKET client) { sendFallbackJson(client, "debug/events"); }
void Win32IDE::handleDbgWatchesEndpoint(SOCKET client) { sendFallbackJson(client, "debug/watches"); }
void Win32IDE::handleDbgLaunchEndpoint(SOCKET client, const std::string& /*body*/) { sendFallbackJson(client, "debug/launch"); }
void Win32IDE::handleDbgAttachEndpoint(SOCKET client, const std::string& /*body*/) { sendFallbackJson(client, "debug/attach"); }
void Win32IDE::handleDbgGoEndpoint(SOCKET client) { sendFallbackJson(client, "debug/go"); }
void Win32IDE::handlePhase12StatusEndpoint(SOCKET client) { sendFallbackJson(client, "phase12/status"); }

#endif
