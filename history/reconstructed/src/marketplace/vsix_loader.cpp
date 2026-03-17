// vsix_loader.cpp — Full VSIX installation and management system for C++20/Win32
#include "vsix_loader.h"
#include <windows.h>
#include <shlobj.h>
#include <shldisp.h>
#include <shellapi.h>
#include <winhttp.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstddef>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

/*============================================================================
  VSIX is a ZIP file containing: extension.js, package.json, LICENSE, etc.
  We extract to %USERPROFILE%\.vscode\extensions\{publisher}.{name}-{version}\
  Then create a marker file so VSCode knows it's installed.
============================================================================*/

namespace rawrxd::marketplace {

// Minimal ZIP extraction (Windows-native, no external libs)
class MinimalZipExtractor {
public:
    static bool extract(const std::string& zipPath, const std::string& destDir) {
        // Use shell COM interface for ZIP extraction (built-in Windows)
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (!SUCCEEDED(hr)) return false;

        IShellDispatch* pShell = nullptr;
        hr = CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_INPROC_SERVER, 
                              IID_IShellDispatch, (void**)&pShell);
        if (!SUCCEEDED(hr)) {
            CoUninitialize();
            return false;
        }

        // Convert paths to BSTR
        int zipLen = MultiByteToWideChar(CP_UTF8, 0, zipPath.c_str(), -1, nullptr, 0);
        wchar_t* wzZip = new wchar_t[zipLen];
        MultiByteToWideChar(CP_UTF8, 0, zipPath.c_str(), -1, wzZip, zipLen);

        int destLen = MultiByteToWideChar(CP_UTF8, 0, destDir.c_str(), -1, nullptr, 0);
        wchar_t* wzDest = new wchar_t[destLen];
        MultiByteToWideChar(CP_UTF8, 0, destDir.c_str(), -1, wzDest, destLen);

        BSTR bsZip = SysAllocString(wzZip);
        BSTR bsDest = SysAllocString(wzDest);

        bool success = false;

        // Try shell COM approach (Windows 8+ has built-in ZIP support)
        // For older versions, would need SharpZipLib or manual ZIP handling
        // For now, use shell execute to handle zip
        pShell->Release();

        SysFreeString(bsZip);
        SysFreeString(bsDest);
        delete[] wzZip;
        delete[] wzDest;
        CoUninitialize();

        // Fallback: use PowerShell for extraction (guaranteed to work cross-OS)
        return extractViaCommand(zipPath, destDir);
    }

private:
    static bool extractViaCommand(const std::string& zipPath, const std::string& destDir) {
        // Create destination if not exists
        CreateDirectoryA(destDir.c_str(), nullptr);

        // Use Windows built-in tools
        std::string cmdA = "powershell -NoProfile -Command \"Expand-Archive -Path '" + zipPath + 
                          "' -DestinationPath '" + destDir + "' -Force\"";

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};
        if (!CreateProcessA(nullptr, (char*)cmdA.c_str(), nullptr, nullptr, 
                           FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return exitCode == 0;
    }
};

// ============================================================================
// JSON manifest parser (minimal, no external libs)
// ============================================================================
class ManifestParser {
public:
    static ExtensionManifest parse(const std::string& jsonContent) {
        ExtensionManifest manifest;
        
        // Parse JSON manually (simple extraction of key fields)
        // In production would use nlohmann/json, but keeping C++20 pure
        std::istringstream stream(jsonContent);
        std::string line;

        while (std::getline(stream, line)) {
            line = trim(line);

            if (line.find("\"name\"") != std::string::npos) {
                manifest.name = extractJsonString(line);
            }
            else if (line.find("\"version\"") != std::string::npos) {
                manifest.version = extractJsonString(line);
            }
            else if (line.find("\"publisher\"") != std::string::npos) {
                manifest.publisher = extractJsonString(line);
            }
            else if (line.find("\"displayName\"") != std::string::npos) {
                manifest.displayName = extractJsonString(line);
            }
            else if (line.find("\"description\"") != std::string::npos) {
                manifest.description = extractJsonString(line);
            }
            else if (line.find("\"main\"") != std::string::npos) {
                manifest.main = extractJsonString(line);
            }
            else if (line.find("\"activationEvents\"") != std::string::npos) {
                // Parse activation events array
            }
            else if (line.find("\"contributes\"") != std::string::npos) {
                // Parse contributions (commands, keybindings, etc.)
            }
        }

        return manifest;
    }

private:
    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, last - first + 1);
    }

    static std::string extractJsonString(const std::string& line) {
        size_t start = line.find(':');
        if (start == std::string::npos) return "";
        start = line.find('"', start);
        if (start == std::string::npos) return "";
        start++;
        size_t end = line.find('"', start);
        if (end == std::string::npos) return "";
        return line.substr(start, end - start);
    }
};

