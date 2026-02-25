// RawrXD AI Completion Engine - Pure Win32 (No Qt)
// Replaces: ai_completion_provider.cpp, ghost_text_renderer.cpp
// Code completion via local LLM with ghost text rendering

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE  
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "winhttp.lib")

// ============================================================================
// CONFIGURATION
// ============================================================================
#define MAX_COMPLETION_SIZE     (64 * 1024)  // 64KB max completion
#define MAX_CONTEXT_SIZE        (256 * 1024) // 256KB context
#define MAX_COMPLETIONS         16
#define COMPLETION_DEBOUNCE_MS  150

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef struct {
    wchar_t text[4096];
    float score;
    int line_offset;        // Where completion starts relative to cursor
    int char_offset;
    BOOL is_multiline;
    int line_count;
} CompletionItem;

typedef struct {
    wchar_t endpoint[256];      // e.g., L"localhost"
    WORD port;                  // e.g., 11434 for Ollama
    wchar_t model_name[64];     // e.g., L"codellama"
    char api_path[256];         // e.g., "/api/generate"
    int timeout_ms;
    int max_tokens;
    float temperature;
    BOOL use_fim;               // Fill-in-middle mode
} AICompletionConfig;

typedef struct {
    // Configuration
    AICompletionConfig config;
    
    // HTTP session
    HINTERNET hSession;
    HINTERNET hConnect;
    
    // State
    BOOL pending_request;
    HANDLE request_thread;
    volatile BOOL cancel_requested;
    DWORD last_request_time;
    
    // Current completion context
    char* prefix_buffer;
    char* suffix_buffer;
    char* file_type;
    char* file_path;
    
    // Results
    CompletionItem completions[MAX_COMPLETIONS];
    int completion_count;
    
    // Metrics
    double last_latency_ms;
    int total_requests;
    int successful_completions;
    
    // Callbacks
    void (*on_completions_ready)(CompletionItem* items, int count, void* user_data);
    void (*on_error)(const wchar_t* error, void* user_data);
    void* callback_user_data;
    
    CRITICAL_SECTION cs;
} AICompletionEngine;

// Ghost text state (for overlay rendering)
typedef struct {
    HWND editor_hwnd;           // Editor window to overlay
    HWND overlay_hwnd;          // Ghost text overlay window
    
    wchar_t ghost_text[4096];
    int ghost_line;
    int ghost_column;
    BOOL visible;
    float opacity;
    
    COLORREF ghost_color;       // Default: dim gray
    HFONT ghost_font;
    
    // Animation
    BOOL fading;
    DWORD fade_start;
} GhostTextState;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
static DWORD WINAPI CompletionRequestThread(LPVOID param);
static void ParseOllamaResponse(AICompletionEngine* engine, const char* json);
static LRESULT CALLBACK GhostTextWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================================
// ENGINE CREATION
// ============================================================================

__declspec(dllexport)
AICompletionEngine* AICompletion_Create(const AICompletionConfig* config) {
    AICompletionEngine* engine = (AICompletionEngine*)calloc(1, sizeof(AICompletionEngine));
    if (!engine) return NULL;
    
    InitializeCriticalSection(&engine->cs);
    
    // Set defaults
    if (config) {
        memcpy(&engine->config, config, sizeof(AICompletionConfig));
    } else {
        wcscpy_s(engine->config.endpoint, 256, L"localhost");
        engine->config.port = 11434;  // Ollama default
        wcscpy_s(engine->config.model_name, 64, L"codellama");
        strcpy_s(engine->config.api_path, 256, "/api/generate");
        engine->config.timeout_ms = 10000;
        engine->config.max_tokens = 256;
        engine->config.temperature = 0.2f;
        engine->config.use_fim = TRUE;
    }
    
    // Allocate buffers
    engine->prefix_buffer = (char*)malloc(MAX_CONTEXT_SIZE);
    engine->suffix_buffer = (char*)malloc(MAX_CONTEXT_SIZE);
    engine->file_type = (char*)malloc(64);
    engine->file_path = (char*)malloc(MAX_PATH);
    
    // Create WinHTTP session
    engine->hSession = WinHttpOpen(
        L"RawrXD/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        NULL, NULL, 0
    );
    
    if (engine->hSession) {
        engine->hConnect = WinHttpConnect(
            engine->hSession,
            engine->config.endpoint,
            engine->config.port,
            0
        );
    }
    
    return engine;
}

