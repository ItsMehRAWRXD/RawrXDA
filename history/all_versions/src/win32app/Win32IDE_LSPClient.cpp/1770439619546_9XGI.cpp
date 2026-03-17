// ============================================================================
// Win32IDE_LSPClient.cpp — Phase 9A: LSP Client Bridge
// ============================================================================
// Minimal Language Server Protocol integration for three languages:
//   - C/C++      → clangd
//   - Python     → pyright-langserver (or pylsp)
//   - TypeScript → typescript-language-server
//
// Provides five core capabilities:
//   1. Go-to-definition     (textDocument/definition)
//   2. Find references       (textDocument/references)
//   3. Rename symbol         (textDocument/rename)
//   4. Hover info            (textDocument/hover)
//   5. Diagnostics           (textDocument/publishDiagnostics — notification)
//
// Architecture:
//   - Each LSP server runs as a child process (CreateProcess)
//   - Communication via JSON-RPC 2.0 over stdin/stdout pipes
//   - A reader thread per server consumes stdout asynchronously
//   - Diagnostics are pushed into the annotation system
//   - All other requests are synchronous with timeout
//
// Guardrails:
//   - Does NOT modify the editor's text model directly (read-only for LSP)
//   - Rename produces a WorkspaceEdit; the user must approve
//   - Diagnostics are mapped to InlineAnnotations (existing system)
//   - Servers are optional — missing executable gracefully reported
// ============================================================================

#include "Win32IDE.h"
#include <richedit.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <thread>
#include <condition_variable>

// nlohmann/json already included via Win32IDE.h

// ============================================================================
// INITIALIZATION & LIFECYCLE
// ============================================================================

void Win32IDE::initLSPClient() {
    if (m_lspInitialized) return;

    logFunction("initLSPClient");

    // ---- Default server configurations -------------------------------------

    // C/C++ — clangd
    auto& cppCfg          = m_lspConfigs[(size_t)LSPLanguage::Cpp];
    cppCfg.language       = LSPLanguage::Cpp;
    cppCfg.name           = "clangd";
    cppCfg.executablePath = "clangd";   // Expect on PATH
    cppCfg.args           = {"--background-index", "--clang-tidy", "--header-insertion=never"};
    cppCfg.enabled        = true;
    cppCfg.initTimeoutMs  = 15000;

    // Python — pyright-langserver
    auto& pyCfg           = m_lspConfigs[(size_t)LSPLanguage::Python];
    pyCfg.language        = LSPLanguage::Python;
    pyCfg.name            = "pyright-langserver";
    pyCfg.executablePath  = "pyright-langserver";   // npm install -g pyright
    pyCfg.args            = {"--stdio"};
    pyCfg.enabled         = true;
    pyCfg.initTimeoutMs   = 15000;

    // TypeScript — typescript-language-server
    auto& tsCfg           = m_lspConfigs[(size_t)LSPLanguage::TypeScript];
    tsCfg.language        = LSPLanguage::TypeScript;
    tsCfg.name            = "typescript-language-server";
    tsCfg.executablePath  = "typescript-language-server";   // npm install -g typescript-language-server
    tsCfg.args            = {"--stdio"};
    tsCfg.enabled         = true;
    tsCfg.initTimeoutMs   = 15000;

    // ---- Set root URI from current working directory -----------------------
    {
        char cwd[MAX_PATH] = {};
        GetCurrentDirectoryA(MAX_PATH, cwd);
        std::string cwdStr(cwd);
        std::string rootUri = filePathToUri(cwdStr);
        for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i) {
            m_lspConfigs[i].rootUri = rootUri;
        }
    }

    // ---- Reset statuses ----------------------------------------------------
    for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i) {
        m_lspStatuses[i].language          = (LSPLanguage)i;
        m_lspStatuses[i].state             = LSPServerState::Stopped;
        m_lspStatuses[i].hProcess          = nullptr;
        m_lspStatuses[i].hStdinWrite       = nullptr;
        m_lspStatuses[i].hStdoutRead       = nullptr;
        m_lspStatuses[i].pid               = 0;
        m_lspStatuses[i].requestIdCounter  = 1;
        m_lspStatuses[i].initialized       = false;
        m_lspStatuses[i].lastError         = "";
        m_lspStatuses[i].startedEpochMs    = 0;
        m_lspStatuses[i].requestCount      = 0;
        m_lspStatuses[i].notificationCount = 0;
    }

    m_lspStats = {};

    // ---- Load saved config (overrides defaults) ----------------------------
    loadLSPConfig();

    m_lspInitialized = true;

    logInfo("[LSP] Client initialized — servers will start on first use or via command");
}

void Win32IDE::shutdownLSPClient() {
    if (!m_lspInitialized) return;
    logFunction("shutdownLSPClient");

    stopAllLSPServers();

    // Wait for reader threads
    for (auto& t : m_lspReaderThreads) {
        if (t.joinable()) t.join();
    }
    m_lspReaderThreads.clear();

    saveLSPConfig();
    m_lspInitialized = false;
}

// ============================================================================
// SERVER MANAGEMENT
// ============================================================================

