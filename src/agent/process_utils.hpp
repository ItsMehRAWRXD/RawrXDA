/**
 * @file process_utils.hpp
 * @brief Lightweight process execution and environment utilities
 *
 * Replaces Qt QProcess, qEnvironmentVariable, QSysInfo, QCryptographicHash
 * with pure C++20 / Win32 / POSIX implementations.
 *
 * Architecture: No Qt, no exceptions.
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <thread>
#include <sstream>
#include <functional>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════
// ProcessResult — Result from an external process execution
// ═══════════════════════════════════════════════════════════════════════════

struct ProcResult {
    int exitCode = -1;
    std::string stdoutStr;
    std::string stderrStr;
    bool timedOut = false;

    bool ok() const { return exitCode == 0 && !timedOut; }
};

// ═══════════════════════════════════════════════════════════════════════════
// Environment variable access (replaces qEnvironmentVariable)
// ═══════════════════════════════════════════════════════════════════════════

inline std::string getEnvVar(const char* name, const std::string& fallback = "") {
#ifdef _WIN32
    char buf[4096];
    DWORD n = GetEnvironmentVariableA(name, buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf)) return std::string(buf, n);
    return fallback;
#else
    const char* val = std::getenv(name);
    return val ? std::string(val) : fallback;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// Process Execution (replaces QProcess)
// ═══════════════════════════════════════════════════════════════════════════

namespace proc {

/**
 * @brief Build a command-line string from program + args
 */
inline std::string buildCmdLine(const std::string& program,
                                 const std::vector<std::string>& args)
{
    std::string cmdLine = program;
    for (const auto& arg : args) {
        cmdLine += ' ';
        if (arg.find(' ') != std::string::npos || arg.find('"') != std::string::npos) {
            cmdLine += '"' + arg + '"';
        } else {
            cmdLine += arg;
        }
    }
    return cmdLine;
}

#ifdef _WIN32

/**
 * @brief Run a process synchronously with stdout/stderr capture (Windows)
 */
inline ProcResult run(const std::string& program,
                      const std::vector<std::string>& args,
                      int timeoutMs = 120000,
                      const std::string& workingDir = "")
{
    ProcResult result;
    std::string cmdLine = buildCmdLine(program, args);

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
    HANDLE hStderrRead = nullptr, hStderrWrite = nullptr;

    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0) ||
        !CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
        result.stderrStr = "Failed to create pipes";
        return result;
    }

    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    BOOL ok = CreateProcessA(
        nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr,
        workingDir.empty() ? nullptr : workingDir.c_str(),
        &si, &pi
    );

    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);

    if (!ok) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);
        result.stderrStr = "CreateProcess failed for: " + program;
        return result;
    }

    // Read pipes in background to avoid deadlock
    auto readPipe = [](HANDLE h) -> std::string {
        std::string data;
        char buf[4096];
        DWORD n;
        while (ReadFile(h, buf, sizeof(buf), &n, nullptr) && n > 0)
            data.append(buf, n);
        return data;
    };

    std::string stdoutData, stderrData;
    std::thread convergence_a([&]() { stdoutData = readPipe(hStdoutRead); });
    std::thread convergence_b([&]() { stderrData = readPipe(hStderrRead); });

    DWORD waitMs = (timeoutMs > 0) ? static_cast<DWORD>(timeoutMs) : INFINITE;
    DWORD waitResult = WaitForSingleObject(pi.hProcess, waitMs);

    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result.timedOut = true;
        result.exitCode = -1;
    } else {
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        result.exitCode = static_cast<int>(exitCode);
    }

    convergence_a.join();
    convergence_b.join();

    result.stdoutStr = std::move(stdoutData);
    result.stderrStr = std::move(stderrData);

    CloseHandle(hStdoutRead);
    CloseHandle(hStderrRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return result;
}

/**
 * @brief Fire-and-forget process launch (replaces QProcess::startDetached)
 */
inline bool startDetached(const std::string& program,
                          const std::vector<std::string>& args)
{
    std::string cmdLine = buildCmdLine(program, args);
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');
    BOOL ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr,
                             FALSE, DETACHED_PROCESS, nullptr, nullptr, &si, &pi);
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return ok != 0;
}

#else // POSIX

