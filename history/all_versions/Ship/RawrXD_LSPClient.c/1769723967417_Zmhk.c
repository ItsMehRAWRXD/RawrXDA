// RawrXD LSP Client - Pure Win32 (No Qt)
// Replaces: lsp_client.cpp
// JSON-RPC communication with language servers via CreateProcess + pipes

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE  
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#pragma comment(lib, "kernel32.lib")

// ============================================================================
// CONFIGURATION
// ============================================================================
#define MAX_MESSAGE_SIZE    (1024 * 1024)  // 1MB
#define MAX_PENDING_REQUESTS 64

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef struct {
    wchar_t command[MAX_PATH];
    wchar_t arguments[1024];
    wchar_t workspace_root[MAX_PATH];
    wchar_t language[32];
    BOOL auto_start;
} LSPServerConfig;

typedef struct {
    int id;
    char method[64];
    void (*callback)(const char* result, void* user_data);
    void* user_data;
    BOOL pending;
} PendingRequest;

typedef struct {
    // Process handles
    HANDLE hProcess;
    HANDLE hStdinWrite;
    HANDLE hStdoutRead;
    HANDLE hReaderThread;
    DWORD dwPid;
    BOOL running;
    BOOL initialized;
    
    // Configuration
    LSPServerConfig config;
    
    // Request tracking
    int next_request_id;
    PendingRequest pending_requests[MAX_PENDING_REQUESTS];
    
    // Message buffer
    char* recv_buffer;
    int recv_buffer_size;
    int recv_buffer_pos;
    
    // Callbacks
    void (*on_diagnostics)(const wchar_t* uri, const char* diagnostics_json, void* user_data);
    void (*on_completion)(const char* completions_json, void* user_data);
    void (*on_hover)(const char* hover_json, void* user_data);
    void (*on_definition)(const char* location_json, void* user_data);
    void (*on_error)(const wchar_t* error, void* user_data);
    void* callback_user_data;
    
    CRITICAL_SECTION cs;
} LSPClient;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
static DWORD WINAPI LSPReaderThread(LPVOID param);
static void LSP_ProcessMessage(LSPClient* client, const char* message, int length);
static void LSP_SendMessage(LSPClient* client, const char* json);

// ============================================================================
// JSON HELPERS (Simple, no external lib)
// ============================================================================

// Build a JSON-RPC request string
static int JSON_BuildRequest(char* buffer, int buffer_size, int id, const char* method, const char* params) {
    if (params && *params) {
        return snprintf(buffer, buffer_size,
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\",\"params\":%s}",
            id, method, params);
    } else {
        return snprintf(buffer, buffer_size,
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\"}",
            id, method);
    }
}

// Build a JSON-RPC notification string (no id)
static int JSON_BuildNotification(char* buffer, int buffer_size, const char* method, const char* params) {
    if (params && *params) {
        return snprintf(buffer, buffer_size,
            "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}",
            method, params);
    } else {
        return snprintf(buffer, buffer_size,
            "{\"jsonrpc\":\"2.0\",\"method\":\"%s\"}",
            method);
    }
}

// Simple JSON value extraction (returns pointer to value, sets length)
static const char* JSON_GetValue(const char* json, const char* key, int* out_len) {
    char search[128];
    snprintf(search, 128, "\"%s\":", key);
    
    const char* pos = strstr(json, search);
    if (!pos) return NULL;
    
    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;
    
    const char* start = pos;
    
    if (*pos == '"') {
        // String value
        pos++;
        start = pos;
        while (*pos && *pos != '"') {
            if (*pos == '\\') pos++;
            pos++;
        }
        *out_len = (int)(pos - start);
    } else if (*pos == '{' || *pos == '[') {
        // Object or array - find matching brace
        char open = *pos;
        char close = (open == '{') ? '}' : ']';
        int depth = 1;
        pos++;
        while (*pos && depth > 0) {
            if (*pos == open) depth++;
            else if (*pos == close) depth--;
            else if (*pos == '"') {
                pos++;
                while (*pos && *pos != '"') {
                    if (*pos == '\\') pos++;
                    pos++;
                }
            }
            pos++;
        }
        *out_len = (int)(pos - start);
    } else {
        // Number, bool, null
        while (*pos && *pos != ',' && *pos != '}' && *pos != ']') pos++;
        *out_len = (int)(pos - start);
    }
    
    return start;
}