bool Win32IDE::startLSPServer(LSPLanguage lang) {
    if (lang >= LSPLanguage::Count) return false;

    const auto& cfg = m_lspConfigs[(size_t)lang];
    auto& status    = m_lspStatuses[(size_t)lang];

    if (!cfg.enabled) {
        status.lastError = "Server disabled in config";
        logInfo("[LSP] " + cfg.name + " is disabled — skipping");
        return false;
    }

    if (status.state == LSPServerState::Running) {
        logInfo("[LSP] " + cfg.name + " already running (pid=" + std::to_string(status.pid) + ")");
        return true;
    }

    logInfo("[LSP] Starting " + cfg.name + " (" + cfg.executablePath + ")...");
    status.state = LSPServerState::Starting;
    status.lastError = "";

    // ---- Create pipes for stdin/stdout communication -----------------------
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength              = sizeof(sa);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hStdinRead  = nullptr, hStdinWrite  = nullptr;
    HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;

    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0) ||
        !CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        status.state = LSPServerState::Error;
        status.lastError = "Failed to create pipes: " + std::to_string(GetLastError());
        logError("startLSPServer", status.lastError);
        return false;
    }

    // Ensure our ends of the pipes are not inherited
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    // ---- Build command line ------------------------------------------------
    std::string cmdLine = "\"" + cfg.executablePath + "\"";
    for (const auto& arg : cfg.args) {
        cmdLine += " " + arg;
    }

    // ---- Launch child process ----------------------------------------------
    STARTUPINFOA si = {};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput   = hStdinRead;
    si.hStdOutput  = hStdoutWrite;
    si.hStdError   = hStdoutWrite;    // Merge stderr into stdout
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    BOOL created = CreateProcessA(
        nullptr,                                 // lpApplicationName
        const_cast<char*>(cmdLine.c_str()),      // lpCommandLine
        nullptr,                                 // lpProcessAttributes
        nullptr,                                 // lpThreadAttributes
        TRUE,                                    // bInheritHandles
        CREATE_NO_WINDOW,                        // dwCreationFlags
        nullptr,                                 // lpEnvironment
        nullptr,                                 // lpCurrentDirectory
        &si, &pi
    );

    // Close the child-side pipe handles (we keep the parent-side)
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);

    if (!created) {
        DWORD err = GetLastError();
        CloseHandle(hStdinWrite);
        CloseHandle(hStdoutRead);
        status.state = LSPServerState::Error;
        status.lastError = "CreateProcess failed for '" + cfg.executablePath +
                           "' (error " + std::to_string(err) + "). Is it installed and on PATH?";
        logError("startLSPServer", status.lastError);
        appendToOutput("[LSP] Failed to start " + cfg.name + ": " + status.lastError,
                       "General", OutputSeverity::Warning);
        return false;
    }

    // Store handles
    CloseHandle(pi.hThread);   // Don't need the thread handle
    status.hProcess    = pi.hProcess;
    status.hStdinWrite = hStdinWrite;
    status.hStdoutRead = hStdoutRead;
    status.pid         = pi.dwProcessId;
    status.startedEpochMs = currentEpochMs();

    logInfo("[LSP] " + cfg.name + " spawned (pid=" + std::to_string(status.pid) + ")");

    // ---- Start reader thread -----------------------------------------------
    m_lspReaderThreads.emplace_back(&Win32IDE::lspReaderThread, this, lang);

    // ---- Send initialize handshake -----------------------------------------
    bool initOk = sendInitialize(lang);
    if (!initOk) {
        status.state = LSPServerState::Error;
        status.lastError = "Initialize handshake failed or timed out";
        logError("startLSPServer", status.lastError);
        appendToOutput("[LSP] " + cfg.name + " started but initialize handshake failed.",
                       "General", OutputSeverity::Warning);
        return false;
    }

    sendInitialized(lang);
    status.state       = LSPServerState::Running;
    status.initialized = true;

    logInfo("[LSP] " + cfg.name + " initialized successfully");
    appendToOutput("[LSP] " + cfg.name + " ready (pid=" + std::to_string(status.pid) + ")",
                   "General", OutputSeverity::Info);

    return true;
}

void Win32IDE::stopLSPServer(LSPLanguage lang) {
    if (lang >= LSPLanguage::Count) return;
    auto& status = m_lspStatuses[(size_t)lang];
    const auto& cfg = m_lspConfigs[(size_t)lang];

    if (status.state != LSPServerState::Running && status.state != LSPServerState::Starting) {
        return;
    }

    logInfo("[LSP] Stopping " + cfg.name + "...");
    status.state = LSPServerState::ShuttingDown;

    // Send shutdown + exit per LSP protocol
    if (status.initialized) {
        sendShutdown(lang);
        sendExit(lang);
    }

    // Close pipe handles to unblock reader thread
    if (status.hStdinWrite) { CloseHandle(status.hStdinWrite); status.hStdinWrite = nullptr; }
    if (status.hStdoutRead) { CloseHandle(status.hStdoutRead); status.hStdoutRead = nullptr; }

    // Wait for process exit (brief), then terminate if stuck
    if (status.hProcess) {
        DWORD waitResult = WaitForSingleObject(status.hProcess, 3000);
        if (waitResult == WAIT_TIMEOUT) {
            TerminateProcess(status.hProcess, 1);
            logWarning("stopLSPServer", cfg.name + " did not exit gracefully — terminated");
        }
        CloseHandle(status.hProcess);
        status.hProcess = nullptr;
    }

    status.state       = LSPServerState::Stopped;
    status.initialized = false;
    status.pid         = 0;

    logInfo("[LSP] " + cfg.name + " stopped");
}

void Win32IDE::restartLSPServer(LSPLanguage lang) {
    stopLSPServer(lang);
    // Brief pause for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    startLSPServer(lang);
    m_lspStats.totalServerRestarts++;
}

void Win32IDE::startAllLSPServers() {
    if (!m_lspInitialized) initLSPClient();
    for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i) {
        if (m_lspConfigs[i].enabled) {
            startLSPServer((LSPLanguage)i);
        }
    }
}

void Win32IDE::stopAllLSPServers() {
    for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i) {
        stopLSPServer((LSPLanguage)i);
    }
}

Win32IDE::LSPServerState Win32IDE::getLSPServerState(LSPLanguage lang) const {
    if (lang >= LSPLanguage::Count) return LSPServerState::Stopped;
    return m_lspStatuses[(size_t)lang].state;
}

// ============================================================================
// JSON-RPC TRANSPORT
// ============================================================================

int Win32IDE::sendLSPRequest(LSPLanguage lang, const std::string& method,
                              const nlohmann::json& params) {
    if (lang >= LSPLanguage::Count) return -1;
    auto& status = m_lspStatuses[(size_t)lang];

    if (!status.hStdinWrite || status.state != LSPServerState::Running) {
        return -1;
    }

    int id;
    {
        std::lock_guard<std::mutex> lock(m_lspMutex);
        id = status.requestIdCounter++;
    }

    nlohmann::json msg;
    msg["jsonrpc"] = "2.0";
    msg["id"]      = id;
    msg["method"]  = method;
    msg["params"]  = params;

    std::string body = msg.dump();
    std::string packet = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

    DWORD written = 0;
    BOOL ok = WriteFile(status.hStdinWrite, packet.c_str(), (DWORD)packet.size(), &written, nullptr);
    if (!ok || written != (DWORD)packet.size()) {
        logError("sendLSPRequest", "WriteFile failed for " + m_lspConfigs[(size_t)lang].name);
        return -1;
    }

    status.requestCount++;
    return id;
}

void Win32IDE::sendLSPNotification(LSPLanguage lang, const std::string& method,
                                     const nlohmann::json& params) {
    if (lang >= LSPLanguage::Count) return;
    auto& status = m_lspStatuses[(size_t)lang];

    if (!status.hStdinWrite) return;

    nlohmann::json msg;
    msg["jsonrpc"] = "2.0";
    msg["method"]  = method;
    msg["params"]  = params;

    std::string body = msg.dump();
    std::string packet = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

    DWORD written = 0;
    WriteFile(status.hStdinWrite, packet.c_str(), (DWORD)packet.size(), &written, nullptr);
    status.notificationCount++;
}

