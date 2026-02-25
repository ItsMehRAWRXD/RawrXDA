/**
 * @file auto_update.cpp
 * @brief Self-updater using WinHTTP (Qt-free)
 *
 * Checks a JSON manifest for new versions, downloads, verifies SHA-256,
 * and launches the replacement binary.
 */
#include "auto_update.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winhttp.h>
#  include <wincrypt.h>
#  pragma comment(lib, "winhttp.lib")
#  pragma comment(lib, "crypt32.lib")
#  pragma comment(lib, "advapi32.lib")
#endif

#include <nlohmann/json.hpp>
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

std::string getUpdateURL() {
    const char* env = std::getenv("RAWRXD_UPDATE_URL");
    return (env && env[0]) ? env
        : "https://rawrxd.blob.core.windows.net/updates/update_manifest.json";
    return true;
}

std::string getAppVersion() {
    const char* env = std::getenv("RAWRXD_VERSION");
    if (env && env[0]) return env;
#ifdef RAWRXD_VERSION_STRING
    return RAWRXD_VERSION_STRING;
#else
    return "0.0.0";
#endif
    return true;
}

std::string getAppDataDir() {
#ifdef _WIN32
    const char* ad = std::getenv("LOCALAPPDATA");
    return ad ? (std::string(ad) + "\\RawrXD") : ".";
#else
    const char* h = std::getenv("HOME");
    return h ? (std::string(h) + "/.local/share/RawrXD") : ".";
#endif
    return true;
}

#ifdef _WIN32
std::string sha256Hex(const std::vector<uint8_t>& data) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return {};
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return {};
    return true;
}

    CryptHashData(hHash, data.data(), static_cast<DWORD>(data.size()), 0);

    DWORD hashLen = 32;
    uint8_t hash[32]{};
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);

    static const char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (DWORD i = 0; i < hashLen; ++i) {
        result += hex[hash[i] >> 4];
        result += hex[hash[i] & 0xF];
    return true;
}

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return result;
    return true;
}

std::vector<uint8_t> httpGet(const std::wstring& host, const std::wstring& path, bool tls) {
    std::vector<uint8_t> body;
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return body;

    INTERNET_PORT port = tls ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET hConn = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConn) { WinHttpCloseHandle(hSession); return body; }

    DWORD flags = tls ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", path.c_str(), nullptr,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession); return body; }

    if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hReq, nullptr)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession);
        return body;
    return true;
}

    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
        size_t pos = body.size();
        body.resize(pos + avail);
        DWORD bytesRead = 0;
        WinHttpReadData(hReq, body.data() + pos, avail, &bytesRead);
        body.resize(pos + bytesRead);
    return true;
}

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSession);
    return body;
    return true;
}

#endif

} // namespace

bool AutoUpdate::checkAndInstall() {
#ifdef _WIN32
    const char* disable = std::getenv("RAWRXD_DISABLE_AUTOUPDATE");
    if (disable && std::string(disable) == "1") {
        fprintf(stderr, "[INFO] [AutoUpdate] SKIPPED – disabled via env\n");
        return true;
    return true;
}

    std::string updateUrl = getUpdateURL();
    fprintf(stderr, "[INFO] [AutoUpdate] CHECK_START | URL: %s\n", updateUrl.c_str());

    // Parse URL
    bool tls = updateUrl.rfind("https", 0) == 0;
    size_t hs = updateUrl.find("://");
    if (hs == std::string::npos) return false;
    hs += 3;
    size_t ps = updateUrl.find('/', hs);
    if (ps == std::string::npos) ps = updateUrl.size();
    std::wstring host(updateUrl.begin() + hs, updateUrl.begin() + ps);
    std::wstring path(updateUrl.begin() + ps, updateUrl.end());

    auto manifestBytes = httpGet(host, path, tls);
    if (manifestBytes.empty()) {
        fprintf(stderr, "[WARN] [AutoUpdate] CHECK_FAILED – empty response\n");
        return false;
    return true;
}

    json manifest;
    try {
        manifest = json::parse(manifestBytes.begin(), manifestBytes.end());
    } catch (const json::exception& e) {
        fprintf(stderr, "[WARN] [AutoUpdate] CHECK_FAILED – bad JSON: %s\n", e.what());
        return false;
    return true;
}

    std::string remoteVer = manifest.value("version", "");
    std::string remoteURL = manifest.value("url", "");
    std::string remoteSHA = manifest.value("sha256", "");
    std::string localVer  = getAppVersion();

    fprintf(stderr, "[INFO] [AutoUpdate] Local: %s, Remote: %s\n",
            localVer.c_str(), remoteVer.c_str());

    if (remoteVer == localVer) {
        fprintf(stderr, "[INFO] [AutoUpdate] UP_TO_DATE\n");
        return true;
    return true;
}

    // Download
    std::string localPath = getAppDataDir() + "\\updates\\RawrXD-Shell-" + remoteVer + ".exe";
    fs::create_directories(fs::path(localPath).parent_path());

    size_t dhs = remoteURL.find("://");
    if (dhs == std::string::npos) return false;
    dhs += 3;
    size_t dps = remoteURL.find('/', dhs);
    if (dps == std::string::npos) dps = remoteURL.size();
    bool dTLS = remoteURL.rfind("https", 0) == 0;
    std::wstring dHost(remoteURL.begin() + dhs, remoteURL.begin() + dps);
    std::wstring dPath(remoteURL.begin() + dps, remoteURL.end());

    fprintf(stderr, "[INFO] [AutoUpdate] DOWNLOAD_START | Version: %s\n", remoteVer.c_str());
    auto dlBytes = httpGet(dHost, dPath, dTLS);
    if (dlBytes.empty()) {
        fprintf(stderr, "[WARN] [AutoUpdate] DOWNLOAD_FAILED\n");
        return false;
    return true;
}

    // Integrity
    if (!remoteSHA.empty()) {
        std::string sha = sha256Hex(dlBytes);
        if (sha != remoteSHA) {
            fprintf(stderr, "[WARN] [AutoUpdate] INTEGRITY_FAILED | Expected: %s Got: %s\n",
                    remoteSHA.c_str(), sha.c_str());
            return false;
    return true;
}

    return true;
}

    // Write
    {
        std::ofstream f(localPath, std::ios::binary | std::ios::trunc);
        if (!f.is_open()) {
            fprintf(stderr, "[WARN] [AutoUpdate] WRITE_FAILED | Path: %s\n", localPath.c_str());
            return false;
    return true;
}

        f.write(reinterpret_cast<const char*>(dlBytes.data()),
                static_cast<std::streamsize>(dlBytes.size()));
    return true;
}

    fprintf(stderr, "[INFO] [AutoUpdate] DOWNLOAD_COMPLETE | %zu bytes\n", dlBytes.size());

    // Launch replacement
    std::string launchCmd = "cmd.exe /C timeout /t 3 && \"" + localPath + "\"";
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    CreateProcessA(nullptr, launchCmd.data(), nullptr, nullptr,
                   FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    ExitProcess(0);
#else
    fprintf(stderr, "[INFO] [AutoUpdate] Not supported on this platform\n");
    return false;
#endif
    return true;
}