static int JSON_GetInt(const char* json, const char* key, int default_val) {
    int len;
    const char* val = JSON_GetValue(json, key, &len);
    if (!val) return default_val;
    return atoi(val);
}

// ============================================================================
// CLIENT CREATION
// ============================================================================

__declspec(dllexport)
LSPClient* LSPClient_Create(const LSPServerConfig* config) {
    LSPClient* client = (LSPClient*)calloc(1, sizeof(LSPClient));
    if (!client) return NULL;
    
    InitializeCriticalSection(&client->cs);
    
    if (config) {
        memcpy(&client->config, config, sizeof(LSPServerConfig));
    }
    
    client->recv_buffer = (char*)malloc(MAX_MESSAGE_SIZE);
    client->recv_buffer_size = MAX_MESSAGE_SIZE;
    client->next_request_id = 1;
    
    return client;
}

__declspec(dllexport)
void LSPClient_Destroy(LSPClient* client) {
    if (!client) return;
    
    LSPClient_StopServer(client);
    
    DeleteCriticalSection(&client->cs);
    if (client->recv_buffer) free(client->recv_buffer);
    free(client);
}

// ============================================================================
// SERVER MANAGEMENT
// ============================================================================

__declspec(dllexport)
BOOL LSPClient_StartServer(LSPClient* client) {
    if (!client || client->running) return FALSE;
    
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hStdinRead, hStdoutWrite;
    
    // Create pipes
    if (!CreatePipe(&hStdinRead, &client->hStdinWrite, &sa, 0)) return FALSE;
    if (!CreatePipe(&client->hStdoutRead, &hStdoutWrite, &sa, 0)) {
        CloseHandle(hStdinRead);
        CloseHandle(client->hStdinWrite);
        return FALSE;
    }
    
    // Don't inherit our end of pipes
    SetHandleInformation(client->hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(client->hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    
    PROCESS_INFORMATION pi;
    wchar_t cmdline[2048];
    swprintf(cmdline, 2048, L"\"%s\" %s", client->config.command, client->config.arguments);
    
    if (!CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, 
                        client->config.workspace_root[0] ? client->config.workspace_root : NULL, 
                        &si, &pi)) {
        CloseHandle(hStdinRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(client->hStdinWrite);
        CloseHandle(client->hStdoutRead);
        return FALSE;
    }
    
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
    CloseHandle(pi.hThread);
    
    client->hProcess = pi.hProcess;
    client->dwPid = pi.dwProcessId;
    client->running = TRUE;
    
    // Start reader thread
    client->hReaderThread = CreateThread(NULL, 0, LSPReaderThread, client, 0, NULL);
    
    // Send initialize request
    char params[4096];
    char uri[MAX_PATH * 2];
    
    // Convert workspace to file URI
    if (client->config.workspace_root[0]) {
        WideCharToMultiByte(CP_UTF8, 0, client->config.workspace_root, -1, uri, MAX_PATH * 2, NULL, NULL);
        // Replace backslashes
        for (char* p = uri; *p; p++) if (*p == '\\') *p = '/';
    } else {
        strcpy(uri, "");
    }
    
    snprintf(params, sizeof(params),
        "{"
        "\"processId\":%d,"
        "\"rootUri\":\"file:///%s\","
        "\"capabilities\":{"
            "\"textDocument\":{"
                "\"completion\":{\"completionItem\":{\"snippetSupport\":false}},"
                "\"hover\":{},"
                "\"definition\":{},"
                "\"publishDiagnostics\":{\"relatedInformation\":true},"
                "\"formatting\":{}"
            "}"
        "}"
        "}",
        GetCurrentProcessId(),
        uri);
    
    char request[8192];
    int len = JSON_BuildRequest(request, sizeof(request), client->next_request_id++, "initialize", params);
    
    // Store pending request
    for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
        if (!client->pending_requests[i].pending) {
            client->pending_requests[i].id = client->next_request_id - 1;
            strcpy(client->pending_requests[i].method, "initialize");
            client->pending_requests[i].pending = TRUE;
            break;
        }
    }
    
    LSP_SendMessage(client, request);
    
    return TRUE;
}

__declspec(dllexport)
void LSPClient_StopServer(LSPClient* client) {
    if (!client || !client->running) return;
    
    // Send shutdown
    char request[256];
    JSON_BuildRequest(request, sizeof(request), client->next_request_id++, "shutdown", NULL);
    LSP_SendMessage(client, request);
    
    // Send exit
    char notify[256];
    JSON_BuildNotification(notify, sizeof(notify), "exit", NULL);
    LSP_SendMessage(client, notify);
    
    client->running = FALSE;
    
    // Wait for reader thread
    if (client->hReaderThread) {
        WaitForSingleObject(client->hReaderThread, 2000);
        CloseHandle(client->hReaderThread);
        client->hReaderThread = NULL;
    }
    
    // Close handles
    if (client->hStdinWrite) { CloseHandle(client->hStdinWrite); client->hStdinWrite = NULL; }
    if (client->hStdoutRead) { CloseHandle(client->hStdoutRead); client->hStdoutRead = NULL; }
    if (client->hProcess) {
        TerminateProcess(client->hProcess, 0);
        CloseHandle(client->hProcess);
        client->hProcess = NULL;
    }
    
    client->initialized = FALSE;
}

// ============================================================================
// MESSAGE SENDING
// ============================================================================

static void LSP_SendMessage(LSPClient* client, const char* json) {
    if (!client || !client->running) return;
    
    int content_length = (int)strlen(json);
    char header[128];
    int header_len = snprintf(header, sizeof(header), "Content-Length: %d\r\n\r\n", content_length);
    
    EnterCriticalSection(&client->cs);
    
    DWORD written;
    WriteFile(client->hStdinWrite, header, header_len, &written, NULL);
    WriteFile(client->hStdinWrite, json, content_length, &written, NULL);
    
    LeaveCriticalSection(&client->cs);
}

// ============================================================================
// DOCUMENT OPERATIONS
// ============================================================================

__declspec(dllexport)
void LSPClient_OpenDocument(LSPClient* client, const wchar_t* path, const wchar_t* language, const wchar_t* text) {
    if (!client || !client->initialized) return;
    
    char uri[MAX_PATH * 2];
    char lang[64];
    
    // Convert path to URI
    WideCharToMultiByte(CP_UTF8, 0, path, -1, uri, sizeof(uri), NULL, NULL);
    for (char* p = uri; *p; p++) if (*p == '\\') *p = '/';
    
    WideCharToMultiByte(CP_UTF8, 0, language, -1, lang, sizeof(lang), NULL, NULL);
    
    // Convert text to UTF-8
    int textLen = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    char* utf8Text = (char*)malloc(textLen * 2);  // Extra space for escaping
    WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8Text, textLen, NULL, NULL);
    
    // Escape JSON special chars in text
    char* escapedText = (char*)malloc(textLen * 4);
    char* src = utf8Text;
    char* dst = escapedText;
    while (*src) {
        if (*src == '"') { *dst++ = '\\'; *dst++ = '"'; }
        else if (*src == '\\') { *dst++ = '\\'; *dst++ = '\\'; }
        else if (*src == '\n') { *dst++ = '\\'; *dst++ = 'n'; }
        else if (*src == '\r') { *dst++ = '\\'; *dst++ = 'r'; }
        else if (*src == '\t') { *dst++ = '\\'; *dst++ = 't'; }
        else *dst++ = *src;
        src++;
    }
    *dst = 0;
    
    char* params = (char*)malloc(strlen(escapedText) + 1024);
    sprintf(params,
        "{\"textDocument\":{\"uri\":\"file:///%s\",\"languageId\":\"%s\",\"version\":1,\"text\":\"%s\"}}",
        uri, lang, escapedText);
    
    char* notify = (char*)malloc(strlen(params) + 256);
    JSON_BuildNotification(notify, (int)strlen(params) + 256, "textDocument/didOpen", params);
    LSP_SendMessage(client, notify);
    
    free(notify);
    free(params);
    free(escapedText);
    free(utf8Text);
}