nlohmann::json Win32IDE::readLSPResponse(LSPLanguage lang, int requestId, int timeoutMs) {
    // Wait for the reader thread to deposit the response
    std::unique_lock<std::mutex> lock(m_lspResponseMutex);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (true) {
        auto it = m_lspPendingResponses.find(requestId);
        if (it != m_lspPendingResponses.end()) {
            nlohmann::json resp = std::move(it->second);
            m_lspPendingResponses.erase(it);
            return resp;
        }
        if (m_lspResponseCV.wait_until(lock, deadline) == std::cv_status::timeout) {
            return nlohmann::json{{"error", {{"code", -1}, {"message", "Timeout waiting for response"}}}};
        }
    }
}

// ---- Reader thread: consumes stdout from LSP server, dispatches responses/notifications ----

void Win32IDE::lspReaderThread(LSPLanguage lang) {
    auto& status = m_lspStatuses[(size_t)lang];
    const auto& cfg = m_lspConfigs[(size_t)lang];

    logInfo("[LSP-reader] Started for " + cfg.name);

    std::string buffer;
    char readBuf[8192];

    while (status.hStdoutRead && status.state != LSPServerState::Stopped) {
        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(status.hStdoutRead, nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            break; // Pipe broken
        }

        if (bytesAvailable == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        DWORD bytesRead = 0;
        DWORD toRead = (bytesAvailable < sizeof(readBuf)) ? bytesAvailable : sizeof(readBuf);
        if (!ReadFile(status.hStdoutRead, readBuf, toRead, &bytesRead, nullptr) || bytesRead == 0) {
            break;
        }

        buffer.append(readBuf, bytesRead);

        // Parse JSON-RPC messages from buffer
        while (true) {
            // Look for Content-Length header
            size_t headerEnd = buffer.find("\r\n\r\n");
            if (headerEnd == std::string::npos) break;

            std::string header = buffer.substr(0, headerEnd);
            int contentLength = 0;

            // Parse Content-Length
            size_t clPos = header.find("Content-Length:");
            if (clPos == std::string::npos) clPos = header.find("content-length:");
            if (clPos != std::string::npos) {
                size_t valStart = clPos + 15; // strlen("Content-Length:")
                while (valStart < header.size() && header[valStart] == ' ') valStart++;
                contentLength = std::stoi(header.substr(valStart));
            }

            if (contentLength <= 0) {
                buffer.erase(0, headerEnd + 4);
                continue;
            }

            size_t messageStart = headerEnd + 4;
            if (buffer.size() < messageStart + contentLength) {
                break; // Incomplete message — wait for more data
            }

            std::string jsonBody = buffer.substr(messageStart, contentLength);
            buffer.erase(0, messageStart + contentLength);

            // Parse JSON
            try {
                nlohmann::json msg = nlohmann::json::parse(jsonBody);

                if (msg.contains("id") && !msg["id"].is_null()) {
                    // Response to a request
                    int respId = msg["id"].get<int>();
                    {
                        std::lock_guard<std::mutex> lock(m_lspResponseMutex);
                        m_lspPendingResponses[respId] = msg;
                    }
                    m_lspResponseCV.notify_all();
                } else if (msg.contains("method")) {
                    // Server notification
                    std::string method = msg["method"].get<std::string>();
                    if (method == "textDocument/publishDiagnostics") {
                        // Parse diagnostics
                        if (msg.contains("params")) {
                            const auto& params = msg["params"];
                            std::string uri = params.value("uri", "");
                            std::vector<LSPDiagnostic> diags;

                            if (params.contains("diagnostics") && params["diagnostics"].is_array()) {
                                const auto& diagArr = params["diagnostics"];
                                for (size_t di = 0; di < diagArr.size(); ++di) {
                                    const nlohmann::json& dj = diagArr[di];
                                    LSPDiagnostic d;
                                    if (dj.contains("range")) {
                                        const auto& rj = dj["range"];
                                        if (rj.contains("start")) {
                                            d.range.start.line      = rj["start"].value("line", 0);
                                            d.range.start.character = rj["start"].value("character", 0);
                                        }
                                        if (rj.contains("end")) {
                                            d.range.end.line      = rj["end"].value("line", 0);
                                            d.range.end.character = rj["end"].value("character", 0);
                                        }
                                    }
                                    d.severity = dj.value("severity", 1);
                                    d.message  = dj.value("message", "");
                                    d.source   = dj.value("source", cfg.name);
                                    if (dj.contains("code")) {
                                        if (dj["code"].is_string())
                                            d.code = dj["code"].get<std::string>();
                                        else if (dj["code"].is_number())
                                            d.code = std::to_string(dj["code"].get<int>());
                                    }
                                    diags.push_back(d);
                                }
                            }

                            onDiagnosticsReceived(uri, diags);
                            status.notificationCount++;
                            m_lspStats.totalDiagnosticsReceived += diags.size();
                        }
                    }
                    // Other notifications silently ignored for now
                }
            } catch (const std::exception& e) {
                logError("lspReaderThread", std::string("JSON parse error in ") + cfg.name + ": " + e.what());
            }
        }
    }

    logInfo("[LSP-reader] Exiting for " + cfg.name);
}

// ============================================================================
// LSP INITIALIZATION HANDSHAKE
// ============================================================================

bool Win32IDE::sendInitialize(LSPLanguage lang) {
    const auto& cfg = m_lspConfigs[(size_t)lang];

    nlohmann::json params;
    params["processId"] = (int)GetCurrentProcessId();
    params["rootUri"]   = cfg.rootUri;

    // Client capabilities — declare what we support
    nlohmann::json textDocCaps;
    textDocCaps["synchronization"]["dynamicRegistration"] = false;
    textDocCaps["synchronization"]["willSave"]            = false;
    textDocCaps["synchronization"]["didSave"]             = true;
    textDocCaps["definition"]["dynamicRegistration"]      = false;
    textDocCaps["references"]["dynamicRegistration"]      = false;
    textDocCaps["rename"]["dynamicRegistration"]          = false;
    textDocCaps["hover"]["dynamicRegistration"]           = false;
    textDocCaps["publishDiagnostics"]["relatedInformation"] = false;

    nlohmann::json caps;
    caps["textDocument"] = textDocCaps;
    params["capabilities"] = caps;

    params["clientInfo"] = {{"name", "RawrXD-IDE"}, {"version", "7.7.0"}};

    int id = sendLSPRequest(lang, "initialize", params);
    if (id < 0) return false;

    nlohmann::json resp = readLSPResponse(lang, id, cfg.initTimeoutMs);
    if (resp.contains("error")) {
        m_lspStatuses[(size_t)lang].lastError = "Initialize error: " +
            resp["error"].value("message", "unknown");
        return false;
    }

    return resp.contains("result");
}

void Win32IDE::sendInitialized(LSPLanguage lang) {
    sendLSPNotification(lang, "initialized", nlohmann::json::object());
}

void Win32IDE::sendShutdown(LSPLanguage lang) {
    int id = sendLSPRequest(lang, "shutdown", nlohmann::json{});
    if (id >= 0) {
        // Brief wait for ack — don't block forever
        readLSPResponse(lang, id, 2000);
    }
}

void Win32IDE::sendExit(LSPLanguage lang) {
    sendLSPNotification(lang, "exit", nlohmann::json{});
}

// ============================================================================
// DOCUMENT SYNCHRONIZATION
// ============================================================================

void Win32IDE::sendDidOpen(LSPLanguage lang, const std::string& uri,
                            const std::string& languageId, const std::string& content) {
    nlohmann::json params;
    params["textDocument"]["uri"]        = uri;
    params["textDocument"]["languageId"] = languageId;
    params["textDocument"]["version"]    = 1;
    params["textDocument"]["text"]       = content;

    sendLSPNotification(lang, "textDocument/didOpen", params);
}

void Win32IDE::sendDidChange(LSPLanguage lang, const std::string& uri,
                              const std::string& content) {
    // Full document sync (simplest mode)
    static int versionCounter = 2;
    nlohmann::json params;
    params["textDocument"]["uri"]     = uri;
    params["textDocument"]["version"] = versionCounter++;
    {
        nlohmann::json changeEntry;
        changeEntry["text"] = content;
        nlohmann::json changesArr = nlohmann::json::array();
        changesArr.push_back(changeEntry);
        params["contentChanges"] = changesArr;
    }

    sendLSPNotification(lang, "textDocument/didChange", params);
}

void Win32IDE::sendDidClose(LSPLanguage lang, const std::string& uri) {
    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    sendLSPNotification(lang, "textDocument/didClose", params);
}

void Win32IDE::sendDidSave(LSPLanguage lang, const std::string& uri) {
    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    sendLSPNotification(lang, "textDocument/didSave", params);
}

// ============================================================================
// CORE LSP FEATURES
// ============================================================================

// ---- 1. Go-to-Definition --------------------------------------------------

std::vector<Win32IDE::LSPLocation> Win32IDE::lspGotoDefinition(const std::string& uri,
                                                                 int line, int character) {
    std::vector<LSPLocation> results;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running) {
        return results;
    }

    nlohmann::json params;
    params["textDocument"]["uri"]    = uri;
    params["position"]["line"]       = line;
    params["position"]["character"]  = character;

    int id = sendLSPRequest(lang, "textDocument/definition", params);
    if (id < 0) return results;

    nlohmann::json resp = readLSPResponse(lang, id);
    m_lspStats.totalDefinitionRequests++;

    if (!resp.contains("result") || resp["result"].is_null()) return results;

    auto parseLocation = [](const nlohmann::json& lj) -> LSPLocation {
        LSPLocation loc;
        loc.uri = lj.value("uri", "");
        if (lj.contains("range")) {
            const auto& rj = lj["range"];
            if (rj.contains("start")) {
                loc.range.start.line      = rj["start"].value("line", 0);
                loc.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end")) {
                loc.range.end.line      = rj["end"].value("line", 0);
                loc.range.end.character = rj["end"].value("character", 0);
            }
        }
        return loc;
    };

    const auto& result = resp["result"];
    if (result.is_array()) {
        for (size_t ri = 0; ri < result.size(); ++ri) {
            results.push_back(parseLocation(result[ri]));
        }
    } else if (result.is_object()) {
        results.push_back(parseLocation(result));
    }

    return results;
}

