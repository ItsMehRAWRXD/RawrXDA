#include "SovereignMCPBridge.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <vector>
// Phase 48: Production Win32 pipe-based JSON-RPC MCP transport.

static bool g_bridgeDebug = (GetEnvironmentVariableA("RAWR_PROBE_DEBUG", nullptr, 0) > 0);

static std::string hexDump(const void* data, size_t len, size_t max = 256) {
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    size_t dumpLen = (len < max) ? len : max;
    std::string out;
    char buf[8];
    for (size_t i = 0; i < dumpLen; ++i) {
        snprintf(buf, sizeof(buf), "%02x ", bytes[i]);
        out += buf;
        if ((i + 1) % 32 == 0) out += "\n";
    }
    if (len > max) out += "...";
    return out;
}

static void bridgeLog(const std::string& msg) {
    if (g_bridgeDebug) {
        std::cerr << "[BRIDGE] " << msg << std::endl;
    }
}

namespace {

bool writeAllWithTimeout(HANDLE h, const char* data, DWORD size, DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    DWORD total = 0;
    while (total < size) {
        if ((GetTickCount() - start) >= timeoutMs) {
            return false;
        }

        DWORD wrote = 0;
        const char* ptr = data + total;
        const DWORD remain = size - total;
        const BOOL ok = WriteFile(h, ptr, remain, &wrote, nullptr);
        if (!ok || wrote == 0) {
            return false;
        }
        total += wrote;
    }
    return true;
}

bool readWithTimeout(HANDLE h, char* dst, DWORD toRead, DWORD& outRead, DWORD timeoutMs) {
    outRead = 0;
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        DWORD avail = 0;
        if (!PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr)) {
            return false;
        }

        if (avail > 0) {
            const DWORD want = (avail < toRead) ? avail : toRead;
            const BOOL ok = ReadFile(h, dst, want, &outRead, nullptr);
            return ok && outRead > 0;
        }

        Sleep(1);
    }

    return false;
}

std::string findOnPath(const char* exeName) {
    char resolved[MAX_PATH] = {0};
    DWORD len = SearchPathA(nullptr, exeName, nullptr, MAX_PATH, resolved, nullptr);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    return std::string(resolved, resolved + len);
}

std::string extractPyScriptPath(const std::string& command) {
    // Prefer the last .py occurrence so we resolve the script path, not py.exe.
    const size_t pyPos = command.rfind(".py");
    if (pyPos == std::string::npos) {
        return {};
    }

    const size_t qStart = command.rfind('"', pyPos);
    const size_t qEnd = command.find('"', pyPos);
    if (qStart != std::string::npos && qEnd != std::string::npos && qEnd > qStart) {
        return command.substr(qStart + 1, qEnd - qStart - 1);
    }

    size_t start = pyPos;
    while (start > 0 && !std::isspace(static_cast<unsigned char>(command[start - 1]))) {
        --start;
    }
    size_t end = pyPos + 3;
    while (end < command.size() && !std::isspace(static_cast<unsigned char>(command[end]))) {
        ++end;
    }
    return command.substr(start, end - start);
}

std::vector<std::string> buildSpawnCandidates(const std::string& command) {
    std::vector<std::string> candidates;
    candidates.push_back(command);

    const std::string scriptPath = extractPyScriptPath(command);
    if (scriptPath.empty()) {
        return candidates;
    }

    const std::string pyLauncher = findOnPath("py.exe");
    const std::string pythonExe  = findOnPath("python.exe");
    const std::string python3Exe = findOnPath("python3.exe");

    if (!pythonExe.empty()) {
        candidates.push_back("\"" + pythonExe + "\" \"" + scriptPath + "\"");
    }
    if (!python3Exe.empty()) {
        candidates.push_back("\"" + python3Exe + "\" \"" + scriptPath + "\"");
    }
    if (!pyLauncher.empty()) {
        candidates.push_back("\"" + pyLauncher + "\" -3 \"" + scriptPath + "\"");
    }

    return candidates;
}