__declspec(dllexport)
void LSPClient_CloseDocument(LSPClient* client, const wchar_t* path) {
    if (!client || !client->initialized) return;
    
    char uri[MAX_PATH * 2];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, uri, sizeof(uri), NULL, NULL);
    for (char* p = uri; *p; p++) if (*p == '\\') *p = '/';
    
    char params[1024];
    snprintf(params, sizeof(params), "{\"textDocument\":{\"uri\":\"file:///%s\"}}", uri);
    
    char notify[2048];
    JSON_BuildNotification(notify, sizeof(notify), "textDocument/didClose", params);
    LSP_SendMessage(client, notify);
}

__declspec(dllexport)
void LSPClient_RequestCompletion(LSPClient* client, const wchar_t* path, int line, int character,
                                  void (*callback)(const char*, void*), void* user_data) {
    if (!client || !client->initialized) return;
    
    char uri[MAX_PATH * 2];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, uri, sizeof(uri), NULL, NULL);
    for (char* p = uri; *p; p++) if (*p == '\\') *p = '/';
    
    char params[1024];
    snprintf(params, sizeof(params),
        "{\"textDocument\":{\"uri\":\"file:///%s\"},\"position\":{\"line\":%d,\"character\":%d}}",
        uri, line, character);
    
    int id = client->next_request_id++;
    
    // Store callback
    for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
        if (!client->pending_requests[i].pending) {
            client->pending_requests[i].id = id;
            strcpy(client->pending_requests[i].method, "textDocument/completion");
            client->pending_requests[i].callback = callback;
            client->pending_requests[i].user_data = user_data;
            client->pending_requests[i].pending = TRUE;
            break;
        }
    }
    
    char request[2048];
    JSON_BuildRequest(request, sizeof(request), id, "textDocument/completion", params);
    LSP_SendMessage(client, request);
}

