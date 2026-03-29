#if !defined(_MSC_VER)

#include "Win32IDE.h"
#include "../core/native_debugger_engine.h"

#include <windows.h>
#include <winsock2.h>

#include <cstdlib>
#include <sstream>
#include <string>

namespace {
using RawrXD::Debugger::NativeDebuggerEngine;

void logDebugUnavailable(const char* action) {
    OutputDebugStringA("[RawrXD][Phase12] Native debugger unavailable on non-MSVC toolchain: ");
    OutputDebugStringA(action);
    OutputDebugStringA("\n");
}

void sendHttpJson(SOCKET client, const std::string& body) {
    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n"
         << "Content-Type: application/json\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Content-Length: " << body.size() << "\r\n\r\n"
         << body;
    const std::string payload = resp.str();
    send(client, payload.c_str(), static_cast<int>(payload.size()), 0);
}

void sendHttpError(SOCKET client, const std::string& message) {
    const std::string body = std::string("{\"ok\":false,\"error\":\"") + message + "\"}";
    std::ostringstream resp;
    resp << "HTTP/1.1 400 Bad Request\r\n"
         << "Content-Type: application/json\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Content-Length: " << body.size() << "\r\n\r\n"
         << body;
    const std::string payload = resp.str();
    send(client, payload.c_str(), static_cast<int>(payload.size()), 0);
}

bool extractJsonString(const std::string& body, const std::string& key, std::string& out) {
    const std::string quoted = "\"" + key + "\"";
    const size_t keyPos = body.find(quoted);
    if (keyPos == std::string::npos) {
        return false;
    }
    const size_t colon = body.find(':', keyPos + quoted.size());
    if (colon == std::string::npos) {
        return false;
    }
    const size_t qStart = body.find('"', colon + 1);
    if (qStart == std::string::npos) {
        return false;
    }
    const size_t qEnd = body.find('"', qStart + 1);
    if (qEnd == std::string::npos || qEnd <= qStart) {
        return false;
    }
    out = body.substr(qStart + 1, qEnd - qStart - 1);
    return true;
}

bool extractJsonUint64(const std::string& body, const std::string& key, uint64_t& out) {
    const std::string quoted = "\"" + key + "\"";
    const size_t keyPos = body.find(quoted);
    if (keyPos == std::string::npos) {
        return false;
    }
    const size_t colon = body.find(':', keyPos + quoted.size());
    if (colon == std::string::npos) {
        return false;
    }
    out = std::strtoull(body.c_str() + colon + 1, nullptr, 10);
    return true;
}
}  // namespace

void Win32IDE::initPhase12() {
    m_phase12Initialized = false;
    logDebugUnavailable("initPhase12");
}

void Win32IDE::shutdownPhase12() {
    m_phase12Initialized = false;
    logDebugUnavailable("shutdownPhase12");
}

void Win32IDE::cmdDbgStepOver() {
    logDebugUnavailable("cmdDbgStepOver");
}

void Win32IDE::cmdDbgStepInto() {
    logDebugUnavailable("cmdDbgStepInto");
}

void Win32IDE::cmdDbgStepOut() {
    logDebugUnavailable("cmdDbgStepOut");
}

void Win32IDE::cmdDbgBreak() {
    logDebugUnavailable("cmdDbgBreak");
}

void Win32IDE::cmdDbgLaunch() {
    startDebugging();
}

void Win32IDE::cmdDbgAttach() {
    logDebugUnavailable("cmdDbgAttach");
}

void Win32IDE::cmdDbgDetach() {
    logDebugUnavailable("cmdDbgDetach");
}

void Win32IDE::cmdDbgGo() {
    logDebugUnavailable("cmdDbgGo");
}

void Win32IDE::cmdDbgAddBP() {
    logDebugUnavailable("cmdDbgAddBP");
}