std::string extractExecutablePath(const std::string& commandLine) {
    if (commandLine.empty()) {
        return {};
    }

    if (commandLine[0] == '"') {
        const size_t end = commandLine.find('"', 1);
        if (end != std::string::npos && end > 1) {
            return commandLine.substr(1, end - 1);
        }
    }

    const size_t spacePos = commandLine.find(' ');
    if (spacePos == std::string::npos) {
        return commandLine;
    }
    return commandLine.substr(0, spacePos);
}

bool fileExists(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    const DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

} // namespace

namespace RawrXD::Runtime {

std::string SovereignMCPBridge::lastError() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_lastError;
}

void SovereignMCPBridge::setLastError(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError = msg;
}

SovereignMCPBridge& SovereignMCPBridge::instance() {
    static SovereignMCPBridge inst;
    return inst;
}

bool SovereignMCPBridge::spawnServer(const std::string& command, DWORD timeoutMs) {
    if (m_running.load()) return true;

    setLastError("stage=spawn_pre_check command=" + command);
    const auto candidates = buildSpawnCandidates(command);
    for (const auto& candidate : candidates) {
        const std::string exePath = extractExecutablePath(candidate);
        if (!exePath.empty() && !fileExists(exePath)) {
            setLastError("stage=spawn_exe_not_found candidate=" + candidate + " exe=" + exePath);
            continue;
        }

        setLastError("stage=spawn_create_pipes candidate=" + candidate);

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE stdinRead  = INVALID_HANDLE_VALUE, stdinWrite  = INVALID_HANDLE_VALUE;
        HANDLE stdoutRead = INVALID_HANDLE_VALUE, stdoutWrite = INVALID_HANDLE_VALUE;

        const BOOL inPipeOk = CreatePipe(&stdinRead, &stdinWrite, &sa, 65536);

        if (!inPipeOk) {
            setLastError("stage=spawn_create_stdin_pipe_failed candidate=" + candidate + " gle=" + std::to_string(GetLastError()));
            continue;
        }

        const BOOL outPipeOk = CreatePipe(&stdoutRead, &stdoutWrite, &sa, 65536);

        if (!outPipeOk) {
            CloseHandle(stdinRead);
            CloseHandle(stdinWrite);
            setLastError("stage=spawn_create_stdout_pipe_failed candidate=" + candidate + " gle=" + std::to_string(GetLastError()));
            continue;
        }

        SetHandleInformation(stdinWrite,  HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(stdoutRead,  HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.hStdInput  = stdinRead;
        si.hStdOutput = stdoutWrite;
        // Capture stderr into the same pipe so startup failures are observable.
        si.hStdError  = stdoutWrite;
        si.dwFlags    = STARTF_USESTDHANDLES;

        std::vector<char> cmdLine(candidate.begin(), candidate.end());
        cmdLine.push_back('\0');

        setLastError("stage=spawn_create_process candidate=" + candidate);
        BOOL ok = CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr,
                                 TRUE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
                                 nullptr, nullptr, &si, &m_procInfo);
        CloseHandle(stdinRead);
        CloseHandle(stdoutWrite);
        if (!ok) {
            setLastError("stage=spawn_create_failed candidate=" + candidate + " gle=" + std::to_string(GetLastError()));
            CloseHandle(stdinWrite);
            CloseHandle(stdoutRead);
            continue;
        }

        Sleep(100);
        DWORD childExitCode = 0;
        if (GetExitCodeProcess(m_procInfo.hProcess, &childExitCode) && childExitCode != STILL_ACTIVE) {
            setLastError("stage=spawn_immediate_exit candidate=" + candidate + " code=" + std::to_string(childExitCode));
            CloseHandle(stdinWrite);
            CloseHandle(stdoutRead);
            CloseHandle(m_procInfo.hProcess);
            CloseHandle(m_procInfo.hThread);
            m_procInfo = {};
            continue;
        }

        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(stdoutRead, nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            const DWORD peekErr = GetLastError();
            if (peekErr == ERROR_BROKEN_PIPE) {
                setLastError("stage=spawn_pipe_broken candidate=" + candidate);
                CloseHandle(stdinWrite);
                CloseHandle(stdoutRead);
                CloseHandle(m_procInfo.hProcess);
                CloseHandle(m_procInfo.hThread);
                m_procInfo = {};
                continue;
            }
            setLastError("stage=spawn_pipe_peek_failed candidate=" + candidate + " gle=" + std::to_string(peekErr));
            CloseHandle(stdinWrite);
            CloseHandle(stdoutRead);
            CloseHandle(m_procInfo.hProcess);
            CloseHandle(m_procInfo.hThread);
            m_procInfo = {};
            continue;
        }
        setLastError("stage=spawn_pipe_connected candidate=" + candidate + " avail=" + std::to_string(bytesAvailable));

        m_stdinWrite  = stdinWrite;
        m_stdoutRead  = stdoutRead;
        m_running.store(true);

        const DWORD initTimeout = (timeoutMs >= 500) ? timeoutMs : 500;

        const int64_t initId = m_nextId.fetch_add(1);
        json initReq;
        initReq["jsonrpc"] = "2.0";
        initReq["id"]      = initId;
        initReq["method"]  = "initialize";
        initReq["params"]["protocolVersion"]       = "2024-11-05";
        initReq["params"]["clientInfo"]["name"]    = "RawrXD-Sovereign";
        initReq["params"]["clientInfo"]["version"] = "2.0";
        if (!writeRequest(initReq)) {
            setLastError("stage=init_write_failed candidate=" + candidate);
            shutdown();
            continue;
        }

        // Post-write probe: give server time to process and check stdout pipe
        Sleep(200);
        {
            DWORD probeAvail = 0;
            BOOL probeOk = PeekNamedPipe(m_stdoutRead, nullptr, 0, nullptr, &probeAvail, nullptr);
            DWORD probeGle = probeOk ? 0 : GetLastError();
            DWORD childCode2 = 0;
            GetExitCodeProcess(m_procInfo.hProcess, &childCode2);
            bridgeLog("post-write probe: peek_ok=" + std::to_string(probeOk) +
                      " avail=" + std::to_string(probeAvail) +
                      " peek_gle=" + std::to_string(probeGle) +
                      " child=" + std::to_string(childCode2));
            if (probeAvail > 0) {
                // Dump first bytes for diagnostics
                char peek[512] = {};
                DWORD peekRead = 0;
                PeekNamedPipe(m_stdoutRead, peek, (probeAvail < 512 ? probeAvail : 512), &peekRead, nullptr, nullptr);
                bridgeLog("post-write RX peek (" + std::to_string(peekRead) + " bytes):\n" +
                          hexDump(peek, peekRead));
            }
            setLastError("stage=init_post_write candidate=" + candidate +
                         " probe_avail=" + std::to_string(probeAvail) +
                         " child=" + std::to_string(childCode2));
        }

        json initResp;
        if (!readResponseForId(initId, initResp, initTimeout)) {
            Sleep(120);
            std::string raw = readAvailableOutput(8192);
            DWORD childExitCode = 0;
            std::string codeSuffix;
            if (m_procInfo.hProcess && GetExitCodeProcess(m_procInfo.hProcess, &childExitCode)) {
                codeSuffix = " child_code=" + std::to_string(childExitCode);
            }
            setLastError("stage=init_read_failed candidate=" + candidate + codeSuffix +
                         (raw.empty() ? std::string() : " output=" + raw));
            shutdown();
            continue;
        }
        if (initResp.contains("error")) {
            setLastError("stage=init_error_response candidate=" + candidate + " payload=" + initResp.dump());
            shutdown();
            continue;
        }

        json notif;
        notif["jsonrpc"] = "2.0";
        notif["method"]  = "notifications/initialized";
        if (!writeRequest(notif)) {
            setLastError("stage=initialized_notify_write_failed candidate=" + candidate);
            shutdown();
            continue;
        }
        setLastError("stage=spawn_success candidate=" + candidate + " pid=" + std::to_string(m_procInfo.dwProcessId));
        return true;
    }

    if (lastError().empty()) {
        setLastError("No spawn candidates available or all failed");
    }
    return false;
}

json SovereignMCPBridge::callTool(const std::string& toolName,
                                   const json& args, DWORD timeoutMs) {
    if (!m_running.load()) return json{{"error", "MCP bridge not running"}};
    std::lock_guard<std::mutex> lock(m_callMutex);

    const int64_t reqId = m_nextId.fetch_add(1);
    json req;
    req["jsonrpc"]             = "2.0";
    req["id"]                  = reqId;
    req["method"]              = "tools/call";
    req["params"]["name"]      = toolName;
    req["params"]["arguments"] = args;
    if (!writeRequest(req)) return json{{"error", "write failed"}};

    json resp;
    if (!readResponseForId(reqId, resp, timeoutMs)) return json{{"error", "timeout"}};
    if (resp.contains("result")) return resp["result"];
    if (resp.contains("error"))  return resp["error"];
    return resp;
}

json SovereignMCPBridge::listTools(DWORD timeoutMs) {
    if (!m_running.load()) return json{{"error", "MCP bridge not running"}};
    std::lock_guard<std::mutex> lock(m_callMutex);

    const int64_t reqId = m_nextId.fetch_add(1);
    json req;
    req["jsonrpc"] = "2.0";
    req["id"] = reqId;
    req["method"] = "tools/list";
    req["params"] = json::object();

    if (!writeRequest(req)) return json{{"error", "write failed"}};

    json resp;
    if (!readResponseForId(reqId, resp, timeoutMs)) {
        return json{{"error", "timeout"}};
    }

    if (resp.contains("result")) return resp["result"];
    if (resp.contains("error")) return resp["error"];
    return resp;
}

void SovereignMCPBridge::shutdown() {
    const bool wasRunning = m_running.exchange(false);
    (void)wasRunning;

    if (m_procInfo.hProcess && m_stdinWrite != INVALID_HANDLE_VALUE) {
        // Best-effort graceful shutdown for MCP servers that support explicit shutdown.
        json req;
        req["jsonrpc"] = "2.0";
        req["id"] = m_nextId.fetch_add(1);
        req["method"] = "shutdown";
        req["params"] = json::object();
        (void)writeRequest(req);
    }

    // Closing stdin is the EOF signal the child uses to exit its read loop.
    if (m_stdinWrite != INVALID_HANDLE_VALUE) {
        CloseHandle(m_stdinWrite);
        m_stdinWrite = INVALID_HANDLE_VALUE;
    }

    if (m_procInfo.hProcess) {
        DWORD wait = WaitForSingleObject(m_procInfo.hProcess, 2000);
        if (wait != WAIT_OBJECT_0) {
            TerminateProcess(m_procInfo.hProcess, 0);
            WaitForSingleObject(m_procInfo.hProcess, 2000);
        }

        CloseHandle(m_procInfo.hProcess);
        CloseHandle(m_procInfo.hThread);
        m_procInfo = {};
    }

    if (m_stdoutRead != INVALID_HANDLE_VALUE) {
        CloseHandle(m_stdoutRead);
        m_stdoutRead = INVALID_HANDLE_VALUE;
    }
}

bool SovereignMCPBridge::writeRequest(const json& req) {
    if (m_stdinWrite == INVALID_HANDLE_VALUE || m_stdinWrite == nullptr) {
        bridgeLog("writeRequest: stdin handle invalid");
        return false;
    }
    const std::string payload = req.dump();
    const std::string header = "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n";
    const std::string msg = header + payload;
    bridgeLog("writeRequest: " + std::to_string(msg.size()) + " bytes");
    bridgeLog("TX hex:\n" + hexDump(msg.data(), msg.size()));
    const bool ok = writeAllWithTimeout(m_stdinWrite, msg.data(), static_cast<DWORD>(msg.size()), 5000);
    if (ok) {
        FlushFileBuffers(m_stdinWrite);
        bridgeLog("writeRequest: write+flush OK");
    } else {
        bridgeLog("writeRequest: writeAllWithTimeout FAILED gle=" + std::to_string(GetLastError()));
    }
    return ok;
}

bool SovereignMCPBridge::readMessage(json& out, DWORD timeoutMs) {
    std::string line;
    size_t contentLength = 0;
    bool sawContentLength = false;

    while (true) {
        if (!readLine(line, timeoutMs)) {
            return false;
        }

        if (line.rfind("Content-Length:", 0) == 0) {
            std::string value = line.substr(std::string("Content-Length:").size());
            value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
                return std::isspace(c) != 0;
            }), value.end());

            try {
                contentLength = static_cast<size_t>(std::stoull(value));
                sawContentLength = true;
            } catch (...) {
                return false;
            }
            continue;
        }

        // Ignore preamble/noise lines until we have a valid Content-Length header.
        if (line == "\n" || line == "\r\n") {
            if (sawContentLength) {
                break;
            }
            continue;
        }
    }

    if (contentLength == 0 || contentLength > (8u * 1024u * 1024u)) {
        bridgeLog("readMessage: bad contentLength=" + std::to_string(contentLength));
        return false;
    }

    bridgeLog("readMessage: reading payload, contentLength=" + std::to_string(contentLength));
    std::vector<char> payload(contentLength);
    if (!readExact(payload.data(), payload.size(), timeoutMs)) {
        bridgeLog("readMessage: readExact FAILED for " + std::to_string(contentLength) + " bytes");
        return false;
    }

    bridgeLog("readMessage: payload hex (" + std::to_string(payload.size()) + " bytes):\n" +
              hexDump(payload.data(), payload.size(), 512));
    bridgeLog("readMessage: payload text: " + std::string(payload.begin(), payload.end()));

    try {
        out = json::parse(std::string(payload.begin(), payload.end()));
        bridgeLog("readMessage: parsed type=" + std::to_string(static_cast<int>(out.type())));
        return true;
    } catch (const std::exception& e) {
        bridgeLog("readMessage: json::parse exception: " + std::string(e.what()));
        return false;
    } catch (...) {
        return false;
    }
}

