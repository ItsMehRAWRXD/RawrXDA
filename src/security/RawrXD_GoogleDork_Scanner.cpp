// RawrXD_GoogleDork_Scanner.cpp — Google Dork Scanner + bug-safe SQLi detection
// Bug 1: Infinite loop — maxIterations, empty response, last-chunk length, duplicate detection
// Bug 2: Malformed URL — replaceAll(suffix, value); boolean payloads true/false/0/1
// In-house: WinHTTP, rawrxd_json export, no external deps.
#ifdef _WIN32
#define RAWRXD_DORKSCANNER_EXPORTS
#endif
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include "RawrXD_GoogleDork_Scanner.h"
#include "core/rawrxd_json.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>

namespace RawrXD {
namespace Security {

namespace {

const int DEFAULT_MAX_ITERATIONS = 100;  // Bug 1: prevent infinite extraction loop
const int DEFAULT_DELAY_MS = 1500;
const int DEFAULT_TIMEOUT_MS = 10000;

// Boolean payloads for blind SQLi (Bug 2)
const char* BOOLEAN_PAYLOADS[] = { "null", "true", "false", "0", "1" };
const int BOOLEAN_PAYLOAD_COUNT = 5;

// Replace all occurrences of suffix in baseUrl with value (Bug 2: not literal "=null")
std::string replaceSuffix(const std::string& baseUrl, const std::string& suffix, const std::string& value) {
    std::string out = baseUrl;
    size_t pos = 0;
    while ((pos = out.find(suffix, pos)) != std::string::npos) {
        out.replace(pos, suffix.size(), value);
        pos += value.size();
    }
    return out;
}

// --- Bug-safe data extraction (Exploit2-style): max iterations, empty exit, last-chunk check, duplicate block ---
std::string extractDataSafe(const std::string& response, const std::string& leftDelim, const std::string& rightDelim,
                            int maxIterations, bool* ok) {
    if (ok) *ok = true;
    std::string collected;
    std::string lastBlock;
    int iterations = 0;
    size_t start = 0;

    while (iterations < maxIterations) {
        size_t i = response.find(leftDelim, start);
        if (i == std::string::npos) break;

        size_t j = response.find(rightDelim, i + leftDelim.size());
        if (j == std::string::npos) break;

        std::string block = response.substr(i + leftDelim.size(), j - (i + leftDelim.size()));
        if (block.empty()) {
            start = j + rightDelim.size();
            iterations++;
            continue;
        }

        if (block == lastBlock) break;
        lastBlock = block;

        if (!collected.empty()) collected += ",";
        collected += block;

        start = j + rightDelim.size();
        iterations++;

        if (block.size() < rightDelim.size() + 2) break;
    }

    return collected;
}

// --- Built-in dork patterns ---
const char* BUILTIN_DORKS[] = {
    "inurl:.php?id=",
    "inurl:.asp?id=",
    "inurl:.jsp?id=",
    "inurl:.php?cat=",
    "inurl:.php?page=",
    "inurl:.php?item=",
    "inurl:.php?file=",
    "inurl:.php?doc=",
    "inurl:index.php?id=",
    "inurl:view.php?id=",
    "inurl:page.php?id=",
    "inurl:product.php?id=",
    "inurl:article.php?id=",
    "inurl:detail.php?id=",
    "inurl:download.php?file=",
    "inurl:include.php?file=",
    "inurl:admin/login.php",
    "inurl:administrator/login",
    "inurl:wp-content",
    "filetype:sql inurl:backup",
    "filetype:log inurl:log",
    "filetype:bak inurl:backup",
    "intitle:index.of config",
    "intitle:index.of .env",
    "inurl:phpmyadmin",
    "inurl:mysql",
};
const int BUILTIN_DORK_COUNT = sizeof(BUILTIN_DORKS) / sizeof(BUILTIN_DORKS[0]);

// Error-based SQLi signatures (MySQL, PostgreSQL, MSSQL, Oracle, SQLite)
bool matchErrorBased(const std::string& body, char* dbType, int dbTypeSize) {
    const char* mysql[] = { "You have an error in your SQL syntax", "mysql_fetch", "MySqlException", "Warning: mysql_", nullptr };
    const char* pg[] = { "pg_query()", "PostgreSQL", "PG::Error", nullptr };
    const char* mssql[] = { "Microsoft SQL Server", "ODBC SQL Server", "SqlException", nullptr };
    const char* oracle[] = { "ORA-", "Oracle error", "OracleException", nullptr };
    const char* sqlite[] = { "SQLite", "sqlite3.", nullptr };

    auto test = [&body](const char** sigs, const char* name) {
        for (int i = 0; sigs[i]; i++)
            if (body.find(sigs[i]) != std::string::npos) return name;
        return static_cast<const char*>(nullptr);
    };

    const char* r = test(mysql, "mysql");
    if (!r) r = test(pg, "postgresql");
    if (!r) r = test(mssql, "mssql");
    if (!r) r = test(oracle, "oracle");
    if (!r) r = test(sqlite, "sqlite");
    if (r && dbType && dbTypeSize > 0) {
        strncpy_s(dbType, dbTypeSize, r, _TRUNCATE);
        return true;
    }
    return r != nullptr;
}

// WinHTTP simple GET (in-house)
int httpGet(const std::string& url, std::string& outBody, int timeoutMs, const char* userAgent) {
#ifdef _WIN32
    // WinHTTP is wide-char only; convert URL and UA (ASCII/UTF-8) to UTF-16.
    auto to_wide = [](const std::string& s) -> std::wstring {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        if (n <= 0) n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), nullptr, 0);
        if (n <= 0) return {};
        std::wstring w;
        w.resize((size_t)n);
        int wrote = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
        if (wrote <= 0) wrote = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), w.data(), n);
        if (wrote <= 0) return {};
        return w;
    };

    std::wstring wurl = to_wide(url);
    if (wurl.empty()) return -1;

    URL_COMPONENTS uc = {};
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = (DWORD)_countof(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = (DWORD)_countof(path);

    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc))
        return -1;

    std::wstring wua = to_wide(userAgent ? std::string(userAgent) : std::string());
    if (wua.empty()) wua = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";

    HINTERNET hSession = WinHttpOpen(wua.c_str(),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return -1;

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return -1; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return -1; }

    if (timeoutMs > 0) {
        WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    }

    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return -1; }
    if (!WinHttpReceiveResponse(hRequest, nullptr)) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return -1; }

    DWORD status = 0, statusSize = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

    outBody.clear();
    DWORD available = 0;
    do {
        if (!WinHttpQueryDataAvailable(hRequest, &available) || available == 0) break;
        std::vector<char> buf(available);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, buf.data(), available, &read)) break;
        outBody.append(buf.data(), read);
    } while (true);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return (int)status;