void Win32IDE::cmdDbgRemoveBP() {
    logDebugUnavailable("cmdDbgRemoveBP");
}

void Win32IDE::cmdDbgEnableBP() {
    logDebugUnavailable("cmdDbgEnableBP");
}

void Win32IDE::cmdDbgClearBPs() {
    logDebugUnavailable("cmdDbgClearBPs");
}

void Win32IDE::cmdDbgListBPs() {
    logDebugUnavailable("cmdDbgListBPs");
}

void Win32IDE::cmdDbgAddWatch() {
    logDebugUnavailable("cmdDbgAddWatch");
}

void Win32IDE::cmdDbgRemoveWatch() {
    logDebugUnavailable("cmdDbgRemoveWatch");
}

void Win32IDE::cmdDbgSwitchThread() {
    logDebugUnavailable("cmdDbgSwitchThread");
}

void Win32IDE::cmdDbgEvaluate() {
    logDebugUnavailable("cmdDbgEvaluate");
}

void Win32IDE::cmdDbgKill() {
    stopDebugging();
}

void Win32IDE::cmdDbgRegisters() {
    logDebugUnavailable("cmdDbgRegisters");
}

void Win32IDE::cmdDbgStack() {
    logDebugUnavailable("cmdDbgStack");
}

void Win32IDE::cmdDbgMemory() {
    logDebugUnavailable("cmdDbgMemory");
}

void Win32IDE::cmdDbgDisasm() {
    logDebugUnavailable("cmdDbgDisasm");
}

void Win32IDE::cmdDbgModules() {
    logDebugUnavailable("cmdDbgModules");
}

void Win32IDE::cmdDbgThreads() {
    logDebugUnavailable("cmdDbgThreads");
}

void Win32IDE::cmdDbgSetRegister() {
    logDebugUnavailable("cmdDbgSetRegister");
}

void Win32IDE::cmdDbgSearchMemory() {
    logDebugUnavailable("cmdDbgSearchMemory");
}

void Win32IDE::cmdDbgSymbolPath() {
    logDebugUnavailable("cmdDbgSymbolPath");
}

void Win32IDE::cmdDbgStatus() {
    logDebugUnavailable("cmdDbgStatus");
}

void Win32IDE::handleDbgGoEndpoint(SOCKET client) {
    const auto result = NativeDebuggerEngine::Instance().go();
    if (!result.success) {
        sendHttpError(client, result.detail);
        return;
    }
    sendHttpJson(client, std::string("{\"ok\":true,\"message\":\"") + result.detail + "\"}");
}

void Win32IDE::handleDbgAttachEndpoint(SOCKET client, const std::string& body) {
    uint64_t pid = 0;
    if (!extractJsonUint64(body, "pid", pid) || pid == 0) {
        sendHttpError(client, "Missing or invalid 'pid' field");
        return;
    }
    std::ostringstream json;
    json << "{\"ok\":true,\"message\":\"Attach request captured in non-MSVC fallback\",\"pid\":" << pid << "}";
    sendHttpJson(client, json.str());
}

void Win32IDE::handleDbgLaunchEndpoint(SOCKET client, const std::string& body) {
    std::string path;
    std::string args;
    extractJsonString(body, "args", args);
    if (!extractJsonString(body, "path", path) || path.empty()) {
        sendHttpError(client, "Missing 'path' field");
        return;
    }

    const auto result = NativeDebuggerEngine::Instance().launchProcess(path, args, "");
    if (!result.success) {
        sendHttpError(client, result.detail);
        return;
    }
    setCurrentBinaryForReverseEngineering(path);
    sendHttpJson(client, std::string("{\"ok\":true,\"message\":\"") + result.detail + "\"}");
}

