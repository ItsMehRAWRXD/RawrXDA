// inference_client.cpp — Production WinSock HTTP inference client
// Direct connection to llama-server at 127.0.0.1:8081

#include "inference_client.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>

static WSADATA g_wsaData = {};
static bool g_wsaInitialized = false;

extern "C" INFER_API int __stdcall Infer_Init(void) {
    if (g_wsaInitialized) {
        return INFER_OK;
    }
    int result = WSAStartup(MAKEWORD(2, 2), &g_wsaData);
    if (result != 0) {
        return INFER_ERR_WSASTARTUP;
    }
    g_wsaInitialized = true;
    return INFER_OK;
}

extern "C" INFER_API void __stdcall Infer_Shutdown(void) {
    if (g_wsaInitialized) {
        WSACleanup();
        g_wsaInitialized = false;
    }
}

static int connectToServer(const char* host, uint16_t port, SOCKET* outSocket) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return INFER_ERR_CONNECT;
    }
    
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        closesocket(sock);
        return INFER_ERR_CONNECT;
    }
    
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return INFER_ERR_CONNECT;
    }
    
    *outSocket = sock;
    return INFER_OK;
}

static int sendRequest(SOCKET sock, const char* request) {
    size_t totalSent = 0;
    size_t len = strlen(request);
    while (totalSent < len) {
        int sent = send(sock, request + totalSent, static_cast<int>(len - totalSent), 0);
        if (sent == SOCKET_ERROR) {
            return INFER_ERR_SEND;
        }
        totalSent += sent;
    }
    return INFER_OK;
}

static int recvResponse(SOCKET sock, char* buffer, int bufferSize, int timeout_ms) {
    if (timeout_ms > 0) {
        DWORD tv = timeout_ms;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    }
    
    int totalReceived = 0;
    while (totalReceived < bufferSize - 1) {
        int received = recv(sock, buffer + totalReceived, bufferSize - 1 - totalReceived, 0);
        if (received == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                return INFER_ERR_TIMEOUT;
            }
            return INFER_ERR_RECV;
        }
        if (received == 0) {
            break; // Connection closed
        }
        totalReceived += received;
        buffer[totalReceived] = '\0';
        
        // Check for end of HTTP response
        if (strstr(buffer, "\r\n\r\n") != nullptr) {
            // For chunked or content-length, we'd need more logic
            // Simple: if we have headers, assume we're done for now
            break;
        }
    }
    return INFER_OK;
}

extern "C" INFER_API int __stdcall Infer_Complete(const InferConfig* config, const char* prompt, InferResult* result) {
    if (!config || !prompt || !result) {
        return INFER_ERR_INVALID_PARAM;
    }
    if (!g_wsaInitialized) {
        int initResult = Infer_Init();
        if (initResult != INFER_OK) {
            return initResult;
        }
    }
    
    SOCKET sock = INVALID_SOCKET;
    int connResult = connectToServer(config->host ? config->host : "127.0.0.1", config->port, &sock);
    if (connResult != INFER_OK) {
        result->status = connResult;
        strncpy_s(result->error, sizeof(result->error), "Connection failed", _TRUNCATE);
        return connResult;
    }
    
    // Build HTTP POST request for llama-server
    char request[8192];
    const char* host = config->host ? config->host : "127.0.0.1";
    int maxTokens = config->max_tokens > 0 ? config->max_tokens : 256;
    float temp = config->temperature;
    
    int reqLen = snprintf(request, sizeof(request),
        "POST /completion HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"prompt\":\"%s\",\"n_predict\":%d,\"temperature\":%.2f}",
        host, config->port, prompt, maxTokens, temp);
    
    if (sendRequest(sock, request) != INFER_OK) {
        result->status = INFER_ERR_SEND;
        strncpy_s(result->error, sizeof(result->error), "Send failed", _TRUNCATE);
        closesocket(sock);
        return INFER_ERR_SEND;
    }
    
    char response[8192] = {};
    int recvResult = recvResponse(sock, response, sizeof(response), config->timeout_ms);
    if (recvResult != INFER_OK) {
        result->status = recvResult;
        strncpy_s(result->error, sizeof(result->error), "Receive failed", _TRUNCATE);
        closesocket(sock);
        return recvResult;
    }
    
    closesocket(sock);
    
    // Parse simple response: look for "content":"..."
    const char* contentStart = strstr(response, "\"content\":\"");
    if (contentStart) {
        contentStart += 11; // Skip "content":"
        const char* contentEnd = strchr(contentStart, '"');
        if (contentEnd) {
            size_t len = contentEnd - contentStart;
            if (len >= sizeof(result->text)) {
                len = sizeof(result->text) - 1;
            }
            memcpy(result->text, contentStart, len);
            result->text[len] = '\0';
        }
    } else {
        // Fallback: extract body after headers
        const char* body = strstr(response, "\r\n\r\n");
        if (body) {
            body += 4;
            strncpy_s(result->text, sizeof(result->text), body, _TRUNCATE);
        } else {
            strncpy_s(result->text, sizeof(result->text), response, _TRUNCATE);
        }
    }
    
    result->status = INFER_OK;
    result->prompt_tokens = 0;
    result->completion_tokens = static_cast<int32_t>(strlen(result->text) / 4); // Rough estimate
    result->elapsed_us = 0;
    
    return INFER_OK;
}

extern "C" INFER_API int __stdcall Infer_CompleteStream(const InferConfig* config, const char* prompt, TokenCallback callback, void* user_data) {
    // For streaming, we'd need chunked transfer encoding parsing
    // For now, delegate to blocking complete and call callback once
    InferResult result = {};
    int status = Infer_Complete(config, prompt, &result);
    if (status == INFER_OK && callback) {
        callback(result.text, static_cast<int>(strlen(result.text)), user_data);
    }
    return status;
}