__declspec(dllexport)
void AICompletion_Destroy(AICompletionEngine* engine) {
    if (!engine) return;
    
    // Cancel pending request
    engine->cancel_requested = TRUE;
    if (engine->request_thread) {
        WaitForSingleObject(engine->request_thread, 1000);
        CloseHandle(engine->request_thread);
    }
    
    // Close HTTP handles
    if (engine->hConnect) WinHttpCloseHandle(engine->hConnect);
    if (engine->hSession) WinHttpCloseHandle(engine->hSession);
    
    // Free buffers
    if (engine->prefix_buffer) free(engine->prefix_buffer);
    if (engine->suffix_buffer) free(engine->suffix_buffer);
    if (engine->file_type) free(engine->file_type);
    if (engine->file_path) free(engine->file_path);
    
    DeleteCriticalSection(&engine->cs);
    free(engine);
}

// ============================================================================
// COMPLETION REQUESTS
// ============================================================================

__declspec(dllexport)
BOOL AICompletion_RequestCompletions(
    AICompletionEngine* engine,
    const char* prefix,
    const char* suffix,
    const char* file_path,
    const char* file_type
) {
    if (!engine || !prefix) return FALSE;
    
    EnterCriticalSection(&engine->cs);
    
    // Debounce - ignore requests too close together
    DWORD now = GetTickCount();
    if (now - engine->last_request_time < COMPLETION_DEBOUNCE_MS) {
        LeaveCriticalSection(&engine->cs);
        return FALSE;
    }
    
    // Cancel any pending request
    if (engine->pending_request) {
        engine->cancel_requested = TRUE;
        LeaveCriticalSection(&engine->cs);
        WaitForSingleObject(engine->request_thread, 500);
        EnterCriticalSection(&engine->cs);
    }
    
    // Store context
    strncpy_s(engine->prefix_buffer, MAX_CONTEXT_SIZE, prefix, _TRUNCATE);
    if (suffix) {
        strncpy_s(engine->suffix_buffer, MAX_CONTEXT_SIZE, suffix, _TRUNCATE);
    } else {
        engine->suffix_buffer[0] = '\0';
    }
    if (file_path) strncpy_s(engine->file_path, MAX_PATH, file_path, _TRUNCATE);
    if (file_type) strncpy_s(engine->file_type, 64, file_type, _TRUNCATE);
    
    engine->last_request_time = now;
    engine->cancel_requested = FALSE;
    engine->pending_request = TRUE;
    
    // Start async request
    engine->request_thread = CreateThread(
        NULL, 0, CompletionRequestThread, engine, 0, NULL
    );
    
    LeaveCriticalSection(&engine->cs);
    return TRUE;
}

__declspec(dllexport)
void AICompletion_Cancel(AICompletionEngine* engine) {
    if (!engine) return;
    engine->cancel_requested = TRUE;
}

// ============================================================================
// HTTP REQUEST THREAD
// ============================================================================