void Win32IDE::handleDbgDisasmEndpoint(SOCKET client, const std::string& body) {
    uint64_t address = 0;
    uint64_t lines = 32;
    extractJsonUint64(body, "lines", lines);
    std::string addressString;
    if (extractJsonString(body, "address", addressString) &&
        addressString.size() > 2 && addressString[0] == '0' &&
        (addressString[1] == 'x' || addressString[1] == 'X')) {
        address = std::strtoull(addressString.c_str() + 2, nullptr, 16);
    }
    std::ostringstream json;
    json << "{\"ok\":true,\"address\":\"0x" << std::hex << address << std::dec
         << "\",\"lines\":" << lines
         << ",\"note\":\"non-MSVC fallback disassembly endpoint\"}";
    sendHttpJson(client, json.str());
}

void Win32IDE::handleDbgMemoryEndpoint(SOCKET client, const std::string& body) {
    uint64_t size = 256;
    uint64_t address = 0;
    extractJsonUint64(body, "size", size);
    std::string addressString;
    if (extractJsonString(body, "address", addressString) &&
        addressString.size() > 2 && addressString[0] == '0' &&
        (addressString[1] == 'x' || addressString[1] == 'X')) {
        address = std::strtoull(addressString.c_str() + 2, nullptr, 16);
    }
    std::ostringstream json;
    json << "{\"ok\":true,\"address\":\"0x" << std::hex << address << std::dec
         << "\",\"size\":" << size
         << ",\"bytes\":\"\",\"note\":\"non-MSVC fallback memory endpoint\"}";
    sendHttpJson(client, json.str());
}

void Win32IDE::handlePhase12StatusEndpoint(SOCKET client) {
    std::ostringstream json;
    json << "{"
         << "\"phase\":12,"
         << "\"name\":\"Native Debugger Engine\","
         << "\"initialized\":" << (m_phase12Initialized ? "true" : "false") << ","
         << "\"lane\":\"non-msvc-fallback\""
         << "}";
    sendHttpJson(client, json.str());
}

void Win32IDE::handleDbgWatchesEndpoint(SOCKET client) {
    sendHttpJson(client, "{\"ok\":true,\"watches\":[]}");
}

void Win32IDE::handleDbgEventsEndpoint(SOCKET client) {
    sendHttpJson(client, "{\"ok\":true,\"events\":[]}");
}

void Win32IDE::handleDbgThreadsEndpoint(SOCKET client) {
    sendHttpJson(client, "{\"ok\":true,\"threads\":[]}");
}

void Win32IDE::handleDbgModulesEndpoint(SOCKET client) {
    sendHttpJson(client, "{\"ok\":true,\"modules\":[]}");
}

void Win32IDE::handleDbgStatusEndpoint(SOCKET client) {
    std::ostringstream json;
    json << "{"
         << "\"ok\":true,"
         << "\"phase\":12,"
         << "\"initialized\":" << (m_phase12Initialized ? "true" : "false")
         << "}";
    sendHttpJson(client, json.str());
}

void Win32IDE::handleDbgBreakpointsEndpoint(SOCKET client) {
    sendHttpJson(client, "{\"ok\":true,\"breakpoints\":[]}");
}

void Win32IDE::handleDbgRegistersEndpoint(SOCKET client) {
    sendHttpJson(client, "{\"ok\":true,\"registers\":{}}");
}

void Win32IDE::handleDbgStackEndpoint(SOCKET client) {
    sendHttpJson(client, "{\"ok\":true,\"stack\":[]}");
}

void Win32IDE::handleReSetBinaryEndpoint(SOCKET client, const std::string& body) {
    std::string path;
    if (!extractJsonString(body, "path", path) || path.empty()) {
        sendHttpError(client, "Missing 'path' field");
        return;
    }
    setCurrentBinaryForReverseEngineering(path);
    appendToOutput("[RE] Binary set via API: " + path + "\n", "Output", OutputSeverity::Info);
    sendHttpJson(client, std::string("{\"ok\":true,\"message\":\"RE binary set to ") + path + "\"}");
}

#endif  // !defined(_MSC_VER)