#else
    (void)url; (void)outBody; (void)timeoutMs; (void)userAgent;
    return -1;
#endif
}

} // namespace

struct GoogleDorkScannerImpl {
    DorkScannerConfig config;
    std::vector<DorkTarget> results;
    std::function<void(int, int, const std::string&)> progressCb;
    std::mutex resultsMutex;

    GoogleDorkScannerImpl() {
        config.threadCount = 4;
        config.delayMs = DEFAULT_DELAY_MS;
        config.timeoutMs = DEFAULT_TIMEOUT_MS;
        config.userAgent = nullptr;
        config.proxyUrl = nullptr;
        config.maxIterations = DEFAULT_MAX_ITERATIONS;
        config.enableErrorBased = 1;
        config.enableTimeBased = 1;
        config.enableBoolean = 1;
    }
};

GoogleDorkScanner::GoogleDorkScanner() : m_impl(new GoogleDorkScannerImpl()) {}
GoogleDorkScanner::GoogleDorkScanner(const DorkScannerConfig& config) : m_impl(new GoogleDorkScannerImpl()) {
    static_cast<GoogleDorkScannerImpl*>(m_impl)->config = config;
}
GoogleDorkScanner::~GoogleDorkScanner() { delete static_cast<GoogleDorkScannerImpl*>(m_impl); }

bool GoogleDorkScanner::initialize() { return true; }