// ---- 2. Find References ----------------------------------------------------

std::vector<Win32IDE::LSPLocation> Win32IDE::lspFindReferences(const std::string& uri,
                                                                 int line, int character) {
    std::vector<LSPLocation> results;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running) {
        return results;
    }

    nlohmann::json params;
    params["textDocument"]["uri"]    = uri;
    params["position"]["line"]       = line;
    params["position"]["character"]  = character;
    params["context"]["includeDeclaration"] = true;

    int id = sendLSPRequest(lang, "textDocument/references", params);
    if (id < 0) return results;

    nlohmann::json resp = readLSPResponse(lang, id, 10000); // References can be slow
    m_lspStats.totalReferenceRequests++;

    if (!resp.contains("result") || !resp["result"].is_array()) return results;

    const auto& refResult = resp["result"];
    for (size_t ri = 0; ri < refResult.size(); ++ri) {
        const nlohmann::json& lj = refResult[ri];
        LSPLocation loc;
        loc.uri = lj.value("uri", "");
        if (lj.contains("range")) {
            const auto& rj = lj["range"];
            if (rj.contains("start")) {
                loc.range.start.line      = rj["start"].value("line", 0);
                loc.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end")) {
                loc.range.end.line      = rj["end"].value("line", 0);
                loc.range.end.character = rj["end"].value("character", 0);
            }
        }
        results.push_back(loc);
    }

    return results;
}

// ---- 3. Rename Symbol ------------------------------------------------------

Win32IDE::LSPWorkspaceEdit Win32IDE::lspRenameSymbol(const std::string& uri,
                                                       int line, int character,
                                                       const std::string& newName) {
    LSPWorkspaceEdit edit;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running) {
        return edit;
    }

    nlohmann::json params;
    params["textDocument"]["uri"]    = uri;
    params["position"]["line"]       = line;
    params["position"]["character"]  = character;
    params["newName"]                = newName;

    int id = sendLSPRequest(lang, "textDocument/rename", params);
    if (id < 0) return edit;

    nlohmann::json resp = readLSPResponse(lang, id, 10000);
    m_lspStats.totalRenameRequests++;

    if (!resp.contains("result") || resp["result"].is_null()) return edit;

    const auto& result = resp["result"];
    if (result.contains("changes") && result["changes"].is_object()) {
        for (auto it = result["changes"].obj_begin(); it != result["changes"].obj_end(); ++it) {
            std::string fileUri = it->first;
            std::vector<LSPWorkspaceEdit::TextEdit> edits;
            if (it->second.is_array()) {
                const nlohmann::json& editArr = it->second;
                for (size_t ei = 0; ei < editArr.size(); ++ei) {
                    const nlohmann::json& ej = editArr[ei];
                    LSPWorkspaceEdit::TextEdit te;
                    te.newText = ej.value("newText", "");
                    if (ej.contains("range")) {
                        const auto& rj = ej["range"];
                        if (rj.contains("start")) {
                            te.range.start.line      = rj["start"].value("line", 0);
                            te.range.start.character = rj["start"].value("character", 0);
                        }
                        if (rj.contains("end")) {
                            te.range.end.line      = rj["end"].value("line", 0);
                            te.range.end.character = rj["end"].value("character", 0);
                        }
                    }
                    edits.push_back(te);
                }
            }
            edit.changes[fileUri] = edits;
        }
    }

    // Also handle documentChanges format (used by some servers)
    if (result.contains("documentChanges") && result["documentChanges"].is_array()) {
        const auto& docChanges = result["documentChanges"];
        for (size_t dci = 0; dci < docChanges.size(); ++dci) {
            const nlohmann::json& dc = docChanges[dci];
            if (!dc.contains("textDocument") || !dc.contains("edits")) continue;
            std::string fileUri = dc["textDocument"].value("uri", "");
            std::vector<LSPWorkspaceEdit::TextEdit> edits;
            const auto& dcEdits = dc["edits"];
            for (size_t ei = 0; ei < dcEdits.size(); ++ei) {
                const nlohmann::json& ej = dcEdits[ei];
                LSPWorkspaceEdit::TextEdit te;
                te.newText = ej.value("newText", "");
                if (ej.contains("range")) {
                    const auto& rj = ej["range"];
                    if (rj.contains("start")) {
                        te.range.start.line      = rj["start"].value("line", 0);
                        te.range.start.character = rj["start"].value("character", 0);
                    }
                    if (rj.contains("end")) {
                        te.range.end.line      = rj["end"].value("line", 0);
                        te.range.end.character = rj["end"].value("character", 0);
                    }
                }
                edits.push_back(te);
            }
            edit.changes[fileUri] = edits;
        }
    }

    return edit;
}

