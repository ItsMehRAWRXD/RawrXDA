#include "dap_client_service.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>
#include <thread>


#ifdef _WIN32
#include <Windows.h>
#endif

namespace RawrXD::Debug
{

DapClientService::DapClientService() = default;
DapClientService::~DapClientService()
{
    stopAdapter();
}

bool DapClientService::isRunning() const
{
    return running_.load();
}

void DapClientService::setEventCallback(EventCallback cb)
{
    std::lock_guard<std::mutex> lock(ioMutex_);
    eventCallback_ = std::move(cb);
}

#ifdef _WIN32
std::wstring DapClientService::toWide(const std::string& s)
{
    if (s.empty())
        return L"";
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0')
        out.pop_back();
    return out;
}

std::string DapClientService::toUtf8(const std::wstring& ws)
{
    if (ws.empty())
        return "";
    const int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), len, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0')
        out.pop_back();
    return out;
}
#endif

std::string DapClientService::quoteArg(const std::string& a) const
{
    if (a.find(' ') == std::string::npos && a.find('"') == std::string::npos)
        return a;
    std::string out = "\"";
    for (char c : a)
    {
        if (c == '"')
            out += "\\\"";
        else
            out.push_back(c);
    }
    out += "\"";
    return out;
}

bool DapClientService::startAdapter(const std::string& adapterExe, const std::vector<std::string>& args)
{
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(ioMutex_);
    if (running_)
        return true;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE outRead = nullptr, outWrite = nullptr;
    HANDLE inRead = nullptr, inWrite = nullptr;

    if (!CreatePipe(&outRead, &outWrite, &sa, 0))
        return false;
    if (!SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(outRead);
        CloseHandle(outWrite);
        return false;
    }
    if (!CreatePipe(&inRead, &inWrite, &sa, 0))
    {
        CloseHandle(outRead);
        CloseHandle(outWrite);
        return false;
    }
    if (!SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(outRead);
        CloseHandle(outWrite);
        CloseHandle(inRead);
        CloseHandle(inWrite);
        return false;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = inRead;
    si.hStdOutput = outWrite;
    si.hStdError = outWrite;

    std::wstring cmd = L"\"" + toWide(adapterExe) + L"\"";
    for (const auto& a : args)
    {
        cmd += L" ";
        cmd += toWide(quoteArg(a));
    }

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    CloseHandle(inRead);
    CloseHandle(outWrite);

    if (!ok)
    {
        CloseHandle(outRead);
        CloseHandle(inWrite);
        return false;
    }

    process_ = pi.hProcess;
    thread_ = pi.hThread;
    stdoutRead_ = outRead;
    stdinWrite_ = inWrite;
    running_ = true;
    {
        std::lock_guard<std::mutex> initLock(initMutex_);
        initializedEventSeen_ = false;
    }
    {
        std::lock_guard<std::mutex> responseLock(responseMutex_);
        pendingResponses_.clear();
    }
    readerThread_ = std::thread([this]() { readerLoop(); });
    return true;
#else
    (void)adapterExe;
    (void)args;
    return false;
#endif
}

void DapClientService::stopAdapter()
{
#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lock(ioMutex_);
        if (!running_)
            return;

        running_ = false;
        if (stdinWrite_)
        {
            CloseHandle(static_cast<HANDLE>(stdinWrite_));
            stdinWrite_ = nullptr;
        }
        if (stdoutRead_)
        {
            CloseHandle(static_cast<HANDLE>(stdoutRead_));
            stdoutRead_ = nullptr;
        }
    }

    responseCv_.notify_all();
    if (readerThread_.joinable())
    {
        readerThread_.join();
    }

    std::lock_guard<std::mutex> lock(ioMutex_);
    if (thread_)
    {
        CloseHandle(static_cast<HANDLE>(thread_));
        thread_ = nullptr;
    }
    if (process_)
    {
        TerminateProcess(static_cast<HANDLE>(process_), 0);
        CloseHandle(static_cast<HANDLE>(process_));
        process_ = nullptr;
    }

    std::lock_guard<std::mutex> responseLock(responseMutex_);
    pendingResponses_.clear();
#endif
    {
        std::lock_guard<std::mutex> initLock(initMutex_);
        initializedEventSeen_ = false;
    }
}

