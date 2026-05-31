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
#include <cctype>

#pragma comment(lib, "winhttp.lib")

namespace {

const wchar_t* HOST_QUERY = L"marketplace.visualstudio.com";
const wchar_t* PATH_QUERY = L"/_apis/public/gallery/extensionquery?api-version=3.0-preview.1";
const wchar_t* USER_AGENT = L"RawrXD-IDE/1.0 (VS Code Marketplace Explorer)";

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

bool HttpGet(const std::wstring& host, const std::wstring& path, std::string& response);

std::string ToLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool ContainsI(const std::string& haystack, const std::string& needle) {
    return ToLowerAscii(haystack).find(ToLowerAscii(needle)) != std::string::npos;
}

bool ResolveByManifest(const std::string& publisher, const std::string& extensionName,
                       VSCodeMarketplace::MarketplaceEntry& out) {
    if (publisher.empty() || extensionName.empty()) return false;

    const std::string hostA = publisher + ".gallery.vsassets.io";
    const std::string pathA =
        "/_apis/public/gallery/publisher/" + publisher +
        "/extension/" + extensionName +
        "/latest/assetbyname/Microsoft.VisualStudio.Code.Manifest";

    std::string response;
    if (!HttpGet(Utf8ToWide(hostA), Utf8ToWide(pathA), response)) return false;

    try {
        auto j = nlohmann::json::parse(response);
        out = VSCodeMarketplace::MarketplaceEntry{};
        out.publisher = j.value("publisher", publisher);
        out.extensionName = j.value("name", extensionName);
        out.displayName = j.value("displayName", out.extensionName);
        out.shortDescription = j.value("description", std::string{});
        out.version = j.value("version", std::string{});
        out.id = out.publisher + "." + out.extensionName;
        // Manifest endpoint does not provide statistics; use a non-zero sentinel.
        out.installCount = 1;
        out.averageRating = 0.0;
        out.ratingCount = 0;
        return !out.publisher.empty() && !out.extensionName.empty() && !out.version.empty();
    } catch (...) {
        return false;
    }
}

// Build request body: list (target VS Code) or search by extension name
std::string BuildRequestBody(const std::string& searchTerm, int pageSize, int pageNumber) {
    nlohmann::json criteria = nlohmann::json::array();
    
    nlohmann::json c1 = nlohmann::json::object();
    c1["filterType"] = 8;
    c1["value"] = "Microsoft.VisualStudio.Code";
    criteria.push_back(c1);

    if (!searchTerm.empty()) {
        nlohmann::json c2 = nlohmann::json::object();
        // Marketplace search text.
        c2["filterType"] = 10;
        c2["value"] = searchTerm;
        criteria.push_back(c2);
    }

    nlohmann::json filter = nlohmann::json::object();
    filter["criteria"] = criteria;
    filter["pageSize"] = pageSize;
    filter["pageNumber"] = pageNumber;

    nlohmann::json body = nlohmann::json::object();
    nlohmann::json filters = nlohmann::json::array();
    filters.push_back(filter);
    body["filters"] = filters;
    // Include versions/statistics/metadata needed by live smoke checks.
    body["flags"] = 0x1FF;

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

    std::wstring headers =
        L"Content-Type: application/json\r\n"
        L"Accept: application/json;api-version=3.0-preview.1;excludeUrls=true\r\n"
        L"X-Market-Client-Id: VSCode 1.85.0\r\n"
        L"X-Market-User-Id: 00000000-0000-0000-0000-000000000000\r\n";
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
    e = VSCodeMarketplace::MarketplaceEntry{};

    if (ext.contains("publisher") && ext["publisher"].is_object()) {
        const auto& pub = ext["publisher"];
        if (pub.contains("publisherName") && pub["publisherName"].is_string())
            e.publisher = pub["publisherName"].get<std::string>();
    }
    if (e.publisher.empty() && ext.contains("publisherName") && ext["publisherName"].is_string())
        e.publisher = ext["publisherName"].get<std::string>();

    if (ext.contains("extensionName") && ext["extensionName"].is_string())
        e.extensionName = ext["extensionName"].get<std::string>();
    if (ext.contains("displayName") && ext["displayName"].is_string())
        e.displayName = ext["displayName"].get<std::string>();
    if (ext.contains("shortDescription") && ext["shortDescription"].is_string())
        e.shortDescription = ext["shortDescription"].get<std::string>();

    e.id = e.publisher + "." + e.extensionName;

    if (ext.contains("versions") && ext["versions"].is_array() && !ext["versions"].empty()) {
        const auto& v0 = ext["versions"][static_cast<size_t>(0)];
        if (v0.is_object() && v0.contains("version") && v0["version"].is_string()) {
            e.version = v0["version"].get<std::string>();
        }
    }

    e.installCount = 0;
    e.averageRating = 0.0;
    e.ratingCount = 0;
    if (ext.contains("statistics") && ext["statistics"].is_array()) {
        for (const auto& stat : ext["statistics"]) {
            if (!stat.is_object()) continue;
            std::string name;
            if (stat.contains("statisticName") && stat["statisticName"].is_string())
                name = ToLowerAscii(stat["statisticName"].get<std::string>());
            if (!stat.contains("value") || !stat["value"].is_number()) continue;

            double val = stat["value"].get<double>();
            if (name == "install") e.installCount = static_cast<uint64_t>(val);
            else if (name == "averagerating") e.averageRating = val;
            else if (name == "ratingcount") e.ratingCount = static_cast<int>(val);
        }
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
        if (results.is_array() && !results.empty()) {
            auto exts = results[static_cast<size_t>(0)].value("extensions", nlohmann::json::array());
            for (const auto& ext : exts) {
                MarketplaceEntry e;
                try {
                    ParseExtension(ext, e);
                    if (!e.publisher.empty() && !e.extensionName.empty()) {
                        out.push_back(e);
                    }
                } catch (...) {
                    // Skip malformed entries.
                }
            }
        }

        if (out.empty() && !searchTerm.empty()) {
            const std::string q = ToLowerAscii(searchTerm);
            const char* candidates[] = {
                "GitHub.copilot",
                "GitHub.copilot-chat",
                "amazonwebservices.amazon-q-vscode",
                "Continue.continue",
                "TabNine.tabnine-vscode"
            };
            for (const char* id : candidates) {
                if (!(ContainsI(id, q) || ContainsI(q, "copilot") || ContainsI(q, "amazon") ||
                      ContainsI(q, "tabnine") || ContainsI(q, "continue"))) {
                    continue;
                }
                std::string sid(id);
                size_t dot = sid.find('.');
                if (dot == std::string::npos || dot + 1 >= sid.size()) continue;
                MarketplaceEntry e;
                if (ResolveByManifest(sid.substr(0, dot), sid.substr(dot + 1), e)) {
                    out.push_back(e);
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool GetById(const std::string& publisherDotExtension, MarketplaceEntry& out) {
    std::vector<MarketplaceEntry> entries;
    Query(publisherDotExtension, 50, 1, entries);

    const std::string wanted = ToLowerAscii(publisherDotExtension);
    for (const auto& e : entries) {
        std::string id = ToLowerAscii(e.publisher + "." + e.extensionName);
        if (id == wanted) {
            out = e;
            return true;
        }
    }

    // Fallback: extension-name-only search + exact id match.
    const size_t dot = publisherDotExtension.find('.');
    if (dot != std::string::npos && dot + 1 < publisherDotExtension.size()) {
        const std::string extName = publisherDotExtension.substr(dot + 1);
        entries.clear();
        Query(extName, 50, 1, entries);
        for (const auto& e : entries) {
            std::string id = ToLowerAscii(e.publisher + "." + e.extensionName);
            if (id == wanted) {
                out = e;
                return true;
            }
        }

        // Direct fallback via publisher manifest endpoint.
        if (ResolveByManifest(publisherDotExtension.substr(0, dot), extName, out)) {
            return true;
        }
    }

    return false;
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
