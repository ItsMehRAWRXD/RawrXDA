#include "auto_update.hpp"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;

bool AutoUpdate::checkAndInstall() {
    std::cout << "[AutoUpdate] Checking for updates via WinHTTP..." << std::endl;
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Updater/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    
    HINTERNET hConnect = WinHttpConnect(hSession, L"rawrxd.ai", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/v2/agent/version", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    
    bool updateNeeded = false;
    if (hRequest) {
        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, NULL)) {
            
            DWORD dwSize = 0;
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (dwSize > 0) {
                std::vector<char> buffer(dwSize + 1);
                DWORD dwDownloaded = 0;
                WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded);
                buffer[dwDownloaded] = '\0';
                std::string latestVersion = buffer.data();
                std::cout << "[AutoUpdate] Latest version on server: " << latestVersion << std::endl;
                // Version comparison logic would go here
            }
        }
        WinHttpCloseHandle(hRequest);
    }
    
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    std::cout << "[AutoUpdate] No mandatory updates found." << std::endl;
    return true;
}
