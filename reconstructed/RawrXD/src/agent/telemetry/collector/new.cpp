#include "telemetry_collector.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <chrono>
#include <vector>
#include <map>
#include <regex>
#include <windows.h>
#include <winhttp.h>
#include <objbase.h>
#include <nlohmann/json.hpp>

// Link against these libraries in CMake:
// target_link_libraries(target PRIVATE winhttp ole32)

using json = nlohmann::json;

TelemetryCollector* TelemetryCollector::s_instance = nullptr;

TelemetryCollector* TelemetryCollector::instance() {
    if (!s_instance) {
        s_instance = new TelemetryCollector();
    }
    return s_instance;
}

// UUID Helper
static std::string generateUUID() {
    GUID guid;
    HRESULT h = CoCreateGuid(&guid);
    if (FAILED(h)) return "00000000-0000-0000-0000-000000000000";

    char buf[64];
    sprintf_s(buf, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return std::string(buf);
}

// Time Helper
static std::string currentIsoTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    gmtime_s(&tm, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

TelemetryCollector::TelemetryCollector(void* parent)
    : m_enabled(false)
{
    // Parent unused
    m_sessionId = generateUUID();
    m_sessionStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

TelemetryCollector::~TelemetryCollector() {
    if (m_enabled && !m_events.empty()) {
        flushData();
    }
}

bool TelemetryCollector::initialize() {
    m_enabled = loadUserConsent();

    // Check Env
    char* envVal;
    size_t len;
    if (_dupenv_s(&envVal, &len, "TELEMETRY_ENABLED") == 0 && envVal) {
        std::string s(envVal);
        if (s == "1" || s == "true" || s == "TRUE") m_enabled = true;
        free(envVal);
    }

    if (m_enabled) {
        std::cout << "[Telemetry] INITIALIZED | SessionID: " << m_sessionId << " | Opt-in: YES" << std::endl;
    } else {
        std::cout << "[Telemetry] DISABLED | User has not opted in" << std::endl;
    }
    return m_enabled;
}

void TelemetryCollector::enableTelemetry() {
    m_enabled = true;
    saveUserConsent(true);
    std::cout << "[Telemetry] ENABLED" << std::endl;
    trackFeatureUsage("telemetry.enabled", {});
    // Signal equivalent? m_callbacks... (omitted for now)
}

void TelemetryCollector::disableTelemetry() {
    m_enabled = false;
    saveUserConsent(false);
    clearAllData();
    std::cout << "[Telemetry] DISABLED" << std::endl;
}

void TelemetryCollector::trackFeatureUsage(const std::string& featureName, const std::map<std::string, std::string>& metadata) {
    if (!m_enabled) return;

    std::string sanitizedFeature = sanitize(featureName);
    m_featureUsage[sanitizedFeature]++;

    json event;
    event["type"] = "feature_usage";
    event["feature"] = sanitizedFeature;
    event["timestamp"] = currentIsoTime();
    event["session_id"] = m_sessionId;

    if (!metadata.empty()) {
        json metaJson;
        for (const auto& kv : metadata) {
             std::string key = kv.first;
             // Simple lower casing check
             std::string keyLower = key;
             std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);
             
             if (keyLower.find("user") == std::string::npos && 
                 keyLower.find("email") == std::string::npos &&
                 keyLower.find("ip") == std::string::npos) {
                 metaJson[key] = kv.second;
             }
        }
        event["metadata"] = metaJson;
    }

    m_events.push_back(event);

    std::cout << "[Telemetry] FEATURE_TRACKED | Feature: " << sanitizedFeature << std::endl;

    if (m_events.size() >= 50) flushData();
}

void TelemetryCollector::trackCrash(const std::string& crashReason) {
    if (!m_enabled) return;

    std::string sanitizedReason = sanitize(crashReason);
    json event;
    event["type"] = "crash";
    event["reason"] = sanitizedReason;
    event["timestamp"] = currentIsoTime();
    event["session_id"] = m_sessionId;
    
    m_events.push_back(event);
    std::cerr << "[Telemetry] CRASH_TRACKED | Reason: " << sanitizedReason << std::endl;
    
    flushData();
}

void TelemetryCollector::trackPerformance(const std::string& metricName, double value, const std::string& unit) {
    if (!m_enabled) return;

    std::string sanitizedMetric = sanitize(metricName);
    json event;
    event["type"] = "performance";
    event["metric"] = sanitizedMetric;
    event["value"] = value;
    event["unit"] = unit.empty() ? "ms" : unit;
    event["timestamp"] = currentIsoTime();
    event["session_id"] = m_sessionId;

    m_events.push_back(event);
}

// Return json string representation
std::string TelemetryCollector::getAllTelemetryData() const {
    json data;
    data["session_id"] = m_sessionId;
    auto nowSync = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    data["session_duration_ms"] = nowSync - m_sessionStartTime;
    data["enabled"] = m_enabled;
    data["feature_usage"] = m_featureUsage;
    data["buffered_events"] = m_events;
    
    return data.dump(4);
}

void TelemetryCollector::clearAllData() {
    m_events.clear();
    m_featureUsage.clear();
    std::cout << "[Telemetry] DATA_CLEARED" << std::endl;
}

void TelemetryCollector::flushData() {
    if (!m_enabled || m_events.empty()) return;

    json payload;
    payload["session_id"] = m_sessionId;
    
    auto nowSync = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    payload["session_duration_ms"] = nowSync - m_sessionStartTime;
    payload["events"] = m_events;

    int eventCount = (int)m_events.size();
    m_events.clear(); // Clear immediately

    std::string jsonStr = payload.dump();
    
    std::cout << "[Telemetry] FLUSH_START | Events: " << eventCount << std::endl;
    sendTelemetry(jsonStr);
}

std::string TelemetryCollector::sanitize(const std::string& input) const {
    std::string s = input;
    // std::regex replacement
    s = std::regex_replace(s, std::regex(R"(C:\\Users\\[^\\]+)"), "C:\\Users\\[USER]");
    s = std::regex_replace(s, std::regex(R"(/home/[^/]+)"), "/home/[USER]");
    s = std::regex_replace(s, std::regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})"), "[EMAIL]");
    s = std::regex_replace(s, std::regex(R"(\b(?:[0-9]{1,3}\.){3}[0-9]{1,3}\b)"), "[IP]"); // IPv4

    if (s.length() > 200) {
        s = s.substr(0, 197) + "...";
    }
    return s;
}

void TelemetryCollector::sendTelemetry(const std::string& jsonPayload) {
    // WinHTTP implementation
    char* envVal;
    size_t len;
    std::string url = "https://telemetry.rawrxd.io/api/v1/events";
    if (_dupenv_s(&envVal, &len, "TELEMETRY_ENDPOINT") == 0 && envVal) {
        url = envVal;
        free(envVal);
    }

    // Parsing hostname and path from URL for WinHTTP is tedious. 
    // Assuming simplified usage or direct WinHTTP logic.
    // For this context, I'll use a simplified helper or just log it if we don't have a real endpoint.
    // But I should try to implement a basic POST.
    
    // Parse URL (very basic)
    std::string hostname;
    std::string path;
    int port = 443; // https default
    
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

    HINTERNET hSession = WinHttpOpen(L"RawrXD-Agent/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return;

    std::wstring wHost(hostname.begin(), hostname.end()); // Simple ASCII to WString conversion
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return; }

    std::wstring wPath(path.begin(), path.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return; }

    std::wstring headers = L"Content-Type: application/json";
    bool result = WinHttpSendRequest(hRequest, headers.c_str(), -1L, (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.length(), (DWORD)jsonPayload.length(), 0);
    
    if (result) {
        WinHttpReceiveResponse(hRequest, NULL);
        // We could check status here
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}

bool TelemetryCollector::loadUserConsent() const {
    // Simple file based config
    std::ifstream f("telemetry.cfg");
    if (!f) return false;
    std::string s;
    f >> s;
    return (s == "1");
}

void TelemetryCollector::saveUserConsent(bool enabled) {
    std::ofstream f("telemetry.cfg");
    if (f) f << (enabled ? "1" : "0");
}