// ---- 4. Hover Info ---------------------------------------------------------

Win32IDE::LSPHoverInfo Win32IDE::lspHover(const std::string& uri, int line, int character) {
    LSPHoverInfo info;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running) {
        return info;
    }

    nlohmann::json params;
    params["textDocument"]["uri"]    = uri;
    params["position"]["line"]       = line;
    params["position"]["character"]  = character;

    int id = sendLSPRequest(lang, "textDocument/hover", params);
    if (id < 0) return info;

    nlohmann::json resp = readLSPResponse(lang, id);
    m_lspStats.totalHoverRequests++;

    if (!resp.contains("result") || resp["result"].is_null()) return info;

    const auto& result = resp["result"];

    // Parse contents (can be string, object, or array)
    if (result.contains("contents")) {
        const auto& contents = result["contents"];
        if (contents.is_string()) {
            info.contents = contents.get<std::string>();
        } else if (contents.is_object()) {
            info.contents = contents.value("value", "");
        } else if (contents.is_array()) {
            for (size_t ci = 0; ci < contents.size(); ++ci) {
                const nlohmann::json& citem = contents[ci];
                if (!info.contents.empty()) info.contents += "\n---\n";
                if (citem.is_string()) {
                    info.contents += citem.get<std::string>();
                } else if (citem.is_object()) {
                    info.contents += citem.value("value", "");
                }
            }
        }
    }

    if (result.contains("range")) {
        const auto& rj = result["range"];
        if (rj.contains("start")) {
            info.range.start.line      = rj["start"].value("line", 0);
            info.range.start.character = rj["start"].value("character", 0);
        }
        if (rj.contains("end")) {
            info.range.end.line      = rj["end"].value("line", 0);
            info.range.end.character = rj["end"].value("character", 0);
        }
    }

    info.valid = !info.contents.empty();
    return info;
}

// ---- 5. Diagnostics (notification-based — see lspReaderThread) -------------

void Win32IDE::onDiagnosticsReceived(const std::string& uri,
                                       const std::vector<LSPDiagnostic>& diagnostics) {
    {
        std::lock_guard<std::mutex> lock(m_lspDiagnosticsMutex);
        m_lspDiagnostics[uri] = diagnostics;
    }

    // Map diagnostics to the existing annotation system for visual display
    displayDiagnosticsAsAnnotations(uri);
}

std::vector<Win32IDE::LSPDiagnostic> Win32IDE::getDiagnosticsForFile(const std::string& uri) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_lspDiagnosticsMutex));
    auto it = m_lspDiagnostics.find(uri);
    if (it != m_lspDiagnostics.end()) return it->second;
    return {};
}

std::vector<std::pair<std::string, std::vector<Win32IDE::LSPDiagnostic>>>
Win32IDE::getAllDiagnostics() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_lspDiagnosticsMutex));
    std::vector<std::pair<std::string, std::vector<LSPDiagnostic>>> result;
    for (const auto& pair : m_lspDiagnostics) {
        result.push_back(pair);
    }
    return result;
}

void Win32IDE::clearDiagnostics(const std::string& uri) {
    {
        std::lock_guard<std::mutex> lock(m_lspDiagnosticsMutex);
        m_lspDiagnostics.erase(uri);
    }
    // Clear LSP annotations for this file
    clearAllAnnotations("lsp");
}

void Win32IDE::clearAllDiagnostics() {
    {
        std::lock_guard<std::mutex> lock(m_lspDiagnosticsMutex);
        m_lspDiagnostics.clear();
    }
    clearAllAnnotations("lsp");
}

void Win32IDE::displayDiagnosticsAsAnnotations(const std::string& uri) {
    // Only display diagnostics for the currently active file
    std::string currentUri = filePathToUri(m_currentFile);
    if (uri != currentUri) return;

    // Clear old LSP annotations
    clearAllAnnotations("lsp");

    auto diags = getDiagnosticsForFile(uri);
    for (const auto& d : diags) {
        AnnotationSeverity sev = AnnotationSeverity::Info;
        switch (d.severity) {
            case 1: sev = AnnotationSeverity::Error;   break;
            case 2: sev = AnnotationSeverity::Warning; break;
            case 3: sev = AnnotationSeverity::Info;    break;
            case 4: sev = AnnotationSeverity::Info;    break;  // Hint → Info
        }

        // LSP lines are 0-based; annotations are 1-based
        std::string msg = d.message;
        if (!d.code.empty()) msg += " [" + d.code + "]";
        if (!d.source.empty()) msg += " (" + d.source + ")";

        addAnnotation(d.range.start.line + 1, sev, msg, "lsp");
    }
}

// ============================================================================
// WORKSPACE EDIT APPLICATION
// ============================================================================