void DapClientService::readerLoop()
{
    while (running_)
    {
        nlohmann::json msg;
        if (!recvJson(msg, 250))
        {
            if (!running_)
            {
                break;
            }

#ifdef _WIN32
            // Distinguish timeout (adapter still alive, no data yet) from a broken pipe
            // (adapter process exited). PeekNamedPipe failing is the reliable indicator.
            bool adapterDied = false;
            {
                std::lock_guard<std::mutex> lock(ioMutex_);
                if (process_ && WaitForSingleObject(static_cast<HANDLE>(process_), 0) == WAIT_OBJECT_0)
                {
                    adapterDied = true;
                    running_ = false;
                }
            }
            if (adapterDied)
            {
                // Wake any threads blocked in request() so they don't dead-lock
                responseCv_.notify_all();

                // Fire a synthetic "exited" event so the IDE can clean up its session state
                EventCallback callbackCopy;
                {
                    std::lock_guard<std::mutex> lock(ioMutex_);
                    callbackCopy = eventCallback_;
                }
                if (callbackCopy)
                {
                    try
                    {
                        callbackCopy("exited", nlohmann::json::object());
                    }
                    catch (...)
                    {
                    }
                }
                break;
            }
#endif
            continue;
        }

        const std::string type = msg.value("type", "");
        if (type == "event")
        {
            const std::string eventName = msg.value("event", "");
            if (eventName == "initialized")
            {
                {
                    std::lock_guard<std::mutex> lock(initMutex_);
                    initializedEventSeen_ = true;
                }
                initCv_.notify_all();
            }
            EventCallback callbackCopy;
            {
                std::lock_guard<std::mutex> lock(ioMutex_);
                callbackCopy = eventCallback_;
            }
            if (callbackCopy)
            {
                // Protect the reader thread from exceptions thrown by the callback
                try
                {
                    callbackCopy(eventName, msg.value("body", nlohmann::json::object()));
                }
                catch (...)
                {
                }
            }
            continue;
        }

        if (type == "response")
        {
            const int requestSeq = msg.value("request_seq", -1);
            if (requestSeq >= 0)
            {
                {
                    std::lock_guard<std::mutex> lock(responseMutex_);
                    pendingResponses_[requestSeq] = std::move(msg);
                }
                responseCv_.notify_all();
            }
        }
    }
}

bool DapClientService::sendJson(const nlohmann::json& msg)
{
#ifdef _WIN32
    if (!running_ || !stdinWrite_)
        return false;
    const std::string body = msg.dump();
    const std::string framed = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    DWORD written = 0;
    BOOL ok = WriteFile(static_cast<HANDLE>(stdinWrite_), framed.data(), static_cast<DWORD>(framed.size()), &written,
                        nullptr);
    return ok && written == framed.size();
#else
    (void)msg;
    return false;
#endif
}

bool DapClientService::recvExact(char* dst, size_t len, uint32_t timeoutMs)
{
#ifdef _WIN32
    const auto start = std::chrono::steady_clock::now();
    size_t got = 0;
    while (got < len)
    {
        if (!running_)
            return false;
        DWORD avail = 0;
        if (!PeekNamedPipe(static_cast<HANDLE>(stdoutRead_), nullptr, 0, nullptr, &avail, nullptr))
            return false;
        if (avail == 0)
        {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
                    .count() > timeoutMs)
            {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        DWORD br = 0;
        if (!ReadFile(static_cast<HANDLE>(stdoutRead_), dst + got,
                      static_cast<DWORD>(std::min<size_t>(len - got, avail)), &br, nullptr))
        {
            return false;
        }
        got += br;
    }
    return true;
#else
    (void)dst;
    (void)len;
    (void)timeoutMs;
    return false;
#endif
}

bool DapClientService::recvLine(std::string& line, uint32_t timeoutMs)
{
    line.clear();
    const auto start = std::chrono::steady_clock::now();
    while (true)
    {
        char c = 0;
        if (!recvExact(&c, 1, timeoutMs))
            return false;
        line.push_back(c);
        const size_t n = line.size();
        if (n >= 2 && line[n - 2] == '\r' && line[n - 1] == '\n')
            return true;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() >
            timeoutMs)
        {
            return false;
        }
    }
}

