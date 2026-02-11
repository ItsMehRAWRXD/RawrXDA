// ============================================================================
// auto_update_system.cpp — Automatic Update System Implementation
// ============================================================================
//
// PURPOSE:
//   Checks GitHub Releases API via WinHTTP for new versions.
//   Parses the JSON response manually (lightweight, no nlohmann dependency).
//   Runs on a background thread for async check. Callback-based notification.
//
// PATTERN:   Result-based error handling, no exceptions
// THREADING: Background thread for async, CRITICAL_SECTION for state protection
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../../include/auto_update_system.h"
#include <winhttp.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>

#pragma comment(lib, "winhttp.lib")

namespace RawrXD {
namespace Update {

// ============================================================================
// Version string constants
// ============================================================================
static const wchar_t* USER_AGENT = L"RawrXD-IDE-UpdateChecker/1.0";
static const wchar_t* GITHUB_API_HOST = L"api.github.com";

// ============================================================================
// Singleton
// ============================================================================

AutoUpdateSystem& AutoUpdateSystem::instance() {
    static AutoUpdateSystem s_instance;
    return s_instance;
}

AutoUpdateSystem::AutoUpdateSystem()
    : m_currentVersion{1, 0, 0, 0}
    , m_lastResult{}
    , m_checked(false) {
    m_repoOwner[0] = '\0';
    m_repoName[0] = '\0';
    InitializeCriticalSection(&m_cs);

    // Default repository
    strncpy_s(m_repoOwner, sizeof(m_repoOwner), "ItsMehRAWRXD", _TRUNCATE);
    strncpy_s(m_repoName, sizeof(m_repoName), "RawrXD", _TRUNCATE);
}

AutoUpdateSystem::~AutoUpdateSystem() {
    DeleteCriticalSection(&m_cs);
}

// ============================================================================
// Configuration
// ============================================================================

void AutoUpdateSystem::setCurrentVersion(uint32_t major, uint32_t minor,
                                          uint32_t patch, uint32_t build) {
    EnterCriticalSection(&m_cs);
    m_currentVersion.major = major;
    m_currentVersion.minor = minor;
    m_currentVersion.patch = patch;
    m_currentVersion.build = build;
    LeaveCriticalSection(&m_cs);
}

void AutoUpdateSystem::setRepository(const char* owner, const char* repo) {
    EnterCriticalSection(&m_cs);
    if (owner) strncpy_s(m_repoOwner, sizeof(m_repoOwner), owner, _TRUNCATE);
    if (repo)  strncpy_s(m_repoName, sizeof(m_repoName), repo, _TRUNCATE);
    LeaveCriticalSection(&m_cs);
}

// ============================================================================
// Version Parsing
// ============================================================================

bool AutoUpdateSystem::parseVersionTag(const char* tag, VersionInfo* out) {
    if (!tag || !out) return false;

    // Skip leading 'v' or 'V'
    const char* p = tag;
    if (*p == 'v' || *p == 'V') p++;

    // Parse X.Y.Z[.W]
    int parts[4] = {0, 0, 0, 0};
    int idx = 0;

    while (*p && idx < 4) {
        if (*p >= '0' && *p <= '9') {
            parts[idx] = parts[idx] * 10 + (*p - '0');
        } else if (*p == '.') {
            idx++;
        } else {
            break; // Stop at non-numeric (e.g., "-beta")
        }
        p++;
    }

    if (idx < 1) return false; // Need at least X.Y

    out->major = static_cast<uint32_t>(parts[0]);
    out->minor = static_cast<uint32_t>(parts[1]);
    out->patch = static_cast<uint32_t>(parts[2]);
    out->build = static_cast<uint32_t>(parts[3]);

    return true;
}

// ============================================================================
// WinHTTP GET
// ============================================================================

UpdateCheckResult AutoUpdateSystem::httpGet(const wchar_t* host, const wchar_t* path,
                                             char* outBuffer, size_t outBufferSize) {
    HINTERNET hSession = WinHttpOpen(USER_AGENT,
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        return UpdateCheckResult::error("WinHttpOpen failed", static_cast<int>(GetLastError()));
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host,
                                         INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return UpdateCheckResult::error("WinHttpConnect failed", static_cast<int>(GetLastError()));
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path,
                                             NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return UpdateCheckResult::error("WinHttpOpenRequest failed", static_cast<int>(GetLastError()));
    }

    // GitHub requires User-Agent header
    WinHttpAddRequestHeaders(hRequest,
        L"Accept: application/vnd.github.v3+json\r\n",
        (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS,
                                    0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent) {
        int err = static_cast<int>(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return UpdateCheckResult::error("WinHttpSendRequest failed", err);
    }

    BOOL received = WinHttpReceiveResponse(hRequest, NULL);
    if (!received) {
        int err = static_cast<int>(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return UpdateCheckResult::error("WinHttpReceiveResponse failed", err);
    }

    // Check HTTP status code
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    if (statusCode != 200) {
        char msg[128];
        snprintf(msg, sizeof(msg), "GitHub API returned HTTP %lu", statusCode);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return UpdateCheckResult::error(msg, static_cast<int>(statusCode));
    }

    // Read response body
    size_t totalRead = 0;
    DWORD bytesAvailable = 0;

    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        DWORD toRead = bytesAvailable;
        if (totalRead + toRead >= outBufferSize - 1) {
            toRead = static_cast<DWORD>(outBufferSize - 1 - totalRead);
        }
        if (toRead == 0) break;

        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, outBuffer + totalRead, toRead, &bytesRead);
        totalRead += bytesRead;
    }
    outBuffer[totalRead] = '\0';

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    auto result = UpdateCheckResult::ok();
    return result;
}

// ============================================================================
// Lightweight JSON string extractor (no full parser needed)
// ============================================================================

static const char* findJsonString(const char* json, const char* key,
                                   char* outValue, size_t outLen) {
    // Find "key": "value" pattern
    char searchKey[128];
    snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);

    const char* keyPos = strstr(json, searchKey);
    if (!keyPos) return nullptr;

    // Skip key and find colon
    const char* p = keyPos + strlen(searchKey);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

    if (*p != '"') return nullptr;
    p++; // Skip opening quote

    // Copy value until closing quote
    size_t i = 0;
    while (*p && *p != '"' && i < outLen - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++; // Skip escape char
            switch (*p) {
                case 'n':  outValue[i++] = '\n'; break;
                case 't':  outValue[i++] = '\t'; break;
                case '"':  outValue[i++] = '"';  break;
                case '\\': outValue[i++] = '\\'; break;
                default:   outValue[i++] = *p;   break;
            }
        } else {
            outValue[i++] = *p;
        }
        p++;
    }
    outValue[i] = '\0';