bool SovereignMCPBridge::readResponseForId(int64_t id, json& out, DWORD timeoutMs) {
    const DWORD start = GetTickCount();

    while ((GetTickCount() - start) < timeoutMs) {
        const DWORD elapsed = GetTickCount() - start;
        const DWORD remaining = timeoutMs - elapsed;
        if (remaining == 0) {
            bridgeLog("readResponseForId: timeout expired waiting for id=" + std::to_string(id));
            return false;
        }

        json msg;
        if (!readMessage(msg, remaining)) {
            bridgeLog("readResponseForId: readMessage failed, remaining=" + std::to_string(remaining) +
                      "ms, looking for id=" + std::to_string(id));
            return false;
        }

        bridgeLog("readResponseForId: got message type=" +
                  std::to_string(static_cast<int>(msg.type())) +
                  " is_obj=" + std::to_string(msg.is_object()) +
                  " is_str=" + std::to_string(msg.is_string()) +
                  " dump=" + msg.dump().substr(0, 300));

        if (!msg.contains("id")) {
            bridgeLog("readResponseForId: message has no 'id' key (type=" +
                      std::to_string(static_cast<int>(msg.type())) + "), skipping");
            // If it parsed as a string, try re-parsing the string content as JSON
            if (msg.is_string()) {
                bridgeLog("readResponseForId: attempting re-parse of string content");
                try {
                    msg = json::parse(msg.get<std::string>());
                    bridgeLog("readResponseForId: re-parsed type=" +
                              std::to_string(static_cast<int>(msg.type())) +
                              " has_id=" + std::to_string(msg.contains("id")));
                    if (msg.contains("id")) {
                        // Fall through to id matching below
                        goto check_id;
                    }
                } catch (...) {
                    bridgeLog("readResponseForId: re-parse failed");
                }
            }
            continue;
        }
check_id:
        const json& msgId = msg["id"];

        if (msgId.is_number_integer() && msgId.get<int64_t>() == id) {
            bridgeLog("readResponseForId: matched id=" + std::to_string(id));
            out = std::move(msg);
            return true;
        }

        if (msgId.is_string()) {
            try {
                if (std::stoll(msgId.get<std::string>()) == id) {
                    bridgeLog("readResponseForId: matched string id=" + std::to_string(id));
                    out = std::move(msg);
                    return true;
                }
            } catch (...) {
                continue;
            }
        }
        bridgeLog("readResponseForId: id mismatch, got " + msgId.dump() + " want " + std::to_string(id));
    }
    bridgeLog("readResponseForId: loop timeout expired for id=" + std::to_string(id));
    return false;
}

