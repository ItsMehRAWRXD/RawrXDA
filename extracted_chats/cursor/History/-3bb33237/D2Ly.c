/**
 * BigDaddyG Native Ollama Client - Pure C
 * 
 * Standalone executable that BigDaddyG IDE calls via command line
 * Compiles with MSVC, Clang, or MinGW - no dependencies!
 * 
 * Compile:
 *   cl ollama-native.c /Fe:ollama-native.exe winhttp.lib
 *   gcc ollama-native.c -o ollama-native.exe -lwinhttp
 *   clang ollama-native.c -o ollama-native.exe -lwinhttp
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "winhttp.lib")

// ============================================================================
// CONFIGURATION
// ============================================================================

#define OLLAMA_HOST L"localhost"
#define OLLAMA_PORT 11441
#define OLLAMA_PATH L"/api/chat"
#define BUFFER_SIZE 8192

// ============================================================================
// JSON ESCAPE
// ============================================================================

char* escape_json_string(const char* input) {
    if (!input) return NULL;
    
    size_t len = strlen(input);
    char* output = (char*)malloc(len * 2 + 1);
    if (!output) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (input[i]) {
            case '"':  output[j++] = '\\'; output[j++] = '"'; break;
            case '\\': output[j++] = '\\'; output[j++] = '\\'; break;
            case '\n': output[j++] = '\\'; output[j++] = 'n'; break;
            case '\r': output[j++] = '\\'; output[j++] = 'r'; break;
            case '\t': output[j++] = '\\'; output[j++] = 't'; break;
            default: output[j++] = input[i]; break;
        }
    }
    output[j] = '\0';
    return output;
}

// ============================================================================
// HTTP REQUEST
// ============================================================================

int send_ollama_request(const char* model, const char* prompt) {
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    int success = 0;
    
    // Escape prompt
    char* escaped_prompt = escape_json_string(prompt);
    if (!escaped_prompt) {
        fprintf(stderr, "Error: Failed to escape prompt\n");
        return 0;
    }
    
    // Build JSON request
    char json_buffer[65536];
    int json_len = snprintf(json_buffer, sizeof(json_buffer),
        "{\"message\":\"%s\",\"model\":\"%s\",\"parameters\":{}}",
        escaped_prompt,
        model
    );
    
    free(escaped_prompt);
    
    if (json_len < 0 || json_len >= sizeof(json_buffer)) {
        fprintf(stderr, "Error: Request too large\n");
        return 0;
    }
    
    // Initialize WinHTTP
    hSession = WinHttpOpen(
        L"BigDaddyG-Native/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    
    if (!hSession) {
        fprintf(stderr, "Error: WinHttpOpen failed (%lu)\n", GetLastError());
        goto cleanup;
    }
    
    // Connect to Ollama
    hConnect = WinHttpConnect(hSession, OLLAMA_HOST, OLLAMA_PORT, 0);
    if (!hConnect) {
        fprintf(stderr, "Error: WinHttpConnect failed (%lu)\n", GetLastError());
        goto cleanup;
    }
    
    // Create request
    hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        OLLAMA_PATH,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0
    );
    
    if (!hRequest) {
        fprintf(stderr, "Error: WinHttpOpenRequest failed (%lu)\n", GetLastError());
        goto cleanup;
    }
    
    // Set headers
    LPCWSTR headers = L"Content-Type: application/json\r\n";
    WinHttpAddRequestHeaders(
        hRequest,
        headers,
        (DWORD)-1L,
        WINHTTP_ADDREQ_FLAG_ADD
    );
    
    // Send request
    if (!WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        (LPVOID)json_buffer,
        (DWORD)json_len,
        (DWORD)json_len,
        0
    )) {
        fprintf(stderr, "Error: WinHttpSendRequest failed (%lu)\n", GetLastError());
        goto cleanup;
    }
    
    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        fprintf(stderr, "Error: WinHttpReceiveResponse failed (%lu)\n", GetLastError());
        goto cleanup;
    }
    
    // Get status code
    DWORD status_code = 0;
    DWORD size = sizeof(DWORD);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        NULL,
        &status_code,
        &size,
        NULL
    );
    
    if (status_code != 200) {
        fprintf(stderr, "Error: HTTP %lu\n", status_code);
        goto cleanup;
    }
    
    // Read response
    DWORD bytes_available = 0;
    DWORD bytes_read = 0;
    char buffer[BUFFER_SIZE];
    
    // Output response directly to stdout
    do {
        bytes_available = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytes_available)) {
            break;
        }
        
        if (bytes_available == 0) break;
        
        DWORD to_read = bytes_available < BUFFER_SIZE ? bytes_available : BUFFER_SIZE;
        
        if (!WinHttpReadData(hRequest, buffer, to_read, &bytes_read)) {
            break;
        }
        
        if (bytes_read > 0) {
            fwrite(buffer, 1, bytes_read, stdout);
        }
        
    } while (bytes_available > 0);
    
    success = 1;
    
cleanup:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    
    return success;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <model> <prompt>\n", argv[0]);
        fprintf(stderr, "Example: %s deepseek-r1:1.5b \"Write hello world\"\n", argv[0]);
        return 1;
    }
    
    const char* model = argv[1];
    const char* prompt = argv[2];
    
    if (!send_ollama_request(model, prompt)) {
        return 1;
    }
    
    return 0;
}