__declspec(dllexport)
void LSPClient_RequestHover(LSPClient* client, const wchar_t* path, int line, int character,
                            void (*callback)(const char*, void*), void* user_data) {
    if (!client || !client->initialized) return;
    
    char uri[MAX_PATH * 2];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, uri, sizeof(uri), NULL, NULL);
    for (char* p = uri; *p; p++) if (*p == '\\') *p = '/';
    
    char params[1024];
    snprintf(params, sizeof(params),
        "{\"textDocument\":{\"uri\":\"file:///%s\"},\"position\":{\"line\":%d,\"character\":%d}}",
        uri, line, character);
    
    int id = client->next_request_id++;
    
    for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
        if (!client->pending_requests[i].pending) {
            client->pending_requests[i].id = id;
            strcpy(client->pending_requests[i].method, "textDocument/hover");
            client->pending_requests[i].callback = callback;
            client->pending_requests[i].user_data = user_data;
            client->pending_requests[i].pending = TRUE;
            break;
        }
    }
    
    char request[2048];
    JSON_BuildRequest(request, sizeof(request), id, "textDocument/hover", params);
    LSP_SendMessage(client, request);
}

__declspec(dllexport)
void LSPClient_RequestDefinition(LSPClient* client, const wchar_t* path, int line, int character,
                                  void (*callback)(const char*, void*), void* user_data) {
    if (!client || !client->initialized) return;
    
    char uri[MAX_PATH * 2];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, uri, sizeof(uri), NULL, NULL);
    for (char* p = uri; *p; p++) if (*p == '\\') *p = '/';
    
    char params[1024];
    snprintf(params, sizeof(params),
        "{\"textDocument\":{\"uri\":\"file:///%s\"},\"position\":{\"line\":%d,\"character\":%d}}",
        uri, line, character);
    
    int id = client->next_request_id++;
    
    for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
        if (!client->pending_requests[i].pending) {
            client->pending_requests[i].id = id;
            strcpy(client->pending_requests[i].method, "textDocument/definition");
            client->pending_requests[i].callback = callback;
            client->pending_requests[i].user_data = user_data;
            client->pending_requests[i].pending = TRUE;
            break;
        }
    }
    
    char request[2048];
    JSON_BuildRequest(request, sizeof(request), id, "textDocument/definition", params);
    LSP_SendMessage(client, request);
}

// ============================================================================
// MESSAGE PROCESSING
// ============================================================================

