#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winhttp.h>
#include <memory.h>

#define OLLAMA_API_URL "http://localhost:11434/api/generate"
#define OLLAMA_BUFFER_SIZE 4096

#pragma comment(lib, "winhttp.lib")

int send_ollama_request(const char* prompt);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <prompt>\n", argv[0]);
        return 1;
    }

    printf("Ollama Windows Wrapper\n");
    printf("Prompt: %s\n", argv[1]);
    
    int status = send_ollama_request(argv[1]);
    if (status != 0) {
        fprintf(stderr, "Failed to send request (%d)\n", status);
    }

    return status;
}

/// @brief 
/// @param prompt 
/// @return 
int send_ollama_request(const char* prompt) {
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    BOOL bResults = FALSE;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    LPSTR pszOutBuffer = NULL;
    
    // Parse URL: http://localhost:11434/api/generate
    LPCWSTR pwszServerName = L"localhost";
    INTERNET_PORT nPort = 11434;
    LPCWSTR pwszPath = L"/api/generate";
    
    // Build JSON payload
    char jsonPayload[OLLAMA_BUFFER_SIZE];
    snprintf(jsonPayload, sizeof(jsonPayload),
        "{\"model\":\"llama2\",\"prompt\":\"%s\",\"stream\":false}", prompt);
    
    // Initialize WinHTTP
    hSession = WinHttpOpen(L"Ollama Client/1.0",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (!hSession) {
        fprintf(stderr, "Error: WinHttpOpen failed (%lu)\n", GetLastError());
        goto cleanup;
    }
    
    // Connect to server
    hConnect = WinHttpConnect(hSession, pwszServerName, nPort, 0);
    if (!hConnect) {
        fprintf(stderr, "Error: WinHttpConnect failed (%lu)\n", GetLastError());
        goto cleanup;
    }
    
    // Create HTTP request
    hRequest = WinHttpOpenRequest(hConnect, L"POST", pwszPath,
                                  NULL, WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        fprintf(stderr, "Error: WinHttpOpenRequest failed (%lu)\n", GetLastError());
        goto cleanup;
    }
    
    // Set headers
    LPCWSTR pwszHeaders = L"Content-Type: application/json\r\n";
    bResults = WinHttpSendRequest(hRequest, pwszHeaders, -1,
                                  jsonPayload, (DWORD)strlen(jsonPayload),
                                  (DWORD)strlen(jsonPayload), 0);
    
    if (!bResults) {
        fprintf(stderr, "Error: WinHttpSendRequest failed (%lu)\n", GetLastError());
        goto cleanup;
    }
    
    // Receive response
    bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResults) {
        fprintf(stderr, "Error: WinHttpReceiveResponse failed (%lu)\n", GetLastError());
        goto cleanup;
    }
    
    // Read response body
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            fprintf(stderr, "Error: WinHttpQueryDataAvailable failed (%lu)\n", GetLastError());
            break;
        }
        
        if (dwSize == 0) break;
        
        DWORD bufferSize = dwSize + 1;
        if (bufferSize > OLLAMA_BUFFER_SIZE) bufferSize = OLLAMA_BUFFER_SIZE;
        pszOutBuffer = (LPSTR)malloc(bufferSize);
        if (!pszOutBuffer) {
            fprintf(stderr, "Error: Out of memory\n");
            break;
        }
        
        ZeroMemory(pszOutBuffer, bufferSize);
        
        if (WinHttpReadData(hRequest, pszOutBuffer, bufferSize - 1, &dwDownloaded)) {
            printf("%s", pszOutBuffer);
        }
        
        free(pszOutBuffer);
        pszOutBuffer = NULL;
        
    } while (dwSize > 0);
    
cleanup:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    
    return bResults ? 0 : 1;
}

{
  "configurations": [{
    "name": "Win32",
    "includePath": [
      "${workspaceFolder}/**",
      "C:/msys64/mingw64/include/**"
    ],
    "compilerPath": "C:/msys64/mingw64/bin/gcc.exe",
    "cStandard": "c11",
    "intelliSenseMode": "gcc-x64"
  }],
  "version": 4
}