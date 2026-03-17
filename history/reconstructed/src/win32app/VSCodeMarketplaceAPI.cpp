// VSCodeMarketplaceAPI.cpp — Implementation using WinHTTP and nlohmann/json

#include "VSCodeMarketplaceAPI.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

#pragma comment(lib, "winhttp.lib")

namespace {

const wchar_t* HOST_QUERY   = L"marketplace.visualstudio.com";
const wchar_t* PATH_QUERY   = L"/_apis/public/gallery/extensionquery?api-version=3.0-preview.1";
const wchar_t* USER_AGENT   = L"RawrXD-IDE/1.0 (VS Code Marketplace Explorer)";

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

// Build request body: list (target VS Code) or search by extension name
std::string BuildRequestBody(const std::string& searchTerm, int pageSize, int pageNumber) {
    nlohmann::json criteria = nlohmann::json::array();
    criteria.push_back({{"filterType", 8}, {"value", "Microsoft.VisualStudio.Code"}});
    if (!searchTerm.empty())
        criteria.push_back({{"filterType", 7}, {"value", searchTerm}});
    nlohmann::json filter = {
        {"criteria", criteria},
        {"pageSize", pageSize},
        {"pageNumber", pageNumber}
    };
    nlohmann::json body = {
        {"filters", nlohmann::json::array({filter})},
        {"flags", 0x201}  // IncludeVersions (1) + IncludeLatestVersionOnly (0x200)
    };
    return body.dump();
}

bool HttpPost(const std::wstring& host, const std::wstring& path,
              const std::string& body, std::string& response) {
    HINTERNET hSession = WinHttpOpen(USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = WINHTTP_FLAG_SECURE;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    std::wstring headers = L"Content-Type: application/json\r\nAccept: application/json\r\n";
    if (!WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)headers.size(), WINHTTP_ADDREQ_FLAG_ADD)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    response.clear();
    DWORD bytesRead;
    char buf[8192];
    do {
        bytesRead = 0;
        if (!WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) || bytesRead == 0) break;
        response.append(buf, bytesRead);
    } while (bytesRead > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

bool HttpGet(const std::wstring& host, const std::wstring& path, std::string& response) {
    HINTERNET hSession = WinHttpOpen(USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    response.clear();
    DWORD bytesRead;
    char buf[8192];
    do {
        bytesRead = 0;
        if (!WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) || bytesRead == 0) break;
        response.append(buf, bytesRead);
    } while (bytesRead > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

// Download binary from URL to file (WinHTTP read raw bytes)
bool HttpGetBinary(const std::wstring& host, const std::wstring& path, const std::string& savePath) {
    HINTERNET hSession = WinHttpOpen(USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    std::ofstream out(savePath, std::ios::binary);
    if (!out) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD bytesRead;
    char buf[32768];
    bool ok = true;
    do {
        bytesRead = 0;
        if (!WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead)) { ok = false; break; }
        if (bytesRead == 0) break;
        out.write(buf, bytesRead);
    } while (bytesRead > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok && out.good();
}

void ParseExtension(const nlohmann::json& ext, VSCodeMarketplace::MarketplaceEntry& e) {
    e.publisher      = ext.value("publisher", nlohmann::json::object()).value("publisherName", "");
    e.extensionName  = ext.value("extensionName", "");
    e.displayName    = ext.value("displayName", "");
    e.shortDescription = ext.value("shortDescription", "");
    e.id = e.publisher + "." + e.extensionName;

    const auto& versions = ext.value("versions", nlohmann::json::array());
    if (!versions.empty() && versions.at(0).contains("version"))
        e.version = versions.at(0).at("version").get<std::string>();
    else
        e.version.clear();

    e.installCount   = 0;
    e.averageRating  = 0.0;
    e.ratingCount    = 0;
    for (const auto& stat : ext.value("statistics", nlohmann::json::array())) {
        std::string name = stat.value("statisticName", "");
        if (name == "install") e.installCount = stat.value("value", 0);
        else if (name == "averagerating") e.averageRating = stat.value("value", 0.0);
        else if (name == "ratingcount") e.ratingCount = (int)stat.value("value", 0);
    }
}

} // namespace

namespace VSCodeMarketplace {

bool Query(const std::string& searchTerm, int pageSize, int pageNumber,
           std::vector<MarketplaceEntry>& out) {
    out.clear();
    std::string body = BuildRequestBody(searchTerm, pageSize, pageNumber);
    std::string response;
    if (!HttpPost(HOST_QUERY, PATH_QUERY, body, response)) return false;

    try {
        auto j = nlohmann::json::parse(response);
        auto& results = j["results"];
        if (!results.is_array() || results.empty()) return true;
        auto exts = results[static_cast<size_t>(0)].value("extensions", nlohmann::json::array());
        for (const auto& ext : exts) {
            MarketplaceEntry e;
            ParseExtension(ext, e);
            out.push_back(e);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool GetById(const std::string& publisherDotExtension, MarketplaceEntry& out) {
    std::string body = BuildRequestBody(publisherDotExtension, 1, 1);
    std::string response;
    if (!HttpPost(HOST_QUERY, PATH_QUERY, body, response)) return false;

    try {
        auto j = nlohmann::json::parse(response);
        auto& results = j["results"];
        if (!results.is_array() || results.empty()) return false;
        auto exts = results[static_cast<size_t>(0)].value("extensions", nlohmann::json::array());
        if (exts.empty()) return false;
        ParseExtension(exts[static_cast<size_t>(0)], out);
        return true;
    } catch (...) {
        return false;
    }
}

bool DownloadVsix(const std::string& publisher, const std::string& extensionName,
                  const std::string& version, const std::string& savePath) {
    if (publisher.empty() || extensionName.empty() || version.empty() || savePath.empty())
        return false;

    // https://{publisher}.gallery.vsassets.io/_apis/public/gallery/publisher/{publisher}/extension/{name}/{version}/assetbyname/Microsoft.VisualStudio.Services.VSIXPackage
    std::string hostA = publisher + ".gallery.vsassets.io";
    std::string pathA = "/_apis/public/gallery/publisher/" + publisher + "/extension/" + extensionName + "/" + version + "/assetbyname/Microsoft.VisualStudio.Services.VSIXPackage";
    std::wstring host = Utf8ToWide(hostA);
    std::wstring path = Utf8ToWide(pathA);

    return HttpGetBinary(host, path, savePath);
}

std::string ItemUrl(const std::string& publisher, const std::string& extensionName) {
    return "https://marketplace.visualstudio.com/items?itemName=" + publisher + "." + extensionName;
}

} // namespace VSCodeMarketplace