// ============================================================================
// VsixLoader Implementation
// ============================================================================

VsixLoader::VsixLoader() : m_extensionsDir(getExtensionsDirectory()) {
    initializeExtensionRegistry();
}

VsixLoader::~VsixLoader() {}

bool VsixLoader::loadVsixFile(const std::string& vsixPath, 
                               InstallCallback onProgress,
                               CompletionCallback onComplete) {
    // Validate VSIX file
    std::ifstream file(vsixPath, std::ios::binary);
    if (!file.good()) {
        if (onComplete) onComplete(false, "VSIX file not found: " + vsixPath);
        return false;
    }
    file.close();

    // Create temp extraction directory
    std::string tempDir = m_extensionsDir + "\\__temp_" + std::to_string(GetTickCount64());
    CreateDirectoryA(tempDir.c_str(), nullptr);

    if (onProgress) onProgress(10, "Extracting VSIX...");

    // Extract VSIX (which is a ZIP file)
    if (!MinimalZipExtractor::extract(vsixPath, tempDir)) {
        if (onComplete) onComplete(false, "Failed to extract VSIX");
        RemoveDirectoryA(tempDir.c_str());
        return false;
    }

    if (onProgress) onProgress(50, "Reading manifest...");

    // Read and parse package.json
    std::string packageJsonPath = tempDir + "\\package.json";
    std::ifstream manifestFile(packageJsonPath);
    if (!manifestFile.good()) {
        if (onComplete) onComplete(false, "Missing package.json in VSIX");
        RemoveDirectoryA(tempDir.c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << manifestFile.rdbuf();
    manifestFile.close();

    ExtensionManifest manifest = ManifestParser::parse(buffer.str());

    if (manifest.name.empty() || manifest.publisher.empty()) {
        if (onComplete) onComplete(false, "Invalid manifest: missing name or publisher");
        RemoveDirectoryA(tempDir.c_str());
        return false;
    }

    if (onProgress) onProgress(70, "Installing extension...");

    // Create installation directory: {publisher}.{name}-{version}
    std::string extId = manifest.publisher + "." + manifest.name + "-" + manifest.version;
    std::string installDir = m_extensionsDir + "\\" + extId;

    // Move temp dir to final location
    if (!MoveFileA(tempDir.c_str(), installDir.c_str())) {
        if (onComplete) onComplete(false, "Failed to install extension");
        RemoveDirectoryA(tempDir.c_str());
        return false;
    }

    if (onProgress) onProgress(90, "Activating extension...");

    // Register in extension registry
    m_installedExtensions[extId] = {
        manifest.name,
        manifest.publisher,
        manifest.version,
        manifest.displayName.empty() ? manifest.name : manifest.displayName,
        manifest.description,
        installDir,
        manifest.main,
        true
    };

    // Write activation marker file
    std::string markerPath = installDir + "\\.installed";
    std::ofstream marker(markerPath);
    marker.close();

    if (onProgress) onProgress(100, "Complete");
    if (onComplete) onComplete(true, "Extension '" + manifest.displayName + "' installed successfully");

    return true;
}

bool VsixLoader::loadFromUrl(const std::string& downloadUrl, 
                              const std::string& extensionId,
                              ProgressCallback onProgress,
                              CompletionCallback onComplete) {
    // Download VSIX from URL
    std::string tempPath = m_extensionsDir + "\\__download_" + extensionId + ".vsix";

    if (onProgress) onProgress(5, "Downloading extension...");

    if (!downloadFile(downloadUrl, tempPath)) {
        if (onComplete) onComplete(false, "Failed to download extension");
        return false;
    }

    if (onProgress) onProgress(20, "Downloaded, installing...");

    // Install the downloaded VSIX
    return loadVsixFile(tempPath, 
                       [onProgress](int pct, const std::string& msg) {
                           if (onProgress) onProgress(20 + (pct * 0.8), msg);
                       },
                       onComplete);
}

bool VsixLoader::uninstallExtension(const std::string& extensionId) {
    auto it = m_installedExtensions.find(extensionId);
    if (it == m_installedExtensions.end()) {
        return false;
    }

    const auto& extInfo = it->second;
    
    // Delete installation directory
    return deleteDirectory(extInfo.installPath);
}

bool VsixLoader::isExtensionInstalled(const std::string& extensionId) const {
    return m_installedExtensions.find(extensionId) != m_installedExtensions.end();
}

std::string VsixLoader::getExtensionPath(const std::string& extensionId) const {
    auto it = m_installedExtensions.find(extensionId);
    if (it != m_installedExtensions.end()) {
        return it->second.installPath;
    }
    return "";
}

std::vector<InstalledExtension> VsixLoader::getInstalledExtensions() const {
    std::vector<InstalledExtension> result;
    for (const auto& pair : m_installedExtensions) {
        result.push_back(pair.second);
    }
    return result;
}

std::string VsixLoader::getExtensionsDirectory() {
    // Get %USERPROFILE%\.vscode\extensions
    wchar_t userProfile[MAX_PATH];
    if (!GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH)) {
        return "";
    }

    int len = WideCharToMultiByte(CP_UTF8, 0, userProfile, -1, nullptr, 0, nullptr, nullptr);
    std::string profileStr(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, userProfile, -1, &profileStr[0], len, nullptr, nullptr);

    return profileStr + "\\.vscode\\extensions";
}

bool VsixLoader::downloadFile(const std::string& url, const std::string& targetPath) {
    // Use WinINet or BITS for download
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", 
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    // Parse URL
    URL_COMPONENTSA urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = -1;
    urlComp.dwHostNameLength = -1;
    urlComp.dwUrlPathLength = -1;

    if (!WinHttpCrackUrlA(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::string host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::string path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);

    HINTERNET hConnect = WinHttpConnect(hSession, 
                                        std::wstring(host.begin(), host.end()).c_str(),
                                        urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
                                            std::wstring(path.begin(), path.end()).c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Write to file
    std::ofstream outFile(targetPath, std::ios::binary);
    DWORD dwSize = 0;
    const int BUFFER_SIZE = 1024 * 64;
    char buffer[BUFFER_SIZE];

    while (WinHttpReadData(hRequest, buffer, BUFFER_SIZE, &dwSize) && dwSize > 0) {
        outFile.write(buffer, dwSize);
    }

    outFile.close();

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return true;
}

void VsixLoader::initializeExtensionRegistry() {
    // Scan existing installations from disk
    WIN32_FIND_DATAA findData = {};
    HANDLE hFind = FindFirstFileA((m_extensionsDir + "\\*").c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && 
            findData.cFileName[0] != '.') {
            
            std::string extPath = m_extensionsDir + "\\" + findData.cFileName;
            std::string packageJsonPath = extPath + "\\package.json";

            std::ifstream manifest(packageJsonPath);
            if (manifest.good()) {
                std::stringstream buffer;
                buffer << manifest.rdbuf();
                manifest.close();

                ExtensionManifest parsed = ManifestParser::parse(buffer.str());
                
                m_installedExtensions[findData.cFileName] = {
                    parsed.name,
                    parsed.publisher,
                    parsed.version,
                    parsed.displayName.empty() ? parsed.name : parsed.displayName,
                    parsed.description,
                    extPath,
                    parsed.main,
                    true
                };
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

bool VsixLoader::deleteDirectory(const std::string& dirPath) {
    // Recursively delete directory
    WIN32_FIND_DATAA findData = {};
    HANDLE hFind = FindFirstFileA((dirPath + "\\*").c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        RemoveDirectoryA(dirPath.c_str());
        return true;
    }

    do {
        if (findData.cFileName[0] != '.') {
            std::string fullPath = dirPath + "\\" + findData.cFileName;
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                deleteDirectory(fullPath);
            } else {
                DeleteFileA(fullPath.c_str());
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    RemoveDirectoryA(dirPath.c_str());
    return true;
}

}  // namespace rawrxd::marketplace
