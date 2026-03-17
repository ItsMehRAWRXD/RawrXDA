#include "auto_update.hpp"
#include <windows.h>
#include <wininet.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>

#pragma comment(lib, "wininet.lib")

using json = nlohmann::json;

static std::string getUpdateURL() {
    const char* envUrl = std::getenv("RAWRXD_UPDATE_URL");
    return envUrl && envUrl[0] != '\0' 
        ? std::string(envUrl)
        : "https://rawrxd.blob.core.windows.net/updates/update_manifest.json";
}


static void logUpdateEvent(const std::string& event, const std::string& detail = std::string(), int64_t latencyMs = -1) {
    // Silent logging - no instrumentation
}

bool AutoUpdate::checkAndInstall() {
    auto timer_start = std::chrono::steady_clock::now();
    
    // Check if auto-update is enabled via environment variable
    const char* enabled = std::getenv("RAWRXD_AUTO_UPDATE_ENABLED");
    if (enabled && std::string(enabled) == "false") {
        logUpdateEvent("SKIPPED", "Auto-update disabled");
        return true;
    }
    
    std::string updateUrl = getUpdateURL();
    
    // WinINet request for update manifest
    HINTERNET hSession = InternetOpenA("RawrXD/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hSession) {
        logUpdateEvent("SESSION_FAILED");
        return false;
    }
    
    HINTERNET hConnect = InternetOpenUrlA(hSession, updateUrl.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect) {
        InternetCloseHandle(hSession);
        logUpdateEvent("CHECK_FAILED", "Could not open URL");
        return false;
    }
    
    // Read response
    char buffer[8192];
    DWORD bytesRead = 0;
    std::string response;
    
    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }
    
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hSession);
    
    auto checkLatency = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - timer_start).count();
    
    // Parse JSON response
    try {
        json manifest = json::parse(response);
        
        std::string remoteVer = manifest.value("version", "");
        std::string remoteURL = manifest.value("url", "");
        
        if (remoteVer.empty() || remoteURL.empty()) {
            logUpdateEvent("INVALID_MANIFEST");
            return false;
        }
        
        // Compare versions
        std::string localVer = "1.0.0";  // Placeholder
        
        logUpdateEvent("VERSION_CHECK", "Local: " + localVer + ", Remote: " + remoteVer, checkLatency);
        
        if (remoteVer == localVer) {
            logUpdateEvent("UP_TO_DATE", remoteVer);
            return true;
        }
        
        logUpdateEvent("UPDATE_AVAILABLE", remoteVer);
        return true;
        
    } catch (const std::exception& e) {
        logUpdateEvent("JSON_PARSE_FAILED", e.what());
        return false;
    }
}








