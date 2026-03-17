#include "telemetry_collector.hpp"
#include "license_enforcement.h"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <chrono>
#include <random>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

TelemetryCollector& TelemetryCollector::instance() {
    static TelemetryCollector s_instance;
    return s_instance;
}

bool TelemetryCollector::initialize() {
    char buf[256];
    if (GetEnvironmentVariableA("TELEMETRY_ENABLED", buf, sizeof(buf)) > 0) {
        m_enabled = (std::string(buf) == "1");
    } else {
        m_enabled = loadUserConsent();
    }
    
    if (m_enabled) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::stringstream ss;
        for (int i = 0; i < 32; i++) ss << std::hex << dis(gen);
        m_sessionId = ss.str();
        m_sessionStartTime = std::chrono::system_clock::now().time_since_epoch().count();
        std::cout << "[Telemetry] Initialized (Anonymous session: " << m_sessionId << ")" << std::endl;
    }
    
    return m_enabled;
}

void TelemetryCollector::trackFeatureUsage(const std::string& featureName, const nlohmann::json& metadata) {
    if (!m_enabled) return;
    m_featureUsage[featureName]++;
    nlohmann::json event = {
        {"type", "feature"},
        {"name", featureName},
        {"metadata", metadata},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
    m_events.push_back(event);
}

void TelemetryCollector::sendTelemetry(const nlohmann::json& payload) {
    if (!m_enabled) return;
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Telemetry/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return;
    
    HINTERNET hConnect = WinHttpConnect(hSession, L"telemetry.rawrxd.ai", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConnect) {
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/v1/telemetry", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (hRequest) {
            std::string data = payload.dump();
            WinHttpSendRequest(hRequest, L"Content-Type: application/json", -1, (LPVOID)data.c_str(), (DWORD)data.size(), (DWORD)data.size(), 0);
            WinHttpReceiveResponse(hRequest, NULL);
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
}

bool TelemetryCollector::loadUserConsent() const {
    // Check registry for consent
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\RawrXD", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 0;
        DWORD size = sizeof(value);
        RegQueryValueExA(hKey, "TelemetryConsent", NULL, NULL, (LPBYTE)&value, &size);
        RegCloseKey(hKey);
        return (value == 1);
    }
    return false;
}

void TelemetryCollector::saveUserConsent(bool enabled) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\RawrXD", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = enabled ? 1 : 0;
        RegSetValueExA(hKey, "TelemetryConsent", 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}
