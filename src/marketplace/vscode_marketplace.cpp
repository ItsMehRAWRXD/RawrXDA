#include "vscode_marketplace.hpp"
#include <string>
#include <vector>
#include <windows.h>
#include <winhttp.h>
#include <fstream>

#pragma comment(lib, "winhttp.lib")

namespace VSCodeMarketplace {

struct MarketplaceEntry {
    std::string name;
    std::string publisher;
    std::string version;
    std::string description;
    std::string downloadUrl;
    int installCount;
    float rating;
};

bool Query(const std::string& searchTerm, int page, int pageSize, 
           std::vector<MarketplaceEntry>& results) {
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD-IDE/1.0", 
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return false;
    
    // Microsoft Marketplace API endpoint
    HINTERNET hConnect = WinHttpConnect(hSession, 
        L"marketplace.visualstudio.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    std::wstring path = L"/_apis/public/gallery/extensionquery";
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), 
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
        WINHTTP_FLAG_SECURE);
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Simplified query - real implementation would build proper JSON
    std::string body = "{\"filters\":[{\"criteria\":[{\"filterType\":8,\"value\":\"" + 
                       searchTerm + "\"}],\"pageNumber\":" + std::to_string(page) + 
                       ",\"pageSize\":" + std::to_string(pageSize) + "}]}";
    
    std::wstring headers = L"Content-Type: application/json\r\nAccept: application/json;api-version=3.0-preview.1";
    BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), -1, 
        (LPVOID)body.c_str(), (DWORD)body.length(), (DWORD)body.length(), 0);
    
    if (bResults) {
        WinHttpReceiveResponse(hRequest, NULL);
        
        DWORD dwSize = 0;
        WinHttpQueryDataAvailable(hRequest, &dwSize);
        if (dwSize > 0) {
            std::vector<char> buffer(dwSize + 1);
            DWORD dwDownloaded = 0;
            WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded);
            
            // Simplified - real implementation would parse JSON
            MarketplaceEntry entry;
            entry.name = "sample-extension";
            entry.publisher = "publisher";
            entry.version = "1.0.0";
            entry.description = "Sample extension";
            entry.installCount = 1000;
            entry.rating = 4.5f;
            results.push_back(entry);
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return !results.empty();
}

bool DownloadVsix(const std::string& publisher, const std::string& extName, 
                  const std::string& version, const std::string& destPath) {
    
    std::wstring wPublisher(publisher.begin(), publisher.end());
    std::wstring wExtName(extName.begin(), extName.end());
    std::wstring wVersion(version.begin(), version.end());
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD-IDE/1.0", 
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return false;
        
    HINTERNET hConnect = WinHttpConnect(hSession, L"marketplace.visualstudio.com", 
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    std::wstring path = L"/_apis/public/gallery/publishers/" + wPublisher + 
                        L"/vsextensions/" + wExtName + L"/" + wVersion + 
                        L"/vspackage";
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0);
    WinHttpReceiveResponse(hRequest, NULL);
    
    std::ofstream outFile(destPath, std::ios::binary);
    DWORD dwSize = 0;
    do {
        dwSize = 0;
        WinHttpQueryDataAvailable(hRequest, &dwSize);
        if (dwSize == 0) break;
        
        std::vector<BYTE> buffer(dwSize);
        DWORD dwDownloaded = 0;
        WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded);
        outFile.write((char*)buffer.data(), dwDownloaded);
    } while (dwSize > 0);
    
    outFile.close();
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return true;
}

} // namespace VSCodeMarketplace
