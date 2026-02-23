/**
 * Example: GGUF / Ollama-style API usage (C++20, Qt-free)
 *
 * Demonstrates calling a local inference endpoint (e.g. Ollama on port 11434)
 * with Win32 HTTP. For a full server, use RawrXD-Win32IDE or a dedicated
 * HTTP server process.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include <iostream>
#include <string>
#include <cstdlib>

static bool queryOllamaHealth(const wchar_t* host = L"localhost", int port = 11434) {
#ifdef _WIN32
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Example/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!hSession) return false;
    HINTERNET hConnect = WinHttpConnect(hSession, host, (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/version",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }
    BOOL sent = WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0);
    if (!sent) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }
    BOOL received = WinHttpReceiveResponse(hRequest, nullptr);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return received == TRUE;
#else
    (void)host;
    (void)port;
    return false;
#endif
}

int main(int argc, char* argv[]) {
    std::cout << "=== GGUF / Ollama API Example (C++20) ===" << std::endl;

    int port = 11434;
    if (argc > 1) port = std::atoi(argv[1]);

    std::cout << "Checking health at localhost:" << port << " ..." << std::endl;
    if (queryOllamaHealth(L"localhost", port)) {
        std::cout << "OK: Endpoint is reachable (Ollama or compatible API)." << std::endl;
        std::cout << "  POST http://localhost:" << port << "/api/generate" << std::endl;
        std::cout << "  GET  http://localhost:" << port << "/api/tags" << std::endl;
    } else {
        std::cout << "No server on port " << port << ". Start Ollama or RawrXD-Win32IDE." << std::endl;
    }

    return 0;
}
