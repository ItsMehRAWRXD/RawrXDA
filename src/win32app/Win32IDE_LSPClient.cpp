// Win32IDE_LSPClient.cpp — JSON-RPC LSP client (clangd / pyright / tsserver) over stdin/stdout.

#include "Win32IDE.h"
#include "Win32IDE_Types.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <richedit.h>
#include <sstream>
#include <thread>
#include <unordered_set>

static constexpr size_t kMaxLspMessageBytes = 4u * 1024u * 1024u;
static constexpr size_t kMaxLspBufferBytes = 8u * 1024u * 1024u;
static constexpr size_t kMaxLspConfigBytes = 2u * 1024u * 1024u;
static constexpr size_t kMaxLspArgs = 64u;
static constexpr size_t kMaxLspArgBytes = 1024u;

// Standard LSP semantic token types (index order per spec)
static const char* s_semanticTokenTypes[] = {
    "namespace",      // 0
    "type",           // 1
    "class",          // 2
    "enum",           // 3
    "interface",      // 4
    "struct",         // 5
    "typeParameter",  // 6
    "parameter",      // 7
    "variable",       // 8
    "property",       // 9
    "enumMember",     // 10
    "event",          // 11
    "function",       // 12
    "method",         // 13
    "macro",          // 14
    "keyword",        // 15
    "modifier",       // 16
    "comment",        // 17
    "string",         // 18
    "number",         // 19
    "regexp",         // 20
    "operator",       // 21
    "decorator",      // 22
};
static constexpr int s_numSemanticTokenTypes = sizeof(s_semanticTokenTypes) / sizeof(s_semanticTokenTypes[0]);

// nlohmann/json already included via Win32IDE.h

void Win32IDE::initLSPClient()
{
    if (m_lspInitialized)
        return;

    logFunction("initLSPClient");

    // C/C++ — clangd
    auto& cppCfg = m_lspConfigs[(size_t)LSPLanguage::Cpp];
    cppCfg.language = LSPLanguage::Cpp;
    cppCfg.name = "clangd";
    cppCfg.executablePath = "clangd";  // Expect on PATH
    cppCfg.args = {"--background-index", "--clang-tidy", "--header-insertion=never"};
    cppCfg.enabled = true;
    cppCfg.initTimeoutMs = 15000;

    // Python — pyright-langserver
    auto& pyCfg = m_lspConfigs[(size_t)LSPLanguage::Python];
    pyCfg.language = LSPLanguage::Python;
    pyCfg.name = "pyright-langserver";
    pyCfg.executablePath = "pyright-langserver";  // npm install -g pyright
    pyCfg.args = {"--stdio"};
    pyCfg.enabled = true;
    pyCfg.initTimeoutMs = 15000;

    // TypeScript — typescript-language-server
    auto& tsCfg = m_lspConfigs[(size_t)LSPLanguage::TypeScript];
    tsCfg.language = LSPLanguage::TypeScript;
    tsCfg.name = "typescript-language-server";
    tsCfg.executablePath = "typescript-language-server";  // npm install -g typescript-language-server
    tsCfg.args = {"--stdio"};
    tsCfg.enabled = true;
    tsCfg.initTimeoutMs = 15000;

    // ---- Set root URI from current working directory -----------------------
    {
        char cwd[MAX_PATH] = {};
        GetCurrentDirectoryA(MAX_PATH, cwd);
        std::string cwdStr(cwd);
        std::string rootUri = filePathToUri(cwdStr);
        for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i)
        {
            m_lspConfigs[i].rootUri = rootUri;
        }
    }

    // ---- Reset statuses ----------------------------------------------------
    for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i)
    {
        m_lspStatuses[i].language = (LSPLanguage)i;
        m_lspStatuses[i].state = LSPServerState::Stopped;
        m_lspStatuses[i].hProcess = nullptr;
        m_lspStatuses[i].hStdinWrite = nullptr;
        m_lspStatuses[i].hStdoutRead = nullptr;
        m_lspStatuses[i].pid = 0;
        m_lspStatuses[i].requestIdCounter = 1;
        m_lspStatuses[i].initialized = false;
        m_lspStatuses[i].lastError = "";
        m_lspStatuses[i].startedEpochMs = 0;
        m_lspStatuses[i].requestCount = 0;
        m_lspStatuses[i].notificationCount = 0;
    }

    m_lspStats = {};

    // ---- Load saved config (overrides defaults) ----------------------------
    loadLSPConfig();

    m_lspInitialized = true;

    logInfo("[LSP] Client initialized — servers will start on first use or via command");
}

void Win32IDE::shutdownLSPClient()
{
    if (!m_lspInitialized)
        return;
    logFunction("shutdownLSPClient");

    stopAllLSPServers();

    // Wait for reader threads
    for (auto& t : m_lspReaderThreads)
    {
        if (t.joinable())
        {
            if (t.get_id() == std::this_thread::get_id())
            {
                t.detach();
            }
            else
            {
                t.join();
            }
        }
    }
    m_lspReaderThreads.clear();

    saveLSPConfig();
    m_lspInitialized = false;
}

bool Win32IDE::startLSPServer(LSPLanguage lang)
{
    if (lang >= LSPLanguage::Count)
        return false;

    const auto& cfg = m_lspConfigs[(size_t)lang];
    auto& status = m_lspStatuses[(size_t)lang];

    if (!cfg.enabled)
    {
        status.lastError = "Server disabled in config";
        logInfo("[LSP] " + cfg.name + " is disabled — skipping");
        return false;
    }

    if (status.state == LSPServerState::Running)
    {
        logInfo("[LSP] " + cfg.name + " already running (pid=" + std::to_string(status.pid) + ")");
        return true;
    }

    logInfo("[LSP] Starting " + cfg.name + " (" + cfg.executablePath + ")...");
    status.state = LSPServerState::Starting;
    status.lastError = "";

    // ---- Create pipes for stdin/stdout communication -----------------------
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hStdinRead = nullptr, hStdinWrite = nullptr;
    HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;

    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0) || !CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0))
    {
        // If the second CreatePipe failed, close the first pair
        if (hStdinRead)
            CloseHandle(hStdinRead);
        if (hStdinWrite)
            CloseHandle(hStdinWrite);
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
    for (const auto& arg : cfg.args)
    {
        cmdLine += " " + arg;
    }

    // ---- Launch child process ----------------------------------------------
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;  // Merge stderr into stdout
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    BOOL created = CreateProcessA(nullptr,                             // lpApplicationName
                                  const_cast<char*>(cmdLine.c_str()),  // lpCommandLine
                                  nullptr,                             // lpProcessAttributes
                                  nullptr,                             // lpThreadAttributes
                                  TRUE,                                // bInheritHandles
                                  CREATE_NO_WINDOW,                    // dwCreationFlags
                                  nullptr,                             // lpEnvironment
                                  nullptr,                             // lpCurrentDirectory
                                  &si, &pi);

    // Close the child-side pipe handles (we keep the parent-side)
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);

    if (!created)
    {
        DWORD err = GetLastError();
        CloseHandle(hStdinWrite);
        CloseHandle(hStdoutRead);
        status.state = LSPServerState::Error;
        status.lastError = "CreateProcess failed for '" + cfg.executablePath + "' (error " + std::to_string(err) +
                           "). Is it installed and on PATH?";
        logError("startLSPServer", status.lastError);
        appendToOutput("[LSP] Failed to start " + cfg.name + ": " + status.lastError, "General",
                       OutputSeverity::Warning);
        return false;
    }

    // Store handles
    CloseHandle(pi.hThread);  // Don't need the thread handle
    status.hProcess = pi.hProcess;
    status.hStdinWrite = hStdinWrite;
    status.hStdoutRead = hStdoutRead;
    status.pid = pi.dwProcessId;
    status.startedEpochMs = currentEpochMs();

    logInfo("[LSP] " + cfg.name + " spawned (pid=" + std::to_string(status.pid) + ")");

    // ---- Start reader thread -----------------------------------------------
    m_lspReaderThreads.emplace_back(&Win32IDE::lspReaderThread, this, lang);

    // ---- Send initialize handshake -----------------------------------------
    bool initOk = sendInitialize(lang);
    if (!initOk)
    {
        status.state = LSPServerState::Error;
        status.lastError = "Initialize handshake failed or timed out";
        logError("startLSPServer", status.lastError);
        appendToOutput("[LSP] " + cfg.name + " started but initialize handshake failed.", "General",
                       OutputSeverity::Warning);
        return false;
    }

    sendInitialized(lang);
    status.state = LSPServerState::Running;
    status.initialized = true;

    logInfo("[LSP] " + cfg.name + " initialized successfully");
    appendToOutput("[LSP] " + cfg.name + " ready (pid=" + std::to_string(status.pid) + ")", "General",
                   OutputSeverity::Info);

    return true;
}

void Win32IDE::stopLSPServer(LSPLanguage lang)
{
    if (lang >= LSPLanguage::Count)
        return;
    auto& status = m_lspStatuses[(size_t)lang];
    const auto& cfg = m_lspConfigs[(size_t)lang];

    if (status.state != LSPServerState::Running && status.state != LSPServerState::Starting)
    {
        return;
    }

    logInfo("[LSP] Stopping " + cfg.name + "...");
    status.state = LSPServerState::ShuttingDown;

    // Send shutdown + exit per LSP protocol
    if (status.initialized)
    {
        sendShutdown(lang);
        sendExit(lang);
    }

    // Close pipe handles to unblock reader thread
    if (status.hStdinWrite)
    {
        CloseHandle(status.hStdinWrite);
        status.hStdinWrite = nullptr;
    }
    if (status.hStdoutRead)
    {
        CloseHandle(status.hStdoutRead);
        status.hStdoutRead = nullptr;
    }

    // Wait for process exit (brief), then terminate if stuck
    if (status.hProcess)
    {
        DWORD waitResult = WaitForSingleObject(status.hProcess, 3000);
        if (waitResult == WAIT_TIMEOUT)
        {
            TerminateProcess(status.hProcess, 1);
            logWarning("stopLSPServer", cfg.name + " did not exit gracefully — terminated");
        }
        CloseHandle(status.hProcess);
        status.hProcess = nullptr;
    }

    status.state = LSPServerState::Stopped;
    status.initialized = false;
    status.pid = 0;

    logInfo("[LSP] " + cfg.name + " stopped");
}

void Win32IDE::restartLSPServer(LSPLanguage lang)
{
    stopLSPServer(lang);
    // Brief pause for handle/process cleanup (keep minimal for responsive restarts)
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    startLSPServer(lang);
    m_lspStats.totalServerRestarts++;
}

void Win32IDE::startAllLSPServers()
{
    if (!m_lspInitialized)
        initLSPClient();
    for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i)
    {
        if (m_lspConfigs[i].enabled)
        {
            startLSPServer((LSPLanguage)i);
        }
    }
}

void Win32IDE::stopAllLSPServers()
{
    for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i)
    {
        stopLSPServer((LSPLanguage)i);
    }
}

Win32IDE::LSPServerState Win32IDE::getLSPServerState(LSPLanguage lang) const
{
    if (lang >= LSPLanguage::Count)
        return LSPServerState::Stopped;
    return m_lspStatuses[(size_t)lang].state;
}

int Win32IDE::sendLSPRequest(LSPLanguage lang, const std::string& method, const nlohmann::json& params)
{
    if (lang >= LSPLanguage::Count)
        return -1;
    auto& status = m_lspStatuses[(size_t)lang];

    const bool allowDuringStartup = (method == "initialize");
    if ((!status.hStdinWrite || (status.state != LSPServerState::Running &&
                                 !(allowDuringStartup && status.state == LSPServerState::Starting))) &&
        !allowDuringStartup && status.state != LSPServerState::Starting)
    {
        if (!m_lspInitialized)
            initLSPClient();
        if (!startLSPServer(lang))
            return -1;
    }

    if (!status.hStdinWrite ||
        (status.state != LSPServerState::Running && !(allowDuringStartup && status.state == LSPServerState::Starting)))
    {
        return -1;
    }

    int id;
    {
        std::lock_guard<std::mutex> lock(m_lspMutex);
        id = status.requestIdCounter++;
    }

    nlohmann::json msg;
    msg["jsonrpc"] = "2.0";
    msg["id"] = id;
    msg["method"] = method;
    msg["params"] = params;

    std::string body = msg.dump();
    std::string packet = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

    DWORD written = 0;
    BOOL ok = WriteFile(status.hStdinWrite, packet.c_str(), (DWORD)packet.size(), &written, nullptr);
    if (!ok || written != (DWORD)packet.size())
    {
        logError("sendLSPRequest", "WriteFile failed for " + m_lspConfigs[(size_t)lang].name);
        return -1;
    }

    status.requestCount++;
    return id;
}

