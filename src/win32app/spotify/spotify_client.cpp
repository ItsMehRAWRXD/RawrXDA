// ============================================================================
// spotify_client.cpp — Spotify Web API client implementation (WinHttp)
// ============================================================================

#include "spotify_client.hpp"
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

namespace RawrXD {

namespace {

std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &out[0], n);
    return out;
}

std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string out(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &out[0], n, nullptr, nullptr);
    return out;
}

std::string urlEncode(const std::string& s) {
    std::ostringstream enc;
    enc << std::hex;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            enc << c;
        else
            enc << '%' << std::setfill('0') << std::setw(2) << (int)c;
    }
    return enc.str();
}

std::string base64Encode(const std::string& in) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(tbl[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(tbl[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

} // anonymous

void SpotifyClient::setAccessToken(const std::string& token, int expiresInSeconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_accessToken = token;
    m_tokenExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(expiresInSeconds);
}

bool SpotifyClient::hasValidToken() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_accessToken.empty() &&
           std::chrono::steady_clock::now() < m_tokenExpiry - std::chrono::seconds(60);
}

std::string SpotifyClient::getAuthorizationUrl() const {
    std::string scope = "user-read-private user-read-email user-read-playback-state user-modify-playback-state user-read-currently-playing";
    std::string state = "rawrxd_spotify";
    std::ostringstream url;
    url << "https://accounts.spotify.com/authorize?"
        << "client_id=" << urlEncode(m_clientId)
        << "&response_type=code"
        << "&redirect_uri=" << urlEncode(m_redirectUri)
        << "&scope=" << urlEncode(scope)
        << "&state=" << urlEncode(state)
        << "&show_dialog=true";
    return url.str();
}

std::string SpotifyClient::tokenRequest(const std::string& body) {
    std::wstring wHost = L"accounts.spotify.com";
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Spotify/1.0",
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), 443, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/token",
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    std::string auth = m_clientId + ":" + m_clientSecret;
    std::string authB64 = base64Encode(auth);
    std::wstring wAuth = utf8ToWide("Authorization: Basic " + authB64);
    std::wstring wContent = utf8ToWide("Content-Type: application/x-www-form-urlencoded");
    WinHttpAddRequestHeaders(hRequest, wAuth.c_str(), (DWORD)wAuth.size(), WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, wContent.c_str(), (DWORD)wContent.size(), WINHTTP_ADDREQ_FLAG_ADD);

    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }
    if (!WinHttpReceiveResponse(hRequest, nullptr)) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    std::string response;
    DWORD dwSize = 0;
    do {
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (!dwSize) break;
        std::string chunk(dwSize, 0);
        DWORD dwRead = 0;
        if (!WinHttpReadData(hRequest, &chunk[0], dwSize, &dwRead)) break;
        if (dwRead) response.append(chunk.data(), dwRead);
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

bool SpotifyClient::exchangeCodeForTokens(const std::string& code) {
    std::string body = "grant_type=authorization_code&code=" + urlEncode(code) +
                       "&redirect_uri=" + urlEncode(m_redirectUri);
    std::string response = tokenRequest(body);
    if (response.empty()) return false;
    try {
        auto j = nlohmann::json::parse(response);
        if (j.contains("error")) return false;
        std::string access = j.value("access_token", "");
        std::string refresh = j.value("refresh_token", "");
        int exp = j.value("expires_in", 3600);
        if (access.empty()) return false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_accessToken = access;
            m_refreshToken = refresh;
            m_tokenExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(exp);
        }
        if (m_persistTokens) m_persistTokens(refresh, access, exp);
        return true;
    } catch (...) { return false; }
}

bool SpotifyClient::refreshAccessToken() {
    if (m_refreshToken.empty()) return false;
    std::string body = "grant_type=refresh_token&refresh_token=" + urlEncode(m_refreshToken);
    std::string response = tokenRequest(body);
    if (response.empty()) return false;
    try {
        auto j = nlohmann::json::parse(response);
        if (j.contains("error")) return false;
        std::string access = j.value("access_token", "");
        std::string refresh = j.value("refresh_token", m_refreshToken);
        int exp = j.value("expires_in", 3600);
        if (access.empty()) return false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_accessToken = access;
            if (!refresh.empty()) m_refreshToken = refresh;
            m_tokenExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(exp);
        }
        if (m_persistTokens) m_persistTokens(m_refreshToken, access, exp);
        return true;
    } catch (...) { return false; }
}