static DWORD WINAPI CompletionRequestThread(LPVOID param) {
    AICompletionEngine* engine = (AICompletionEngine*)param;
    
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    
    // Build JSON request body
    char* request_body = (char*)malloc(MAX_CONTEXT_SIZE + 1024);
    char* escaped_prefix = (char*)malloc(MAX_CONTEXT_SIZE * 2);
    char* escaped_suffix = (char*)malloc(MAX_CONTEXT_SIZE * 2);
    
    // JSON-escape the strings
    int j = 0;
    for (int i = 0; engine->prefix_buffer[i] && j < MAX_CONTEXT_SIZE * 2 - 2; i++) {
        char c = engine->prefix_buffer[i];
        if (c == '"' || c == '\\') escaped_prefix[j++] = '\\';
        if (c == '\n') { escaped_prefix[j++] = '\\'; escaped_prefix[j++] = 'n'; }
        else if (c == '\r') { escaped_prefix[j++] = '\\'; escaped_prefix[j++] = 'r'; }
        else if (c == '\t') { escaped_prefix[j++] = '\\'; escaped_prefix[j++] = 't'; }
        else escaped_prefix[j++] = c;
    }
    escaped_prefix[j] = '\0';
    
    j = 0;
    for (int i = 0; engine->suffix_buffer[i] && j < MAX_CONTEXT_SIZE * 2 - 2; i++) {
        char c = engine->suffix_buffer[i];
        if (c == '"' || c == '\\') escaped_suffix[j++] = '\\';
        if (c == '\n') { escaped_suffix[j++] = '\\'; escaped_suffix[j++] = 'n'; }
        else if (c == '\r') { escaped_suffix[j++] = '\\'; escaped_suffix[j++] = 'r'; }
        else if (c == '\t') { escaped_suffix[j++] = '\\'; escaped_suffix[j++] = 't'; }
        else escaped_suffix[j++] = c;
    }
    escaped_suffix[j] = '\0';
    
    // Convert model name to char
    char model_name[64];
    WideCharToMultiByte(CP_UTF8, 0, engine->config.model_name, -1, model_name, 64, NULL, NULL);
    
    // Build prompt with FIM (Fill-In-Middle) format
    if (engine->config.use_fim) {
        snprintf(request_body, MAX_CONTEXT_SIZE + 1024,
            "{\"model\":\"%s\",\"prompt\":\"<PRE>%s<SUF>%s<MID>\",\"stream\":false,"
            "\"options\":{\"num_predict\":%d,\"temperature\":%.2f}}",
            model_name, escaped_prefix, escaped_suffix,
            engine->config.max_tokens, engine->config.temperature
        );
    } else {
        snprintf(request_body, MAX_CONTEXT_SIZE + 1024,
            "{\"model\":\"%s\",\"prompt\":\"Complete the following code:\\n\\n%s\",\"stream\":false,"
            "\"options\":{\"num_predict\":%d,\"temperature\":%.2f}}",
            model_name, escaped_prefix,
            engine->config.max_tokens, engine->config.temperature
        );
    }
    
    free(escaped_prefix);
    free(escaped_suffix);
    
    if (engine->cancel_requested) {
        free(request_body);
        engine->pending_request = FALSE;
        return 0;
    }
    
    // Make HTTP request
    wchar_t api_path_w[256];
    MultiByteToWideChar(CP_UTF8, 0, engine->config.api_path, -1, api_path_w, 256);
    
    HINTERNET hRequest = WinHttpOpenRequest(
        engine->hConnect,
        L"POST",
        api_path_w,
        NULL, NULL, NULL,
        0
    );
    
    if (!hRequest) {
        free(request_body);
        engine->pending_request = FALSE;
        if (engine->on_error) {
            engine->on_error(L"Failed to create HTTP request", engine->callback_user_data);
        }
        return 0;
    }
    
    // Set headers
    WinHttpAddRequestHeaders(hRequest,
        L"Content-Type: application/json\r\n",
        -1, WINHTTP_ADDREQ_FLAG_ADD);
    
    // Set timeout
    int timeout = engine->config.timeout_ms;
    WinHttpSetTimeouts(hRequest, timeout, timeout, timeout, timeout);
    
    // Send request
    DWORD body_len = (DWORD)strlen(request_body);
    BOOL sent = WinHttpSendRequest(hRequest, NULL, 0, request_body, body_len, body_len, 0);
    
    if (!sent || engine->cancel_requested) {
        WinHttpCloseHandle(hRequest);
        free(request_body);
        engine->pending_request = FALSE;
        return 0;
    }
    
    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        free(request_body);
        engine->pending_request = FALSE;
        if (engine->on_error) {
            engine->on_error(L"Failed to receive response", engine->callback_user_data);
        }
        return 0;
    }
    
    // Read response body
    char* response = (char*)malloc(MAX_COMPLETION_SIZE);
    DWORD total_read = 0;
    DWORD bytes_read;
    
    while (WinHttpReadData(hRequest, response + total_read, 
           MAX_COMPLETION_SIZE - total_read - 1, &bytes_read) && bytes_read > 0) {
        total_read += bytes_read;
        if (engine->cancel_requested) break;
    }
    response[total_read] = '\0';
    
    WinHttpCloseHandle(hRequest);
    free(request_body);
    
    if (engine->cancel_requested) {
        free(response);
        engine->pending_request = FALSE;
        return 0;
    }
    
    // Parse response
    ParseOllamaResponse(engine, response);
    free(response);
    
    // Calculate latency
    QueryPerformanceCounter(&end);
    engine->last_latency_ms = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart) * 1000.0;
    engine->total_requests++;
    
    if (engine->completion_count > 0) {
        engine->successful_completions++;
        
        // Callback with results
        if (engine->on_completions_ready) {
            engine->on_completions_ready(
                engine->completions, 
                engine->completion_count,
                engine->callback_user_data
            );
        }
    }
    
    engine->pending_request = FALSE;
    return 0;
}

// ============================================================================
// RESPONSE PARSING
// ============================================================================