bool DapClientService::recvJson(nlohmann::json& out, uint32_t timeoutMs)
{
    out = nlohmann::json::object();
    std::string header;
    size_t contentLen = 0;

    while (true)
    {
        if (!recvLine(header, timeoutMs))
            return false;
        if (header == "\r\n")
            break;

        const std::string key = "Content-Length:";
        if (header.rfind(key, 0) == 0)
        {
            std::string value = header.substr(key.size());
            value.erase(
                std::remove_if(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; }),
                value.end());
            contentLen = static_cast<size_t>(std::strtoul(value.c_str(), nullptr, 10));
        }
    }

    if (contentLen == 0)
        return false;
    std::string body(contentLen, '\0');
    if (!recvExact(body.data(), contentLen, timeoutMs))
        return false;

    try
    {
        out = nlohmann::json::parse(body);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

nlohmann::json DapClientService::request(const std::string& command, const nlohmann::json& arguments,
                                         uint32_t timeoutMs)
{
    nlohmann::json req;
    req["type"] = "request";
    req["seq"] = nextSeq_++;
    req["command"] = command;
    req["arguments"] = arguments;

    if (!sendJson(req))
        return nlohmann::json::object();

    const int reqSeq = req["seq"].get<int>();
    std::unique_lock<std::mutex> lock(responseMutex_);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (running_)
    {
        const auto it = pendingResponses_.find(reqSeq);
        if (it != pendingResponses_.end())
        {
            nlohmann::json response = std::move(it->second);
            pendingResponses_.erase(it);
            return response;
        }

        if (responseCv_.wait_until(lock, deadline) == std::cv_status::timeout)
        {
            break;
        }
    }

    pendingResponses_.erase(reqSeq);
    return nlohmann::json::object();
}

bool DapClientService::waitForInitializedEvent(uint32_t timeoutMs)
{
    std::unique_lock<std::mutex> lock(initMutex_);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (running_ && !initializedEventSeen_)
    {
        if (initCv_.wait_until(lock, deadline) == std::cv_status::timeout)
        {
            break;
        }
    }
    return initializedEventSeen_;
}

bool DapClientService::initialize(const std::string& clientName, const std::string& rootPath,
                                  const std::string& adapterId)
{
    nlohmann::json args;
    args["clientID"] = "rawrxd";
    args["clientName"] = clientName;
    args["adapterID"] = adapterId;
    args["pathFormat"] = "path";
    args["linesStartAt1"] = true;
    args["columnsStartAt1"] = true;
    args["supportsVariableType"] = true;
    args["supportsVariablePaging"] = true;
    args["supportsRunInTerminalRequest"] = true;
    if (!rootPath.empty())
        args["workspaceFolder"] = rootPath;

    const auto res = request("initialize", args, 20000);
    if (res.empty() || !res.value("success", false))
    {
        return false;
    }
    // DAP: client must receive the "initialized" event before sending launch/setBreakpoints/etc.
    return waitForInitializedEvent(10000);
}

bool DapClientService::launch(const DapLaunchConfig& cfg)
{
    nlohmann::json args;
    args["program"] = cfg.program;
    if (!cfg.cwd.empty())
        args["cwd"] = cfg.cwd;
    args["args"] = cfg.args;
    args["noDebug"] = cfg.noDebug;

    const auto res = request(cfg.request.empty() ? "launch" : cfg.request, args, 20000);
    return !res.empty() && res.value("success", false);
}

bool DapClientService::attach(int processId)
{
    nlohmann::json args;
    args["processId"] = processId;
    const auto res = request("attach", args, 20000);
    return !res.empty() && res.value("success", false);
}

bool DapClientService::configurationDone()
{
    const auto res = request("configurationDone", nlohmann::json::object(), 10000);
    return !res.empty() && res.value("success", false);
}

bool DapClientService::disconnect(bool terminateDebuggee, bool restart)
{
    nlohmann::json args;
    args["terminateDebuggee"] = terminateDebuggee;
    args["restart"] = restart;

    const auto res = request("disconnect", args, 15000);
    return !res.empty() && res.value("success", false);
}

bool DapClientService::setBreakpoints(const std::string& sourcePath,
                                      const std::vector<DapSourceBreakpoint>& breakpoints,
                                      std::vector<int>* outBreakpointIds,
                                      std::vector<DapBreakpointSetResult>* outBinding)
{
    nlohmann::json args;
    args["source"] = nlohmann::json{{"path", sourcePath}};
    nlohmann::json bps = nlohmann::json::array();
    for (const auto& bp : breakpoints)
    {
        nlohmann::json b;
        b["line"] = bp.line;
        if (bp.column.has_value())
            b["column"] = *bp.column;
        if (bp.condition.has_value())
            b["condition"] = *bp.condition;
        bps.push_back(b);
    }
    args["breakpoints"] = bps;

    const auto res = request("setBreakpoints", args, 15000);
    if (res.empty() || !res.value("success", false))
        return false;

    if (outBreakpointIds && res.contains("body") && res["body"].contains("breakpoints"))
    {
        outBreakpointIds->clear();
        for (const auto& bp : res["body"]["breakpoints"])
        {
            outBreakpointIds->push_back(bp.value("id", -1));
        }
    }
    if (outBinding && res.contains("body") && res["body"].contains("breakpoints") &&
        res["body"]["breakpoints"].is_array())
    {
        outBinding->clear();
        for (const auto& bp : res["body"]["breakpoints"])
        {
            if (!bp.is_object())
            {
                continue;
            }
            DapBreakpointSetResult row;
            row.id = bp.value("id", -1);
            row.line = bp.value("line", 0);
            // If the adapter omits "verified", treat as unbound (hollow glyph) — safer than assuming true.
            row.verified = bp.contains("verified") && bp["verified"].is_boolean() ? bp["verified"].get<bool>() : false;
            if (bp.contains("message") && bp["message"].is_string())
            {
                row.message = bp["message"].get<std::string>();
            }
            outBinding->push_back(std::move(row));
        }
    }
    return true;
}

bool DapClientService::setExceptionBreakpoints(const std::vector<std::string>& filters)
{
    nlohmann::json args;
    nlohmann::json filterArr = nlohmann::json::array();
    for (const auto& f : filters)
    {
        filterArr.push_back(f);
    }
    args["filters"] = filterArr;

    const auto res = request("setExceptionBreakpoints", args, 15000);
    if (res.empty() || !res.value("success", false))
        return false;

    return true;
}

bool DapClientService::continueExec(int threadId)
{
    nlohmann::json args;
    if (threadId > 0)
        args["threadId"] = threadId;
    const auto res = request("continue", args, 10000);
    return !res.empty() && res.value("success", false);
}

bool DapClientService::next(int threadId)
{
    nlohmann::json args{{"threadId", threadId}};
    const auto res = request("next", args, 10000);
    return !res.empty() && res.value("success", false);
}

bool DapClientService::stepIn(int threadId)
{
    nlohmann::json args{{"threadId", threadId}};
    const auto res = request("stepIn", args, 10000);
    return !res.empty() && res.value("success", false);
}

bool DapClientService::stepOut(int threadId)
{
    nlohmann::json args{{"threadId", threadId}};
    const auto res = request("stepOut", args, 10000);
    return !res.empty() && res.value("success", false);
}

bool DapClientService::pause(int threadId)
{
    nlohmann::json args{{"threadId", threadId}};
    const auto res = request("pause", args, 10000);
    return !res.empty() && res.value("success", false);
}

std::vector<DapStackFrame> DapClientService::stackTrace(int threadId, int startFrame, int levels)
{
    nlohmann::json args;
    args["threadId"] = threadId;
    args["startFrame"] = startFrame;
    args["levels"] = levels;

    std::vector<DapStackFrame> out;
    const auto res = request("stackTrace", args, 10000);
    if (res.empty() || !res.value("success", false))
        return out;
    if (!res.contains("body") || !res["body"].contains("stackFrames"))
        return out;

    for (const auto& f : res["body"]["stackFrames"])
    {
        DapStackFrame sf;
        sf.id = f.value("id", 0);
        sf.name = f.value("name", "");
        sf.line = f.value("line", 0);
        sf.column = f.value("column", 0);
        if (f.contains("source"))
            sf.sourcePath = f["source"].value("path", "");
        out.push_back(std::move(sf));
    }
    return out;
}

nlohmann::json DapClientService::scopes(int frameId)
{
    const auto res = request("scopes", nlohmann::json{{"frameId", frameId}}, 10000);
    if (res.empty() || !res.value("success", false))
        return nlohmann::json::array();
    return res["body"].value("scopes", nlohmann::json::array());
}

nlohmann::json DapClientService::variables(int variablesReference)
{
    const auto res = request("variables", nlohmann::json{{"variablesReference", variablesReference}}, 10000);
    if (res.empty() || !res.value("success", false))
        return nlohmann::json::array();
    return res["body"].value("variables", nlohmann::json::array());
}

std::string DapClientService::evaluate(int frameId, const std::string& expression, const std::string& context,
                                       uint32_t timeoutMs)
{
    nlohmann::json args;
    args["frameId"] = frameId;
    args["expression"] = expression;
    args["context"] = context;

    const auto res = request("evaluate", args, timeoutMs);
    if (res.empty() || !res.value("success", false))
        return "";
    return res["body"].value("result", "");
}

// DAP readMemory — RFC 1341 §5.2 base64 decode (portable, no dependency)
static std::string base64Decode(const std::string& in)
{
    // Build LUT once using a plain C-array (avoids MSVC static-lambda/std::array issues)
    static unsigned char lut[256];
    static bool lutReady = false;
    if (!lutReady)
    {
        memset(lut, 0xFF, sizeof(lut));
        const char* enc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; enc[i]; ++i)
            lut[static_cast<unsigned char>(enc[i])] = static_cast<unsigned char>(i);
        lut[static_cast<unsigned char>('=')] = 0;
        lutReady = true;
    }
    std::string out;
    out.reserve((in.size() * 3) / 4 + 4);
    unsigned int val = 0;
    int bits = -8;
    for (unsigned char c : in)
    {
        const unsigned char decoded = lut[c];
        if (decoded == 0xFF)
            continue;
        val = (val << 6) | decoded;
        bits += 6;
        if (bits >= 0)
        {
            out.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

std::string DapClientService::readMemory(const std::string& memoryReference, uint64_t offset, uint64_t count,
                                         uint64_t* outBytesRead)
{
    nlohmann::json args;
    args["memoryReference"] = memoryReference;
    args["offset"] = offset;
    args["count"] = count;

    const auto res = request("readMemory", args, 10000);
    if (res.empty() || !res.value("success", false))
    {
        if (outBytesRead)
            *outBytesRead = 0;
        return {};
    }

    const auto& body = res.value("body", nlohmann::json::object());
    const std::string encoded = body.value("data", "");
    if (outBytesRead)
    {
        // unreadableBytes field tells us how many of count bytes could not be read
        const uint64_t unreadable = body.value("unreadableBytes", (uint64_t)0);
        *outBytesRead = (encoded.empty() ? 0 : count - unreadable);
    }
    return base64Decode(encoded);
}

}  // namespace RawrXD::Debug