inline ProcResult run(const std::string& program,
                      const std::vector<std::string>& args,
                      int timeoutMs = 120000,
                      const std::string& workingDir = "")
{
    ProcResult result;
    std::string cmdLine = buildCmdLine(program, args);
    cmdLine += " 2>&1";
    FILE* pipe = popen(cmdLine.c_str(), "r");
    if (!pipe) {
        result.stderrStr = "popen failed for: " + program;
        return result;
    }
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe))
        result.stdoutStr += buf;
    result.exitCode = pclose(pipe);
    result.exitCode = WEXITSTATUS(result.exitCode);
    return result;
}

inline bool startDetached(const std::string& program,
                          const std::vector<std::string>& args)
{
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        std::vector<char*> argv;
        std::string prog = program;
        argv.push_back(prog.data());
        std::vector<std::string> argsCopy = args;
        for (auto& a : argsCopy) argv.push_back(a.data());
        argv.push_back(nullptr);
        execvp(program.c_str(), argv.data());
        _exit(127);
    }
    return pid > 0;
}

#endif

/**
 * @brief Simple synchronous execute returning exit code only (replaces QProcess::execute)
 */
inline int execute(const std::string& program,
                   const std::vector<std::string>& args,
                   int timeoutMs = 120000)
{
    return run(program, args, timeoutMs).exitCode;
}

} // namespace proc

// ═══════════════════════════════════════════════════════════════════════════
// System Info (replaces QSysInfo)
// ═══════════════════════════════════════════════════════════════════════════

namespace sysinfo {

inline std::string machineHostName() {
#ifdef _WIN32
    char buf[256] = {};
    DWORD size = sizeof(buf);
    GetComputerNameA(buf, &size);
    return buf;
#else
    char buf[256] = {};
    gethostname(buf, sizeof(buf));
    return buf;
#endif
}

inline std::string cpuArchitecture() {
#ifdef _WIN32
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x86_64";
        case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        default: return "unknown";
    }
#else
    struct utsname un{};
    uname(&un);
    return un.machine;
#endif
}

inline std::string productType() {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

inline std::string productVersion() {
#ifdef _WIN32
    // Use RtlGetVersion for accurate Windows version
    return "10+";
#else
    struct utsname un{};
    uname(&un);
    return un.release;
#endif
}

inline unsigned int idealThreadCount() {
    unsigned int n = std::thread::hardware_concurrency();
    return n > 0 ? n : 1;
}

} // namespace sysinfo

// ═══════════════════════════════════════════════════════════════════════════
// Simple SHA-256 (replaces QCryptographicHash)
// Uses Windows CNG on Win32, OpenSSL or stub on POSIX.
// ═══════════════════════════════════════════════════════════════════════════

namespace crypto {

#ifdef _WIN32
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

inline std::string sha256hex(const void* data, size_t len) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    UCHAR hash[32] = {};
    DWORD cbHash = sizeof(hash);

    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    BCryptHashData(hHash, const_cast<PUCHAR>(static_cast<const UCHAR*>(data)),
                   static_cast<ULONG>(len), 0);
    BCryptFinishHash(hHash, hash, cbHash, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    char hex[65] = {};
    for (int i = 0; i < 32; ++i)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    return hex;
}

inline std::string sha256hex(const std::string& s) {
    return sha256hex(s.data(), s.size());
}

inline std::string sha256hex(const std::vector<uint8_t>& v) {
    return sha256hex(v.data(), v.size());
}

#else

inline std::string sha256hex(const void* /*data*/, size_t /*len*/) {
    // TODO: implement with OpenSSL EVP or built-in
    return "0000000000000000000000000000000000000000000000000000000000000000";
}

inline std::string sha256hex(const std::string& s) {
    return sha256hex(s.data(), s.size());
}

inline std::string sha256hex(const std::vector<uint8_t>& v) {
    return sha256hex(v.data(), v.size());
}

#endif

} // namespace crypto

// ═══════════════════════════════════════════════════════════════════════════
// Simple HTTP Client (replaces QNetworkAccessManager)
// Uses WinHTTP on Windows, curl fallback on POSIX
// ═══════════════════════════════════════════════════════════════════════════