void Win32IDE::sendLSPNotification(LSPLanguage lang, const std::string& method, const nlohmann::json& params)
{
    if (lang >= LSPLanguage::Count)
        return;
    auto& status = m_lspStatuses[(size_t)lang];

    const bool allowDuringStartup = (method == "initialized");
    if ((!status.hStdinWrite || (status.state != LSPServerState::Running &&
                                 !(allowDuringStartup && status.state == LSPServerState::Starting))) &&
        !allowDuringStartup && status.state != LSPServerState::Starting)
    {
        if (!m_lspInitialized)
            initLSPClient();
        if (!startLSPServer(lang))
            return;
    }

    if (!status.hStdinWrite ||
        (status.state != LSPServerState::Running && !(allowDuringStartup && status.state == LSPServerState::Starting)))
        return;

    nlohmann::json msg;
    msg["jsonrpc"] = "2.0";
    msg["method"] = method;
    msg["params"] = params;

    std::string body = msg.dump();
    std::string packet = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

    DWORD written = 0;
    WriteFile(status.hStdinWrite, packet.c_str(), (DWORD)packet.size(), &written, nullptr);
    status.notificationCount++;
}

nlohmann::json Win32IDE::readLSPResponse(LSPLanguage lang, int requestId, int timeoutMs)
{
    // Wait for the reader thread to deposit the response
    std::unique_lock<std::mutex> lock(m_lspResponseMutex);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (true)
    {
        auto it = m_lspPendingResponses.find(requestId);
        if (it != m_lspPendingResponses.end())
        {
            nlohmann::json resp = std::move(it->second);
            m_lspPendingResponses.erase(it);
            return resp;
        }
        if (m_lspResponseCV.wait_until(lock, deadline) == std::cv_status::timeout)
        {
            nlohmann::json err_obj;
            err_obj["code"] = -1;
            err_obj["message"] = "Timeout waiting for response";
            nlohmann::json root;
            root["error"] = err_obj;
            return root;
        }
    }
}


