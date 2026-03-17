// gguf_proxy_server.cpp
// Production: WinHTTP proxy to a local Ollama/GGUF-compatible HTTP backend.

#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace RawrXD::Standalone {

static std::wstring widen_utf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w;
    w.resize((size_t)n);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

static bool read_all_response(HINTERNET hRequest, std::string& out) {
    out.clear();
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail)) return false;
        if (avail == 0) break;
        std::vector<char> buf;
        buf.resize(avail);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, buf.data(), avail, &read)) return false;
        if (read == 0) break;
        out.append(buf.data(), buf.data() + read);
    }
    return true;
}

static bool winhttp_request(const std::wstring& host,
                            INTERNET_PORT port,
                            const std::wstring& method,
                            const std::wstring& path,
                            const std::wstring& extra_headers,
                            const void* body,
                            DWORD body_len,
                            std::string& response,
                            unsigned long* status_code) {
    response.clear();
    if (status_code) *status_code = 0;

    HINTERNET hSession = WinHttpOpen(L"RawrXD-Standalone-WebBridge/1.0",
                                     WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS,
                                     0);
    if (!hSession) return false;

    WinHttpSetTimeouts(hSession, 5000, 5000, 15000, 15000);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
                                            method.c_str(),
                                            path.c_str(),
                                            nullptr,
                                            WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    BOOL ok = WinHttpSendRequest(hRequest,
                                 extra_headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : extra_headers.c_str(),
                                 extra_headers.empty() ? 0 : (DWORD)-1L,
                                 const_cast<void*>(body),
                                 body_len,
                                 body_len,
                                 0);
    if (!ok) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    ok = WinHttpReceiveResponse(hRequest, nullptr);
    if (!ok) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (status_code) {
        DWORD sc = 0;
        DWORD sc_len = sizeof(sc);
        WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &sc,
                            &sc_len,
                            WINHTTP_NO_HEADER_INDEX);
        *status_code = sc;
    }

    bool readOk = read_all_response(hRequest, response);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return readOk;
}

bool proxy_http_request(const std::string& host,
                        unsigned short port,
                        const std::string& method,
                        const std::string& path,
                        const std::string& content_type,
                        const std::string& body,
                        std::string& response,
                        unsigned long* status_code) {
    response.clear();

    auto wHost = widen_utf8(host.empty() ? "127.0.0.1" : host);
    auto wMethod = widen_utf8(method.empty() ? "GET" : method);
    auto wPath = widen_utf8(path.empty() ? "/" : path);
    if (wHost.empty() || wMethod.empty() || wPath.empty()) return false;

    std::wstring headers;
    if (!content_type.empty()) {
        headers += L"Content-Type: ";
        headers += widen_utf8(content_type);
        headers += L"\r\n";
    }
    headers += L"Accept: application/json\r\n";

    const void* body_ptr = body.empty() ? nullptr : body.data();
    DWORD body_len = body.empty() ? 0u : (DWORD)body.size();

    return winhttp_request(wHost, (INTERNET_PORT)port, wMethod, wPath, headers,
                           body_ptr, body_len, response, status_code) &&
           !response.empty();
}

bool proxy_request_to_gguf(const std::string& host, unsigned short port,
                           const std::string& request, std::string& response) {
    unsigned long sc = 0;
    return proxy_http_request(host, port, "POST", "/api/generate", "application/json", request, response, &sc) &&
           (sc == 0 || (sc >= 200 && sc < 300));
}

} // namespace RawrXD::Standalone