namespace http {

struct Response {
    int statusCode = 0;
    std::string body;
    std::string error;
    bool ok() const { return statusCode >= 200 && statusCode < 300 && error.empty(); }
};

#ifdef _WIN32
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

/**
 * @brief Parse a URL into components
 */
struct UrlParts {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool isHttps = true;
};

inline UrlParts parseUrl(const std::string& url) {
    UrlParts parts;
    std::wstring wurl(url.begin(), url.end());

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t hostBuf[256] = {}, pathBuf[2048] = {};
    uc.lpszHostName = hostBuf;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = pathBuf;
    uc.dwUrlPathLength = 2048;

    if (WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) {
        parts.host = hostBuf;
        parts.path = pathBuf;
        parts.port = uc.nPort;
        parts.isHttps = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    }
    return parts;
}

inline Response request(const std::string& method,
                         const std::string& url,
                         const std::vector<std::pair<std::string, std::string>>& headers = {},
                         const std::string& body = "")
{
    Response resp;
    auto parts = parseUrl(url);
    std::wstring wmethod(method.begin(), method.end());

    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { resp.error = "WinHttpOpen failed"; return resp; }

    HINTERNET hConnect = WinHttpConnect(hSession, parts.host.c_str(), parts.port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); resp.error = "WinHttpConnect failed"; return resp; }

    DWORD flags = parts.isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(), parts.path.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        resp.error = "WinHttpOpenRequest failed"; return resp;
    }

    // Add headers
    for (auto& [k, v] : headers) {
        std::string hdr = k + ": " + v;
        std::wstring whdr(hdr.begin(), hdr.end());
        WinHttpAddRequestHeaders(hRequest, whdr.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Send request
    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
                                   static_cast<DWORD>(body.size()),
                                   static_cast<DWORD>(body.size()), 0);
    if (!sent) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        resp.error = "WinHttpSendRequest failed"; return resp;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        resp.error = "WinHttpReceiveResponse failed"; return resp;
    }

    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    resp.statusCode = static_cast<int>(statusCode);

    // Read response body
    DWORD bytesAvail = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvail) && bytesAvail > 0) {
        std::vector<char> buf(bytesAvail);
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, buf.data(), bytesAvail, &bytesRead);
        resp.body.append(buf.data(), bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return resp;
}

inline Response get(const std::string& url,
                     const std::vector<std::pair<std::string, std::string>>& headers = {}) {
    return request("GET", url, headers);
}

inline Response post(const std::string& url, const std::string& body,
                      const std::vector<std::pair<std::string, std::string>>& headers = {}) {
    return request("POST", url, headers, body);
}

inline Response put(const std::string& url, const std::string& body,
                     const std::vector<std::pair<std::string, std::string>>& headers = {}) {
    return request("PUT", url, headers, body);
}

#else // POSIX — curl fallback

inline Response request(const std::string& method,
                         const std::string& url,
                         const std::vector<std::pair<std::string, std::string>>& headers = {},
                         const std::string& body = "")
{
    Response resp;
    std::string cmd = "curl -s -w '\\n%{http_code}' -X " + method;
    for (auto& [k, v] : headers)
        cmd += " -H '" + k + ": " + v + "'";
    if (!body.empty())
        cmd += " -d '" + body + "'";
    cmd += " '" + url + "' 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { resp.error = "curl popen failed"; return resp; }
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe))
        resp.body += buf;
    int rc = pclose(pipe);
    (void)rc;

    // Last line is status code
    auto pos = resp.body.rfind('\n');
    if (pos != std::string::npos && pos > 0) {
        auto lastNl = resp.body.rfind('\n', pos - 1);
        std::string code = resp.body.substr(lastNl + 1);
        resp.statusCode = std::atoi(code.c_str());
        resp.body = resp.body.substr(0, lastNl + 1);
    }
    return resp;
}

inline Response get(const std::string& url,
                     const std::vector<std::pair<std::string, std::string>>& headers = {}) {
    return request("GET", url, headers);
}

inline Response post(const std::string& url, const std::string& body,
                      const std::vector<std::pair<std::string, std::string>>& headers = {}) {
    return request("POST", url, headers, body);
}

inline Response put(const std::string& url, const std::string& body,
                     const std::vector<std::pair<std::string, std::string>>& headers = {}) {
    return request("PUT", url, headers, body);
}

#endif

} // namespace http

// ═══════════════════════════════════════════════════════════════════════════
// File I/O Helpers (replaces Qt QFile patterns)
// ═══════════════════════════════════════════════════════════════════════════

namespace fileutil {

inline std::string readAll(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string buf(static_cast<size_t>(sz), '\0');
    fread(buf.data(), 1, sz, f);
    fclose(f);
    return buf;
}

inline std::vector<uint8_t> readAllBytes(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    fread(buf.data(), 1, sz, f);
    fclose(f);
    return buf;
}

inline bool writeAll(const std::string& path, const std::string& data) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t written = fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return written == data.size();
}

inline bool writeAll(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t written = fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return written == data.size();
}

} // namespace fileutil