static void ParseOllamaResponse(AICompletionEngine* engine, const char* json) {
    engine->completion_count = 0;
    
    // Find "response" field in Ollama output
    const char* response_key = "\"response\":\"";
    const char* pos = strstr(json, response_key);
    if (!pos) return;
    
    pos += strlen(response_key);
    
    // Extract response text (JSON-unescaped)
    char* completion = (char*)malloc(MAX_COMPLETION_SIZE);
    int j = 0;
    
    while (*pos && *pos != '"' && j < MAX_COMPLETION_SIZE - 1) {
        if (*pos == '\\' && pos[1]) {
            pos++;
            switch (*pos) {
                case 'n': completion[j++] = '\n'; break;
                case 'r': completion[j++] = '\r'; break;
                case 't': completion[j++] = '\t'; break;
                case '"': completion[j++] = '"'; break;
                case '\\': completion[j++] = '\\'; break;
                default: completion[j++] = *pos; break;
            }
        } else {
            completion[j++] = *pos;
        }
        pos++;
    }
    completion[j] = '\0';
    
    // Clean up completion - remove FIM markers if present
    char* clean = completion;
    if (strncmp(clean, "<MID>", 5) == 0) clean += 5;
    char* end_marker = strstr(clean, "<END>");
    if (end_marker) *end_marker = '\0';
    
    // Trim leading/trailing whitespace
    while (*clean == ' ' || *clean == '\t' || *clean == '\n') clean++;
    int len = (int)strlen(clean);
    while (len > 0 && (clean[len-1] == ' ' || clean[len-1] == '\n' || clean[len-1] == '\r')) {
        clean[--len] = '\0';
    }
    
    if (strlen(clean) > 0) {
        // Convert to wide char
        MultiByteToWideChar(CP_UTF8, 0, clean, -1, 
            engine->completions[0].text, 4096);
        engine->completions[0].score = 1.0f;
        engine->completions[0].line_offset = 0;
        engine->completions[0].char_offset = 0;
        engine->completions[0].is_multiline = (strchr(clean, '\n') != NULL);
        engine->completions[0].line_count = 1;
        for (const char* p = clean; *p; p++) {
            if (*p == '\n') engine->completions[0].line_count++;
        }
        
        engine->completion_count = 1;
    }
    
    free(completion);
}

// ============================================================================
// GHOST TEXT OVERLAY
// ============================================================================

static const wchar_t* GHOST_TEXT_CLASS = L"RawrXD_GhostText";
static BOOL ghost_class_registered = FALSE;

__declspec(dllexport)
GhostTextState* GhostText_Create(HWND editor_hwnd) {
    GhostTextState* state = (GhostTextState*)calloc(1, sizeof(GhostTextState));
    if (!state) return NULL;
    
    state->editor_hwnd = editor_hwnd;
    state->ghost_color = RGB(128, 128, 128);  // Gray
    state->opacity = 1.0f;
    
    // Register window class once
    if (!ghost_class_registered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = GhostTextWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = GHOST_TEXT_CLASS;
        wc.hbrBackground = NULL;  // Transparent
        wc.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClassExW(&wc);
        ghost_class_registered = TRUE;
    }
    
    // Create transparent overlay window
    RECT editor_rect;
    GetClientRect(editor_hwnd, &editor_rect);
    
    state->overlay_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        GHOST_TEXT_CLASS,
        NULL,
        WS_POPUP,
        0, 0, 1, 1,  // Will be positioned later
        editor_hwnd,
        NULL,
        GetModuleHandle(NULL),
        state
    );
    
    // Set initial transparency
    SetLayeredWindowAttributes(state->overlay_hwnd, 0, 200, LWA_ALPHA);
    
    return state;
}

__declspec(dllexport)
void GhostText_Destroy(GhostTextState* state) {
    if (!state) return;
    
    if (state->overlay_hwnd) {
        DestroyWindow(state->overlay_hwnd);
    }
    if (state->ghost_font) {
        DeleteObject(state->ghost_font);
    }
    free(state);
}

__declspec(dllexport)
void GhostText_Show(GhostTextState* state, const wchar_t* text, int line, int column) {
    if (!state || !text) return;
    
    wcsncpy_s(state->ghost_text, 4096, text, _TRUNCATE);
    state->ghost_line = line;
    state->ghost_column = column;
    state->visible = TRUE;
    state->opacity = 1.0f;
    state->fading = FALSE;
    
    // Position overlay at cursor location
    // This requires knowing the editor's character position - simplified here
    POINT cursor_pos = { state->ghost_column * 8, state->ghost_line * 16 };  // Approximate
    ClientToScreen(state->editor_hwnd, &cursor_pos);
    
    // Measure text size
    HDC hdc = GetDC(state->overlay_hwnd);
    SIZE text_size;
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &text_size);
    ReleaseDC(state->overlay_hwnd, hdc);
    
    SetWindowPos(state->overlay_hwnd, HWND_TOPMOST,
        cursor_pos.x, cursor_pos.y,
        text_size.cx + 10, text_size.cy + 4,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    
    InvalidateRect(state->overlay_hwnd, NULL, TRUE);
}

