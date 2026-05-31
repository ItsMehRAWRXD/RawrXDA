// TitanIPC.cpp — TitanProxy: IDE-side named pipe client
//
// Spawns RawrXD-TitanHost.exe and forwards inference requests over the pipe.
// If TitanHost crashes the pipe breaks; the next Submit() call restarts it.
// No DLL is loaded inside the IDE process — all AVX-512/alignment faults
// are isolated to TitanHost.exe.

#include "TitanIPC.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>

namespace RawrXD
{

// ============================================================================
// Singleton
// ============================================================================

TitanProxy& TitanProxy::instance()
{
    static TitanProxy s;
    return s;
}

// ============================================================================
// Helpers
// ============================================================================

std::string TitanProxy::pipeName() const
{
    char buf[64];
    snprintf(buf, sizeof(buf), "\\\\.\\pipe\\RawrXD_TitanHost_%u", GetCurrentProcessId());
    return buf;
}

static std::string JsonEscStr(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (unsigned char c : s)
    {
        switch (c)
        {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20)
                {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    out += esc;
                }
                else
                {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
    return out;
}

static std::string ExtractStr(const std::string& json, const char* key)
{
    std::string needle = std::string("\"") + key + "\":\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
        return "";
    pos += needle.size();
    std::string out;
    while (pos < json.size())
    {
        if (json[pos] == '\\' && pos + 1 < json.size())
        {
            ++pos;
            switch (json[pos])
            {
                case '"':
                    out += '"';
                    break;
                case '\\':
                    out += '\\';
                    break;
                case 'n':
                    out += '\n';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case 't':
                    out += '\t';
                    break;
                default:
                    out += json[pos];
                    break;
            }
        }
        else if (json[pos] == '"')
        {
            break;
        }
        else
        {
            out += json[pos];
        }
        ++pos;
    }
    return out;
}

static bool ExtractBool(const std::string& json, const char* key, bool def = false)
{
    std::string needle = std::string("\"") + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
        return def;
    pos += needle.size();
    while (pos < json.size() && json[pos] == ' ')
        ++pos;
    if (json.compare(pos, 4, "true") == 0)
        return true;
    if (json.compare(pos, 5, "false") == 0)
        return false;
    return def;
}

// ============================================================================
// Frame I/O  [uint32_t len][payload]
// ============================================================================

bool TitanProxy::sendFrame(const std::string& json, std::string& outErr)
{
    outErr.clear();
    if (m_hProcess && WaitForSingleObject(m_hProcess, 0) == WAIT_OBJECT_0) { outErr = "TitanProxy: host process already exited"; return false; }
    if (m_hPipe == INVALID_HANDLE_VALUE) { outErr = "TitanProxy: pipe not connected"; return false; }
    if (GetFileType(m_hPipe) != FILE_TYPE_PIPE) { outErr = "TitanProxy: handle is not a pipe"; return false; }
    if (json.empty()) { outErr = "TitanProxy: refusing to send empty frame"; return false; }
    uint32_t len = static_cast<uint32_t>(json.size());
    if (len > TITAN_PIPE_FRAME_MAX) { outErr = "TitanProxy: frame too large"; return false; }
    DWORD written;
    if (!WriteFile(m_hPipe, &len, sizeof(len), &written, nullptr) || written != sizeof(len))
    {
        outErr = "TitanProxy: WriteFile(len) failed: " + std::to_string(GetLastError());
        return false;
    }
    if (!WriteFile(m_hPipe, json.data(), len, &written, nullptr) || written != len)
    {
        outErr = "TitanProxy: WriteFile(payload) failed: " + std::to_string(GetLastError());
        return false;
    }
    return true;
}

bool TitanProxy::recvFrame(std::string& json, uint32_t timeoutMs, std::string& outErr)
{
    outErr.clear();
    json.clear();
    if (m_hPipe == INVALID_HANDLE_VALUE) { outErr = "TitanProxy: pipe not connected"; return false; }
    if (GetFileType(m_hPipe) != FILE_TYPE_PIPE) { outErr = "TitanProxy: handle is not a pipe"; return false; }
    if (timeoutMs == 0) timeoutMs = TITAN_DEFAULT_TIMEOUT_MS;
    if (timeoutMs < 50) timeoutMs = 50;
    if (timeoutMs > 600000) timeoutMs = 600000;
    // Set read timeout via COMMTIMEOUTS-equivalent for named pipe.
    // Named pipes don't support WaitForMultipleObjects on the pipe directly in
    // synchronous mode.  Use PIPE_NOWAIT in a poll loop with a deadline.
    const DWORD deadline = GetTickCount() + timeoutMs;
    uint32_t len = 0;
    DWORD bytesRead = 0;

    // Read length prefix — poll until available or deadline.
    int lengthPollCount = 0;
    while (true)
    {
        if (m_hProcess && WaitForSingleObject(m_hProcess, 0) == WAIT_OBJECT_0) { outErr = "TitanProxy: host exited while waiting for frame"; return false; }
        DWORD avail = 0;
        if (!PeekNamedPipe(m_hPipe, nullptr, 0, nullptr, &avail, nullptr))
        {
            outErr = "TitanProxy: PeekNamedPipe failed: " + std::to_string(GetLastError());
            return false;
        }
        if (avail > (TITAN_PIPE_FRAME_MAX + sizeof(uint32_t))) { outErr = "TitanProxy: pending pipe data exceeds frame cap"; return false; }
        if (avail >= sizeof(uint32_t))
            break;
        if (GetTickCount() >= deadline)
        {
            outErr = "TitanProxy: recv timeout waiting for length prefix";
            return false;
        }
        if (lengthPollCount < 16)
        {
            Sleep(0);
        }
        else
        {
            Sleep(1);
        }
        ++lengthPollCount;
    }

    if (!ReadFile(m_hPipe, &len, sizeof(len), &bytesRead, nullptr) || bytesRead != sizeof(len))
    {
        outErr = "TitanProxy: ReadFile(len) failed: " + std::to_string(GetLastError());
        return false;
    }
    if (len < 2) { outErr = "TitanProxy: frame too short"; return false; }
    if (len == 0 || len > TITAN_PIPE_FRAME_MAX)
    {
        outErr = "TitanProxy: invalid frame length: " + std::to_string(len);
        return false;
    }

    // Read payload — may arrive in pieces.
    json.resize(len);
    DWORD total = 0;
    while (total < len)
    {
        if (m_hProcess && WaitForSingleObject(m_hProcess, 0) == WAIT_OBJECT_0) { outErr = "TitanProxy: host exited while reading payload"; return false; }
        if (GetTickCount() >= deadline)
        {
            outErr = "TitanProxy: recv timeout reading payload";
            return false;
        }
        DWORD got = 0;
        if (!ReadFile(m_hPipe, &json[total], len - total, &got, nullptr) || got == 0)
        {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED)
            {
                outErr = "TitanProxy: pipe broken (TitanHost crashed?)";
            }
            else
            {
                outErr = "TitanProxy: ReadFile(payload) failed: " + std::to_string(err);
            }
            return false;
        }
        total += got;
    }
    return true;
}

// ============================================================================
// Host management
// ============================================================================

bool TitanProxy::ensureHost()
{
    m_lastEnsureHostError.clear();
    if (m_hJob) { CloseHandle(m_hJob); m_hJob = nullptr; }

    if (m_hProcess)
    {
        DWORD exitCode = STILL_ACTIVE;
        if (!GetExitCodeProcess(m_hProcess, &exitCode) || exitCode != STILL_ACTIVE)
        {
            CloseHandle(m_hProcess);
            m_hProcess = nullptr;
        }
    }

    if (m_hPipe != INVALID_HANDLE_VALUE)
    {
        // Quick liveness check.
        DWORD flags = 0;
        if (GetNamedPipeInfo(m_hPipe, &flags, nullptr, nullptr, nullptr))
        {
            return true;
        }
        // Pipe is dead — clean up.
        CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
    }

    if (!m_csInit)
    {
        InitializeCriticalSection(&m_cs);
        m_csInit = true;
    }

    // Find TitanHost.exe candidates:
    // 1) explicit env override
    // 2) sibling build lane release host (build/bin)
    // 3) next to current IDE exe
    // 4) sibling build-ninja host
    // This avoids debug-CRT host mismatches (ucrtbased.dll missing) when multiple lanes coexist.
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* slash = wcsrchr(exePath, L'\\');
    if (!slash)
        slash = wcsrchr(exePath, L'/');
    std::wstring exeDir;
    if (slash)
    {
        *(slash + 1) = L'\0';
        exeDir = std::wstring(exePath);
    }
    else
    {
        exeDir = L"";
    }

    auto parentDir = [](const std::wstring& p) -> std::wstring {
        if (p.empty())
            return {};
        std::wstring t = p;
        while (!t.empty() && (t.back() == L'\\' || t.back() == L'/'))
            t.pop_back();
        const size_t pos = t.find_last_of(L"\\/");
        if (pos == std::wstring::npos)
            return {};
        return t.substr(0, pos);
    };

    std::vector<std::wstring> hostCandidates;

    const auto addCandidate = [&](const std::wstring& candidate)
    {
        if (candidate.empty())
            return;
        if (std::find(hostCandidates.begin(), hostCandidates.end(), candidate) != hostCandidates.end())
            return;
        hostCandidates.emplace_back(candidate);
    };

    wchar_t envHost[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"RAWRXD_TITAN_HOST_PATH", envHost, MAX_PATH) > 0)
    {
        addCandidate(envHost);
    }

    if (!exeDir.empty())
    {
        const std::wstring p1 = parentDir(exeDir);
        const std::wstring p2 = parentDir(p1);
        if (!p2.empty())
        {
            addCandidate(p2 + L"\\build\\bin\\RawrXD-TitanHost.exe");
        }

        addCandidate(exeDir + L"RawrXD-TitanHost.exe");

        if (!p2.empty())
        {
            addCandidate(p2 + L"\\build-ninja\\bin\\RawrXD-TitanHost.exe");
        }
    }
    else
    {
        addCandidate(L"RawrXD-TitanHost.exe");
    }
    if (hostCandidates.size() > 16) hostCandidates.resize(16);
    if (hostCandidates.empty()) { m_lastEnsureHostError = "no TitanHost candidates"; return false; }

    // Create event the host will signal when ready.
    char evName[80];
    snprintf(evName, sizeof(evName), "RawrXD_TitanReady_%u", GetCurrentProcessId());
    HANDLE hReady = CreateEventA(nullptr, TRUE, FALSE, evName);
    if (!hReady) OutputDebugStringA("[TitanProxy] Ready event creation failed; continuing without event sync\n");
    if (hReady) ResetEvent(hReady);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    DWORD lastLaunchError = ERROR_FILE_NOT_FOUND;
    std::vector<std::wstring> attemptedPaths;
    bool launched = false;
    UINT prevErrMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    for (const auto& hostPath : hostCandidates)
    {
        if (hostPath.empty())
            continue;

        const DWORD attrs = GetFileAttributesW(hostPath.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0)
            continue;

        attemptedPaths.emplace_back(hostPath);

        wchar_t cmdLine[512] = {};
        swprintf_s(cmdLine, L"\"%ls\" %u", hostPath.c_str(), GetCurrentProcessId());

        ZeroMemory(&pi, sizeof(pi));
        if (CreateProcessW(nullptr, cmdLine, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        {
            launched = true;
            break;
        }

        lastLaunchError = GetLastError();
        if (lastLaunchError == ERROR_BAD_EXE_FORMAT)
        {
            OutputDebugStringA("[TitanProxy] TitanHost candidate had bad format/arch mismatch\n");
        }
        else if (lastLaunchError == ERROR_MOD_NOT_FOUND)
        {
            OutputDebugStringA("[TitanProxy] TitanHost candidate missing runtime dependency (e.g., ucrtbased.dll)\n");
        }
    }
    SetErrorMode(prevErrMode);

    if (!launched)
    {
        if (hReady)
            CloseHandle(hReady);

        std::ostringstream oss;
        oss << "CreateProcess failed (GetLastError=" << static_cast<unsigned long>(lastLaunchError)
            << ")";
        if (!attemptedPaths.empty())
        {
            oss << ", attempted=";
            for (size_t i = 0; i < attemptedPaths.size(); ++i)
            {
                if (i > 0)
                    oss << " | ";
                const std::wstring& wp = attemptedPaths[i];
                std::string narrow(wp.begin(), wp.end());
                oss << narrow;
            }
        }
        else
        {
            oss << ", no host candidate file found";
        }
        m_lastEnsureHostError = oss.str();
        OutputDebugStringA((std::string("[TitanProxy] ") + m_lastEnsureHostError + "\n").c_str());
        return false;  // host binary missing or dependency load failure
    }

    HANDLE previousProcess = m_hProcess;
    if (previousProcess && previousProcess != pi.hProcess)
    {
        CloseHandle(previousProcess);
    }
    m_hProcess = pi.hProcess;
    SetPriorityClass(m_hProcess, ABOVE_NORMAL_PRIORITY_CLASS);
    CloseHandle(pi.hThread);

    // Wait up to 10 s for the host to create the pipe and signal ready.
    if (hReady)
    {
        WaitForSingleObject(hReady, 10000);
        CloseHandle(hReady);
    }
    else
    {
        Sleep(50);  // fallback wait if ready-event missing (rare)
    }

    // Connect to the pipe.
    std::string pn = pipeName();
    if (m_hProcess && WaitForSingleObject(m_hProcess, 0) == WAIT_OBJECT_0) { m_lastEnsureHostError = "host exited before pipe connect"; return false; }
    for (int retry = 0; retry < 20; ++retry)
    {
        if (m_hProcess && WaitForSingleObject(m_hProcess, 0) == WAIT_OBJECT_0) break;
        m_hPipe = CreateFileA(pn.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (m_hPipe != INVALID_HANDLE_VALUE)
            break;
        DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PIPE_BUSY)
            break;
        Sleep(20);
    }
    if (m_hPipe != INVALID_HANDLE_VALUE)
    {
        SetHandleInformation(m_hPipe, HANDLE_FLAG_INHERIT, 0);
        DWORD pipeMode = PIPE_READMODE_BYTE; SetNamedPipeHandleState(m_hPipe, &pipeMode, nullptr, nullptr);
        SetNamedPipeHandleState(m_hPipe, nullptr, nullptr, nullptr);
        return true;
    }

    DWORD pipeErr = GetLastError();
    DWORD hostExitCode = STILL_ACTIVE;
    if (m_hProcess)
    {
        GetExitCodeProcess(m_hProcess, &hostExitCode);
    }

    std::ostringstream oss;
    oss << "pipe connect failed (CreateFile err=" << static_cast<unsigned long>(pipeErr) << ")"
        << ", pipe=" << pn;
    if (hostExitCode != STILL_ACTIVE)
    {
        oss << ", host_exit_code=" << static_cast<unsigned long>(hostExitCode);
    }
    m_lastEnsureHostError = oss.str();

    if (m_hProcess)
    {
        TerminateProcess(m_hProcess, 1);
        WaitForSingleObject(m_hProcess, 3000);
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
    }
    return false;
}

void TitanProxy::forceKillHost()
{
    if (m_hPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
    }
    if (m_hProcess)
    {
        TerminateProcess(m_hProcess, 1);
        WaitForSingleObject(m_hProcess, 3000);
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
    }
    m_lastLoadedModelPathUtf8.clear();
    m_lastEnsureHostError.clear();
}

void TitanProxy::restartHost()
{
    forceKillHost();
    ensureHost();
}

void TitanProxy::shutdown()
{
    if (m_hPipe != INVALID_HANDLE_VALUE)
    {
        std::string dummy;
        std::string req = "{\"cmd\":\"exit\"}";
        sendFrame(req, dummy);
        CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
    }
    if (m_hProcess)
    {
        WaitForSingleObject(m_hProcess, 3000);
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
    }
    m_lastLoadedModelPathUtf8.clear();
    m_lastEnsureHostError.clear();
    if (m_csInit)
    {
        DeleteCriticalSection(&m_cs);
        m_csInit = false;
    }
}

// ============================================================================
// Public API
// ============================================================================

bool TitanProxy::loadModel(const std::string& pathUtf8, std::string& error)
{
    error.clear();
    if (pathUtf8.find('\0') != std::string::npos) { error = "TitanProxy::loadModel: embedded NUL in path"; return false; }
    if (pathUtf8.find('\n') != std::string::npos || pathUtf8.find('\r') != std::string::npos) { error = "TitanProxy::loadModel: invalid newline in path"; return false; }
    if (pathUtf8.empty())
    {
        error = "TitanProxy::loadModel: empty path";
        return false;
    }
    if (pathUtf8.find_first_not_of(' ') == std::string::npos) { error = "TitanProxy::loadModel: blank path"; return false; }
    if (pathUtf8.size() < 4) { error = "TitanProxy::loadModel: path too short"; return false; }
    if (pathUtf8.size() >= 4096) { error = "TitanProxy::loadModel: path too long"; return false; }
    if (!m_csInit)
    {
        InitializeCriticalSection(&m_cs);
        m_csInit = true;
    }
    EnterCriticalSection(&m_cs);
    struct Guard
    {
        CRITICAL_SECTION* cs;
        ~Guard() { LeaveCriticalSection(cs); }
    } guard{&m_cs};

    if (pathUtf8 == m_lastLoadedModelPathUtf8)
    {
        return true;
    }

    if (!ensureHost())
    {
        error = "TitanProxy: could not start RawrXD-TitanHost.exe. "
            "Check runtime deps (ucrtbased.dll indicates debug host), "
            "or set RAWRXD_TITAN_HOST_PATH to a release host binary. Details: " +
            (m_lastEnsureHostError.empty() ? "(none)" : m_lastEnsureHostError);
        return false;
    }

    std::string req = std::string("{\"cmd\":\"load_model\",\"path\":") + JsonEscStr(pathUtf8) + "}";
    std::string sendErr;
    if (!sendFrame(req, sendErr))
    {
        forceKillHost();
        error = "TitanProxy: load_model send failed: " + sendErr;
        return false;
    }

    std::string resp;
    constexpr uint32_t kLoadTimeoutMs = 600000;  // 10 min — large GGUF map
    if (!recvFrame(resp, kLoadTimeoutMs, error))
    {
        forceKillHost();
        return false;
    }

    const bool ok = ExtractBool(resp, "ok", false);
    if (ok)
    {
        m_lastLoadedModelPathUtf8 = pathUtf8;
        return true;
    }
    error = ExtractStr(resp, "error");
    if (error.empty())
    {
        error = "TitanProxy: load_model rejected by host";
    }
    return false;
}

void TitanProxy::clearLoadedModelCache()
{
    m_lastLoadedModelPathUtf8.clear();
}

bool TitanProxy::submit(const std::string& prompt, uint32_t maxTokens, uint32_t timeoutMs, std::string& completion,
                        std::string& metadata, std::string& error)
{
    completion.clear();
    metadata.clear();
    error.clear();
    if (prompt.empty()) { error = "TitanProxy: empty prompt"; return false; }
    if (prompt.find_first_not_of(" \t\r\n") == std::string::npos) { error = "TitanProxy: blank prompt"; return false; }
    if (prompt.find('\0') != std::string::npos) { error = "TitanProxy: embedded NUL in prompt"; return false; }
    if (prompt.size() > (TITAN_PIPE_FRAME_MAX / 2)) { error = "TitanProxy: prompt too large"; return false; }
    if (maxTokens == 0) maxTokens = 256;
    if (maxTokens < 8) maxTokens = 8;
    if (!m_tokenCb && maxTokens > 4096) maxTokens = 4096;
    if (maxTokens > 8192) maxTokens = 8192;
    if (timeoutMs == 0) timeoutMs = TITAN_DEFAULT_TIMEOUT_MS;
    if (timeoutMs < 1000) timeoutMs = 1000;
    if (timeoutMs > 600000) timeoutMs = 600000;
    if (!m_csInit)
    {
        InitializeCriticalSection(&m_cs);
        m_csInit = true;
    }
    EnterCriticalSection(&m_cs);
    struct Guard
    {
        CRITICAL_SECTION* cs;
        ~Guard() { LeaveCriticalSection(cs); }
    } guard{&m_cs};

    if (!ensureHost())
    {
        error = "TitanProxy: could not start RawrXD-TitanHost.exe. "
            "Check runtime deps (ucrtbased.dll indicates debug host), "
            "or set RAWRXD_TITAN_HOST_PATH to a release host binary. Details: " +
            (m_lastEnsureHostError.empty() ? "(none)" : m_lastEnsureHostError);
        return false;
    }

    if (m_seq == UINT32_MAX) m_seq = 0;
    uint32_t seq = ++m_seq;
    char reqBuf[64];
    snprintf(reqBuf, sizeof(reqBuf), "{\"cmd\":\"infer\",\"seq\":%u,\"max_tokens\":%u,\"prompt\":", seq, maxTokens);
    const std::string escapedPrompt = JsonEscStr(prompt);
    std::string req;
    req.reserve(sizeof(reqBuf) + escapedPrompt.size() + 1);
    req = std::string(reqBuf) + escapedPrompt + "}";
    if (req.size() > TITAN_PIPE_FRAME_MAX) { error = "TitanProxy: request frame too large"; return false; }

    std::string sendErr;
    if (!sendFrame(req, sendErr))
    {
        forceKillHost();
        error = "TitanProxy: send failed: " + sendErr;
        return false;
    }

    // Helper: receive frames, consuming any stream_token frames by invoking
    // m_tokenCb for each token, then returning the first non-stream_token frame.
    auto recvNonStreamFrame = [&](std::string& out) -> bool {
        for (;;) {
            if (!recvFrame(out, timeoutMs, error)) {
                forceKillHost();
                return false;
            }
            // Extract "cmd" field with a simple scan (avoid pulling in full JSON)
            const std::string kStreamCmd = "stream_token";
            size_t cmdPos = out.find("\"cmd\":");
            bool isStreamToken = false;
            if (cmdPos != std::string::npos) {
                size_t vPos = cmdPos + 6;
                while (vPos < out.size() && (out[vPos] == ' ' || out[vPos] == '\t')) ++vPos;
                if (vPos < out.size() && out[vPos] == '"') {
                    ++vPos;
                    if (out.compare(vPos, kStreamCmd.size(), kStreamCmd) == 0)
                        isStreamToken = true;
                }
            }
            if (!isStreamToken)
                return true;  // caller processes this frame normally
            // It is a stream_token frame — fire token callback and loop.
            if (m_tokenCb) {
                std::string tok = ExtractStr(out, "token");
                if (!tok.empty())
                    m_tokenCb(tok);
            }
        }
    };

    std::string resp;
    if (!recvNonStreamFrame(resp))
        return false;

    // Tool-call intercept loop.
    // TitanHost may emit {"cmd":"tool_call",...} frames before the final
    // inference response.  We dispatch each to m_toolCallCb (which calls
    // AgenticExecutor::callTool() in the IDE), send back the result,
    // and then wait for the next frame.  Up to 8 rounds.
    for (int round = 0; round < 8; ++round)
    {
        std::string frameCmd = ExtractStr(resp, "cmd");
        if (frameCmd != "tool_call")
            break;  // not a tool call — process as final response below

        uint32_t toolSeq = 0;
        {  // extract seq number from resp
            std::string needle = "\"seq\":";
            size_t p = resp.find(needle);
            if (p != std::string::npos)
            {
                p += needle.size();
                while (p < resp.size() && (resp[p] == ' ' || resp[p] == '\t'))
                    ++p;
                toolSeq = static_cast<uint32_t>(atoi(resp.c_str() + p));
            }
        }
        if (toolSeq == 0) toolSeq = seq;
        uint32_t toolRound = 0;
        {
            std::string needle = "\"round\":";
            size_t p = resp.find(needle);
            if (p != std::string::npos)
            {
                p += needle.size();
                while (p < resp.size() && (resp[p] == ' ' || resp[p] == '\t'))
                    ++p;
                toolRound = static_cast<uint32_t>(atoi(resp.c_str() + p));
            }
        }
        if (toolRound > 255) toolRound = 255;
        std::string payload = ExtractStr(resp, "payload");
        if (payload.empty()) payload = "{}";
        if (payload.size() > (TITAN_PIPE_FRAME_MAX / 2)) payload.resize(TITAN_PIPE_FRAME_MAX / 2);

        // Execute tool via registered callback (Writer side in the IDE).
        std::string toolResult;
        if (m_toolCallCb)
        {
            toolResult = m_toolCallCb(payload);
        }
        else
        {
            toolResult = "{\"error\":\"no tool_call handler registered\"}";
        }
        if (toolResult.empty()) toolResult = "{}";
        if (toolResult.size() > (TITAN_PIPE_FRAME_MAX / 2)) toolResult.resize(TITAN_PIPE_FRAME_MAX / 2);

        // Escape result and send tool_result frame back to TitanHost.
        char hdrBuf[64];
        snprintf(hdrBuf, sizeof(hdrBuf), "{\"cmd\":\"tool_result\",\"seq\":%u,\"round\":%u,\"result\":", toolSeq,
                 toolRound);
        std::string toolResp = std::string(hdrBuf) + JsonEscStr(toolResult) + "}";
        if (toolResp.size() > TITAN_PIPE_FRAME_MAX) { error = "TitanProxy: tool_result frame too large"; return false; }
        std::string sendErr2;
        if (!sendFrame(toolResp, sendErr2))
        {
            forceKillHost();
            error = "TitanProxy: send tool_result failed: " + sendErr2;
            return false;
        }

        // Wait for the next frame (could be another tool_call or the final response).
        resp.clear();
        if (!recvNonStreamFrame(resp))
            return false;
    }

    bool ok = ExtractBool(resp, "ok", false);
    if (ok)
    {
        completion = ExtractStr(resp, "completion");
        metadata = ExtractStr(resp, "metadata");
        if (metadata.empty()) metadata = "{}";
        if (completion.size() > TITAN_PIPE_FRAME_MAX) completion.resize(TITAN_PIPE_FRAME_MAX);
        return true;
    }
    // Check for fatal flag — host is exiting; pipe will break soon.
    // Do NOT forceKillHost here; host is already cleaning up gracefully.
    bool fatal = ExtractBool(resp, "fatal", false);
    error = ExtractStr(resp, "error");
    if (error.empty())
        error = "TitanProxy: unknown error from host";
    if (error.size() > 2048) error.resize(2048);
    if (fatal)
    {
        // Close pipe handle so next submit() call re-spawns a fresh host.
        CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
        m_lastLoadedModelPathUtf8.clear();
    }
    return false;
}

bool TitanProxy::ping(std::string& buildInfo)
{
    buildInfo.clear();
    if (!m_csInit)
    {
        InitializeCriticalSection(&m_cs);
        m_csInit = true;
    }
    EnterCriticalSection(&m_cs);
    struct Guard
    {
        CRITICAL_SECTION* cs;
        ~Guard() { LeaveCriticalSection(cs); }
    } guard{&m_cs};

    if (!ensureHost())
    {
        buildInfo = "(TitanHost not running; " +
                    (m_lastEnsureHostError.empty() ? std::string("host launch failed") : m_lastEnsureHostError) +
                    ")";
        return false;
    }
    std::string req = "{\"cmd\":\"ping\"}";
    std::string err;
    if (!sendFrame(req, err))
        return false;
    std::string resp;
    if (!recvFrame(resp, 5000, err))
        return false;
    if (resp.size() > TITAN_PIPE_FRAME_MAX) return false;
    buildInfo = ExtractStr(resp, "build");
    if (buildInfo.empty()) buildInfo = "TitanHost/unknown";
    if (buildInfo.size() > 256) buildInfo.resize(256);
    if (buildInfo.find('\0') != std::string::npos) buildInfo = "TitanHost/invalid-build-string";
    return ExtractBool(resp, "pong", false);
}

}  // namespace RawrXD
