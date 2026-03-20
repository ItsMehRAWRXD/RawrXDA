#if defined(_WIN32) && !defined(_MSC_VER)

#include "Win32IDE.h"

#include <winsock2.h>

#include <sstream>
#include <string>

namespace {

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
    appendToOutput("[Vision] Vision encoder UI is unavailable on MinGW lane.\n");
}

void Win32IDE::toggleBreakpoint(const std::string& /*file*/, int /*line*/) {
    appendToOutput("[Debug] Breakpoint toggling is unavailable on MinGW lane.\n");
}

HMENU Win32IDE::createReverseEngineeringMenu() {
    appendToOutput("[RE] Reverse engineering menu unavailable on MinGW lane.\n");
    return nullptr;
}

void Win32IDE::updateMemoryView() {}

void Win32IDE::setCurrentBinaryForReverseEngineering(const std::string& /*path*/) {}

void Win32IDE::debuggerEvaluateExpression(const std::string& /*expression*/) {
    appendToOutput("[Debug] Expression evaluator unavailable on MinGW lane.\n");
}

void Win32IDE::handleReverseEngineeringFunctions() { appendToOutput("[RE] Functions view unavailable on MinGW lane.\n"); }
void Win32IDE::handleReverseEngineeringCFG() { appendToOutput("[RE] CFG view unavailable on MinGW lane.\n"); }
void Win32IDE::handleReverseEngineeringExportGhidra() { appendToOutput("[RE] Ghidra export unavailable on MinGW lane.\n"); }
void Win32IDE::handleReverseEngineeringExportIDA() { appendToOutput("[RE] IDA export unavailable on MinGW lane.\n"); }
void Win32IDE::handleReverseEngineeringDetectVulns() { appendToOutput("[RE] Vulnerability scan unavailable on MinGW lane.\n"); }
void Win32IDE::handleReverseEngineeringCompare() { appendToOutput("[RE] Binary compare unavailable on MinGW lane.\n"); }
void Win32IDE::handleReverseEngineeringCompile() { appendToOutput("[RE] Compile from reverse view unavailable on MinGW lane.\n"); }
void Win32IDE::handleReverseEngineeringDumpBin() { appendToOutput("[RE] DumpBin action unavailable on MinGW lane.\n"); }
void Win32IDE::handleReverseEngineeringDisassemble() { appendToOutput("[RE] Disassembly action unavailable on MinGW lane.\n"); }
void Win32IDE::handleReverseEngineeringAnalyze() { appendToOutput("[RE] Analyze action unavailable on MinGW lane.\n"); }

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
