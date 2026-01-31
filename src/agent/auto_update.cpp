#include "auto_update.hpp"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include "release_agent.hpp" // Utilizing existing WinHTTP helpers if possible, or reimplementing simple ones

#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;
using json = nlohmann::json;

// Helper function to perform GET request using WinHTTP
// Note: This is a simplified version. For production, error handling should be more robust.
static std::vector<uint8_t> httpGet(const std::string& url, int& statusCode) {
    std::vector<uint8_t> responseData;
    statusCode = 0;

    URL_COMPONENTS urlComponents;
    ZeroMemory(&urlComponents, sizeof(urlComponents));
    urlComponents.dwStructSize = sizeof(urlComponents);
    
    wchar_t hostName[256];
    wchar_t urlPath[1024];
    
    urlComponents.lpszHostName = hostName;
    urlComponents.dwHostNameLength = 256;
    urlComponents.lpszUrlPath = urlPath;
    urlComponents.dwUrlPathLength = 1024;
    
    std::wstring wUrl(url.begin(), url.end());
    
    if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.length(), 0, &urlComponents)) {
        return {};
    }

    HINTERNET hSession = WinHttpOpen(L"RawrXD-AutoUpdate/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return {};

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComponents.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return {};
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
        (urlComponents.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        if (WinHttpReceiveResponse(hRequest, NULL)) {
            DWORD dwStatusCode = 0;
            DWORD dwSize = sizeof(dwStatusCode);
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, 
                WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
            statusCode = static_cast<int>(dwStatusCode);
            
            DWORD dwAvailable = 0;
            while (WinHttpQueryDataAvailable(hRequest, &dwAvailable) && dwAvailable > 0) {
                std::vector<uint8_t> chunk(dwAvailable);
                DWORD dwRead = 0;
                if (WinHttpReadData(hRequest, chunk.data(), dwAvailable, &dwRead)) {
                    responseData.insert(responseData.end(), chunk.begin(), chunk.begin() + dwRead);
                }
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return responseData;
}

// Helper to get environment variable
static std::string getEnv(const std::string& name) {
    char* val = nullptr;
    size_t len = 0;
    _dupenv_s(&val, &len, name.c_str());
    if (val && len > 0) {
        std::string s(val);
        free(val);
        return s;
    }
    return "";
}

static std::string getUpdateURL() {
    std::string envUrl = getEnv("RAWRXD_UPDATE_URL");
    return envUrl.empty() 
        ? "https://rawrxd.blob.core.windows.net/updates/update_manifest.json"
        : envUrl;
}

static void logUpdateEvent(const std::string& event, const std::string& detail = "", int64_t latencyMs = -1) {
    std::cout << "[AutoUpdate] " << event;
    if (!detail.empty()) {
        std::cout << " | Detail: " << detail;
    }
    if (latencyMs >= 0) {
        std::cout << " | Latency: " << latencyMs << "ms";
    }
    std::cout << std::endl;
}

// Compute SHA256 of data using BCrypt
static std::string computeSHA256(const std::vector<uint8_t>& data) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    DWORD cbHash = 0;
    DWORD cbData = 0;
    
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) {
        return "";
    }
    
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);
    
    std::vector<BYTE> hash(cbHash);
    
    if (BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0) == 0) {
        BCryptHashData(hHash, (PBYTE)data.data(), (ULONG)data.size(), 0);
        BCryptFinishHash(hHash, hash.data(), cbHash, 0);
        BCryptDestroyHash(hHash);
    }
    
    BCryptCloseAlgorithmProvider(hAlg, 0);
    
    std::stringstream ss;
    for (size_t i = 0; i < std::min(size_t(32), hash.size()); ++i) { // SHA256 is 32 bytes
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}

bool AutoUpdate::checkAndInstall() {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Feature toggle check via config file or env? Using simple env for now as settings replacement
    // or checking a config json if exists.
    // For now assuming enabled unless disabled by env
    if (getEnv("RAWRXD_AUTO_UPDATE_DISABLED") == "1") {
        logUpdateEvent("SKIPPED", "Auto-update disabled in environment");
        return true;
    }
    
    std::string updateUrl = getUpdateURL();
    
    logUpdateEvent("CHECK_START", "URL: " + updateUrl);
    
    int statusCode = 0;
    std::vector<uint8_t> data = httpGet(updateUrl, statusCode);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    if (statusCode != 200 || data.empty()) {
        logUpdateEvent("CHECK_FAILED", "Status: " + std::to_string(statusCode), latency);
        return false;
    }

    json root;
    try {
        root = json::parse(data);
    } catch (...) {
        logUpdateEvent("CHECK_FAILED", "Invalid JSON", latency);
        return false;
    }
    
    std::string remoteVer = root.value("version", "");
    std::string remoteURL = root.value("url", "");
    std::string remoteSHA = root.value("sha256", "");
    
    // Get local version - no QCoreApplication... assume hardcoded or from macro
    // Using a placeholder or macro if available.
    // In agent_main.cpp (if I read it) it might have version.
    std::string localVer = "1.0.0"; // Fallback
#ifdef RAWRXD_VERSION
    localVer = RAWRXD_VERSION;
#endif
    
    logUpdateEvent("VERSION_CHECK", "Local: " + localVer + ", Remote: " + remoteVer, latency);
    
    if (remoteVer == localVer) {
        logUpdateEvent("UP_TO_DATE", remoteVer);
        return true;
    }
    
    // Determine path
    std::string localPath;
    char* appData = nullptr;
    size_t len = 0;
    _dupenv_s(&appData, &len, "APPDATA");
    if (appData && len > 0) {
        localPath = std::string(appData) + "\\RawrXD\\updates\\RawrXD-QtShell-" + remoteVer + ".exe";
        free(appData);
    } else {
        localPath = "updates/RawrXD-Shell-" + remoteVer + ".exe";
    }
    
    fs::create_directories(fs::path(localPath).parent_path());
    
    start = std::chrono::high_resolution_clock::now();
    logUpdateEvent("DOWNLOAD_START", "Version: " + remoteVer + ", URL: " + remoteURL);
    
    data = httpGet(remoteURL, statusCode);
    
    end = std::chrono::high_resolution_clock::now();
    latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    if (statusCode != 200 || data.empty()) {
        logUpdateEvent("DOWNLOAD_FAILED", "Status: " + std::to_string(statusCode), latency);
        return false;
    }
    
    // Check integrity
    std::string sha256 = computeSHA256(data);
    if (sha256 != remoteSHA) {
        logUpdateEvent("INTEGRITY_FAILED", "Expected: " + remoteSHA + ", Got: " + sha256, latency);
        return false;
    }
    
    logUpdateEvent("INTEGRITY_OK", "SHA256: " + sha256);
    
    // Write file
    std::ofstream f(localPath, std::ios::binary);
    if (!f) {
        logUpdateEvent("WRITE_FAILED", "Path: " + localPath);
        return false;
    }
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
    f.close();
    
    logUpdateEvent("DOWNLOAD_COMPLETE", "Path: " + localPath + ", Size: " + std::to_string(data.size()) + " bytes", latency);
    
    // Restart logic
    logUpdateEvent("RESTART_SCHEDULED", "New version: " + remoteVer + ", Delay: 3s");
    
    std::string cmd = "timeout /t 3 && \"" + localPath + "\"";
    
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    
    char* cmdLine = _strdup(("cmd.exe /C " + cmd).c_str());
    if (CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmdLine);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        // Use ExitProcess called from caller or handled gracefully
        exit(0);
    }
    free(cmdLine);
    
    return true;
}
