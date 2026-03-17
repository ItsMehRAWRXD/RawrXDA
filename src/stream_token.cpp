#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <nlohmann/json.hpp>

extern "C" int StreamToken_SetDispatch(void* callbackFn, void* callbackCtx, uint32_t beaconSlot);
extern "C" int StreamToken_Callback(const char* tokenText, int tokenLen, int tokenId, uint64_t flags);

namespace {

struct HttpEndpoint {
    std::wstring host = L"127.0.0.1";
    INTERNET_PORT port = 11434;
    std::wstring path = L"/api/generate";
    bool secure = false;
};

static std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (len <= 0) return {};
    std::wstring out((size_t)len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), len);
    return out;
}

static std::string trim(const std::string& in) {
    size_t a = 0;
    while (a < in.size() && (unsigned char)in[a] <= ' ') ++a;
    size_t b = in.size();
    while (b > a && (unsigned char)in[b - 1] <= ' ') --b;
    return in.substr(a, b - a);
}

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 32);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    return out;
}

static HttpEndpoint parseEndpoint(const char* endpoint) {
    HttpEndpoint ep;
    if (!endpoint || !*endpoint) return ep;

    std::string s(endpoint);
    s = trim(s);

    if (s.rfind("http://", 0) == 0) {
        s.erase(0, 7);
        ep.secure = false;
    } else if (s.rfind("https://", 0) == 0) {
        s.erase(0, 8);
        ep.secure = true;
        ep.port = 443;
    }

    size_t slash = s.find('/');
    std::string hostPort = slash == std::string::npos ? s : s.substr(0, slash);
    if (slash != std::string::npos) ep.path = widen(s.substr(slash));

    size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos) {
        std::string host = hostPort.substr(0, colon);
        std::string port = hostPort.substr(colon + 1);
        if (!host.empty()) ep.host = widen(host);
        try {
            int p = std::stoi(port);
            if (p > 0 && p <= 65535) ep.port = (INTERNET_PORT)p;
        } catch (...) {}
    } else if (!hostPort.empty()) {
        ep.host = widen(hostPort);
    }

    if (ep.path.empty()) ep.path = L"/api/generate";
    return ep;
}

} // namespace

extern "C" __declspec(dllexport)
int RawrXD_StreamSetCallback(void* callbackFn, void* callbackCtx, uint32_t beaconSlot) {
    return StreamToken_SetDispatch(callbackFn, callbackCtx, beaconSlot == 0 ? 9u : beaconSlot);
}

extern "C" __declspec(dllexport)
int RawrXD_StreamGenerate(const char* model,
                          const char* prompt,
                          const char* endpoint,
                          uint32_t timeoutMs) {
#ifdef _WIN32
    if (!model || !*model || !prompt) {
        return -1;
    }

    const HttpEndpoint ep = parseEndpoint(endpoint);

    std::string body =
        std::string("{\"model\":\"") + jsonEscape(model) +
        "\",\"prompt\":\"" + jsonEscape(prompt) +
        "\",\"stream\":true}";

    HINTERNET hSession = WinHttpOpen(
        L"RawrXD-StreamToken/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        return -2;
    }

    DWORD tmo = timeoutMs == 0 ? 120000u : timeoutMs;
    WinHttpSetTimeouts(hSession, tmo, tmo, tmo, tmo);

    HINTERNET hConnect = WinHttpConnect(hSession, ep.host.c_str(), ep.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return -3;
    }

    DWORD flags = ep.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        ep.path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return -4;
    }

    const wchar_t* headers = L"Content-Type: application/json\r\n";
    BOOL sent = WinHttpSendRequest(
        hRequest,
        headers,
        (DWORD)-1,
        (LPVOID)body.data(),
        (DWORD)body.size(),
        (DWORD)body.size(),
        0);
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return -5;
    }

    int tokenId = 0;
    std::string carry;

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) {
            break;
        }

        std::string chunk;
        chunk.resize((size_t)avail);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, chunk.data(), avail, &read) || read == 0) {
            break;
        }
        chunk.resize((size_t)read);
        carry += chunk;

        size_t lineStart = 0;
        for (;;) {
            size_t nl = carry.find('\n', lineStart);
            if (nl == std::string::npos) {
                carry = carry.substr(lineStart);
                break;
            }

            std::string line = trim(carry.substr(lineStart, nl - lineStart));
            lineStart = nl + 1;
            if (line.empty()) continue;

            try {
                auto j = nlohmann::json::parse(line);

                if (j.contains("response") && j["response"].is_string()) {
                    const std::string token = j["response"].get<std::string>();
                    if (!token.empty()) {
                        StreamToken_Callback(token.c_str(), (int)token.size(), tokenId++, 0ull);
                    }
                }

                if (j.contains("done") && j["done"].is_boolean() && j["done"].get<bool>()) {
                    StreamToken_Callback("", 0, tokenId, 1ull);
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return tokenId;
                }
            } catch (...) {
                // Ignore incomplete or non-JSON lines and continue.
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return tokenId;
#else
    (void)model;
    (void)prompt;
    (void)endpoint;
    (void)timeoutMs;
    return -1;
#endif
}