std::string SpotifyClient::apiRequest(const std::string& method, const std::string& path,
                                       const std::string& body, const std::string& contentType) {
    std::string accessToken;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_accessToken.empty() || std::chrono::steady_clock::now() >= m_tokenExpiry - std::chrono::seconds(60)) {
            if (m_refreshToken.empty()) return "";
        } else {
            accessToken = m_accessToken;
        }
    }
    if (accessToken.empty()) {
        if (!refreshAccessToken()) return "";
        std::lock_guard<std::mutex> lock(m_mutex);
        accessToken = m_accessToken;
    }

    std::wstring wHost = L"api.spotify.com";
    std::wstring wPath = utf8ToWide(path);
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Spotify/1.0",
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), 443, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, utf8ToWide(method).c_str(), wPath.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    std::wstring wAuth = utf8ToWide("Authorization: Bearer " + accessToken);
    WinHttpAddRequestHeaders(hRequest, wAuth.c_str(), (DWORD)wAuth.size(), WINHTTP_ADDREQ_FLAG_ADD);
    if (!body.empty()) {
        std::wstring wCt = utf8ToWide("Content-Type: " + contentType);
        WinHttpAddRequestHeaders(hRequest, wCt.c_str(), (DWORD)wCt.size(), WINHTTP_ADDREQ_FLAG_ADD);
    }

    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    body.empty() ? nullptr : (LPVOID)body.c_str(),
                                    (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }
    if (!WinHttpReceiveResponse(hRequest, nullptr)) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (statusCode >= 400) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    std::string response;
    DWORD dwSize = 0;
    do {
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (!dwSize) break;
        std::string chunk(dwSize, 0);
        DWORD dwRead = 0;
        if (!WinHttpReadData(hRequest, &chunk[0], dwSize, &dwRead)) break;
        if (dwRead) response.append(chunk.data(), dwRead);
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

bool SpotifyClient::getCurrentUser(SpotifyUser& out) {
    std::string r = apiRequest("GET", "/v1/me");
    if (r.empty()) return false;
    try {
        auto j = nlohmann::json::parse(r);
        out.id = j.value("id", "");
        out.displayName = j.value("display_name", "");
        if (j.contains("email")) out.email = j["email"].get<std::string>();
        if (j.contains("images") && j["images"].is_array() && !j["images"].empty())
            out.imageUrl = j["images"][0].value("url", "");
        return true;
    } catch (...) { return false; }
}

bool SpotifyClient::getCurrentPlayback(SpotifyTrack& out) {
    std::string r = apiRequest("GET", "/v1/me/player");
    if (r.empty()) return false;
    try {
        auto j = nlohmann::json::parse(r);
        out.isPlaying = j.value("is_playing", false);
        out.progressMs = j.value("progress_ms", 0);
        if (!j.contains("item") || !j["item"].is_object()) return true;
        auto item = j["item"];
        out.id = item.value("id", "");
        out.name = item.value("name", "");
        out.durationMs = item.value("duration_ms", 0);
        if (item.contains("artists") && item["artists"].is_array() && !item["artists"].empty())
            out.artist = item["artists"][0].value("name", "");
        if (item.contains("album") && item["album"].is_object())
            out.album = item["album"].value("name", "");
        return true;
    } catch (...) { return false; }
}

bool SpotifyClient::pause() {
    return !apiRequest("PUT", "/v1/me/player/pause").empty();
}

bool SpotifyClient::play() {
    return !apiRequest("PUT", "/v1/me/player/play", "{}").empty();
}

bool SpotifyClient::skipNext() {
    return !apiRequest("POST", "/v1/me/player/next").empty();
}

bool SpotifyClient::skipPrevious() {
    return !apiRequest("POST", "/v1/me/player/previous").empty();
}

bool SpotifyClient::setVolume(int percent) {
    percent = std::max(0, std::min(100, percent));
    return !apiRequest("PUT", "/v1/me/player/volume?volume_percent=" + std::to_string(percent)).empty();
}

} // namespace RawrXD
