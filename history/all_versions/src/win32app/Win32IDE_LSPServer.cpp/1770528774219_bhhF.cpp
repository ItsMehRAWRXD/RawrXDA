// ============================================================================
// Win32IDE_LSPServer.cpp — Phase 27: LSP Server Win32IDE Integration Bridge
// ============================================================================
// PURPOSE: Bridges the standalone RawrXDLSPServer into the Win32IDE window.
//          Provides IDM command handlers, status display, HTTP endpoints,
//          and menu integration.  The server can run in two modes:
//
//          1. In-process (default): Messages flow through injectMessage() /
//             pollOutgoing() — no child process needed.
//          2. Stdio subprocess: Launched as a child process for external
//             editors (VS Code, Sublime, etc.) to connect via stdio.
//
// IDM COMMANDS (9200+ range):
//   IDM_LSP_SERVER_START          9200   Start embedded LSP server
//   IDM_LSP_SERVER_STOP           9201   Stop embedded LSP server
//   IDM_LSP_SERVER_STATUS         9202   Show server status in output
//   IDM_LSP_SERVER_REINDEX        9203   Rebuild symbol index
//   IDM_LSP_SERVER_STATS          9204   Show server statistics
//   IDM_LSP_SERVER_PUBLISH_DIAG   9205   Trigger diagnostic publish
//   IDM_LSP_SERVER_CONFIG         9206   Show/edit server config
//   IDM_LSP_SERVER_EXPORT_SYMBOLS 9207   Export symbol database to JSON
//   IDM_LSP_SERVER_LAUNCH_STDIO   9208   Launch as stdio subprocess
//
// BUILD: Added to WIN32IDE_SOURCES in CMakeLists.txt.
//        Declared in Win32IDE.h (Phase 27 section).
//
// Copyright (c) 2025 RawrXD Project — All rights reserved.
// ============================================================================

#include "win32app/Win32IDE.h"
#include "lsp/RawrXD_LSPServer.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <fstream>
#include <chrono>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace RawrXD::LSPServer;

// ============================================================================
// LIFECYCLE
// ============================================================================

void Win32IDE::initLSPServer() {
    if (m_lspServer) return;  // Already created

    m_lspServer = std::make_unique<RawrXDLSPServer>();

    // Configure with workspace root
    ServerConfig config;
    config.useStdio       = false;  // In-process mode for IDE
    config.rootPath       = m_currentDirectory;
    config.rootUri        = m_lspServer ? "file:///" + m_currentDirectory : "";
    // Normalize rootUri backslashes
    for (char& c : config.rootUri) {
        if (c == '\\') c = '/';
    }
    config.enableSemanticTokens  = true;
    config.enableHover           = true;
    config.enableCompletion      = true;
    config.enableDefinition      = true;
    config.enableReferences      = true;
    config.enableDocumentSymbol  = true;
    config.enableWorkspaceSymbol = true;
    config.enableDiagnostics     = true;
    config.maxSymbolResults      = 500;
    config.maxCompletionItems    = 100;

    m_lspServer->configure(config);

    LOG_INFO("LSP Server initialized (in-process mode)");
}

void Win32IDE::shutdownLSPServer() {
    if (!m_lspServer) return;

    m_lspServer->stop();
    m_lspServer.reset();

    LOG_INFO("LSP Server shut down");
    (void)0;
}

// ============================================================================
// START / STOP / REINDEX
// ============================================================================

void Win32IDE::cmdLSPServerStart() {
    if (!m_lspServer) {
        initLSPServer();
    }

    if (m_lspServer->isRunning()) {
        appendToOutput("⚠ LSP Server already running\n");
        return;
    }

    if (m_lspServer->start()) {
        appendToOutput("✅ LSP Server started (in-process)\n");
        appendToOutput("   Symbols indexed: " +
                       std::to_string(m_lspServer->getIndexedSymbolCount()) + "\n");
    } else {
        appendToOutput("❌ LSP Server failed to start\n");
    }
}