void Win32IDE::lspReaderThread(LSPLanguage lang)
{
    auto& status = m_lspStatuses[(size_t)lang];
    const auto& cfg = m_lspConfigs[(size_t)lang];

    logInfo("[LSP-reader] Started for " + cfg.name);

    std::string buffer;
    char readBuf[8192];

    while (status.hStdoutRead && status.state != LSPServerState::Stopped)
    {
        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(status.hStdoutRead, nullptr, 0, nullptr, &bytesAvailable, nullptr))
        {
            break;  // Pipe broken
        }

        if (bytesAvailable == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        DWORD bytesRead = 0;
        DWORD toRead = (bytesAvailable < sizeof(readBuf)) ? bytesAvailable : sizeof(readBuf);
        if (!ReadFile(status.hStdoutRead, readBuf, toRead, &bytesRead, nullptr) || bytesRead == 0)
        {
            break;
        }

        buffer.append(readBuf, bytesRead);

        // Fail-closed: prevent unbounded buffer growth from malformed streams.
        if (buffer.size() > kMaxLspBufferBytes)
        {
            buffer.clear();
            break;
        }

        // Parse JSON-RPC messages from buffer
        while (true)
        {
            // Look for Content-Length header
            size_t header_end = buffer.find("\r\n\r\n");
            if (header_end == std::string::npos)
                break;

            std::string header = buffer.substr(0, header_end);
            int content_length = 0;

            // Parse Content-Length
            size_t clPos = header.find("Content-Length:");
            if (clPos == std::string::npos)
                clPos = header.find("content-length:");
            if (clPos != std::string::npos)
            {
                size_t valStart = clPos + 15;  // strlen("Content-Length:")
                while (valStart < header.size() && header[valStart] == ' ')
                    valStart++;

                errno = 0;
                char* end = nullptr;
                const std::string contentLenStr = header.substr(valStart);
                const unsigned long parsed = std::strtoul(contentLenStr.c_str(), &end, 10);
                if (end == contentLenStr.c_str() || errno != 0)
                {
                    buffer.erase(0, header_end + 4);
                    continue;
                }
                while (end && *end == ' ')
                {
                    ++end;
                }
                if (end && *end != '\0')
                {
                    buffer.erase(0, header_end + 4);
                    continue;
                }
                if (parsed > kMaxLspMessageBytes)
                {
                    buffer.erase(0, header_end + 4);
                    continue;
                }
                content_length = static_cast<int>(parsed);
            }

            if (content_length <= 0)
            {
                buffer.erase(0, header_end + 4);
                continue;
            }

            size_t message_start = header_end + 4;
            if (buffer.size() < message_start + content_length)
            {
                break;  // Incomplete message — wait for more data
            }

            std::string json_body = buffer.substr(message_start, content_length);
            buffer.erase(0, message_start + content_length);

            // Parse JSON
            try
            {
                nlohmann::json msg = nlohmann::json::parse(json_body);

                if (msg.contains("id") && !msg["id"].is_null())
                {
                    // Response to a request
                    int resp_id = msg["id"].get<int>();
                    {
                        std::lock_guard<std::mutex> lock(m_lspResponseMutex);
                        m_lspPendingResponses[resp_id] = msg;
                    }
                    m_lspResponseCV.notify_all();
                }
                else if (msg.contains("method"))
                {
                    // Server notification
                    std::string method = msg["method"].get<std::string>();
                    if (method == "textDocument/publishDiagnostics")
                    {
                        // Parse diagnostics
                        if (msg.contains("params"))
                        {
                            const auto& params = msg["params"];
                            std::string uri = params.value("uri", "");
                            std::vector<LSPDiagnostic> diags;

                            if (params.contains("diagnostics") && params["diagnostics"].is_array())
                            {
                                const auto& diagArr = params["diagnostics"];
                                for (size_t di = 0; di < diagArr.size(); ++di)
                                {
                                    const nlohmann::json& dj = diagArr[di];
                                    LSPDiagnostic d;
                                    if (dj.contains("range"))
                                    {
                                        const auto& rj = dj["range"];
                                        if (rj.contains("start"))
                                        {
                                            d.range.start.line = rj["start"].value("line", 0);
                                            d.range.start.character = rj["start"].value("character", 0);
                                        }
                                        if (rj.contains("end"))
                                        {
                                            d.range.end.line = rj["end"].value("line", 0);
                                            d.range.end.character = rj["end"].value("character", 0);
                                        }
                                    }
                                    d.severity = dj.value("severity", 1);
                                    d.message = dj.value("message", "");
                                    d.source = dj.value("source", cfg.name);
                                    if (dj.contains("code"))
                                    {
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
                    else if (method == "window/logMessage" || method == "window/showMessage")
                    {
                        // Route LSP log/show messages to output panel
                        if (msg.contains("params"))
                        {
                            int type = msg["params"].value("type", 4);  // 1=Error,2=Warning,3=Info,4=Log
                            std::string lspMsg = msg["params"].value("message", "");
                            if (!lspMsg.empty())
                            {
                                OutputSeverity sev = OutputSeverity::Info;
                                if (type == 1)
                                    sev = OutputSeverity::Error;
                                else if (type == 2)
                                    sev = OutputSeverity::Warning;
                                appendToOutput("[LSP/" + cfg.name + "] " + lspMsg + "\n", "LSP", sev);
                                status.notificationCount++;
                            }
                        }
                    }
                    // Other notifications silently dropped
                }
            }
            catch (const std::exception& e)
            {
                logError("lspReaderThread", std::string("JSON parse error in ") + cfg.name + ": " + e.what());
            }
        }
    }

    logInfo("[LSP-reader] Exiting for " + cfg.name);
}

bool Win32IDE::sendInitialize(LSPLanguage lang)
{
    const auto& cfg = m_lspConfigs[(size_t)lang];

    nlohmann::json params;
    params["processId"] = (int)GetCurrentProcessId();
    params["rootUri"] = cfg.rootUri;

    // Client capabilities — declare what we support
    nlohmann::json textDocCaps;
    textDocCaps["synchronization"]["dynamicRegistration"] = false;
    textDocCaps["synchronization"]["willSave"] = false;
    textDocCaps["synchronization"]["didSave"] = true;
    textDocCaps["definition"]["dynamicRegistration"] = false;
    textDocCaps["references"]["dynamicRegistration"] = false;
    textDocCaps["rename"]["dynamicRegistration"] = false;
    textDocCaps["hover"]["dynamicRegistration"] = false;
    textDocCaps["formatting"]["dynamicRegistration"] = false;  // Added for Format on Save
    textDocCaps["publishDiagnostics"]["relatedInformation"] = false;

    nlohmann::json caps;
    caps["textDocument"] = textDocCaps;
    params["capabilities"] = caps;

    params["clientInfo"] = {{"name", "RawrXD-IDE"}, {"version", "7.7.0"}};

    int id = sendLSPRequest(lang, "initialize", params);
    if (id < 0)
        return false;

    nlohmann::json resp = readLSPResponse(lang, id, cfg.initTimeoutMs);
    if (resp.contains("error"))
    {
        m_lspStatuses[(size_t)lang].lastError = "Initialize error: " + resp["error"].value("message", "unknown");
        return false;
    }

    return resp.contains("result");
}

void Win32IDE::sendInitialized(LSPLanguage lang)
{
    sendLSPNotification(lang, "initialized", nlohmann::json::object());
}

void Win32IDE::sendShutdown(LSPLanguage lang)
{
    int id = sendLSPRequest(lang, "shutdown", nlohmann::json{});
    if (id >= 0)
    {
        // Brief wait for ack — don't block forever
        readLSPResponse(lang, id, 2000);
    }
}

void Win32IDE::sendExit(LSPLanguage lang)
{
    sendLSPNotification(lang, "exit", nlohmann::json{});
}

void Win32IDE::sendDidOpen(LSPLanguage lang, const std::string& uri, const std::string& languageId,
                           const std::string& content)
{
    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["textDocument"]["languageId"] = languageId;
    params["textDocument"]["version"] = 1;
    params["textDocument"]["text"] = content;

    sendLSPNotification(lang, "textDocument/didOpen", params);
}

void Win32IDE::sendDidChange(LSPLanguage lang, const std::string& uri, const std::string& content)
{
    // Full document sync (simplest mode)
    static int versionCounter = 2;
    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
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

void Win32IDE::sendDidClose(LSPLanguage lang, const std::string& uri)
{
    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    sendLSPNotification(lang, "textDocument/didClose", params);
}

void Win32IDE::sendDidSave(LSPLanguage lang, const std::string& uri)
{
    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    sendLSPNotification(lang, "textDocument/didSave", params);
}


std::vector<Win32IDE::LSPLocation> Win32IDE::lspGotoDefinition(const std::string& uri, int line, int character)
{
    std::vector<LSPLocation> results;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return results;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["position"]["line"] = line;
    params["position"]["character"] = character;

    int id = sendLSPRequest(lang, "textDocument/definition", params);
    if (id < 0)
        return results;

    nlohmann::json resp = readLSPResponse(lang, id);
    m_lspStats.totalDefinitionRequests++;

    if (!resp.contains("result") || resp["result"].is_null())
        return results;

    auto parseLocation = [](const nlohmann::json& lj) -> LSPLocation
    {
        LSPLocation loc;
        loc.uri = lj.value("uri", "");
        if (lj.contains("range"))
        {
            const auto& rj = lj["range"];
            if (rj.contains("start"))
            {
                loc.range.start.line = rj["start"].value("line", 0);
                loc.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end"))
            {
                loc.range.end.line = rj["end"].value("line", 0);
                loc.range.end.character = rj["end"].value("character", 0);
            }
        }
        return loc;
    };

    const auto& result = resp["result"];
    if (result.is_array())
    {
        for (size_t ri = 0; ri < result.size(); ++ri)
        {
            results.push_back(parseLocation(result[ri]));
        }
    }
    else if (result.is_object())
    {
        results.push_back(parseLocation(result));
    }

    return results;
}


std::vector<Win32IDE::LSPLocation> Win32IDE::lspFindReferences(const std::string& uri, int line, int character)
{
    std::vector<LSPLocation> results;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return results;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["position"]["line"] = line;
    params["position"]["character"] = character;
    params["context"]["includeDeclaration"] = true;

    int id = sendLSPRequest(lang, "textDocument/references", params);
    if (id < 0)
        return results;

    nlohmann::json resp = readLSPResponse(lang, id, 10000);  // References can be slow
    m_lspStats.totalReferenceRequests++;

    if (!resp.contains("result") || !resp["result"].is_array())
        return results;

    const auto& refResult = resp["result"];
    for (size_t ri = 0; ri < refResult.size(); ++ri)
    {
        const nlohmann::json& lj = refResult[ri];
        LSPLocation loc;
        loc.uri = lj.value("uri", "");
        if (lj.contains("range"))
        {
            const auto& rj = lj["range"];
            if (rj.contains("start"))
            {
                loc.range.start.line = rj["start"].value("line", 0);
                loc.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end"))
            {
                loc.range.end.line = rj["end"].value("line", 0);
                loc.range.end.character = rj["end"].value("character", 0);
            }
        }
        results.push_back(loc);
    }

    return results;
}


Win32IDE::LSPWorkspaceEdit Win32IDE::lspRenameSymbol(const std::string& uri, int line, int character,
                                                     const std::string& newName)
{
    LSPWorkspaceEdit edit;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return edit;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["position"]["line"] = line;
    params["position"]["character"] = character;
    params["newName"] = newName;

    int id = sendLSPRequest(lang, "textDocument/rename", params);
    if (id < 0)
        return edit;

    nlohmann::json resp = readLSPResponse(lang, id, 10000);
    m_lspStats.totalRenameRequests++;

    if (!resp.contains("result") || resp["result"].is_null())
        return edit;

    const auto& result = resp["result"];
    if (result.contains("changes") && result["changes"].is_object())
    {
        for (auto it = result["changes"].begin(); it != result["changes"].end(); ++it)
        {
            std::string fileUri = it.key();
            std::vector<LSPWorkspaceEdit::TextEdit> edits;
            if (it.value().is_array())
            {
                const nlohmann::json& editArr = it.value();
                for (size_t ei = 0; ei < editArr.size(); ++ei)
                {
                    const nlohmann::json& ej = editArr[ei];
                    LSPWorkspaceEdit::TextEdit te;
                    te.newText = ej.value("newText", "");
                    if (ej.contains("range"))
                    {
                        const auto& rj = ej["range"];
                        if (rj.contains("start"))
                        {
                            te.range.start.line = rj["start"].value("line", 0);
                            te.range.start.character = rj["start"].value("character", 0);
                        }
                        if (rj.contains("end"))
                        {
                            te.range.end.line = rj["end"].value("line", 0);
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
    if (result.contains("documentChanges") && result["documentChanges"].is_array())
    {
        const auto& docChanges = result["documentChanges"];
        for (size_t dci = 0; dci < docChanges.size(); ++dci)
        {
            const nlohmann::json& dc = docChanges[dci];

            // Handle TextDocumentEdit
            if (dc.contains("textDocument") && dc.contains("edits"))
            {
                std::string fileUri = dc["textDocument"].value("uri", "");
                std::vector<LSPWorkspaceEdit::TextEdit> edits;
                const auto& dcEdits = dc["edits"];
                for (size_t ei = 0; ei < dcEdits.size(); ++ei)
                {
                    const nlohmann::json& ej = dcEdits[ei];
                    LSPWorkspaceEdit::TextEdit te;
                    te.newText = ej.value("newText", "");
                    if (ej.contains("range"))
                    {
                        const auto& rj = ej["range"];
                        if (rj.contains("start"))
                        {
                            te.range.start.line = rj["start"].value("line", 0);
                            te.range.start.character = rj["start"].value("character", 0);
                        }
                        if (rj.contains("end"))
                        {
                            te.range.end.line = rj["end"].value("line", 0);
                            te.range.end.character = rj["end"].value("character", 0);
                        }
                    }
                    edits.push_back(te);
                }
                edit.changes[fileUri] = edits;
            }
            // Handle Resource Operations (LSP 3.17)
            else if (dc.contains("kind"))
            {
                std::string kind = dc.value("kind", "");
                LSPWorkspaceEdit::ResourceOperation op;
                if (kind == "create")
                {
                    op.type = LSPWorkspaceEdit::ResourceOperation::Type::Create;
                    op.uri = dc.value("uri", "");
                    op.overwrite = dc.value("options", nlohmann::json::object()).value("overwrite", false);
                    op.ignoreIfExists = dc.value("options", nlohmann::json::object()).value("ignoreIfExists", false);
                    edit.resourceOperations.push_back(op);
                }
                else if (kind == "rename")
                {
                    op.type = LSPWorkspaceEdit::ResourceOperation::Type::Rename;
                    op.uri = dc.value("oldUri", "");
                    op.newUri = dc.value("newUri", "");
                    op.overwrite = dc.value("options", nlohmann::json::object()).value("overwrite", false);
                    op.ignoreIfExists = dc.value("options", nlohmann::json::object()).value("ignoreIfExists", false);
                    edit.resourceOperations.push_back(op);
                }
                else if (kind == "delete")
                {
                    op.type = LSPWorkspaceEdit::ResourceOperation::Type::Delete;
                    op.uri = dc.value("uri", "");
                    op.recursive = dc.value("options", nlohmann::json::object()).value("recursive", false);
                    op.ignoreIfNotExists =
                        dc.value("options", nlohmann::json::object()).value("ignoreIfNotExists", false);
                    edit.resourceOperations.push_back(op);
                }
            }
        }
    }

    return edit;
}


Win32IDE::LSPHoverInfo Win32IDE::lspHover(const std::string& uri, int line, int character)
{
    LSPHoverInfo info;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return info;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["position"]["line"] = line;
    params["position"]["character"] = character;

    int id = sendLSPRequest(lang, "textDocument/hover", params);
    if (id < 0)
        return info;

    nlohmann::json resp = readLSPResponse(lang, id);
    m_lspStats.totalHoverRequests++;

    if (!resp.contains("result") || resp["result"].is_null())
        return info;

    const auto& result = resp["result"];

    // Parse contents (can be string, object, or array)
    if (result.contains("contents"))
    {
        const auto& contents = result["contents"];
        if (contents.is_string())
        {
            info.contents = contents.get<std::string>();
        }
        else if (contents.is_object())
        {
            info.contents = contents.value("value", "");
        }
        else if (contents.is_array())
        {
            for (size_t ci = 0; ci < contents.size(); ++ci)
            {
                const nlohmann::json& citem = contents[ci];
                if (!info.contents.empty())
                    info.contents += "\n---\n";
                if (citem.is_string())
                {
                    info.contents += citem.get<std::string>();
                }
                else if (citem.is_object())
                {
                    info.contents += citem.value("value", "");
                }
            }
        }
    }

    if (result.contains("range"))
    {
        const auto& rj = result["range"];
        if (rj.contains("start"))
        {
            info.range.start.line = rj["start"].value("line", 0);
            info.range.start.character = rj["start"].value("character", 0);
        }
        if (rj.contains("end"))
        {
            info.range.end.line = rj["end"].value("line", 0);
            info.range.end.character = rj["end"].value("character", 0);
        }
    }

    info.valid = !info.contents.empty();
    return info;
}

std::vector<Win32IDE::LSPCompletionItem> Win32IDE::lspCompletion(const std::string& uri, int line, int character)
{
    std::vector<LSPCompletionItem> items;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return items;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["position"]["line"] = line;
    params["position"]["character"] = character;
    params["context"]["triggerKind"] = 1;

    int id = sendLSPRequest(lang, "textDocument/completion", params);
    if (id < 0)
        return items;

    nlohmann::json resp = readLSPResponse(lang, id, 5000);
    m_lspStats.totalCompletionRequests++;
    if (!resp.contains("result") || resp["result"].is_null())
        return items;

    auto parseItem = [](const nlohmann::json& cj) -> LSPCompletionItem
    {
        LSPCompletionItem out;
        out.label = cj.value("label", "");
        out.detail = cj.value("detail", "");
        out.kind = cj.value("kind", 0);
        out.isSnippet = (cj.value("insertTextFormat", 1) == 2);

        if (cj.contains("insertText") && cj["insertText"].is_string())
        {
            out.insertText = cj["insertText"].get<std::string>();
        }
        else if (cj.contains("textEdit") && cj["textEdit"].is_object())
        {
            const auto& te = cj["textEdit"];
            if (te.contains("newText") && te["newText"].is_string())
            {
                out.insertText = te["newText"].get<std::string>();
            }
        }

        if (out.insertText.empty())
        {
            out.insertText = out.label;
        }
        return out;
    };

    const auto& result = resp["result"];
    if (result.is_array())
    {
        for (size_t i = 0; i < result.size(); ++i)
        {
            items.push_back(parseItem(result[i]));
        }
    }
    else if (result.is_object())
    {
        if (result.contains("items") && result["items"].is_array())
        {
            const auto& arr = result["items"];
            for (size_t i = 0; i < arr.size(); ++i)
            {
                items.push_back(parseItem(arr[i]));
            }
        }
    }
    return items;
}

Win32IDE::LSPSignatureHelpInfo Win32IDE::lspSignatureHelp(const std::string& uri, int line, int character,
                                                          int triggerKind)
{
    LSPSignatureHelpInfo out;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return out;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["position"]["line"] = line;
    params["position"]["character"] = character;
    params["context"]["triggerKind"] = triggerKind;
    params["context"]["isRetrigger"] = false;

    int id = sendLSPRequest(lang, "textDocument/signatureHelp", params);
    if (id < 0)
        return out;

    nlohmann::json resp = readLSPResponse(lang, id, 5000);
    m_lspStats.totalSignatureRequests++;
    if (!resp.contains("result") || resp["result"].is_null())
        return out;

    const auto& result = resp["result"];
    if (!result.contains("signatures") || !result["signatures"].is_array())
        return out;
    const auto& signatures = result["signatures"];
    if (signatures.empty())
        return out;

    out.activeSignature = result.value("activeSignature", 0);
    out.activeParameter = result.value("activeParameter", 0);
    if (out.activeSignature < 0)
        out.activeSignature = 0;
    if (out.activeParameter < 0)
        out.activeParameter = 0;

    for (size_t i = 0; i < signatures.size(); ++i)
    {
        const auto& sj = signatures[i];
        std::string label = sj.value("label", "");
        if (!label.empty())
        {
            out.signatures.push_back(label);
        }
    }

    if (out.activeSignature >= (int)signatures.size())
        out.activeSignature = 0;
    const auto& active = signatures[(size_t)out.activeSignature];
    out.activeSignatureLabel = active.value("label", "");
    if (active.contains("documentation"))
    {
        if (active["documentation"].is_string())
        {
            out.activeDocumentation = active["documentation"].get<std::string>();
        }
        else if (active["documentation"].is_object())
        {
            out.activeDocumentation = active["documentation"].value("value", "");
        }
    }

    out.valid = !out.activeSignatureLabel.empty() || !out.signatures.empty();
    return out;
}


void Win32IDE::onDiagnosticsReceived(const std::string& uri, const std::vector<LSPDiagnostic>& diagnostics)
{
    {
        std::lock_guard<std::mutex> lock(m_lspDiagnosticsMutex);
        m_lspDiagnostics[uri] = diagnostics;
    }

    // LSP notifications arrive on worker threads; only the UI thread may touch Win32 controls.
    const bool onUiThread =
        (m_hwndMain && IsWindow(m_hwndMain) && GetWindowThreadProcessId(m_hwndMain, nullptr) == GetCurrentThreadId());
    if (!onUiThread)
    {
        return;
    }

    // Bridge to Problems panel (Output > Problems tab)
    std::string filePath = uriToFilePath(uri);
    if (!filePath.empty() && m_hwndProblemsListView)
    {
        // Clear existing problems for this file
        m_problems.erase(std::remove_if(m_problems.begin(), m_problems.end(),
                                        [&filePath](const ProblemItem& p) { return p.file == filePath; }),
                         m_problems.end());
        m_errorCount = m_warningCount = 0;
        for (const auto& p : m_problems)
        {
            if (p.severity == 0)
                m_errorCount++;
            else if (p.severity == 1)
                m_warningCount++;
        }
        ListView_DeleteAllItems(m_hwndProblemsListView);
        // Rebuild list from remaining problems (other files)
        for (size_t i = 0; i < m_problems.size(); ++i)
        {
            const auto& p = m_problems[i];
            LVITEMA lvi = {LVIF_TEXT};
            lvi.iItem = static_cast<int>(i);
            lvi.pszText = const_cast<char*>((p.severity == 0) ? "Error" : (p.severity == 1) ? "Warning" : "Info");
            ListView_InsertItem(m_hwndProblemsListView, &lvi);
            LVITEMA lviSet = {0};
            lviSet.iSubItem = 1;
            lviSet.pszText = const_cast<char*>(p.message.c_str());
            SendMessage(m_hwndProblemsListView, LVM_SETITEMTEXTA, lvi.iItem, (LPARAM)&lviSet);
            lviSet.iSubItem = 2;
            lviSet.pszText = const_cast<char*>(p.file.c_str());
            SendMessage(m_hwndProblemsListView, LVM_SETITEMTEXTA, lvi.iItem, (LPARAM)&lviSet);
            char ln[32];
            _snprintf_s(ln, 32, _TRUNCATE, "%d", p.line);
            lviSet.iSubItem = 3;
            lviSet.pszText = ln;
            SendMessage(m_hwndProblemsListView, LVM_SETITEMTEXTA, lvi.iItem, (LPARAM)&lviSet);
        }
        // Add new diagnostics (LSP: 1=Error, 2=Warning, 3=Info, 4=Hint → our 0=Error, 1=Warning, 2=Info)
        for (const auto& d : diagnostics)
        {
            int sev = (d.severity == 1) ? 0 : (d.severity == 2) ? 1 : 2;
            addProblem(filePath, d.range.start.line + 1, d.range.start.character + 1, d.message, sev);
        }
        updatePanelContent();
        updateEnhancedStatusBar();
    }

    // Map diagnostics to the existing annotation system for visual display
    displayDiagnosticsAsAnnotations(uri);
}

std::vector<Win32IDE::LSPDiagnostic> Win32IDE::getDiagnosticsForFile(const std::string& uri) const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_lspDiagnosticsMutex));
    auto it = m_lspDiagnostics.find(uri);
    if (it != m_lspDiagnostics.end())
        return it->second;
    return {};
}

std::vector<std::pair<std::string, std::vector<Win32IDE::LSPDiagnostic>>> Win32IDE::getAllDiagnostics() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_lspDiagnosticsMutex));
    std::vector<std::pair<std::string, std::vector<LSPDiagnostic>>> result;
    for (const auto& pair : m_lspDiagnostics)
    {
        result.push_back(pair);
    }
    return result;
}

void Win32IDE::clearDiagnostics(const std::string& uri)
{
    {
        std::lock_guard<std::mutex> lock(m_lspDiagnosticsMutex);
        m_lspDiagnostics.erase(uri);
    }
    // Clear LSP annotations for this file
    clearAllAnnotations("lsp");
}

void Win32IDE::clearAllDiagnostics()
{
    {
        std::lock_guard<std::mutex> lock(m_lspDiagnosticsMutex);
        m_lspDiagnostics.clear();
    }
    clearAllAnnotations("lsp");
}

void Win32IDE::displayDiagnosticsAsAnnotations(const std::string& uri)
{
    // Only display diagnostics for the currently active file
    std::string currentUri = filePathToUri(m_currentFile);
    if (uri != currentUri)
        return;

    // Clear old LSP annotations
    clearAllAnnotations("lsp");

    auto diags = getDiagnosticsForFile(uri);
    for (const auto& d : diags)
    {
        AnnotationSeverity sev = AnnotationSeverity::Info;
        switch (d.severity)
        {
            case 1:
                sev = AnnotationSeverity::Error;
                break;
            case 2:
                sev = AnnotationSeverity::Warning;
                break;
            case 3:
                sev = AnnotationSeverity::Info;
                break;
            case 4:
                sev = AnnotationSeverity::Info;
                break;  // Hint → Info
        }

        // LSP lines are 0-based; annotations are 1-based
        std::string msg = d.message;
        if (!d.code.empty())
            msg += " [" + d.code + "]";
        if (!d.source.empty())
            msg += " (" + d.source + ")";

        addAnnotation(d.range.start.line + 1, sev, msg, "lsp");
    }
}

bool Win32IDE::applyWorkspaceEdit(const LSPWorkspaceEdit& edit)
{
    if (edit.changes.empty() && edit.resourceOperations.empty())
        return false;

    int filesQueued = 0;
    int editsQueued = 0;
    int resourcesQueued = 0;

    // 1. First handle resource operations (LSP 3.17)
    for (const auto& op : edit.resourceOperations)
    {
        std::string path = uriToFilePath(op.uri);
        std::string newPath = op.newUri.empty() ? "" : uriToFilePath(op.newUri);

        auto testPathExists = [](const std::string& p) {
            return std::filesystem::exists(p);
        };

        switch (op.type)
        {
            case LSPWorkspaceEdit::ResourceOperation::Type::Create:
            {
                if (testPathExists(path) && !op.overwrite)
                {
                    if (!op.ignoreIfExists)
                    {
                        appendToOutput("[LSP] Create file failed: " + path + " already exists.", "General",
                                       OutputSeverity::Warning);
                    }
                    continue;
                }
                queueFilePendingEdit(RawrXD::Review::EditSource::Lsp, RawrXD::Review::EditType::Create, path,
                                     std::string(), std::string());
                resourcesQueued++;
                break;
            }
            case LSPWorkspaceEdit::ResourceOperation::Type::Rename:
            {
                if (!testPathExists(path))
                {
                    if (!op.ignoreIfExists)
                    {
                        appendToOutput("[LSP] Rename failed: source " + path + " does not exist.", "General",
                                       OutputSeverity::Warning);
                    }
                    continue;
                }
                if (testPathExists(newPath) && !op.overwrite)
                {
                    appendToOutput("[LSP] Rename failed: target " + newPath + " already exists.", "General",
                                   OutputSeverity::Warning);
                    continue;
                }
                queueFilePendingEdit(RawrXD::Review::EditSource::Lsp, RawrXD::Review::EditType::Rename, path,
                                     std::string(), std::string(), newPath);
                resourcesQueued++;
                break;
            }
            case LSPWorkspaceEdit::ResourceOperation::Type::Delete:
            {
                if (!testPathExists(path))
                {
                    if (!op.ignoreIfNotExists)
                    {
                        appendToOutput("[LSP] Delete failed: " + path + " does not exist.", "General",
                                       OutputSeverity::Warning);
                    }
                    continue;
                }
                std::string oldText;
                DWORD attr = GetFileAttributesA(path.c_str());
                if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
                {
                    std::ifstream ifs(path, std::ios::binary);
                    if (ifs.is_open())
                    {
                        std::ostringstream buffer;
                        buffer << ifs.rdbuf();
                        oldText = buffer.str();
                    }
                }
                queueFilePendingEdit(RawrXD::Review::EditSource::Lsp, RawrXD::Review::EditType::Delete, path,
                                     oldText, std::string());
                resourcesQueued++;
                break;
            }
        }
    }

    // 2. Then handle text changes
    for (const auto& [uri, textEdits] : edit.changes)
    {
        std::string filePath = uriToFilePath(uri);

        std::ifstream rawIfs(filePath, std::ios::binary);
        if (!rawIfs.is_open())
            continue;

        std::ostringstream originalBuffer;
        originalBuffer << rawIfs.rdbuf();
        const std::string originalText = originalBuffer.str();
        rawIfs.close();

        // Read file line-wise for edit reconstruction
        std::ifstream ifs(filePath);
        if (!ifs.is_open())
            continue;

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(ifs, line))
        {
            lines.push_back(line);
        }
        ifs.close();

        // Apply edits in reverse order (bottom-up to preserve positions)
        auto sortedEdits = textEdits;
        std::sort(sortedEdits.begin(), sortedEdits.end(),
                  [](const LSPWorkspaceEdit::TextEdit& a, const LSPWorkspaceEdit::TextEdit& b)
                  {
                      if (a.range.start.line != b.range.start.line)
                          return a.range.start.line > b.range.start.line;
                      return a.range.start.character > b.range.start.character;
                  });

        for (const auto& te : sortedEdits)
        {
            int startLine = te.range.start.line;
            int startChar = te.range.start.character;
            int endLine = te.range.end.line;
            int endChar = te.range.end.character;

            if (startLine < 0 || startLine >= (int)lines.size())
                continue;
            if (endLine < 0 || endLine >= (int)lines.size())
                continue;

            if (startLine == endLine)
            {
                // Single-line edit
                std::string& l = lines[startLine];
                std::string before = l.substr(0, std::min((int)l.size(), startChar));
                std::string after = (endChar < (int)l.size()) ? l.substr(endChar) : "";
                l = before + te.newText + after;
            }
            else
            {
                // Multi-line edit
                std::string firstLine = lines[startLine].substr(0, std::min((int)lines[startLine].size(), startChar));
                std::string lastLine =
                    (endChar < (int)lines[endLine].size()) ? lines[endLine].substr(endChar) : "";
                // Remove lines [startLine+1 ... endLine]
                lines.erase(lines.begin() + startLine + 1, lines.begin() + endLine + 1);
                lines[startLine] = firstLine + te.newText + lastLine;
            }
            editsQueued++;
        }

        std::ostringstream rebuilt;
        for (size_t i = 0; i < lines.size(); ++i)
        {
            rebuilt << lines[i];
            if (i + 1 < lines.size())
                rebuilt << "\n";
        }

        queueFilePendingEdit(RawrXD::Review::EditSource::Lsp, RawrXD::Review::EditType::Replace, filePath,
                             originalText, rebuilt.str());
        filesQueued++;
    }

    if (filesQueued > 0 || resourcesQueued > 0)
    {
        std::string msg = "[LSP] Queued " + std::to_string(editsQueued) + " edits across " +
                          std::to_string(filesQueued) + " file(s)";
        if (resourcesQueued > 0)
            msg += " and " + std::to_string(resourcesQueued) + " resource op(s)";
        msg += ".";
        appendToOutput(msg, "General", OutputSeverity::Info);
    }

    return filesQueued > 0 || resourcesQueued > 0;
}

Win32IDE::LSPLanguage Win32IDE::detectLanguageForFile(const std::string& filePath) const
{
    std::string ext;
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos != std::string::npos)
    {
        ext = filePath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    }

    // C/C++
    if (ext == ".c" || ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".h" || ext == ".hpp" || ext == ".hh" ||
        ext == ".hxx" || ext == ".ipp" || ext == ".inl")
    {
        return LSPLanguage::Cpp;
    }

    // Python
    if (ext == ".py" || ext == ".pyi" || ext == ".pyw")
    {
        return LSPLanguage::Python;
    }

    // TypeScript / JavaScript
    if (ext == ".ts" || ext == ".tsx" || ext == ".js" || ext == ".jsx" || ext == ".mjs" || ext == ".cjs")
    {
        return LSPLanguage::TypeScript;
    }

    return LSPLanguage::Count;  // Unknown
}

std::string Win32IDE::lspLanguageId(LSPLanguage lang) const
{
    switch (lang)
    {
        case LSPLanguage::Cpp:
            return "cpp";
        case LSPLanguage::Python:
            return "python";
        case LSPLanguage::TypeScript:
            return "typescript";
        default:
            return "plaintext";
    }
}

std::string Win32IDE::lspLanguageString(LSPLanguage lang) const
{
    switch (lang)
    {
        case LSPLanguage::Cpp:
            return "C/C++";
        case LSPLanguage::Python:
            return "Python";
        case LSPLanguage::TypeScript:
            return "TypeScript";
        default:
            return "Unknown";
    }
}

Win32IDE::LSPLanguage Win32IDE::lspLanguageFromString(const std::string& name) const
{
    if (name == "C/C++" || name == "cpp" || name == "c" || name == "Cpp")
        return LSPLanguage::Cpp;
    if (name == "Python" || name == "python" || name == "py")
        return LSPLanguage::Python;
    if (name == "TypeScript" || name == "typescript" || name == "ts")
        return LSPLanguage::TypeScript;
    return LSPLanguage::Count;
}

std::string Win32IDE::filePathToUri(const std::string& filePath) const
{
    // Convert "D:\rawrxd\src\foo.cpp" → "file:///D:/rawrxd/src/foo.cpp"
    std::string uri = "file:///";
    for (char c : filePath)
    {
        if (c == '\\')
        {
            uri += '/';
        }
        else if (c == ' ')
        {
            uri += "%20";
        }
        else
        {
            uri += c;
        }
    }
    return uri;
}

std::string Win32IDE::uriToFilePath(const std::string& uri) const
{
    // Convert "file:///D:/rawrxd/src/foo.cpp" → "D:\rawrxd\src\foo.cpp"
    std::string path = uri;
    if (path.substr(0, 8) == "file:///")
    {
        path = path.substr(8);
    }
    else if (path.substr(0, 7) == "file://")
    {
        path = path.substr(7);
    }

    // URL decode %20 → space
    std::string decoded;
    for (size_t i = 0; i < path.size(); ++i)
    {
        if (path[i] == '%' && i + 2 < path.size())
        {
            int hex = 0;
            std::string hexStr = path.substr(i + 1, 2);
            try
            {
                hex = std::stoi(hexStr, nullptr, 16);
            }
            catch (...)
            {
            }
            decoded += (char)hex;
            i += 2;
        }
        else if (path[i] == '/')
        {
            decoded += '\\';
        }
        else
        {
            decoded += path[i];
        }
    }

    return decoded;
}

bool Win32IDE::prepareLSPDocument(const std::string& filePath, LSPLanguage& lang)
{
    if (filePath.empty())
        return false;

    if (!m_lspInitialized)
        initLSPClient();

    lang = detectLanguageForFile(filePath);
    if (lang >= LSPLanguage::Count)
        return false;

    auto& status = m_lspStatuses[(size_t)lang];
    if (status.state == LSPServerState::Running && status.hStdinWrite)
        return true;

    return startLSPServer(lang);
}

void Win32IDE::syncLSPDocumentOpen(const std::string& filePath, const std::string& content)
{
    LSPLanguage lang = LSPLanguage::Count;
    if (!prepareLSPDocument(filePath, lang))
        return;

    const std::string uri = filePathToUri(filePath);
    {
        std::lock_guard<std::mutex> lock(m_lspMutex);
        if (m_lspOpenDocuments.find(uri) != m_lspOpenDocuments.end())
            return;
        m_lspOpenDocuments[uri] = lang;
    }

    sendDidOpen(lang, uri, lspLanguageId(lang), content);
}

void Win32IDE::syncLSPDocumentChange(const std::string& filePath, const std::string& content)
{
    LSPLanguage lang = LSPLanguage::Count;
    if (!prepareLSPDocument(filePath, lang))
        return;

    const std::string uri = filePathToUri(filePath);
    bool shouldOpen = false;
    {
        std::lock_guard<std::mutex> lock(m_lspMutex);
        if (m_lspOpenDocuments.find(uri) == m_lspOpenDocuments.end())
        {
            m_lspOpenDocuments[uri] = lang;
            shouldOpen = true;
        }
    }

    if (shouldOpen)
        sendDidOpen(lang, uri, lspLanguageId(lang), content);

    sendDidChange(lang, uri, content);
}

void Win32IDE::syncLSPDocumentClose(const std::string& filePath)
{
    if (filePath.empty() || !m_lspInitialized)
        return;

    const LSPLanguage lang = detectLanguageForFile(filePath);
    if (lang >= LSPLanguage::Count)
        return;

    const std::string uri = filePathToUri(filePath);
    {
        std::lock_guard<std::mutex> lock(m_lspMutex);
        auto it = m_lspOpenDocuments.find(uri);
        if (it == m_lspOpenDocuments.end())
            return;
        m_lspOpenDocuments.erase(it);
    }

    auto& status = m_lspStatuses[(size_t)lang];
    if (!status.hStdinWrite || (status.state != LSPServerState::Running && status.state != LSPServerState::Starting))
        return;

    sendDidClose(lang, uri);
    clearDiagnostics(uri);
}

void Win32IDE::syncLSPDocumentSave(const std::string& filePath)
{
    LSPLanguage lang = LSPLanguage::Count;
    if (!prepareLSPDocument(filePath, lang))
        return;

    sendDidSave(lang, filePathToUri(filePath));
}

std::string Win32IDE::getLSPStatusString() const
{
    std::ostringstream ss;
    ss << "[LSP] Language Server Status:\n";
    ss << "  ──────────────────────────────────────\n";

    for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i)
    {
        const auto& cfg = m_lspConfigs[i];
        const auto& st = m_lspStatuses[i];
        const char* stateStr = "STOPPED";
        switch (st.state)
        {
            case LSPServerState::Stopped:
                stateStr = "STOPPED";
                break;
            case LSPServerState::Starting:
                stateStr = "STARTING";
                break;
            case LSPServerState::Running:
                stateStr = "RUNNING";
                break;
            case LSPServerState::ShuttingDown:
                stateStr = "SHUTTING DOWN";
                break;
            case LSPServerState::Error:
                stateStr = "ERROR";
                break;
        }

        ss << "  " << lspLanguageString((LSPLanguage)i) << " (" << cfg.name << "):\n";
        ss << "    State:   " << stateStr;
        if (st.pid > 0)
            ss << " (pid=" << st.pid << ")";
        ss << "\n";
        ss << "    Enabled: " << (cfg.enabled ? "yes" : "no") << "\n";
        ss << "    Binary:  " << cfg.executablePath << "\n";
        ss << "    Reqs:    " << st.requestCount << "  Notifs: " << st.notificationCount << "\n";
        if (!st.lastError.empty())
        {
            ss << "    Error:   " << st.lastError << "\n";
        }
    }

    return ss.str();
}

std::string Win32IDE::getLSPStatsString() const
{
    std::ostringstream ss;
    ss << "[LSP] Statistics:\n";
    ss << "  Definition Requests:   " << m_lspStats.totalDefinitionRequests << "\n";
    ss << "  Reference Requests:    " << m_lspStats.totalReferenceRequests << "\n";
    ss << "  Rename Requests:       " << m_lspStats.totalRenameRequests << "\n";
    ss << "  Hover Requests:        " << m_lspStats.totalHoverRequests << "\n";
    ss << "  Completion Requests:   " << m_lspStats.totalCompletionRequests << "\n";
    ss << "  Signature Requests:    " << m_lspStats.totalSignatureRequests << "\n";
    ss << "  Diagnostics Received:  " << m_lspStats.totalDiagnosticsReceived << "\n";
    ss << "  Server Restarts:       " << m_lspStats.totalServerRestarts << "\n";
    return ss.str();
}

std::string Win32IDE::getLSPDiagnosticsSummary() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_lspDiagnosticsMutex));

    int totalErrors = 0;
    int totalWarnings = 0;
    int totalInfo = 0;
    int totalFiles = (int)m_lspDiagnostics.size();

    for (const auto& [uri, diags] : m_lspDiagnostics)
    {
        for (const auto& d : diags)
        {
            switch (d.severity)
            {
                case 1:
                    totalErrors++;
                    break;
                case 2:
                    totalWarnings++;
                    break;
                default:
                    totalInfo++;
                    break;
            }
        }
    }

    std::ostringstream ss;
    ss << "[LSP] Diagnostics: " << totalFiles << " file(s) — " << totalErrors << " errors, " << totalWarnings
       << " warnings, " << totalInfo << " info/hints";
    return ss.str();
}

void Win32IDE::cmdLSPGotoDefinition()
{
    if (!m_lspInitialized)
    {
        appendToOutput("[LSP] Not initialized. Starting configured LSP servers...", "General", OutputSeverity::Info);
        startAllLSPServers();
    }

    // Get cursor position from editor
    CHARRANGE sel = {};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    // Convert character offset to line/column
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column = sel.cpMin - lineStart;

    std::string uri = filePathToUri(m_currentFile);
    auto locations = lspGotoDefinition(uri, lineIndex, column);

    if (locations.empty())
    {
        appendToOutput("[LSP] No definition found at cursor position.", "General", OutputSeverity::Info);
        return;
    }

    auto items = buildPeekItemsFromLspLocations(locations, PeekItemType::Definition, 3, 3);
    showPeekOverlayWithItems(items, lineIndex + 1, column + 1);

    appendToOutput("[LSP] " + std::to_string(locations.size()) + " definitions found — showing peek overlay.",
                   "General", OutputSeverity::Info);
}

void Win32IDE::cmdLSPFindReferences()
{
    if (!m_lspInitialized)
    {
        appendToOutput("[LSP] Not initialized. Starting configured LSP servers...", "General", OutputSeverity::Info);
        startAllLSPServers();
    }

    CHARRANGE sel = {};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column = sel.cpMin - lineStart;

    std::string uri = filePathToUri(m_currentFile);
    auto locations = lspFindReferences(uri, lineIndex, column);

    if (locations.empty())
    {
        appendToOutput("[LSP] No references found at cursor position.", "General", OutputSeverity::Info);
        return;
    }

    auto items = buildPeekItemsFromLspLocations(locations, PeekItemType::Reference, 2, 2);
    showPeekOverlayWithItems(items, lineIndex + 1, column + 1);

    appendToOutput("[LSP] " + std::to_string(locations.size()) + " references found — showing peek overlay.", "General",
                   OutputSeverity::Info);
}

void Win32IDE::cmdLSPRenameSymbol()
{
    if (!m_lspInitialized)
    {
        appendToOutput("[LSP] Not initialized. Starting configured LSP servers...", "General", OutputSeverity::Info);
        startAllLSPServers();
    }

    // Prefer chat input if present; otherwise prompt directly.
    std::string newName = getWindowText(m_hwndCopilotChatInput);
    if (newName.empty())
    {
        char renameBuf[512] = {};
        if (DialogBoxParamA(
                m_hInstance, "AGENT_PROMPT_DLG", m_hwndMain,
                [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR
                {
                    switch (msg)
                    {
                        case WM_INITDIALOG:
                            SetWindowTextA(hwnd, "LSP Rename Symbol");
                            SetWindowTextA(GetDlgItem(hwnd, 101), "Enter new symbol name:");
                            return TRUE;
                        case WM_COMMAND:
                            if (LOWORD(wp) == IDOK)
                            {
                                GetDlgItemTextA(hwnd, 102, (char*)lp, 512);
                                EndDialog(hwnd, IDOK);
                                return TRUE;
                            }
                            else if (LOWORD(wp) == IDCANCEL)
                            {
                                EndDialog(hwnd, IDCANCEL);
                                return TRUE;
                            }
                            break;
                    }
                    return FALSE;
                },
                (LPARAM)renameBuf) != IDOK)
        {
            appendToOutput("[LSP] Rename cancelled", "General", OutputSeverity::Info);
            return;
        }
        newName = renameBuf;
    }

    if (newName.empty())
    {
        appendToOutput("[LSP] New symbol name is required.", "General", OutputSeverity::Warning);
        return;
    }

    CHARRANGE sel = {};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column = sel.cpMin - lineStart;

    std::string uri = filePathToUri(m_currentFile);
    LSPWorkspaceEdit edit = lspRenameSymbol(uri, lineIndex, column, newName);

    if (edit.changes.empty())
    {
        appendToOutput("[LSP] Rename produced no changes. Symbol may not be renameable.", "General",
                       OutputSeverity::Info);
        return;
    }

    // Show what will change (user visibility)
    int totalEdits = 0;
    std::ostringstream ss;
    ss << "[LSP] Rename → '" << newName << "' will modify:\n";
    for (const auto& [fileUri, edits] : edit.changes)
    {
        ss << "  " << uriToFilePath(fileUri) << " (" << edits.size() << " edits)\n";
        totalEdits += (int)edits.size();
    }
    appendToOutput(ss.str(), "General", OutputSeverity::Info);

    // Apply the edit
    bool ok = applyWorkspaceEdit(edit);
    if (ok)
    {
        setWindowText(m_hwndCopilotChatInput, "");
        appendToOutput("[LSP] Rename complete: " + std::to_string(totalEdits) + " edit(s) applied.", "General",
                       OutputSeverity::Info);
    }
    else
    {
        appendToOutput("[LSP] Rename failed — could not apply edits.", "General", OutputSeverity::Error);
    }
}

void Win32IDE::cmdLSPHoverInfo()
{
    if (!m_lspInitialized)
    {
        appendToOutput("[LSP] Not initialized. Starting configured LSP servers...", "General", OutputSeverity::Info);
        startAllLSPServers();
    }

    CHARRANGE sel = {};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column = sel.cpMin - lineStart;

    std::string uri = filePathToUri(m_currentFile);
    LSPHoverInfo hover = lspHover(uri, lineIndex, column);

    if (!hover.valid)
    {
        appendToOutput("[LSP] No hover info at cursor position.", "General", OutputSeverity::Info);
        return;
    }

    // Truncate very long hovers
    std::string display = hover.contents;
    if (display.size() > 2000)
    {
        display = display.substr(0, 2000) + "\n... (truncated)";
    }

    appendToOutput("[LSP] Hover:\n" + display, "General", OutputSeverity::Info);
}

std::string Win32IDE::getLSPConfigFilePath() const
{
    std::string dir = getSessionFilePath();
    size_t pos = dir.find_last_of("/\\");
    if (pos != std::string::npos)
    {
        dir = dir.substr(0, pos + 1) + "lsp.json";
    }
    else
    {
        dir = "lsp.json";
    }
    return dir;
}

void Win32IDE::loadLSPConfig()
{
    std::string path = getLSPConfigFilePath();
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs.is_open())
    {
        logInfo("[LSP] No saved config at " + path + " — using defaults");
        return;
    }

    try
    {
        const std::streamoff fileSize = ifs.tellg();
        if (fileSize < 0 || static_cast<size_t>(fileSize) > kMaxLspConfigBytes)
        {
            logInfo("[LSP] Config too large — using defaults");
            return;
        }
        ifs.seekg(0, std::ios::beg);
        std::string fileContent(static_cast<size_t>(fileSize), '\0');
        if (!fileContent.empty())
        {
            ifs.read(fileContent.data(), static_cast<std::streamsize>(fileContent.size()));
            if (!ifs)
            {
                logInfo("[LSP] Failed to read config fully — using defaults");
                return;
            }
        }
        nlohmann::json j = nlohmann::json::parse(fileContent);

        if (j.contains("servers") && j["servers"].is_array())
        {
            for (size_t si = 0; si < j["servers"].size(); ++si)
            {
                const auto& sj = j["servers"][si];
                std::string langName = sj.value("language", "");
                LSPLanguage lang = lspLanguageFromString(langName);
                if (lang >= LSPLanguage::Count)
                    continue;

                auto& cfg = m_lspConfigs[(size_t)lang];
                if (sj.contains("name"))
                    cfg.name = sj["name"].get<std::string>();
                if (sj.contains("executablePath"))
                    cfg.executablePath = sj["executablePath"].get<std::string>();
                if (sj.contains("enabled"))
                    cfg.enabled = sj["enabled"].get<bool>();
                if (sj.contains("initTimeoutMs"))
                    cfg.initTimeoutMs = sj["initTimeoutMs"].get<int>();
                if (sj.contains("args") && sj["args"].is_array())
                {
                    cfg.args.clear();
                    const size_t argCount = std::min(sj["args"].size(), kMaxLspArgs);
                    for (size_t ai = 0; ai < argCount; ++ai)
                    {
                        if (!sj["args"][ai].is_string())
                            continue;
                        std::string arg = sj["args"][ai].get<std::string>();
                        if (arg.size() > kMaxLspArgBytes)
                            continue;
                        cfg.args.push_back(std::move(arg));
                    }
                }
                if (sj.contains("rootUri"))
                    cfg.rootUri = sj["rootUri"].get<std::string>();
            }
        }

        logInfo("[LSP] Loaded config from " + path);
    }
    catch (const std::exception& e)
    {
        logError("loadLSPConfig", std::string("JSON parse error: ") + e.what());
    }
}

void Win32IDE::saveLSPConfig()
{
    std::string path = getLSPConfigFilePath();

    std::filesystem::path p(path);
    if (p.has_parent_path())
    {
        std::filesystem::create_directories(p.parent_path());
    }

    nlohmann::json j;
    nlohmann::json servers = nlohmann::json::array();

    for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i)
    {
        const auto& cfg = m_lspConfigs[i];
        nlohmann::json sj;
        sj["language"] = lspLanguageString((LSPLanguage)i);
        sj["name"] = cfg.name;
        sj["executablePath"] = cfg.executablePath;
        sj["enabled"] = cfg.enabled;
        sj["initTimeoutMs"] = cfg.initTimeoutMs;
        sj["rootUri"] = cfg.rootUri;

        nlohmann::json argsArr = nlohmann::json::array();
        for (const auto& arg : cfg.args)
        {
            argsArr.push_back(arg);
        }
        sj["args"] = argsArr;

        servers.push_back(sj);
    }
    j["servers"] = servers;

    std::ofstream ofs(path);
    if (ofs.is_open())
    {
        ofs << j.dump(2);
        logInfo("[LSP] Saved config to " + path);
    }
    else
    {
        logError("saveLSPConfig", "Failed to write " + path);
    }
}

void Win32IDE::handleLSPStatusEndpoint(SOCKET client)
{
    nlohmann::json j;
    j["initialized"] = m_lspInitialized;

    nlohmann::json servers = nlohmann::json::array();
    for (size_t i = 0; i < (size_t)LSPLanguage::Count; ++i)
    {
        const auto& cfg = m_lspConfigs[i];
        const auto& st = m_lspStatuses[i];

        nlohmann::json sj;
        sj["language"] = lspLanguageString((LSPLanguage)i);
        sj["name"] = cfg.name;
        sj["enabled"] = cfg.enabled;
        sj["state"] = (int)st.state;

        std::string stateStr = "stopped";
        switch (st.state)
        {
            case LSPServerState::Stopped:
                stateStr = "stopped";
                break;
            case LSPServerState::Starting:
                stateStr = "starting";
                break;
            case LSPServerState::Running:
                stateStr = "running";
                break;
            case LSPServerState::ShuttingDown:
                stateStr = "shutting_down";
                break;
            case LSPServerState::Error:
                stateStr = "error";
                break;
        }
        sj["stateString"] = stateStr;
        sj["pid"] = (int)st.pid;
        sj["requests"] = st.requestCount;
        sj["notifications"] = st.notificationCount;
        if (!st.lastError.empty())
            sj["lastError"] = st.lastError;

        servers.push_back(sj);
    }
    j["servers"] = servers;

    // Stats
    nlohmann::json stats;
    stats["definitionRequests"] = m_lspStats.totalDefinitionRequests;
    stats["referenceRequests"] = m_lspStats.totalReferenceRequests;
    stats["renameRequests"] = m_lspStats.totalRenameRequests;
    stats["hoverRequests"] = m_lspStats.totalHoverRequests;
    stats["completionRequests"] = m_lspStats.totalCompletionRequests;
    stats["signatureRequests"] = m_lspStats.totalSignatureRequests;
    stats["diagnosticsReceived"] = m_lspStats.totalDiagnosticsReceived;
    stats["serverRestarts"] = m_lspStats.totalServerRestarts;
    j["stats"] = stats;

    std::string body = j.dump(2);
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                       "Content-Length: " +
                       std::to_string(body.size()) + "\r\n\r\n" + body;
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// textDocument/semanticTokens/full — LSP 3.16 uint32[] → SemanticToken ranges
std::vector<Win32IDE::SemanticToken> Win32IDE::lspSemanticTokensFull(const std::string& uri)
{
    std::vector<SemanticToken> tokens;

    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return tokens;
    }

    // Check server capability for semantic tokens
    nlohmann::json params;
    params["textDocument"]["uri"] = uri;

    int id = sendLSPRequest(lang, "textDocument/semanticTokens/full", params);
    if (id < 0)
        return tokens;

    nlohmann::json resp = readLSPResponse(lang, id, 10000);  // semantic tokens can be slow
    if (!resp.contains("result") || resp["result"].is_null())
        return tokens;

    const auto& result = resp["result"];
    if (!result.contains("data") || !result["data"].is_array())
        return tokens;

    const auto& data = result["data"];
    if (data.size() % 5 != 0)
    {
        fprintf(stderr, "[LSP] Semantic tokens data array size not multiple of 5: %zu\n", data.size());
        return tokens;
    }

    // Decode the delta-encoded token stream
    int currentLine = 0;
    int currentChar = 0;

    for (size_t i = 0; i + 4 < data.size(); i += 5)
    {
        int deltaLine = data[i].get<int>();
        int deltaStartChar = data[i + 1].get<int>();
        int length = data[i + 2].get<int>();
        int tokenType = data[i + 3].get<int>();
        int tokenModifiers = data[i + 4].get<int>();

        if (deltaLine > 0)
        {
            currentLine += deltaLine;
            currentChar = deltaStartChar;
        }
        else
        {
            currentChar += deltaStartChar;
        }

        SemanticToken tok;
        tok.line = currentLine;
        tok.startChar = currentChar;
        tok.length = length;
        tok.tokenType = tokenType;
        tok.modifiers = tokenModifiers;

        // Map type index to name string
        if (tokenType >= 0 && tokenType < s_numSemanticTokenTypes)
        {
            tok.typeName = s_semanticTokenTypes[tokenType];
        }
        else
        {
            tok.typeName = "unknown";
        }

        tokens.push_back(tok);
    }

    fprintf(stderr, "[LSP] Semantic tokens: %zu tokens for %s\n", tokens.size(), uri.c_str());
    return tokens;
}

void Win32IDE::handleLSPDiagnosticsEndpoint(SOCKET client)
{
    nlohmann::json j = nlohmann::json::array();

    auto allDiags = getAllDiagnostics();
    for (const auto& [uri, diags] : allDiags)
    {
        nlohmann::json fj;
        fj["uri"] = uri;
        fj["file"] = uriToFilePath(uri);

        nlohmann::json diagArr = nlohmann::json::array();
        for (const auto& d : diags)
        {
            nlohmann::json dj;
            dj["severity"] = d.severity;
            dj["message"] = d.message;
            dj["code"] = d.code;
            dj["source"] = d.source;
            dj["startLine"] = d.range.start.line;
            dj["startCharacter"] = d.range.start.character;
            dj["endLine"] = d.range.end.line;
            dj["endCharacter"] = d.range.end.character;
            diagArr.push_back(dj);
        }
        fj["diagnostics"] = diagArr;
        fj["diagnosticCount"] = (int)diags.size();
        j.push_back(fj);
    }

    std::string body = j.dump(2);
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                       "Content-Length: " +
                       std::to_string(body.size()) + "\r\n\r\n" + body;
    send(client, resp.c_str(), (int)resp.size(), 0);
}

bool Win32IDE::lspOrganizeImports()
{
    if (m_currentFile.empty())
        return false;

    LSPLanguage lang = detectLanguageForFile(m_currentFile);
    if (lang >= LSPLanguage::Count)
        return false;
    if (m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
        return false;

    std::string uri = filePathToUri(m_currentFile);

    // Count lines so we can send an "entire file" range
    int lineCount = 0;
    {
        std::ifstream ifs(m_currentFile);
        for (std::string l; std::getline(ifs, l); ++lineCount)
        {
        }
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["range"]["start"]["line"] = 0;
    params["range"]["start"]["character"] = 0;
    params["range"]["end"]["line"] = lineCount > 0 ? lineCount - 1 : 0;
    params["range"]["end"]["character"] = 0;
    params["context"]["diagnostics"] = nlohmann::json::array();
    params["context"]["only"] = nlohmann::json::array({"source.organizeImports"});

    int id = sendLSPRequest(lang, "textDocument/codeAction", params);
    if (id < 0)
        return false;

    nlohmann::json resp = readLSPResponse(lang, id, 10000);
    if (!resp.contains("result") || !resp["result"].is_array())
        return false;

    bool anyApplied = false;
    for (const auto& action : resp["result"])
    {
        // Accept CodeAction objects whose kind matches; skip bare Command objects
        if (!action.contains("edit"))
            continue;
        const auto& editj = action["edit"];

        LSPWorkspaceEdit edit;
        // changes: { uri: TextEdit[] }
        if (editj.contains("changes") && editj["changes"].is_object())
        {
            for (const auto& [fileUri, editsArr] : editj["changes"].items())
            {
                std::vector<LSPWorkspaceEdit::TextEdit> tes;
                for (const auto& ej : editsArr)
                {
                    LSPWorkspaceEdit::TextEdit te;
                    te.newText = ej.value("newText", "");
                    if (ej.contains("range"))
                    {
                        const auto& rj = ej["range"];
                        te.range.start.line = rj["start"].value("line", 0);
                        te.range.start.character = rj["start"].value("character", 0);
                        te.range.end.line = rj["end"].value("line", 0);
                        te.range.end.character = rj["end"].value("character", 0);
                    }
                    tes.push_back(te);
                }
                edit.changes[fileUri] = tes;
            }
        }
        // documentChanges: TextDocumentEdit[]
        else if (editj.contains("documentChanges") && editj["documentChanges"].is_array())
        {
            for (const auto& dc : editj["documentChanges"])
            {
                if (!dc.contains("textDocument") || !dc.contains("edits"))
                    continue;
                std::string fileUri2 = dc["textDocument"].value("uri", "");
                std::vector<LSPWorkspaceEdit::TextEdit> tes;
                for (const auto& ej : dc["edits"])
                {
                    LSPWorkspaceEdit::TextEdit te;
                    te.newText = ej.value("newText", "");
                    if (ej.contains("range"))
                    {
                        const auto& rj = ej["range"];
                        te.range.start.line = rj["start"].value("line", 0);
                        te.range.start.character = rj["start"].value("character", 0);
                        te.range.end.line = rj["end"].value("line", 0);
                        te.range.end.character = rj["end"].value("character", 0);
                    }
                    tes.push_back(te);
                }
                edit.changes[fileUri2] = tes;
            }
        }

        if (!edit.changes.empty() && applyWorkspaceEdit(edit))
        {
            reloadCurrentFile();
            anyApplied = true;
            LOG_INFO("lspOrganizeImports: applied " + action.value("title", "organize imports"));
            break;  // one action is sufficient
        }
    }
    return anyApplied;
}


std::vector<Win32IDE::LSPWorkspaceEdit::TextEdit> Win32IDE::lspDocumentFormatting(const std::string& filePath)
{
    std::vector<LSPWorkspaceEdit::TextEdit> edits;
    LSPLanguage lang = detectLanguageForFile(filePath);
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return edits;
    }

    std::string uri = filePathToUri(filePath);
    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["options"]["tabSize"] = 4;
    params["options"]["insertSpaces"] = true;
    params["options"]["trimTrailingWhitespace"] = true;
    params["options"]["insertFinalNewline"] = true;

    int id = sendLSPRequest(lang, "textDocument/formatting", params);
    if (id < 0)
        return edits;

    nlohmann::json resp = readLSPResponse(lang, id, 5000);
    m_lspStats.totalFormattingRequests++;

    if (!resp.contains("result") || resp["result"].is_null() || !resp["result"].is_array())
        return edits;

    const auto& result = resp["result"];
    for (size_t i = 0; i < result.size(); ++i)
    {
        const auto& ej = result[i];
        LSPWorkspaceEdit::TextEdit te;

        if (ej.contains("range"))
        {
            const auto& rj = ej["range"];
            if (rj.contains("start"))
            {
                te.range.start.line = rj["start"].value("line", 0);
                te.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end"))
            {
                te.range.end.line = rj["end"].value("line", 0);
                te.range.end.character = rj["end"].value("character", 0);
            }
        }
        te.newText = ej.value("newText", "");
        edits.push_back(te);
    }

    return edits;
}

void Win32IDE::cmdLSPFormatDocument()
{
    if (m_currentFile.empty())
    {
        appendToOutput("[LSP] No document currently active.", "General", OutputSeverity::Warning);
        return;
    }

    auto edits = lspDocumentFormatting(m_currentFile);
    if (edits.empty())
    {
        appendToOutput("[LSP] No formatting changes needed.", "General", OutputSeverity::Info);
        return;
    }

    // Build WorkspaceEdit and apply
    nlohmann::json edit;
    std::string uri = filePathToUri(m_currentFile);
    for (const auto& te : edits)
    {
        nlohmann::json textEdit;
        textEdit["range"]["start"]["line"] = te.range.start.line;
        textEdit["range"]["start"]["character"] = te.range.start.character;
        textEdit["range"]["end"]["line"] = te.range.end.line;
        textEdit["range"]["end"]["character"] = te.range.end.character;
        textEdit["newText"] = te.newText;
        edit["changes"][uri].push_back(textEdit);
    }

    applyWorkspaceEdit(edit);
}

bool Win32IDE::applyWorkspaceEdit(const nlohmann::json& editJson)
{
    if (!editJson.is_object())
        return false;

    LSPWorkspaceEdit typedEdit;

    // 1. Handle 'changes' (uri -> TextEdit[])
    if (editJson.contains("changes") && editJson["changes"].is_object())
    {
        for (auto it = editJson["changes"].begin(); it != editJson["changes"].end(); ++it)
        {
            const std::string uri = it.key();
            const nlohmann::json& textEdits = it.value();
            if (!textEdits.is_array())
                continue;

            auto& typedEdits = typedEdit.changes[uri];
            for (const auto& textEdit : textEdits)
            {
                if (!textEdit.is_object() || !textEdit.contains("range") || !textEdit.contains("newText"))
                    continue;

                const auto& range = textEdit["range"];
                if (!range.is_object() || !range.contains("start") || !range.contains("end"))
                    continue;

                const auto& start = range["start"];
                const auto& end = range["end"];
                if (!start.is_object() || !end.is_object())
                    continue;

                LSPWorkspaceEdit::TextEdit typed;
                typed.range.start.line = start.value("line", 0);
                typed.range.start.character = start.value("character", 0);
                typed.range.end.line = end.value("line", 0);
                typed.range.end.character = end.value("character", 0);
                typed.newText = textEdit.value("newText", std::string());
                typedEdits.push_back(std::move(typed));
            }
        }
    }

    // 2. Handle 'documentChanges' (TextDocumentEdit[] | ResourceOperation[])
    if (editJson.contains("documentChanges") && editJson["documentChanges"].is_array())
    {
        const auto& docChanges = editJson["documentChanges"];
        for (const auto& dc : docChanges)
        {
            if (dc.contains("kind"))
            {
                // Resource Operation
                LSPWorkspaceEdit::ResourceOperation op;
                std::string kind = dc.value("kind", "");
                if (kind == "create")
                {
                    op.type = LSPWorkspaceEdit::ResourceOperation::Type::Create;
                    op.uri = dc.value("uri", "");
                    op.overwrite = dc.value("options", nlohmann::json::object()).value("overwrite", false);
                    op.ignoreIfExists = dc.value("options", nlohmann::json::object()).value("ignoreIfExists", false);
                }
                else if (kind == "rename")
                {
                    op.type = LSPWorkspaceEdit::ResourceOperation::Type::Rename;
                    op.uri = dc.value("oldUri", "");
                    op.newUri = dc.value("newUri", "");
                    op.overwrite = dc.value("options", nlohmann::json::object()).value("overwrite", false);
                }
                else if (kind == "delete")
                {
                    op.type = LSPWorkspaceEdit::ResourceOperation::Type::Delete;
                    op.uri = dc.value("uri", "");
                    op.recursive = dc.value("options", nlohmann::json::object()).value("recursive", false);
                    op.ignoreIfNotExists = dc.value("options", nlohmann::json::object()).value("ignoreIfNotExists", false);
                }
                typedEdit.resourceOperations.push_back(op);
            }
            else if (dc.contains("textDocument") && dc.contains("edits"))
            {
                // TextDocumentEdit
                std::string uri = dc["textDocument"].value("uri", "");
                if (dc["edits"].is_array())
                {
                    auto& typedEdits = typedEdit.changes[uri];
                    for (const auto& ej : dc["edits"])
                    {
                        if (!ej.is_object() || !ej.contains("range") || !ej.contains("newText"))
                            continue;
                        const auto& range = ej["range"];
                        LSPWorkspaceEdit::TextEdit typed;
                        typed.range.start.line = range["start"].value("line", 0);
                        typed.range.start.character = range["start"].value("character", 0);
                        typed.range.end.line = range["end"].value("line", 0);
                        typed.range.end.character = range["end"].value("character", 0);
                        typed.newText = ej.value("newText", std::string());
                        typedEdits.push_back(std::move(typed));
                    }
                }
            }
        }
    }

    if (typedEdit.changes.empty() && typedEdit.resourceOperations.empty())
        return false;

    return applyWorkspaceEdit(typedEdit);
}



std::vector<Win32IDE::LSPWorkspaceEdit::TextEdit> Win32IDE::lspRangeFormatting(const std::string& filePath,
                                                                               int startLine, int startChar,
                                                                               int endLine, int endChar)
{
    std::vector<LSPWorkspaceEdit::TextEdit> edits;
    LSPLanguage lang = detectLanguageForFile(filePath);
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return edits;
    }

    std::string uri = filePathToUri(filePath);
    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["range"]["start"]["line"] = startLine;
    params["range"]["start"]["character"] = startChar;
    params["range"]["end"]["line"] = endLine;
    params["range"]["end"]["character"] = endChar;
    params["options"]["tabSize"] = 4;
    params["options"]["insertSpaces"] = true;

    int id = sendLSPRequest(lang, "textDocument/rangeFormatting", params);
    if (id < 0)
        return edits;

    nlohmann::json resp = readLSPResponse(lang, id, 5000);
    m_lspStats.totalFormattingRequests++;

    if (!resp.contains("result") || resp["result"].is_null() || !resp["result"].is_array())
        return edits;

    const auto& result = resp["result"];
    for (size_t i = 0; i < result.size(); ++i)
    {
        const auto& ej = result[i];
        LSPWorkspaceEdit::TextEdit te;

        if (ej.contains("range"))
        {
            const auto& rj = ej["range"];
            if (rj.contains("start"))
            {
                te.range.start.line = rj["start"].value("line", 0);
                te.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end"))
            {
                te.range.end.line = rj["end"].value("line", 0);
                te.range.end.character = rj["end"].value("character", 0);
            }
        }
        te.newText = ej.value("newText", "");
        edits.push_back(te);
    }

    return edits;
}

void Win32IDE::cmdLSPFormatRange()
{
    if (m_currentFile.empty())
    {
        appendToOutput("[LSP] No document currently active.", "General", OutputSeverity::Warning);
        return;
    }

    if (!m_lspInitialized)
    {
        appendToOutput("[LSP] Not initialized. Starting configured LSP servers...", "General", OutputSeverity::Info);
        startAllLSPServers();
    }

    CHARRANGE sel = {};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    if (sel.cpMin == sel.cpMax)
    {
        cmdLSPFormatDocument();
        return;
    }

    int startLine = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    int startLineOffset = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, startLine, 0);
    int startColumn = (sel.cpMin >= startLineOffset) ? (sel.cpMin - startLineOffset) : 0;

    int endLine = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMax);
    int endLineOffset = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, endLine, 0);
    int endColumn = (sel.cpMax >= endLineOffset) ? (sel.cpMax - endLineOffset) : 0;

    auto edits = lspRangeFormatting(m_currentFile, startLine, startColumn, endLine, endColumn);
    if (edits.empty())
    {
        appendToOutput("[LSP] Formatting returned no changes for the selected range.", "General", OutputSeverity::Info);
        return;
    }

    LSPWorkspaceEdit edit;
    edit.changes[filePathToUri(m_currentFile)] = std::move(edits);
    bool ok = applyWorkspaceEdit(edit);
    appendToOutput(ok ? "[LSP] Formatted selected range." : "[LSP] Failed to apply range formatting edits.", "General",
                   ok ? OutputSeverity::Info : OutputSeverity::Error);
}