bool SovereignMCPBridge::readLine(std::string& out, DWORD timeoutMs) {
    out.clear();
    char ch = '\0';
    const DWORD start = GetTickCount();

    while ((GetTickCount() - start) < timeoutMs) {
        DWORD read = 0;
        const DWORD elapsed = GetTickCount() - start;
        if (elapsed >= timeoutMs) break;
        const DWORD remaining = timeoutMs - elapsed;
        if (!readWithTimeout(m_stdoutRead, &ch, 1, read, remaining)) {
            bridgeLog("readLine: readWithTimeout failed after " + std::to_string(elapsed) +
                      "ms, got " + std::to_string(out.size()) + " bytes so far");
            if (!out.empty()) {
                bridgeLog("readLine partial hex:\n" + hexDump(out.data(), out.size()));
            }
            // Check pipe health
            DWORD avail = 0;
            BOOL peekOk = PeekNamedPipe(m_stdoutRead, nullptr, 0, nullptr, &avail, nullptr);
            bridgeLog("readLine: pipe peek_ok=" + std::to_string(peekOk) +
                      " avail=" + std::to_string(avail) +
                      " gle=" + std::to_string(GetLastError()));
            return false;
        }

        out.push_back(ch);
        if (ch == '\n') {
            return true;
        }
        if (out.size() > 64u * 1024u) {
            return false;
        }
    }
    return false;
}

