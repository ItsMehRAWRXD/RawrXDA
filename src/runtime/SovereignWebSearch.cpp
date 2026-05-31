// =============================================================================
// SovereignWebSearch.cpp — Phase 49: WinHTTP-based local search integration
// =============================================================================
// Queries a locally-hosted SearXNG instance (127.0.0.1:8888) over HTTP.
// Falls back gracefully when the service is unavailable.
// Results are cached per-query with a 5-minute TTL.
// =============================================================================
#include "SovereignWebSearch.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

// nlohmann/json is available in the project's 3rdparty path
#include "nlohmann/json.hpp"
using json = nlohmann::json;

namespace RawrXD::Runtime {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Percent-encode a UTF-8 search term for a URL query string.
// Only encodes characters outside [A-Za-z0-9._~-].
static std::string percentEncode(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 3);
    const char hex[] = "0123456789ABCDEF";
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[(c >> 4) & 0xF];
            out += hex[c & 0xF];
        }
    }
    return out;
}

// Narrow string → wide string (ASCII/UTF-8 safe for URL components).
static std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

// Wide string → narrow UTF-8.
static std::string toNarrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

// ---------------------------------------------------------------------------
// Per-entry cache record
// ---------------------------------------------------------------------------
struct CachedEntry {
    std::vector<SearchResult> results;
    std::chrono::steady_clock::time_point expiry;
};

// ---------------------------------------------------------------------------
// SovereignWebSearch implementation
// ---------------------------------------------------------------------------
static const wchar_t* kSearchHost = L"127.0.0.1";
static const INTERNET_PORT kSearchPort = 8888;
// GET /search?q=<query>&format=json&categories=general
static const std::chrono::seconds kCacheTTL{300};  // 5 minutes
static const DWORD kConnectTimeoutMs  = 3000;
static const DWORD kReceiveTimeoutMs  = 8000;
static const size_t kMaxBodyBytes     = 512 * 1024; // 512 KB guard
static const size_t kMaxResults       = 10;

SovereignWebSearch& SovereignWebSearch::instance() {
    static SovereignWebSearch inst;
    return inst;
}

SovereignWebSearch::SovereignWebSearch() = default;
SovereignWebSearch::~SovereignWebSearch() = default;

// ---------------------------------------------------------------------------
// Perform a raw HTTP GET against the local SearXNG endpoint.
// Returns the response body, or empty on any failure.
// ---------------------------------------------------------------------------
std::string SovereignWebSearch::httpGet(const std::string& path) {
    std::string result;

    HINTERNET hSession = WinHttpOpen(
        L"RawrXD-SovereignSearch/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) return result;

    // Set aggressive timeouts — search must be fast.
    WinHttpSetTimeouts(hSession,
        static_cast<int>(kConnectTimeoutMs),
        static_cast<int>(kConnectTimeoutMs),
        static_cast<int>(kReceiveTimeoutMs),
        static_cast<int>(kReceiveTimeoutMs));

    HINTERNET hConnect = WinHttpConnect(hSession, kSearchHost, kSearchPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return result; }

    std::wstring wPath = toWide(path);
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", wPath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    if (!WinHttpSendRequest(hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Validate HTTP status
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (statusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Read body in chunks
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        if (result.size() + bytesAvailable > kMaxBodyBytes) break; // guard
        size_t offset = result.size();
        result.resize(offset + bytesAvailable);
        DWORD bytesRead = 0;
        if (!WinHttpReadData(hRequest, &result[offset], bytesAvailable, &bytesRead)) break;
        result.resize(offset + bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

// ---------------------------------------------------------------------------
// Parse SearXNG JSON response body into SearchResult vector.
// SearXNG format: { "results": [ { "title", "url", "content" }, ... ] }
// ---------------------------------------------------------------------------
static std::vector<SearchResult> parseSearXNG(const std::string& body) {
    std::vector<SearchResult> out;
    try {
        auto j = json::parse(body);
        if (!j.contains("results") || !j["results"].is_array()) return out;
        for (auto& item : j["results"]) {
            if (out.size() >= kMaxResults) break;
            SearchResult r;
            r.title   = item.value("title",   "");
            r.url     = item.value("url",     "");
            r.snippet = item.value("content", "");
            // Truncate overly long snippets
            if (r.snippet.size() > 512) r.snippet.resize(512);
            if (!r.url.empty()) out.push_back(std::move(r));
        }
    } catch (...) {}
    return out;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
std::vector<SearchResult> SovereignWebSearch::query(const std::string& searchTerm) {
    if (searchTerm.empty()) return {};

    // Limit query length to guard against injection / oversized requests.
    const size_t kMaxQueryLen = 256;
    std::string safe = searchTerm.substr(0, kMaxQueryLen);

    // Cache lookup
    {
        std::lock_guard<std::mutex> lk(m_cacheMutex);
        auto it = m_cache.find(safe);
        if (it != m_cache.end()) {
            if (std::chrono::steady_clock::now() < it->second.expiry) {
                return it->second.results;
            }
            m_cache.erase(it); // stale
        }
    }

    // Build request path — percent-encode the query term
    std::string path = "/search?q=" + percentEncode(safe) +
                       "&format=json&categories=general&language=en-US";

    std::string body = httpGet(path);
    std::vector<SearchResult> results;
    if (!body.empty()) {
        results = parseSearXNG(body);
    }

    // Store in cache (even empty result sets, to avoid hammering a down server)
    {
        std::lock_guard<std::mutex> lk(m_cacheMutex);
        CachedEntry entry;
        entry.results = results;
        entry.expiry  = std::chrono::steady_clock::now() + kCacheTTL;
        m_cache[safe] = std::move(entry);
    }

    return results;
}

void SovereignWebSearch::clearCache() {
    std::lock_guard<std::mutex> lk(m_cacheMutex);
    m_cache.clear();
}

} // namespace RawrXD::Runtime
