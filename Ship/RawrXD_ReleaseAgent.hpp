// RawrXD_ReleaseAgent.hpp - Autonomous Release Management
// Pure C++20 - No Qt Dependencies
// Network: WinHTTP, SCM: Git, Build: cl.exe

#pragma once

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <iostream>
#include <filesystem>

#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;

namespace RawrXD {

class ReleaseAgent {
public:
    static bool BumpVersion(const std::string& part) {
        // In the new Ship structure, we might have a version.h instead of CMakeLists.txt
        std::string path = "version.h";
        if (!fs::exists(path)) {
            std::ofstream f(path);
            f << "#define RAWRXD_VERSION_MAJOR 1\n#define RAWRXD_VERSION_MINOR 0\n#define RAWRXD_VERSION_PATCH 0\n";
            f.close();
        }

        std::ifstream f(path);
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();

        std::regex re_major("RAWRXD_VERSION_MAJOR (\\d+)");
        std::regex re_minor("RAWRXD_VERSION_MINOR (\\d+)");
        std::regex re_patch("RAWRXD_VERSION_PATCH (\\d+)");
        
        std::smatch m;
        int major = 1, minor = 0, patch = 0;
        if (std::regex_search(content, m, re_major)) major = std::stoi(m.str(1));
        if (std::regex_search(content, m, re_minor)) minor = std::stoi(m.str(1));
        if (std::regex_search(content, m, re_patch)) patch = std::stoi(m.str(1));

        if (part == "major") { major++; minor = 0; patch = 0; }
        else if (part == "minor") { minor++; patch = 0; }
        else { patch++; }

        std::ofstream out(path);
        out << "#define RAWRXD_VERSION_MAJOR " << major << "\n"
            << "#define RAWRXD_VERSION_MINOR " << minor << "\n"
            << "#define RAWRXD_VERSION_PATCH " << patch << "\n";
        out.close();

        return true;
    }

    static bool Tweet(const std::string& text) {
        char* bearer = getenv("TWITTER_BEARER");
        if (!bearer) return true; // Skip gracefully

        HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        HINTERNET hConnect = WinHttpConnect(hSession, L"api.twitter.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/2/tweets", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

        std::string auth = "Authorization: Bearer " + std::string(bearer);
        std::wstring wauth(auth.begin(), auth.end());
        WinHttpAddRequestHeaders(hRequest, wauth.c_str(), (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
        WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);

        std::string body = "{\"text\":\"" + text + "\"}";
        bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.c_str(), (DWORD)body.length(), (DWORD)body.length(), 0);
        if (ok) ok = WinHttpReceiveResponse(hRequest, NULL);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return ok;
    }

    static bool TagAndUpload() {
        // Simplified git tag
        system("git tag -a autorelease -m \"Auto release\"");
        // Build
        system("build_all.bat");
        return true;
    }
};

} // namespace RawrXD