bool SovereignMCPBridge::readExact(char* dst, size_t size, DWORD timeoutMs) {
    size_t total = 0;
    const DWORD start = GetTickCount();

    while (total < size && (GetTickCount() - start) < timeoutMs) {
        DWORD toRead = static_cast<DWORD>(std::min<size_t>(size - total, 4096u));
        DWORD read = 0;
        const DWORD remaining = timeoutMs - (GetTickCount() - start);
        if (!readWithTimeout(m_stdoutRead, dst + total, toRead, read, remaining)) {
            return false;
        }
        total += read;
    }
    return total == size;
}

std::string SovereignMCPBridge::readAvailableOutput(DWORD maxBytes) {
    if (m_stdoutRead == INVALID_HANDLE_VALUE || maxBytes == 0) {
        return {};
    }

    DWORD avail = 0;
    if (!PeekNamedPipe(m_stdoutRead, nullptr, 0, nullptr, &avail, nullptr) || avail == 0) {
        return {};
    }

    DWORD toRead = (avail > maxBytes) ? maxBytes : avail;
    std::string out;
    out.resize(static_cast<size_t>(toRead));

    DWORD read = 0;
    if (!ReadFile(m_stdoutRead, out.data(), toRead, &read, nullptr) || read == 0) {
        return {};
    }
    out.resize(static_cast<size_t>(read));
    return out;
}

} // namespace RawrXD::Runtime