void Win32IDE::cmdLSPServerStop() {
    if (!m_lspServer || !m_lspServer->isRunning()) {
        appendToOutput("⚠ LSP Server not running\n");
        return;
    }

    m_lspServer->stop();
    appendToOutput("🛑 LSP Server stopped\n");
}

void Win32IDE::cmdLSPServerReindex() {
    if (!m_lspServer) {
        initLSPServer();
    }

    auto startTime = std::chrono::steady_clock::now();
    m_lspServer->rebuildIndex();
    auto endTime = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    appendToOutput("🔄 LSP Server re-indexed workspace\n");
    appendToOutput("   Symbols: " + std::to_string(m_lspServer->getIndexedSymbolCount()) + "\n");
    appendToOutput("   Time:    " + std::to_string((int)ms) + " ms\n");
}

// ============================================================================
// STATUS & STATS DISPLAY
// ============================================================================

void Win32IDE::cmdLSPServerStatus() {
    if (!m_lspServer) {
        appendToOutput("LSP Server: Not created\n");
        return;
    }

    std::string stateStr;
    switch (m_lspServer->getState()) {
        case ServerState::Created:      stateStr = "Created (not started)"; break;
        case ServerState::Initializing: stateStr = "Initializing..."; break;
        case ServerState::Running:      stateStr = "🟢 Running"; break;
        case ServerState::ShuttingDown: stateStr = "Shutting down..."; break;
        case ServerState::Stopped:      stateStr = "🔴 Stopped"; break;
        case ServerState::Error:        stateStr = "❌ Error"; break;
    }

    std::ostringstream oss;
    oss << "═══════════════════════════════════════\n"
        << " RawrXD LSP Server — Phase 27\n"
        << "═══════════════════════════════════════\n"
        << " State:     " << stateStr << "\n"
        << " Mode:      In-process (Win32IDE bridge)\n"
        << " Root:      " << m_lspServer->getConfig().rootPath << "\n"
        << " Symbols:   " << m_lspServer->getIndexedSymbolCount() << "\n"
        << " Tracked:   " << m_lspServer->getTrackedFileCount() << " open docs\n"
        << "═══════════════════════════════════════\n";

    appendToOutput(oss.str());
}

void Win32IDE::cmdLSPServerStats() {
    if (!m_lspServer) {
        appendToOutput("LSP Server: Not created\n");
        return;
    }

    appendToOutput(m_lspServer->getStatsString());
}

// ============================================================================
// DIAGNOSTICS BRIDGE
// ============================================================================

void Win32IDE::cmdLSPServerPublishDiagnostics() {
    if (!m_lspServer || !m_lspServer->isRunning()) {
        appendToOutput("⚠ LSP Server not running — cannot publish diagnostics\n");
        return;
    }

    // Bridge diagnostics from the LSP client (clangd, etc.) to LSP server for re-publishing
    auto allDiags = getAllDiagnostics();
    int count = 0;
    for (const auto& [uri, diags] : allDiags) {
        std::vector<DiagnosticEntry> entries;
        for (const auto& d : diags) {
            DiagnosticEntry entry;
            entry.range.start.line      = d.range.start.line;
            entry.range.start.character = d.range.start.character;
            entry.range.end.line        = d.range.end.line;
            entry.range.end.character   = d.range.end.character;
            entry.severity = static_cast<DiagnosticSeverity>(d.severity);
            entry.code     = d.code;
            entry.source   = d.source;
            entry.message  = d.message;
            entries.push_back(std::move(entry));
            count++;
        }
        m_lspServer->publishDiagnostics(uri, entries);
    }

    appendToOutput("📋 Published " + std::to_string(count) +
                   " diagnostics through LSP server\n");
}

// ============================================================================
// CONFIGURATION DISPLAY
// ============================================================================