std::vector<Win32IDE::LSPSymbolInfo> Win32IDE::lspDocumentSymbols(const std::string& uri)
{
    std::vector<LSPSymbolInfo> symbols;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return symbols;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;

    int id = sendLSPRequest(lang, "textDocument/documentSymbol", params);
    if (id < 0)
        return symbols;

    nlohmann::json resp = readLSPResponse(lang, id, 5000);
    m_lspStats.totalDocumentSymbolRequests++;

    if (!resp.contains("result") || !resp["result"].is_array())
        return symbols;

    const auto& result = resp["result"];
    for (size_t i = 0; i < result.size(); ++i)
    {
        const auto& sj = result[i];
        LSPSymbolInfo sym;

        sym.name = sj.value("name", "");
        sym.kind = sj.value("kind", 0);
        sym.detail = sj.value("detail", "");
        sym.containerName = sj.value("containerName", "");

        if (sj.contains("location"))
        {
            const auto& lj = sj["location"];
            sym.location.uri = lj.value("uri", "");
            if (lj.contains("range"))
            {
                const auto& rj = lj["range"];
                if (rj.contains("start"))
                {
                    sym.location.range.start.line = rj["start"].value("line", 0);
                    sym.location.range.start.character = rj["start"].value("character", 0);
                }
                if (rj.contains("end"))
                {
                    sym.location.range.end.line = rj["end"].value("line", 0);
                    sym.location.range.end.character = rj["end"].value("character", 0);
                }
            }
        }

        symbols.push_back(sym);
    }

    return symbols;
}