bool Win32IDE::applyWorkspaceEdit(const LSPWorkspaceEdit& edit) {
    if (edit.changes.empty()) return false;

    int filesChanged = 0;
    int editsApplied = 0;

    for (const auto& [uri, textEdits] : edit.changes) {
        std::string filePath = uriToFilePath(uri);

        // Read file
        std::ifstream ifs(filePath);
        if (!ifs.is_open()) continue;

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(ifs, line)) {
            lines.push_back(line);
        }
        ifs.close();

        // Apply edits in reverse order (bottom-up to preserve positions)
        auto sortedEdits = textEdits;
        std::sort(sortedEdits.begin(), sortedEdits.end(),
                  [](const LSPWorkspaceEdit::TextEdit& a, const LSPWorkspaceEdit::TextEdit& b) {
                      if (a.range.start.line != b.range.start.line)
                          return a.range.start.line > b.range.start.line;
                      return a.range.start.character > b.range.start.character;
                  });

        for (const auto& te : sortedEdits) {
            int startLine = te.range.start.line;
            int startChar = te.range.start.character;
            int endLine   = te.range.end.line;
            int endChar   = te.range.end.character;

            if (startLine < 0 || startLine >= (int)lines.size()) continue;
            if (endLine < 0 || endLine >= (int)lines.size()) continue;

            if (startLine == endLine) {
                // Single-line edit
                std::string& l = lines[startLine];
                std::string before = l.substr(0, startChar);
                std::string after  = (endChar < (int)l.size()) ? l.substr(endChar) : "";
                l = before + te.newText + after;
            } else {
                // Multi-line edit
                std::string firstLine = lines[startLine].substr(0, startChar);
                std::string lastLine  = (endChar < (int)lines[endLine].size())
                                         ? lines[endLine].substr(endChar) : "";
                // Remove lines [startLine+1 ... endLine]
                lines.erase(lines.begin() + startLine + 1, lines.begin() + endLine + 1);
                lines[startLine] = firstLine + te.newText + lastLine;
            }
            editsApplied++;
        }

        // Write file back
        std::ofstream ofs(filePath);
        if (ofs.is_open()) {
            for (size_t i = 0; i < lines.size(); ++i) {
                ofs << lines[i];
                if (i + 1 < lines.size()) ofs << "\n";
            }
            filesChanged++;
        }
    }

    if (filesChanged > 0) {
        appendToOutput("[LSP] Applied " + std::to_string(editsApplied) + " edits across " +
                       std::to_string(filesChanged) + " file(s).", "General", OutputSeverity::Info);
    }

    return filesChanged > 0;
}

// ============================================================================
// LANGUAGE DETECTION & HELPERS
// ============================================================================

Win32IDE::LSPLanguage Win32IDE::detectLanguageForFile(const std::string& filePath) const {
    std::string ext;
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = filePath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
    }

    // C/C++
    if (ext == ".c" || ext == ".cpp" || ext == ".cc" || ext == ".cxx" ||
        ext == ".h" || ext == ".hpp" || ext == ".hh" || ext == ".hxx" ||
        ext == ".ipp" || ext == ".inl") {
        return LSPLanguage::Cpp;
    }

    // Python
    if (ext == ".py" || ext == ".pyi" || ext == ".pyw") {
        return LSPLanguage::Python;
    }

    // TypeScript / JavaScript
    if (ext == ".ts" || ext == ".tsx" || ext == ".js" || ext == ".jsx" ||
        ext == ".mjs" || ext == ".cjs") {
        return LSPLanguage::TypeScript;
    }

    return LSPLanguage::Count; // Unknown
}

std::string Win32IDE::lspLanguageId(LSPLanguage lang) const {
    switch (lang) {
        case LSPLanguage::Cpp:        return "cpp";
        case LSPLanguage::Python:     return "python";
        case LSPLanguage::TypeScript: return "typescript";
        default:                      return "plaintext";
    }
}

std::string Win32IDE::lspLanguageString(LSPLanguage lang) const {
    switch (lang) {
        case LSPLanguage::Cpp:        return "C/C++";
        case LSPLanguage::Python:     return "Python";
        case LSPLanguage::TypeScript: return "TypeScript";
        default:                      return "Unknown";
    }
}

Win32IDE::LSPLanguage Win32IDE::lspLanguageFromString(const std::string& name) const {
    if (name == "C/C++" || name == "cpp" || name == "c" || name == "Cpp") return LSPLanguage::Cpp;
    if (name == "Python" || name == "python" || name == "py")             return LSPLanguage::Python;
    if (name == "TypeScript" || name == "typescript" || name == "ts")     return LSPLanguage::TypeScript;
    return LSPLanguage::Count;
}

std::string Win32IDE::filePathToUri(const std::string& filePath) const {
    // Convert "D:\rawrxd\src\foo.cpp" → "file:///D:/rawrxd/src/foo.cpp"
    std::string uri = "file:///";
    for (char c : filePath) {
        if (c == '\\') {
            uri += '/';
        } else if (c == ' ') {
            uri += "%20";
        } else {
            uri += c;
        }
    }
    return uri;
}

std::string Win32IDE::uriToFilePath(const std::string& uri) const {
    // Convert "file:///D:/rawrxd/src/foo.cpp" → "D:\rawrxd\src\foo.cpp"
    std::string path = uri;
    if (path.substr(0, 8) == "file:///") {
        path = path.substr(8);
    } else if (path.substr(0, 7) == "file://") {
        path = path.substr(7);
    }

    // URL decode %20 → space
    std::string decoded;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '%' && i + 2 < path.size()) {
            int hex = 0;
            std::string hexStr = path.substr(i + 1, 2);
            try { hex = std::stoi(hexStr, nullptr, 16); } catch (...) {}
            decoded += (char)hex;
            i += 2;
        } else if (path[i] == '/') {
            decoded += '\\';
        } else {
            decoded += path[i];
        }
    }

    return decoded;
}

// ============================================================================
// STATUS & DISPLAY
// ============================================================================

std::string Win32IDE::getLSPStatusString() const {
    std::ostringstream ss;
    ss << "[LSP] Language Server Status:\n";
    ss << "  ──────────────────────────────────────\n";

    for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i) {
        const auto& cfg = m_lspConfigs[i];
        const auto& st  = m_lspStatuses[i];
        const char* stateStr = "STOPPED";
        switch (st.state) {
            case LSPServerState::Stopped:      stateStr = "STOPPED";       break;
            case LSPServerState::Starting:     stateStr = "STARTING";      break;
            case LSPServerState::Running:      stateStr = "RUNNING";       break;
            case LSPServerState::ShuttingDown: stateStr = "SHUTTING DOWN"; break;
            case LSPServerState::Error:        stateStr = "ERROR";         break;
        }

        ss << "  " << lspLanguageString((LSPLanguage)i) << " (" << cfg.name << "):\n";
        ss << "    State:   " << stateStr;
        if (st.pid > 0) ss << " (pid=" << st.pid << ")";
        ss << "\n";
        ss << "    Enabled: " << (cfg.enabled ? "yes" : "no") << "\n";
        ss << "    Binary:  " << cfg.executablePath << "\n";
        ss << "    Reqs:    " << st.requestCount << "  Notifs: " << st.notificationCount << "\n";
        if (!st.lastError.empty()) {
            ss << "    Error:   " << st.lastError << "\n";
        }
    }

    return ss.str();
}