std::vector<DorkTarget> GoogleDorkScanner::scanSingle(const std::string& dork) {
    auto* impl = static_cast<GoogleDorkScannerImpl*>(m_impl);
    impl->results.clear();

    std::string url = "https://www.google.com/search?q=";
    for (unsigned char c : dork) {
        if (std::isalnum(c) || c == '*' || c == ':' || c == '.' || c == '=' || c == '?') url += (char)c;
        else if (c == ' ') url += '+';
        else { char enc[4]; snprintf(enc, sizeof(enc), "%%%02X", c); url += enc; }
    }

    std::string body;
    int status = httpGet(url, body, impl->config.timeoutMs, impl->config.userAgent);
    if (status != 200) return impl->results;

    DorkTarget t;
    t.url = url;
    t.dork = dork;
    t.statusCode = status;
    t.detail = "Dork scanned (no live SQLi test in this stub)";
    impl->results.push_back(t);
    m_results = impl->results;
    return impl->results;
}

std::vector<DorkTarget> GoogleDorkScanner::scanFile(const std::string& dorkFilePath) {
    auto* impl = static_cast<GoogleDorkScannerImpl*>(m_impl);
    impl->results.clear();
    std::ifstream f(dorkFilePath);
    if (!f) return impl->results;
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        auto one = scanSingle(line);
        for (auto& r : one) impl->results.push_back(r);
    }
    m_results = impl->results;
    return impl->results;
}

bool GoogleDorkScanner::exportToJson(const std::string& filePath) const {
    RawrXD::JsonArray arr;
    for (const auto& r : m_results) {
        RawrXD::JsonObject obj;
        obj["url"] = RawrXD::JsonValue(r.url);
        obj["dork"] = RawrXD::JsonValue(r.dork);
        obj["vulnType"] = RawrXD::JsonValue((double)r.vulnType);
        obj["dbType"] = RawrXD::JsonValue(r.dbType);
        obj["detail"] = RawrXD::JsonValue(r.detail);
        obj["statusCode"] = RawrXD::JsonValue((double)r.statusCode);
        arr.push_back(RawrXD::JsonValue(std::move(obj)));
    }
    RawrXD::JsonValue root(std::move(arr));
    std::ofstream f(filePath);
    if (!f) return false;
    f << root.dump(true, 0);
    return true;
}

bool GoogleDorkScanner::exportToCsv(const std::string& filePath) const {
    std::ofstream f(filePath);
    if (!f) return false;
    f << "url,dork,vulnType,dbType,detail,statusCode\n";
    for (const auto& r : m_results)
        f << "\"" << r.url << "\",\"" << r.dork << "\"," << r.vulnType << ",\"" << r.dbType << "\",\"" << r.detail << "\"," << r.statusCode << "\n";
    return true;
}

int GoogleDorkScanner::getBuiltinDorkCount() const { return BUILTIN_DORK_COUNT; }
std::string GoogleDorkScanner::getBuiltinDork(int index) const {
    if (index < 0 || index >= BUILTIN_DORK_COUNT) return "";
    return BUILTIN_DORKS[index];
}

// --- Bug-safe boolean payload test (Bug 2: proper replace + multiple payloads) ---
int DorkScanner_TestBooleanPayloads(const char* baseUrl, char* outVerdict, int outSize) {
    if (!baseUrl || !outVerdict || outSize <= 0) return -1;
    std::string base(baseUrl);
    size_t eq = base.find('=');
    if (eq == std::string::npos) { strncpy_s(outVerdict, outSize, "no_param", _TRUNCATE); return 0; }
    std::string suffix = base.substr(eq);
    std::string body1, body2;
    int s1 = httpGet(replaceSuffix(base, suffix, "=1") + "&raw=1", body1, DEFAULT_TIMEOUT_MS, nullptr);
    int s2 = httpGet(replaceSuffix(base, suffix, "=0") + "&raw=1", body2, DEFAULT_TIMEOUT_MS, nullptr);
    if (s1 != 200 || s2 != 200) { strncpy_s(outVerdict, outSize, "request_failed", _TRUNCATE); return 0; }
    bool diff = (body1 != body2);
    strncpy_s(outVerdict, outSize, diff ? "possible_boolean_sqli" : "no_difference", _TRUNCATE);
    return diff ? 1 : 0;
}

} // namespace Security
} // namespace RawrXD

