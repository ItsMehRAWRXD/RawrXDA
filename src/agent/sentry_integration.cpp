#include "sentry_integration.hpp"
#include <iostream>
#include <chrono>
#include <windows.h>
#include <winhttp.h>
#include <objbase.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ole32.lib")

using json = nlohmann::json;

SentryIntegration* SentryIntegration::s_instance = nullptr;

SentryIntegration* SentryIntegration::instance() {
    if (!s_instance) {
        s_instance = new SentryIntegration();
    }
    return s_instance;
}

static std::string genUuid() {
    GUID guid;
    CoCreateGuid(&guid);
    char buf[64];
    sprintf_s(buf, "%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x", 
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return std::string(buf);
}

static std::string nowIso() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    gmtime_s(&tm, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

static std::string getEnvVar(const std::string& name) {
    char* val = nullptr;
    size_t len = 0;
    if (_dupenv_s(&val, &len, name.c_str()) == 0 && val) {
        std::string s(val);
        free(val);
        return s;
    }
    return "";
}

SentryIntegration::SentryIntegration() {}

SentryIntegration::~SentryIntegration() {}

bool SentryIntegration::initialize() {
    m_dsn = getEnvVar("SENTRY_DSN");
    if (m_dsn.empty()) return false;

    // Parse DSN: https://public@sentry.example.com/1
    // Simplified parsing
    size_t atPos = m_dsn.find('@');
    size_t slashPos = m_dsn.rfind('/');
    if (atPos == std::string::npos || slashPos == std::string::npos) return false;

    std::string proto = m_dsn.substr(0, m_dsn.find("://") + 3);
    m_publicKey = m_dsn.substr(proto.length(), atPos - proto.length());
    std::string host = m_dsn.substr(atPos + 1, slashPos - (atPos + 1));
    m_projectId = m_dsn.substr(slashPos + 1);
    
    // Construct endpoint
    // https://sentry.io/api/PROJECT_ID/store/
    m_sentryEndpoint = "https://" + host + "/api/" + m_projectId + "/store/";

    m_initialized = true;
    addBreadcrumb("Sentry initialized", "sentry");
    return true;
}

void SentryIntegration::captureException(const std::string& exception, const json& context) {
    if (!m_initialized) return;

    json event;
    event["event_id"] = genUuid();
    event["timestamp"] = nowIso();
    event["level"] = "error";
    event["platform"] = "native";
    event["sdk"] = { {"name", "rawrxd-sentry"}, {"version", "1.0"} };
    
    json excData;
    excData["type"] = "Exception";
    excData["value"] = exception;
    if (!context.empty()) excData["context"] = context;

    event["exception"] = { {"values", {excData}} };

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_breadcrumbs.empty()) {
         event["breadcrumbs"] = { {"values", m_breadcrumbs} };
    }

    event["environment"] = getEnvVar("RAWRXD_ENV").empty() ? "production" : getEnvVar("RAWRXD_ENV");

    sendEvent(event);
}

void SentryIntegration::captureMessage(const std::string& message, const std::string& level) {
    if (!m_initialized) return;
    
    json event;
    event["event_id"] = genUuid();
    event["timestamp"] = nowIso();
    event["level"] = level;
    event["platform"] = "native";
    event["message"] = message;
    
    sendEvent(event);
}

void SentryIntegration::addBreadcrumb(const std::string& message, const std::string& category, const std::string& level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    json bc;
    bc["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / 1000.0;
    bc["message"] = message;
    bc["category"] = category;
    bc["level"] = level;
    
    m_breadcrumbs.push_back(bc);
    if (m_breadcrumbs.size() > 50) m_breadcrumbs.erase(m_breadcrumbs.begin());
}

void SentryIntegration::sendEvent(const json& event) {
    // WinHTTP Post
    // Parsing m_sentryEndpoint again to get host/path
    std::string url = m_sentryEndpoint;
    std::string hostname;
    std::string path;
    int port = 443;

    size_t schemeEnd = url.find("://");
    std::string bareUrl = (schemeEnd != std::string::npos) ? url.substr(schemeEnd + 3) : url;
    size_t pathStart = bareUrl.find("/");
    if (pathStart != std::string::npos) {
        hostname = bareUrl.substr(0, pathStart);
        path = bareUrl.substr(pathStart);
    } else {
        hostname = bareUrl;
        path = "/";
    }

    HINTERNET hSession = WinHttpOpen(L"RawrXD-Sentry/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return;

    std::wstring wHost(hostname.begin(), hostname.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return; }

    std::wstring wPath(path.begin(), path.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return; }

    // X-Sentry-Auth
    std::string authHeader = "X-Sentry-Auth: Sentry sentry_version=7, sentry_key=" + m_publicKey + ", sentry_client=rawrxd-sentry/1.0";
    std::wstring wAuth(authHeader.begin(), authHeader.end());
    WinHttpAddRequestHeaders(hRequest, wAuth.c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD);

    std::string body = event.dump();
    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.c_str(), (DWORD)body.length(), (DWORD)body.length(), 0);
    WinHttpReceiveResponse(hRequest, NULL);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}