void Win32IDE::cmdLSPServerConfig() {
    if (!m_lspServer) {
        initLSPServer();
    }

    const auto& cfg = m_lspServer->getConfig();
    std::ostringstream oss;
    oss << "═══════════════════════════════════════\n"
        << " LSP Server Configuration\n"
        << "═══════════════════════════════════════\n"
        << " Transport:        " << (cfg.useStdio ? "stdio" : "in-process") << "\n"
        << " Pipe name:        " << cfg.pipeName << "\n"
        << " Root URI:         " << cfg.rootUri << "\n"
        << " Root path:        " << cfg.rootPath << "\n"
        << " ─── Capabilities ───\n"
        << " Semantic tokens:  " << (cfg.enableSemanticTokens ? "✅" : "❌") << "\n"
        << " Hover:            " << (cfg.enableHover ? "✅" : "❌") << "\n"
        << " Completion:       " << (cfg.enableCompletion ? "✅" : "❌") << "\n"
        << " Definition:       " << (cfg.enableDefinition ? "✅" : "❌") << "\n"
        << " References:       " << (cfg.enableReferences ? "✅" : "❌") << "\n"
        << " Document symbol:  " << (cfg.enableDocumentSymbol ? "✅" : "❌") << "\n"
        << " Workspace symbol: " << (cfg.enableWorkspaceSymbol ? "✅" : "❌") << "\n"
        << " Diagnostics:      " << (cfg.enableDiagnostics ? "✅" : "❌") << "\n"
        << " ─── Limits ───\n"
        << " Max symbols:      " << cfg.maxSymbolResults << "\n"
        << " Max completions:  " << cfg.maxCompletionItems << "\n"
        << " Index throttle:   " << cfg.indexThrottleMs << " ms\n"
        << "═══════════════════════════════════════\n";

    appendToOutput(oss.str());
}

// ============================================================================
// SYMBOL EXPORT
// ============================================================================