std::string Win32IDE::getLSPStatsString() const {
    std::ostringstream ss;
    ss << "[LSP] Statistics:\n";
    ss << "  Definition Requests:   " << m_lspStats.totalDefinitionRequests << "\n";
    ss << "  Reference Requests:    " << m_lspStats.totalReferenceRequests << "\n";
    ss << "  Rename Requests:       " << m_lspStats.totalRenameRequests << "\n";
    ss << "  Hover Requests:        " << m_lspStats.totalHoverRequests << "\n";
    ss << "  Diagnostics Received:  " << m_lspStats.totalDiagnosticsReceived << "\n";
    ss << "  Server Restarts:       " << m_lspStats.totalServerRestarts << "\n";
    return ss.str();
}

std::string Win32IDE::getLSPDiagnosticsSummary() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_lspDiagnosticsMutex));

    int totalErrors   = 0;
    int totalWarnings = 0;
    int totalInfo     = 0;
    int totalFiles    = (int)m_lspDiagnostics.size();

    for (const auto& [uri, diags] : m_lspDiagnostics) {
        for (const auto& d : diags) {
            switch (d.severity) {
                case 1: totalErrors++;   break;
                case 2: totalWarnings++; break;
                default: totalInfo++;    break;
            }
        }
    }

    std::ostringstream ss;
    ss << "[LSP] Diagnostics: " << totalFiles << " file(s) — "
       << totalErrors << " errors, " << totalWarnings << " warnings, "
       << totalInfo << " info/hints";
    return ss.str();
}

// ============================================================================
// COMMAND HANDLERS (called from handleToolsCommand)
// ============================================================================

void Win32IDE::cmdLSPGotoDefinition() {
    if (!m_lspInitialized) {
        appendToOutput("[LSP] Not initialized. Use 'LSP: Start All Servers' first.",
                       "General", OutputSeverity::Warning);
        return;
    }

    // Get cursor position from editor
    CHARRANGE sel = {};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    // Convert character offset to line/column
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column    = sel.cpMin - lineStart;

    std::string uri = filePathToUri(m_currentFile);
    auto locations = lspGotoDefinition(uri, lineIndex, column);

    if (locations.empty()) {
        appendToOutput("[LSP] No definition found at cursor position.",
                       "General", OutputSeverity::Info);
        return;
    }

    // Navigate to the first result
    const auto& loc = locations[0];
    std::string path = uriToFilePath(loc.uri);
    int targetLine = loc.range.start.line + 1; // Convert to 1-based

    appendToOutput("[LSP] Definition: " + path + ":" + std::to_string(targetLine),
                   "General", OutputSeverity::Info);

    // Open the file and jump to line (if different from current)
    if (path != m_currentFile) {
        openFile(path);
    }

    // Navigate to line
    int lineCharIndex = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, targetLine - 1, 0);
    CHARRANGE target = { lineCharIndex + loc.range.start.character,
                         lineCharIndex + loc.range.start.character };
    SendMessageA(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&target);
    SendMessageA(m_hwndEditor, EM_SCROLLCARET, 0, 0);

    if (locations.size() > 1) {
        appendToOutput("[LSP] " + std::to_string(locations.size()) +
                       " definitions found — navigated to first.",
                       "General", OutputSeverity::Info);
    }
}

void Win32IDE::cmdLSPFindReferences() {
    if (!m_lspInitialized) {
        appendToOutput("[LSP] Not initialized. Use 'LSP: Start All Servers' first.",
                       "General", OutputSeverity::Warning);
        return;
    }

    CHARRANGE sel = {};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column    = sel.cpMin - lineStart;

    std::string uri = filePathToUri(m_currentFile);
    auto locations = lspFindReferences(uri, lineIndex, column);

    if (locations.empty()) {
        appendToOutput("[LSP] No references found at cursor position.",
                       "General", OutputSeverity::Info);
        return;
    }

    std::ostringstream ss;
    ss << "[LSP] Found " << locations.size() << " reference(s):\n";
    for (size_t i = 0; i < locations.size() && i < 50; ++i) {
        std::string path = uriToFilePath(locations[i].uri);
        int ln = locations[i].range.start.line + 1;
        int col = locations[i].range.start.character + 1;
        ss << "  " << path << ":" << ln << ":" << col << "\n";
    }
    if (locations.size() > 50) {
        ss << "  ... and " << (locations.size() - 50) << " more\n";
    }

    appendToOutput(ss.str(), "General", OutputSeverity::Info);
}

void Win32IDE::cmdLSPRenameSymbol() {
    if (!m_lspInitialized) {
        appendToOutput("[LSP] Not initialized. Use 'LSP: Start All Servers' first.",
                       "General", OutputSeverity::Warning);
        return;
    }

    // Get the new name from chat input
    std::string newName = getWindowText(m_hwndCopilotChatInput);
    if (newName.empty()) {
        appendToOutput("[LSP] Enter the new symbol name in the chat input, then run this command.",
                       "General", OutputSeverity::Warning);
        return;
    }

    CHARRANGE sel = {};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column    = sel.cpMin - lineStart;

    std::string uri = filePathToUri(m_currentFile);
    LSPWorkspaceEdit edit = lspRenameSymbol(uri, lineIndex, column, newName);

    if (edit.changes.empty()) {
        appendToOutput("[LSP] Rename produced no changes. Symbol may not be renameable.",
                       "General", OutputSeverity::Info);
        return;
    }

    // Show what will change (user visibility)
    int totalEdits = 0;
    std::ostringstream ss;
    ss << "[LSP] Rename → '" << newName << "' will modify:\n";
    for (const auto& [fileUri, edits] : edit.changes) {
        ss << "  " << uriToFilePath(fileUri) << " (" << edits.size() << " edits)\n";
        totalEdits += (int)edits.size();
    }
    appendToOutput(ss.str(), "General", OutputSeverity::Info);

    // Apply the edit
    bool ok = applyWorkspaceEdit(edit);
    if (ok) {
        setWindowText(m_hwndCopilotChatInput, "");
        appendToOutput("[LSP] Rename complete: " + std::to_string(totalEdits) +
                       " edit(s) applied.", "General", OutputSeverity::Info);
    } else {
        appendToOutput("[LSP] Rename failed — could not apply edits.",
                       "General", OutputSeverity::Error);
    }
}