static DWORD WINAPI LSPReaderThread(LPVOID param) {
    LSPClient* client = (LSPClient*)param;
    char header[256];
    int header_pos = 0;
    int content_length = 0;
    BOOL reading_header = TRUE;
    
    while (client->running) {
        DWORD available = 0;
        if (!PeekNamedPipe(client->hStdoutRead, NULL, 0, NULL, &available, NULL)) {
            Sleep(10);
            continue;
        }
        
        if (available == 0) {
            Sleep(10);
            continue;
        }
        
        char byte;
        DWORD bytesRead;
        if (!ReadFile(client->hStdoutRead, &byte, 1, &bytesRead, NULL) || bytesRead == 0) {
            continue;
        }
        
        if (reading_header) {
            header[header_pos++] = byte;
            header[header_pos] = 0;
            
            // Check for end of header
            if (header_pos >= 4 && strcmp(header + header_pos - 4, "\r\n\r\n") == 0) {
                // Parse Content-Length
                const char* cl = strstr(header, "Content-Length:");
                if (cl) {
                    content_length = atoi(cl + 15);
                }
                reading_header = FALSE;
                client->recv_buffer_pos = 0;
            }
        } else {
            if (client->recv_buffer_pos < client->recv_buffer_size - 1) {
                client->recv_buffer[client->recv_buffer_pos++] = byte;
            }
            
            if (client->recv_buffer_pos >= content_length) {
                client->recv_buffer[client->recv_buffer_pos] = 0;
                LSP_ProcessMessage(client, client->recv_buffer, client->recv_buffer_pos);
                
                // Reset for next message
                reading_header = TRUE;
                header_pos = 0;
                content_length = 0;
            }
        }
    }
    
    return 0;
}

static void LSP_ProcessMessage(LSPClient* client, const char* message, int length) {
    (void)length;
    
    // Check for response (has "id" and "result" or "error")
    int idLen;
    const char* idVal = JSON_GetValue(message, "id", &idLen);
    
    if (idVal) {
        int id = atoi(idVal);
        
        // Find pending request
        for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
            if (client->pending_requests[i].pending && client->pending_requests[i].id == id) {
                // Handle initialize response
                if (strcmp(client->pending_requests[i].method, "initialize") == 0) {
                    client->initialized = TRUE;
                    
                    // Send initialized notification
                    char notify[256];
                    JSON_BuildNotification(notify, sizeof(notify), "initialized", "{}");
                    LSP_SendMessage(client, notify);
                }
                
                // Call callback if present
                if (client->pending_requests[i].callback) {
                    int resultLen;
                    const char* result = JSON_GetValue(message, "result", &resultLen);
                    if (result) {
                        char* resultCopy = (char*)malloc(resultLen + 1);
                        memcpy(resultCopy, result, resultLen);
                        resultCopy[resultLen] = 0;
                        client->pending_requests[i].callback(resultCopy, client->pending_requests[i].user_data);
                        free(resultCopy);
                    }
                }
                
                client->pending_requests[i].pending = FALSE;
                break;
            }
        }
    } else {
        // Notification (no id)
        int methodLen;
        const char* method = JSON_GetValue(message, "method", &methodLen);
        
        if (method && strncmp(method, "textDocument/publishDiagnostics", methodLen) == 0) {
            // Diagnostics notification
            if (client->on_diagnostics) {
                int paramsLen;
                const char* params = JSON_GetValue(message, "params", &paramsLen);
                if (params) {
                    int uriLen;
                    const char* uri = JSON_GetValue(params, "uri", &uriLen);
                    if (uri) {
                        wchar_t wuri[MAX_PATH];
                        char uriCopy[MAX_PATH * 2];
                        memcpy(uriCopy, uri, uriLen < MAX_PATH * 2 - 1 ? uriLen : MAX_PATH * 2 - 1);
                        uriCopy[uriLen < MAX_PATH * 2 - 1 ? uriLen : MAX_PATH * 2 - 1] = 0;
                        MultiByteToWideChar(CP_UTF8, 0, uriCopy, -1, wuri, MAX_PATH);
                        
                        char* paramsCopy = (char*)malloc(paramsLen + 1);
                        memcpy(paramsCopy, params, paramsLen);
                        paramsCopy[paramsLen] = 0;
                        
                        client->on_diagnostics(wuri, paramsCopy, client->callback_user_data);
                        free(paramsCopy);
                    }
                }
            }
        }
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

__declspec(dllexport)
void LSPClient_SetDiagnosticsCallback(LSPClient* client, 
                                       void (*callback)(const wchar_t*, const char*, void*),
                                       void* user_data) {
    if (client) {
        client->on_diagnostics = callback;
        client->callback_user_data = user_data;
    }
}

// ============================================================================
// DLL ENTRY
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    return TRUE;
}
