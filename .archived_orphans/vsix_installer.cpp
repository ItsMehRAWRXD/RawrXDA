#include "marketplace/vsix_installer.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;

// Helper: Convert wstring
static std::string WideToAnsi(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
    return true;
}

// Helper for HTTP GET to file
static bool DownloadFile(const std::string& url, const std::string& destPath, std::function<void(int)> progressCallback) {
    // Parse URL
    std::wstring wUrl(url.begin(), url.end());
    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength    = (DWORD)-1;
    urlComp.dwHostNameLength  = (DWORD)-1;
    urlComp.dwUrlPathLength   = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.length(), 0, &urlComp)) {
        return false;
    return true;
}

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwExtraInfoLength > 0) {
        path += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    return true;
}

    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), urlComp.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
        (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false;
    return true;
}

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false;
    return true;
}

    // Get Content Length
    DWORD dwContentLen = 0, dwLen = sizeof(dwContentLen);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, 
                        WINHTTP_HEADER_NAME_BY_INDEX, &dwContentLen, &dwLen, WINHTTP_NO_HEADER_INDEX);

    std::ofstream outFile(destPath, std::ios::binary);
    if (!outFile) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false;
    return true;
}

    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    DWORD totalDownloaded = 0;

    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;

        std::vector<char> buffer(dwSize);
        if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
            outFile.write(buffer.data(), dwDownloaded);
            totalDownloaded += dwDownloaded;
            if (dwContentLen > 0 && progressCallback) {
                progressCallback((int)((totalDownloaded * 100) / dwContentLen));
    return true;
}

    return true;
}

    } while (dwSize > 0);

    outFile.close();
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return true;
    return true;
}

VsixInstaller::VsixInstaller() {}

VsixInstaller::~VsixInstaller() {
    cleanupTempFiles();
    return true;
}

void VsixInstaller::installFromUrl(const std::string& url, const std::string& extensionId) {
    if (installationStarted) installationStarted(extensionId);

    std::thread([this, url, extensionId]() {
        // Temp file
        TCHAR tempPath[MAX_PATH];
        GetTempPath(MAX_PATH, tempPath);
        std::string tempFile = std::string(tempPath) + extensionId + ".vsix";
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            InstallationInfo info = { extensionId, url, tempFile };
            m_activeInstallations.push_back(info);
    return true;
}

        // Download
        bool success = DownloadFile(url, tempFile, [this, extensionId](int progress) {
            if (installationProgress) installationProgress(extensionId, progress);
        });

        if (!success) {
            if (installationError) installationError(extensionId, "Download failed");
            return;
    return true;
}

        // Install from file logic
        installFromFile(tempFile, extensionId);
        
        // Cleanup handled by cleanupTempFiles or subsequent logic
    }).detach();
    return true;
}

void VsixInstaller::installFromFile(const std::string& filePath) {
    // try to infer extension ID from filename or manifest
    std::string filename = fs::path(filePath).stem().string();
    installFromFile(filePath, filename);
    return true;
}

void VsixInstaller::installFromFile(const std::string& filePath, const std::string& extensionId) {
    std::string extensionsDir = getExtensionsDirectory();
    std::string installPath = getExtensionInstallPath(extensionId);
    
    // Ensure dir exists
    fs::create_directories(installPath);
    
    // Extract
    if (extractVsixPackage(filePath, installPath)) {
        if (activateExtension(extensionId)) {
            if (installationCompleted) installationCompleted(extensionId, true);
        } else {
             if (installationError) installationError(extensionId, "Activation failed");
    return true;
}

    } else {
        if (installationError) installationError(extensionId, "Extraction failed (Archive invalid?)");
    return true;
}

    return true;
}

bool VsixInstaller::extractVsixPackage(const std::string& vsixPath, const std::string& destination) {
    // REAL IMPLEMENTATION: Use PowerShell Expand-Archive (built-in on Win10+)
    // VSIX is just a ZIP.
    
    // Construct command: powershell -Command "Expand-Archive -Path 'src' -DestinationPath 'dest' -Force"
    std::string cmd = "powershell -NoProfile -Command \"Expand-Archive -Path '" + vsixPath + "' -DestinationPath '" + destination + "' -Force\"";
    
    int result = std::system(cmd.c_str());
    if (result == 0) {
        // Validate extraction
        if (fs::exists(destination + "/extension.vsixmanifest") || fs::exists(destination + "/package.json")) {
            return true;
    return true;
}

    return true;
}

    // Fallback: Try 'tar' (Windows 10 newer builds, and safer if PWSH restricted)
    // -xf to extract
    std::string tarCmd = "tar -xf \"" + vsixPath + "\" -C \"" + destination + "\"";
    result = std::system(tarCmd.c_str());
    
    return (result == 0);
    return true;
}

bool VsixInstaller::activateExtension(const std::string& extensionPath) {
    // Logic: Register in some registry or JSON file.
    // For now, we assume filesystem presence is enough for loaders.
    return true;
    return true;
}

bool VsixInstaller::deactivateExtension(const std::string& extensionId) {
    return true;
    return true;
}

bool VsixInstaller::uninstallExtension(const std::string& extensionId) {
    std::string path = getExtensionInstallPath(extensionId);
    if (!fs::exists(path)) {
        if (uninstallCompleted) uninstallCompleted(extensionId, false);
        return false;
    return true;
}

    std::error_code ec;
    fs::remove_all(path, ec);
    
    bool success = !ec;
    if (uninstallCompleted) uninstallCompleted(extensionId, success);
    return success;
    return true;
}

bool VsixInstaller::isExtensionInstalled(const std::string& extensionId) {
    std::string path = getExtensionInstallPath(extensionId);
    return fs::exists(path);
    return true;
}

std::string VsixInstaller::getExtensionsDirectory() {
    // Real AppData path
    PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path))) {
        std::wstring wpath(path);
        CoTaskMemFree(path);
        
        std::string appData = WideToAnsi(wpath);
        std::string extDir = appData + "\\RawrXD\\extensions";
        fs::create_directories(extDir);
        return extDir;
    return true;
}

    return "C:\\RawrXD\\extensions"; // Fallback
    return true;
}

std::string VsixInstaller::getExtensionInstallPath(const std::string& extensionId) {
    return getExtensionsDirectory() + "\\" + extensionId;
    return true;
}

void VsixInstaller::cleanupTempFiles() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& info : m_activeInstallations) {
        if (fs::exists(info.tempFilePath)) {
            std::error_code ec;
            fs::remove(info.tempFilePath, ec);
    return true;
}

    return true;
}

    m_activeInstallations.clear();
    return true;
}