// --- C API ---
using namespace RawrXD::Security;

static void defaultProgress(int, int, const char*, void*) {}

extern "C" {

DORKSCANNER_API void* DorkScanner_Create(const DorkScannerConfig* config) {
    if (config) {
        DorkScannerConfig c = *config;
        if (c.maxIterations <= 0) c.maxIterations = DEFAULT_MAX_ITERATIONS;
        return new GoogleDorkScanner(c);
    }
    return new GoogleDorkScanner();
}

DORKSCANNER_API void DorkScanner_Destroy(void* scanner) {
    delete static_cast<GoogleDorkScanner*>(scanner);
}

DORKSCANNER_API int DorkScanner_Initialize(void* scanner, void* userData) {
    (void)userData;
    return static_cast<GoogleDorkScanner*>(scanner)->initialize() ? 1 : 0;
}

DORKSCANNER_API void DorkScanner_SetProgressCallback(void* scanner, DorkProgressFn fn) {
    if (!scanner || !fn) return;
    static_cast<GoogleDorkScanner*>(scanner)->setProgressCallback(
        [fn](int cur, int tot, const std::string& msg) { fn(cur, tot, msg.c_str(), nullptr); });
}

DORKSCANNER_API int DorkScanner_ScanSingle(void* scanner, const char* dork, DorkResult* results, int maxResults) {
    if (!scanner || !dork) return 0;
    auto vec = static_cast<GoogleDorkScanner*>(scanner)->scanSingle(dork);
    int n = 0;
    for (const auto& r : vec) {
        if (results && n < maxResults) {
            strncpy_s(results[n].url, r.url.c_str(), _TRUNCATE);
            strncpy_s(results[n].dork, r.dork.c_str(), _TRUNCATE);
            results[n].vulnType = r.vulnType;
            strncpy_s(results[n].dbType, r.dbType.c_str(), _TRUNCATE);
            strncpy_s(results[n].detail, r.detail.c_str(), _TRUNCATE);
            results[n].statusCode = r.statusCode;
        }
        n++;
    }
    return n;
}

DORKSCANNER_API int DorkScanner_ScanFile(void* scanner, const char* dorkFilePath, DorkResult* results, int maxResults) {
    if (!scanner || !dorkFilePath) return 0;
    auto vec = static_cast<GoogleDorkScanner*>(scanner)->scanFile(dorkFilePath);
    int n = 0;
    for (const auto& r : vec) {
        if (results && n < maxResults) {
            strncpy_s(results[n].url, r.url.c_str(), _TRUNCATE);
            strncpy_s(results[n].dork, r.dork.c_str(), _TRUNCATE);
            results[n].vulnType = r.vulnType;
            strncpy_s(results[n].dbType, r.dbType.c_str(), _TRUNCATE);
            strncpy_s(results[n].detail, r.detail.c_str(), _TRUNCATE);
            results[n].statusCode = r.statusCode;
        }
        n++;
    }
    return n;
}

DORKSCANNER_API int DorkScanner_ExportToJson(void* scanner, const char* filePath) {
    if (!scanner || !filePath) return 0;
    return static_cast<const GoogleDorkScanner*>(scanner)->exportToJson(filePath) ? 1 : 0;
}

DORKSCANNER_API int DorkScanner_GetBuiltinDorkCount(void* scanner) {
    if (!scanner) return BUILTIN_DORK_COUNT;
    return static_cast<const GoogleDorkScanner*>(scanner)->getBuiltinDorkCount();
}

DORKSCANNER_API int DorkScanner_GetBuiltinDork(void* scanner, int index, char* buf, int bufSize) {
    if (!buf || bufSize <= 0) return 0;
    buf[0] = '\0';
    std::string s = scanner ? static_cast<const GoogleDorkScanner*>(scanner)->getBuiltinDork(index) : (index >= 0 && index < BUILTIN_DORK_COUNT ? BUILTIN_DORKS[index] : "");
    if (s.empty()) return 0;
    strncpy_s(buf, bufSize, s.c_str(), _TRUNCATE);
    return 1;
}

} // extern "C"