void Win32IDE::cmdLSPDocumentSymbols()
{
    if (m_currentFile.empty())
    {
        appendToOutput("[LSP] No document currently active.", "General", OutputSeverity::Warning);
        return;
    }

    if (!m_lspInitialized)
    {
        appendToOutput("[LSP] Not initialized. Starting configured LSP servers...", "General", OutputSeverity::Info);
        startAllLSPServers();
    }

    showGoToSymbolPicker();
}


namespace
{

constexpr int kWorkspaceSymbolTimeoutMs = 6000;
constexpr size_t kMaxMergedWorkspaceSymbols = 800;

static std::string makeWorkspaceSymbolDedupKey(const Win32IDE::LSPSymbolInfo& s)
{
    return s.location.uri + '\x1e' + std::to_string(s.location.range.start.line) + '\x1e' +
           std::to_string(s.location.range.start.character) + '\x1e' + s.name;
}

static bool parseSymbolInformationJson(const nlohmann::json& sj, Win32IDE::LSPSymbolInfo& sym)
{
    sym.name = sj.value("name", "");
    sym.kind = sj.value("kind", 0);
    sym.detail = sj.value("detail", "");
    sym.containerName = sj.value("containerName", "");

    if (sj.contains("location") && sj["location"].is_object())
    {
        const auto& lj = sj["location"];
        sym.location.uri = lj.value("uri", "");
        if (lj.contains("range") && lj["range"].is_object())
        {
            const auto& rj = lj["range"];
            if (rj.contains("start"))
            {
                sym.location.range.start.line = rj["start"].value("line", 0);
                sym.location.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end"))
            {
                sym.location.range.end.line = rj["end"].value("line", 0);
                sym.location.range.end.character = rj["end"].value("character", 0);
            }
        }
    }
    // Handle DocumentSymbol (recursive/hierarchical) by picking selectionRange OR range as location
    else if (sj.contains("selectionRange") && sj["selectionRange"].is_object())
    {
        const auto& rj = sj["selectionRange"];
        sym.location.uri = "current";  // Placeholder, often filled by caller for doc symbols
        if (rj.contains("start"))
        {
            sym.location.range.start.line = rj["start"].value("line", 0);
            sym.location.range.start.character = rj["start"].value("character", 0);
        }
        if (rj.contains("end"))
        {
            sym.location.range.end.line = rj["end"].value("line", 0);
            sym.location.range.end.character = rj["end"].value("character", 0);
        }
    }

    return !sym.name.empty();
}

}  // namespace

std::vector<Win32IDE::LSPSymbolInfo> Win32IDE::lspWorkspaceSymbols(const std::string& query)
{
    std::vector<LSPSymbolInfo> symbols;
    symbols.reserve(256);
    std::unordered_set<std::string> seen;
    seen.reserve(512);

    nlohmann::json params;
    params["query"] = query;

    for (size_t li = 0; li < (size_t)LSPLanguage::Count; ++li)
    {
        const auto lang = static_cast<LSPLanguage>(li);
        if (!m_lspConfigs[li].enabled)
            continue;
        if (m_lspStatuses[li].state != LSPServerState::Running)
            continue;

        const int id = sendLSPRequest(lang, "workspace/symbol", params);
        if (id < 0)
            continue;

        const nlohmann::json resp = readLSPResponse(lang, id, kWorkspaceSymbolTimeoutMs);
        m_lspStats.totalWorkspaceSymbolRequests++;

        if (resp.contains("error"))
            continue;
        if (!resp.contains("result") || !resp["result"].is_array())
            continue;

        const auto& result = resp["result"];
        for (size_t i = 0; i < result.size() && symbols.size() < kMaxMergedWorkspaceSymbols; ++i)
        {
            LSPSymbolInfo sym{};
            if (!parseSymbolInformationJson(result[i], sym))
                continue;

            const std::string key = makeWorkspaceSymbolDedupKey(sym);
            if (!seen.insert(key).second)
                continue;

            symbols.push_back(std::move(sym));
        }

        if (symbols.size() >= kMaxMergedWorkspaceSymbols)
            break;
    }

    return symbols;
}


std::vector<Win32IDE::LSPInlayHint> Win32IDE::lspInlayHints(const std::string& uri, int startLine, int endLine)
{
    std::vector<LSPInlayHint> hints;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
        return hints;

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["range"]["start"] = {{"line", startLine}, {"character", 0}};
    params["range"]["end"] = {{"line", endLine}, {"character", 999}};

    int id = sendLSPRequest(lang, "textDocument/inlayHint", params);
    if (id < 0)
        return hints;

    nlohmann::json resp = readLSPResponse(lang, id, 3000);
    if (!resp.contains("result") || !resp["result"].is_array())
        return hints;

    for (const auto& hj : resp["result"])
    {
        LSPInlayHint hint;
        if (hj.contains("position"))
        {
            hint.position.line = hj["position"].value("line", 0);
            hint.position.character = hj["position"].value("character", 0);
        }

        if (hj.contains("label"))
        {
            if (hj["label"].is_string())
            {
                hint.label = hj["label"].get<std::string>();
            }
            else if (hj["label"].is_array())
            {
                for (const auto& part : hj["label"])
                {
                    hint.label += part.value("value", "");
                }
            }
        }

        hint.kind = (hj.value("kind", 1) == 1) ? "type" : "parameter";
        hint.tooltip = hj.value("tooltip", "");
        hint.paddingLeft = hj.value("paddingLeft", false);
        hint.paddingRight = hj.value("paddingRight", false);

        hints.push_back(hint);
    }

    return hints;
}


void Win32IDE::cmdLSPInlayHints()
{
    if (m_currentFile.empty())
        return;

    std::string uri = filePathToUri(m_currentFile);
    auto hints = lspInlayHints(uri, 0, 500);

    std::ostringstream ss;
    ss << "[LSP] Received " << hints.size() << " inlay hints for " << m_currentFile << "\n";
    for (const auto& h : hints)
    {
        ss << "  L" << (h.position.line + 1) << ":" << h.position.character << " [" << h.kind << "] " << h.label
           << "\n";
    }
    appendToOutput(ss.str(), "General", OutputSeverity::Info);

    m_lastInlayHints = std::move(hints);
    InvalidateRect(m_hwndEditor, NULL, FALSE);
}


void Win32IDE::cmdLSPWorkspaceSymbols()
{
    if (!m_lspInitialized)
    {
        appendToOutput("[LSP] Not initialized. Starting configured LSP servers...", "General", OutputSeverity::Info);
        startAllLSPServers();
    }

    showGoToWorkspaceSymbolPicker();
}


std::vector<Win32IDE::LSPLocation> Win32IDE::lspImplementation(const std::string& uri, int line, int character)
{
    std::vector<LSPLocation> results;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return results;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["position"]["line"] = line;
    params["position"]["character"] = character;

    int id = sendLSPRequest(lang, "textDocument/implementation", params);
    if (id < 0)
        return results;

    nlohmann::json resp = readLSPResponse(lang, id, 10000);
    m_lspStats.totalImplementationRequests++;

    if (!resp.contains("result") || resp["result"].is_null())
        return results;

    auto parseLocation = [](const nlohmann::json& lj) -> LSPLocation
    {
        LSPLocation loc;
        loc.uri = lj.value("uri", "");
        if (lj.contains("range"))
        {
            const auto& rj = lj["range"];
            if (rj.contains("start"))
            {
                loc.range.start.line = rj["start"].value("line", 0);
                loc.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end"))
            {
                loc.range.end.line = rj["end"].value("line", 0);
                loc.range.end.character = rj["end"].value("character", 0);
            }
        }
        return loc;
    };

    const auto& result = resp["result"];
    if (result.is_array())
    {
        for (size_t ri = 0; ri < result.size(); ++ri)
        {
            results.push_back(parseLocation(result[ri]));
        }
    }
    else if (result.is_object())
    {
        results.push_back(parseLocation(result));
    }

    return results;
}

void Win32IDE::cmdLSPImplementation()
{
    if (m_currentFile.empty())
        return;

    if (!m_lspInitialized)
    {
        appendToOutput("[LSP] Not initialized. Starting configured LSP servers...", "General", OutputSeverity::Info);
        startAllLSPServers();
    }

    if (!m_hwndEditor)
        return;

    CHARRANGE sel = {};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column = (sel.cpMin >= lineStart) ? (sel.cpMin - lineStart) : 0;

    std::string uri = filePathToUri(m_currentFile);
    auto locations = lspImplementation(uri, lineIndex, column);

    if (locations.empty())
    {
        appendToOutput("[LSP] No implementations found.", "General", OutputSeverity::Info);
        return;
    }

    auto items = buildPeekItemsFromLspLocations(locations, PeekItemType::Definition, 3, 3);
    showPeekOverlayWithItems(items, lineIndex + 1, column + 1);

    appendToOutput("[LSP] " + std::to_string(locations.size()) + " implementation(s) found — showing peek overlay.",
                   "General", OutputSeverity::Info);
}


std::vector<Win32IDE::LSPLocation> Win32IDE::lspTypeDefinition(const std::string& uri, int line, int character)
{
    std::vector<LSPLocation> results;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return results;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["position"]["line"] = line;
    params["position"]["character"] = character;

    int id = sendLSPRequest(lang, "textDocument/typeDefinition", params);
    if (id < 0)
        return results;

    nlohmann::json resp = readLSPResponse(lang, id, 5000);
    m_lspStats.totalTypeDefinitionRequests++;

    if (!resp.contains("result") || resp["result"].is_null())
        return results;

    auto parseLocation = [](const nlohmann::json& lj) -> LSPLocation
    {
        LSPLocation loc;
        loc.uri = lj.value("uri", "");
        if (lj.contains("range"))
        {
            const auto& rj = lj["range"];
            if (rj.contains("start"))
            {
                loc.range.start.line = rj["start"].value("line", 0);
                loc.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end"))
            {
                loc.range.end.line = rj["end"].value("line", 0);
                loc.range.end.character = rj["end"].value("character", 0);
            }
        }
        return loc;
    };

    const auto& result = resp["result"];
    if (result.is_array())
    {
        for (size_t ri = 0; ri < result.size(); ++ri)
        {
            results.push_back(parseLocation(result[ri]));
        }
    }
    else if (result.is_object())
    {
        results.push_back(parseLocation(result));
    }

    return results;
}

void Win32IDE::cmdLSPTypeDefinition()
{
    if (m_currentFile.empty())
        return;

    if (!m_lspInitialized)
    {
        appendToOutput("[LSP] Not initialized. Starting configured LSP servers...", "General", OutputSeverity::Info);
        startAllLSPServers();
    }

    if (!m_hwndEditor)
        return;

    CHARRANGE sel = {};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column = (sel.cpMin >= lineStart) ? (sel.cpMin - lineStart) : 0;

    std::string uri = filePathToUri(m_currentFile);
    auto locations = lspTypeDefinition(uri, lineIndex, column);

    if (locations.empty())
    {
        appendToOutput("[LSP] Type definition not found.", "General", OutputSeverity::Info);
        return;
    }

    auto items = buildPeekItemsFromLspLocations(locations, PeekItemType::Definition, 3, 3);
    showPeekOverlayWithItems(items, lineIndex + 1, column + 1);

    appendToOutput("[LSP] " + std::to_string(locations.size()) +
                       " type definition result(s) found — showing peek overlay.",
                   "General", OutputSeverity::Info);
}