__declspec(dllexport)
void GhostText_Hide(GhostTextState* state) {
    if (!state) return;
    state->visible = FALSE;
    ShowWindow(state->overlay_hwnd, SW_HIDE);
}

__declspec(dllexport)
void GhostText_StartFade(GhostTextState* state) {
    if (!state || !state->visible) return;
    state->fading = TRUE;
    state->fade_start = GetTickCount();
}

__declspec(dllexport)
void GhostText_Accept(GhostTextState* state, HWND editor) {
    if (!state || !state->visible || !editor) return;
    
    // Send ghost text to editor as if typed
    for (const wchar_t* p = state->ghost_text; *p; p++) {
        SendMessageW(editor, WM_CHAR, *p, 0);
    }
    
    GhostText_Hide(state);
}

__declspec(dllexport)
void GhostText_AcceptLine(GhostTextState* state, HWND editor) {
    if (!state || !state->visible || !editor) return;
    
    // Send only first line
    for (const wchar_t* p = state->ghost_text; *p && *p != L'\n'; p++) {
        SendMessageW(editor, WM_CHAR, *p, 0);
    }
    
    GhostText_Hide(state);
}

__declspec(dllexport)
void GhostText_AcceptWord(GhostTextState* state, HWND editor) {
    if (!state || !state->visible || !editor) return;
    
    // Send until space/newline
    for (const wchar_t* p = state->ghost_text; *p && *p != L' ' && *p != L'\n'; p++) {
        SendMessageW(editor, WM_CHAR, *p, 0);
    }
}

// ============================================================================
// GHOST TEXT WINDOW PROC
// ============================================================================

static LRESULT CALLBACK GhostTextWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GhostTextState* state = (GhostTextState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return 0;
        }
        
        case WM_PAINT: {
            if (!state || !state->visible) return DefWindowProc(hwnd, msg, wParam, lParam);
            
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Set up drawing
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, state->ghost_color);
            
            if (state->ghost_font) {
                SelectObject(hdc, state->ghost_font);
            }
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            // Draw ghost text with slight transparency effect
            DrawTextW(hdc, state->ghost_text, -1, &rect, 
                DT_LEFT | DT_TOP | DT_NOCLIP);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_TIMER: {
            if (state && state->fading) {
                DWORD elapsed = GetTickCount() - state->fade_start;
                state->opacity = 1.0f - (elapsed / 300.0f);  // 300ms fade
                
                if (state->opacity <= 0.0f) {
                    state->opacity = 0.0f;
                    state->fading = FALSE;
                    KillTimer(hwnd, 1);
                    GhostText_Hide(state);
                } else {
                    SetLayeredWindowAttributes(hwnd, 0, 
                        (BYTE)(state->opacity * 200), LWA_ALPHA);
                }
            }
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// CONFIGURATION HELPERS
// ============================================================================

__declspec(dllexport)
void AICompletion_SetEndpoint(AICompletionEngine* engine, const wchar_t* endpoint, WORD port) {
    if (!engine) return;
    
    EnterCriticalSection(&engine->cs);
    wcscpy_s(engine->config.endpoint, 256, endpoint);
    engine->config.port = port;
    
    // Reconnect
    if (engine->hConnect) WinHttpCloseHandle(engine->hConnect);
    engine->hConnect = WinHttpConnect(engine->hSession, endpoint, port, 0);
    
    LeaveCriticalSection(&engine->cs);
}

__declspec(dllexport)
void AICompletion_SetModel(AICompletionEngine* engine, const wchar_t* model_name) {
    if (!engine) return;
    wcscpy_s(engine->config.model_name, 64, model_name);
}

__declspec(dllexport)
void AICompletion_SetCallbacks(
    AICompletionEngine* engine,
    void (*on_ready)(CompletionItem* items, int count, void* user_data),
    void (*on_error)(const wchar_t* error, void* user_data),
    void* user_data
) {
    if (!engine) return;
    engine->on_completions_ready = on_ready;
    engine->on_error = on_error;
    engine->callback_user_data = user_data;
}

__declspec(dllexport)
double AICompletion_GetLastLatency(AICompletionEngine* engine) {
    return engine ? engine->last_latency_ms : 0.0;
}

__declspec(dllexport)
void AICompletion_GetStats(AICompletionEngine* engine, int* total, int* successful) {
    if (!engine) return;
    if (total) *total = engine->total_requests;
    if (successful) *successful = engine->successful_completions;
}