void Win32IDE::cmdLSPServerExportSymbols() {
    if (!m_lspServer) {
        appendToOutput("⚠ LSP Server not created\n");
        return;
    }

    // Use workspace/symbol with empty query to get everything
    json params;
    params["query"] = "";

    // Inject as a request through the server's dispatch
    json request;
    request["jsonrpc"] = "2.0";
    request["id"]      = 99999;
    request["method"]  = "workspace/symbol";
    request["params"]  = params;

    m_lspServer->injectMessage(request.dump());

    // Poll for the response (with timeout)
    std::string response;
    for (int i = 0; i < 50; i++) {  // 5 seconds max
        response = m_lspServer->pollOutgoing();
        if (!response.empty()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (response.empty()) {
        appendToOutput("❌ Timeout waiting for symbol export\n");
        return;
    }

    // Write to file
    std::string exportPath = m_currentDirectory + "\\rawrxd_symbols.json";
    {
        std::ofstream ofs(exportPath);
        if (ofs.is_open()) {
            // Pretty-print the response
            try {
                json parsed = json::parse(response);
                ofs << parsed.dump(2);
            } catch (...) {
                ofs << response;
            }
        }
    }

    appendToOutput("📦 Symbols exported to: " + exportPath + "\n");
    appendToOutput("   Total symbols: " +
                   std::to_string(m_lspServer->getIndexedSymbolCount()) + "\n");
}

// ============================================================================
// STDIO SUBPROCESS LAUNCHER (for external editors)
// ============================================================================

void Win32IDE::cmdLSPServerLaunchStdio() {
    // Launch ourselves as a child process with --lsp-server flag
    // External editors (VS Code, Sublime) connect to this via stdio

    std::string exePath;
    {
        char buf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        exePath = buf;
    }

    std::string cmdLine = "\"" + exePath + "\" --lsp-server";
    cmdLine += " --root \"" + m_currentDirectory + "\"";

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    BOOL ok = CreateProcessA(
        nullptr,
        const_cast<char*>(cmdLine.c_str()),
        nullptr, nullptr, FALSE,
        CREATE_NEW_CONSOLE,
        nullptr, nullptr,
        &si, &pi
    );

    if (ok) {
        appendToOutput("🚀 LSP Server subprocess launched (PID " +
                       std::to_string(pi.dwProcessId) + ")\n");
        appendToOutput("   External editors can connect via stdio pipe\n");
        appendToOutput("   Command: " + cmdLine + "\n");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        appendToOutput("❌ Failed to launch LSP server subprocess (error " +
                       std::to_string(GetLastError()) + ")\n");
    }
}

// ============================================================================
// HTTP ENDPOINT (for local server integration)
// ============================================================================

void Win32IDE::handleLSPServerStatusEndpoint(SOCKET client) {
    json response;

    if (!m_lspServer) {
        response["status"]  = "not_created";
        response["running"] = false;
    } else {
        auto stats = m_lspServer->getStats();
        response["status"]            = m_lspServer->isRunning() ? "running" : "stopped";
        response["running"]           = m_lspServer->isRunning();
        response["symbolsIndexed"]    = stats.symbolsIndexed;
        response["filesTracked"]      = stats.filesTracked;
        response["totalRequests"]     = stats.totalRequests;
        response["totalErrors"]       = stats.totalErrors;
        response["avgResponseMs"]     = stats.avgResponseMs;
        response["hoverRequests"]     = stats.hoverRequests;
        response["completionRequests"] = stats.completionRequests;
        response["definitionRequests"] = stats.definitionRequests;
        response["referenceRequests"] = stats.referenceRequests;
        response["semanticTokenReqs"] = stats.semanticTokenRequests;
    }

    std::string body = response.dump(2);
    std::string httpResponse =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + body;

    send(client, httpResponse.c_str(), (int)httpResponse.size(), 0);
}

// ============================================================================
// COMMAND ROUTER (called from handleToolsCommand)
// ============================================================================

bool Win32IDE::handleLSPServerCommand(int commandId) {
    switch (commandId) {
        case IDM_LSP_SERVER_START:          cmdLSPServerStart();           return true;
        case IDM_LSP_SERVER_STOP:           cmdLSPServerStop();            return true;
        case IDM_LSP_SERVER_STATUS:         cmdLSPServerStatus();          return true;
        case IDM_LSP_SERVER_REINDEX:        cmdLSPServerReindex();         return true;
        case IDM_LSP_SERVER_STATS:          cmdLSPServerStats();           return true;
        case IDM_LSP_SERVER_PUBLISH_DIAG:   cmdLSPServerPublishDiagnostics(); return true;
        case IDM_LSP_SERVER_CONFIG:         cmdLSPServerConfig();          return true;
        case IDM_LSP_SERVER_EXPORT_SYMBOLS: cmdLSPServerExportSymbols();   return true;
        case IDM_LSP_SERVER_LAUNCH_STDIO:   cmdLSPServerLaunchStdio();     return true;
        default: return false;
    }
}

// ============================================================================
// IN-PROCESS LSP MESSAGE FORWARDING
// ============================================================================

void Win32IDE::forwardToLSPServer(const std::string& method, const json& params) {
    if (!m_lspServer || !m_lspServer->isRunning()) return;

    json request;
    request["jsonrpc"] = "2.0";
    request["method"]  = method;
    request["params"]  = params;

    // Notifications don't have an id
    if (method.find("textDocument/did") == 0 ||
        method == "initialized" || method == "exit") {
        m_lspServer->injectMessage(request.dump());
    } else {
        // Request — assign an id
        static std::atomic<int> nextId{50000};
        request["id"] = nextId.fetch_add(1);
        m_lspServer->injectMessage(request.dump());
    }
}

void Win32IDE::notifyLSPServerDidOpen(const std::string& uri,
                                       const std::string& languageId,
                                       const std::string& content) {
    json params;
    params["textDocument"]["uri"]        = uri;
    params["textDocument"]["languageId"] = languageId;
    params["textDocument"]["version"]    = 1;
    params["textDocument"]["text"]       = content;
    forwardToLSPServer("textDocument/didOpen", params);
}

void Win32IDE::notifyLSPServerDidChange(const std::string& uri,
                                         const std::string& content,
                                         int version) {
    json params;
    params["textDocument"]["uri"]     = uri;
    params["textDocument"]["version"] = version;
    params["contentChanges"]                      = json::array();
    params["contentChanges"][(size_t)0]["text"] = content;
    forwardToLSPServer("textDocument/didChange", params);
}

void Win32IDE::notifyLSPServerDidClose(const std::string& uri) {
    json params;
    params["textDocument"]["uri"] = uri;
    forwardToLSPServer("textDocument/didClose", params);
}