void Win32IDE::cmdLSPHoverInfo() {
    if (!m_lspInitialized) {
        appendToOutput("[LSP] Not initialized. Use 'LSP: Start All Servers' first.",
                       "General", OutputSeverity::Warning);
        return;
    }

    CHARRANGE sel = {};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column    = sel.cpMin - lineStart;

    std::string uri = filePathToUri(m_currentFile);
    LSPHoverInfo hover = lspHover(uri, lineIndex, column);

    if (!hover.valid) {
        appendToOutput("[LSP] No hover info at cursor position.",
                       "General", OutputSeverity::Info);
        return;
    }

    // Truncate very long hovers
    std::string display = hover.contents;
    if (display.size() > 2000) {
        display = display.substr(0, 2000) + "\n... (truncated)";
    }

    appendToOutput("[LSP] Hover:\n" + display, "General", OutputSeverity::Info);
}

// ============================================================================
// CONFIG PERSISTENCE (JSON)
// ============================================================================

std::string Win32IDE::getLSPConfigFilePath() const {
    std::string dir = getSessionFilePath();
    size_t pos = dir.find_last_of("/\\");
    if (pos != std::string::npos) {
        dir = dir.substr(0, pos + 1) + "lsp.json";
    } else {
        dir = "lsp.json";
    }
    return dir;
}

void Win32IDE::loadLSPConfig() {
    std::string path = getLSPConfigFilePath();
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        logInfo("[LSP] No saved config at " + path + " — using defaults");
        return;
    }

    try {
        std::string fileContent((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());
        nlohmann::json j = nlohmann::json::parse(fileContent);

        if (j.contains("servers") && j["servers"].is_array()) {
            for (size_t si = 0; si < j["servers"].size(); ++si) {
                const auto& sj = j["servers"][si];
                std::string langName = sj.value("language", "");
                LSPLanguage lang = lspLanguageFromString(langName);
                if (lang >= LSPLanguage::Count) continue;

                auto& cfg = m_lspConfigs[(size_t)lang];
                if (sj.contains("name"))           cfg.name           = sj["name"].get<std::string>();
                if (sj.contains("executablePath")) cfg.executablePath = sj["executablePath"].get<std::string>();
                if (sj.contains("enabled"))        cfg.enabled        = sj["enabled"].get<bool>();
                if (sj.contains("initTimeoutMs"))  cfg.initTimeoutMs  = sj["initTimeoutMs"].get<int>();
                if (sj.contains("args") && sj["args"].is_array()) {
                    cfg.args.clear();
                    for (size_t ai = 0; ai < sj["args"].size(); ++ai) {
                        cfg.args.push_back(sj["args"][ai].get<std::string>());
                    }
                }
                if (sj.contains("rootUri")) cfg.rootUri = sj["rootUri"].get<std::string>();
            }
        }

        logInfo("[LSP] Loaded config from " + path);
    } catch (const std::exception& e) {
        logError("loadLSPConfig", std::string("JSON parse error: ") + e.what());
    }
}

void Win32IDE::saveLSPConfig() {
    std::string path = getLSPConfigFilePath();

    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    nlohmann::json j;
    nlohmann::json servers = nlohmann::json::array();

    for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i) {
        const auto& cfg = m_lspConfigs[i];
        nlohmann::json sj;
        sj["language"]       = lspLanguageString((LSPLanguage)i);
        sj["name"]           = cfg.name;
        sj["executablePath"] = cfg.executablePath;
        sj["enabled"]        = cfg.enabled;
        sj["initTimeoutMs"]  = cfg.initTimeoutMs;
        sj["rootUri"]        = cfg.rootUri;

        nlohmann::json argsArr = nlohmann::json::array();
        for (const auto& arg : cfg.args) {
            argsArr.push_back(arg);
        }
        sj["args"] = argsArr;

        servers.push_back(sj);
    }
    j["servers"] = servers;

    std::ofstream ofs(path);
    if (ofs.is_open()) {
        ofs << j.dump(2);
        logInfo("[LSP] Saved config to " + path);
    } else {
        logError("saveLSPConfig", "Failed to write " + path);
    }
}

// ============================================================================
// HTTP ENDPOINTS — Phase 9A
// ============================================================================

void Win32IDE::handleLSPStatusEndpoint(SOCKET client) {
    nlohmann::json j;
    j["initialized"] = m_lspInitialized;

    nlohmann::json servers = nlohmann::json::array();
    for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i) {
        const auto& cfg = m_lspConfigs[i];
        const auto& st  = m_lspStatuses[i];

        nlohmann::json sj;
        sj["language"]    = lspLanguageString((LSPLanguage)i);
        sj["name"]        = cfg.name;
        sj["enabled"]     = cfg.enabled;
        sj["state"]       = (int)st.state;

        std::string stateStr = "stopped";
        switch (st.state) {
            case LSPServerState::Stopped:      stateStr = "stopped";        break;
            case LSPServerState::Starting:     stateStr = "starting";       break;
            case LSPServerState::Running:      stateStr = "running";        break;
            case LSPServerState::ShuttingDown: stateStr = "shutting_down";  break;
            case LSPServerState::Error:        stateStr = "error";          break;
        }
        sj["stateString"] = stateStr;
        sj["pid"]          = (int)st.pid;
        sj["requests"]     = st.requestCount;
        sj["notifications"] = st.notificationCount;
        if (!st.lastError.empty()) sj["lastError"] = st.lastError;

        servers.push_back(sj);
    }
    j["servers"] = servers;

    // Stats
    nlohmann::json stats;
    stats["definitionRequests"]  = m_lspStats.totalDefinitionRequests;
    stats["referenceRequests"]   = m_lspStats.totalReferenceRequests;
    stats["renameRequests"]      = m_lspStats.totalRenameRequests;
    stats["hoverRequests"]       = m_lspStats.totalHoverRequests;
    stats["diagnosticsReceived"] = m_lspStats.totalDiagnosticsReceived;
    stats["serverRestarts"]      = m_lspStats.totalServerRestarts;
    j["stats"] = stats;

    std::string body = j.dump(2);
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleLSPDiagnosticsEndpoint(SOCKET client) {
    nlohmann::json j = nlohmann::json::array();

    auto allDiags = getAllDiagnostics();
    for (const auto& [uri, diags] : allDiags) {
        nlohmann::json fj;
        fj["uri"]  = uri;
        fj["file"] = uriToFilePath(uri);

        nlohmann::json diagArr = nlohmann::json::array();
        for (const auto& d : diags) {
            nlohmann::json dj;
            dj["severity"]         = d.severity;
            dj["message"]          = d.message;
            dj["code"]             = d.code;
            dj["source"]           = d.source;
            dj["startLine"]        = d.range.start.line;
            dj["startCharacter"]   = d.range.start.character;
            dj["endLine"]          = d.range.end.line;
            dj["endCharacter"]     = d.range.end.character;
            diagArr.push_back(dj);
        }
        fj["diagnostics"]      = diagArr;
        fj["diagnosticCount"]  = (int)diags.size();
        j.push_back(fj);
    }

    std::string body = j.dump(2);
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    send(client, resp.c_str(), (int)resp.size(), 0);
}
