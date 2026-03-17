#include <iostream>
#include <string>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

int main() {
    std::cout << "Testing WinHTTP connection to Ollama...\n";

    // Initialize WinHTTP
    HINTERNET hSession = WinHttpOpen(L"Test/1.0",
                                     WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        std::cerr << "WinHttpOpen failed: " << GetLastError() << "\n";
        return 1;
    }

    // Set timeouts
    DWORD timeout = 10000; // 10 seconds
    WinHttpSetTimeouts(hSession, timeout, timeout, timeout, timeout);

    // Connect
    HINTERNET hConnect = WinHttpConnect(hSession, L"localhost", 11434, 0);
    if (!hConnect) {
        std::cerr << "WinHttpConnect failed: " << GetLastError() << "\n";
        WinHttpCloseHandle(hSession);
        return 1;
    }

    // Open request
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/version",
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        std::cerr << "WinHttpOpenRequest failed: " << GetLastError() << "\n";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1;
    }

    // Send request
    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent) {
        std::cerr << "WinHttpSendRequest failed: " << GetLastError() << "\n";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        std::cerr << "WinHttpReceiveResponse failed: " << GetLastError() << "\n";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1;
    }

    // Read response
    std::string response;
    char buffer[4096];
    DWORD bytesRead;
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }

    // Cleanup
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    std::cout << "Response: " << response << "\n";
    return response.empty() ? 1 : 0;
}