    return p + 1; // Position after closing quote
}

// ============================================================================
// Parse GitHub Releases JSON
// ============================================================================

UpdateCheckResult AutoUpdateSystem::parseReleasesJson(const char* json, size_t jsonLen) {
    (void)jsonLen;

    UpdateCheckResult result{};
    result.success = true;

    // Extract tag_name from the first release (latest)
    char tagName[64] = {};
    if (!findJsonString(json, "tag_name", tagName, sizeof(tagName))) {
        return UpdateCheckResult::error("Could not find tag_name in response");
    }
    strncpy_s(result.tagName, sizeof(result.tagName), tagName, _TRUNCATE);

    // Parse version from tag
    VersionInfo latest{};
    if (!parseVersionTag(tagName, &latest)) {
        return UpdateCheckResult::error("Could not parse version from tag");
    }
    result.latestVersion = latest;

    // Check if update is available
    result.updateAvailable = latest.isNewerThan(m_currentVersion);

    // Extract browser_download_url (first asset)
    char downloadUrl[512] = {};
    if (findJsonString(json, "browser_download_url", downloadUrl, sizeof(downloadUrl))) {
        strncpy_s(result.downloadUrl, sizeof(result.downloadUrl), downloadUrl, _TRUNCATE);
    } else {
        // Fallback: construct release page URL
        snprintf(result.downloadUrl, sizeof(result.downloadUrl),
                 "https://github.com/%s/%s/releases/tag/%s",
                 m_repoOwner, m_repoName, tagName);
    }

    // Extract release notes (body field)
    findJsonString(json, "body", result.releaseNotes, sizeof(result.releaseNotes));

    return result;
}

// ============================================================================
// Synchronous Check
// ============================================================================

UpdateCheckResult AutoUpdateSystem::checkForUpdates() {
    EnterCriticalSection(&m_cs);

    // Build the API path: /repos/{owner}/{repo}/releases/latest
    wchar_t apiPath[256] = {};
    wchar_t wOwner[64] = {};
    wchar_t wRepo[64] = {};
    MultiByteToWideChar(CP_ACP, 0, m_repoOwner, -1, wOwner, 64);
    MultiByteToWideChar(CP_ACP, 0, m_repoName, -1, wRepo, 64);
    _snwprintf_s(apiPath, _countof(apiPath), _TRUNCATE,
                 L"/repos/%s/%s/releases/latest", wOwner, wRepo);

    LeaveCriticalSection(&m_cs);

    // Perform HTTP request (64KB buffer for release JSON)
    static constexpr size_t BUFFER_SIZE = 65536;
    char* responseBuffer = static_cast<char*>(malloc(BUFFER_SIZE));
    if (!responseBuffer) {
        return UpdateCheckResult::error("Memory allocation failed");
    }

    UpdateCheckResult httpResult = httpGet(GITHUB_API_HOST, apiPath,
                                           responseBuffer, BUFFER_SIZE);
    if (!httpResult.success) {
        free(responseBuffer);
        EnterCriticalSection(&m_cs);
        m_lastResult = httpResult;
        m_checked = true;
        LeaveCriticalSection(&m_cs);
        return httpResult;
    }

    // Parse the JSON response
    UpdateCheckResult parseResult = parseReleasesJson(responseBuffer, strlen(responseBuffer));
    free(responseBuffer);

    EnterCriticalSection(&m_cs);
    m_lastResult = parseResult;
    m_checked = true;
    LeaveCriticalSection(&m_cs);

    if (parseResult.success) {
        char debugMsg[256];
        snprintf(debugMsg, sizeof(debugMsg),
                 "[Update] Check complete: current=%u.%u.%u, latest=%s, update=%s\n",
                 m_currentVersion.major, m_currentVersion.minor, m_currentVersion.patch,
                 parseResult.tagName,
                 parseResult.updateAvailable ? "YES" : "no");
        OutputDebugStringA(debugMsg);
    }

    return parseResult;
}

// ============================================================================
// Asynchronous Check (background thread)
// ============================================================================

struct AsyncCheckContext {
    AutoUpdateSystem*    system;
    UpdateCheckCallback  callback;
    void*                userData;
};

static DWORD WINAPI asyncCheckThread(LPVOID param) {
    auto* ctx = static_cast<AsyncCheckContext*>(param);

    UpdateCheckResult result = ctx->system->checkForUpdates();

    if (ctx->callback) {
        ctx->callback(&result, ctx->userData);
    }

    delete ctx;
    return 0;
}

void AutoUpdateSystem::checkForUpdatesAsync(UpdateCheckCallback callback, void* userData) {
    auto* ctx = new AsyncCheckContext;
    ctx->system = this;
    ctx->callback = callback;
    ctx->userData = userData;

    HANDLE hThread = CreateThread(NULL, 0, asyncCheckThread, ctx, 0, NULL);
    if (hThread) {
        CloseHandle(hThread); // Let it run detached
    } else {
        // Thread creation failed — invoke callback with error on caller thread
        UpdateCheckResult err = UpdateCheckResult::error("CreateThread failed", static_cast<int>(GetLastError()));
        if (callback) callback(&err, userData);
        delete ctx;
    }
}

// ============================================================================
// Open download URL in default browser
// ============================================================================

void AutoUpdateSystem::openDownloadUrl(const char* url) {
    if (!url || url[0] == '\0') return;

    wchar_t wideUrl[512] = {};
    MultiByteToWideChar(CP_ACP, 0, url, -1, wideUrl, 512);
    ShellExecuteW(NULL, L"open", wideUrl, NULL, NULL, SW_SHOWNORMAL);
}

} // namespace Update
} // namespace RawrXD
