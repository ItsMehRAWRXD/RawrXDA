// RawrXD Model Router - Pure Win32 (No Qt)
// Replaces: model_router_widget.cpp, universal_model_router.cpp, multi_modal_model_router.cpp
// Routes inference requests to appropriate backends (local GGUF, Ollama, cloud APIs)

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
#define MAX_MODELS          64
#define MAX_BACKENDS        16
#define MAX_PROMPT_SIZE     (512 * 1024)  // 512KB
#define MAX_RESPONSE_SIZE   (1024 * 1024) // 1MB

// ============================================================================
// ENUMS
// ============================================================================

typedef enum {
    BACKEND_LOCAL_GGUF = 0,
    BACKEND_OLLAMA,
    BACKEND_OPENAI,
    BACKEND_ANTHROPIC,
    BACKEND_HUGGINGFACE,
    BACKEND_CUSTOM_HTTP
} BackendType;

typedef enum {
    MODEL_CAP_CHAT = 0x01,
    MODEL_CAP_COMPLETION = 0x02,
    MODEL_CAP_EMBEDDING = 0x04,
    MODEL_CAP_CODE = 0x08,
    MODEL_CAP_VISION = 0x10,
    MODEL_CAP_REASONING = 0x20
} ModelCapability;

typedef enum {
    ROUTE_AUTO = 0,
    ROUTE_BY_CAPABILITY,
    ROUTE_BY_LATENCY,
    ROUTE_BY_COST,
    ROUTE_ROUND_ROBIN,
    ROUTE_SPECIFIC
} RoutingStrategy;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef struct {
    wchar_t name[64];
    wchar_t display_name[128];
    BackendType backend;
    int capabilities;               // Bitmask of ModelCapability
    
    // Backend-specific config
    wchar_t model_path[MAX_PATH];   // For GGUF
    wchar_t endpoint[256];          // For HTTP backends
    wchar_t api_key[256];           // For cloud APIs
    WORD port;
    
    // Performance metrics
    double avg_latency_ms;
    int request_count;
    int error_count;
    BOOL available;
    
    // Parameters
    int context_length;
    int max_tokens;
    float temperature;
    float top_p;
} ModelInfo;

typedef struct {
    wchar_t name[64];
    BackendType type;
    wchar_t endpoint[256];
    wchar_t api_key[256];
    WORD port;
    
    HINTERNET hSession;
    HINTERNET hConnect;
    BOOL connected;
    
    int timeout_ms;
    double avg_latency_ms;
    int request_count;
} Backend;

typedef struct {
    // Registered models and backends
    ModelInfo models[MAX_MODELS];
    int model_count;
    Backend backends[MAX_BACKENDS];
    int backend_count;
    
    // Routing
    RoutingStrategy strategy;
    int current_model_index;        // For round-robin
    
    // Local inference engine (function pointer)
    char* (*local_generate)(const char* model_path, const char* prompt, int max_tokens, void* engine);
    void* local_engine;
    
    // Callbacks
    void (*on_model_changed)(const wchar_t* model_name, void* user_data);
    void (*on_response)(const char* response, void* user_data);
    void (*on_stream_chunk)(const char* chunk, void* user_data);
    void (*on_error)(const wchar_t* error, void* user_data);
    void* callback_user_data;
    
    CRITICAL_SECTION cs;
} ModelRouter;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
static char* CallOllama(ModelRouter* router, ModelInfo* model, const char* prompt, int max_tokens);
static char* CallOpenAI(ModelRouter* router, ModelInfo* model, const char* prompt, int max_tokens);
static char* CallLocalGGUF(ModelRouter* router, ModelInfo* model, const char* prompt, int max_tokens);
static ModelInfo* SelectModel(ModelRouter* router, int required_caps);

// ============================================================================
// ROUTER CREATION
// ============================================================================

__declspec(dllexport)
ModelRouter* ModelRouter_Create(void) {
    ModelRouter* router = (ModelRouter*)calloc(1, sizeof(ModelRouter));
    if (!router) return NULL;
    
    InitializeCriticalSection(&router->cs);
    router->strategy = ROUTE_AUTO;
    
    return router;
}

__declspec(dllexport)
void ModelRouter_Destroy(ModelRouter* router) {
    if (!router) return;
    
    // Close backend connections
    for (int i = 0; i < router->backend_count; i++) {
        if (router->backends[i].hConnect) WinHttpCloseHandle(router->backends[i].hConnect);
        if (router->backends[i].hSession) WinHttpCloseHandle(router->backends[i].hSession);
    }
    
    DeleteCriticalSection(&router->cs);
    free(router);
}

// ============================================================================
// MODEL REGISTRATION
// ============================================================================

__declspec(dllexport)
int ModelRouter_RegisterModel(
    ModelRouter* router,
    const wchar_t* name,
    const wchar_t* display_name,
    BackendType backend,
    int capabilities,
    const wchar_t* model_path_or_endpoint
) {
    if (!router || router->model_count >= MAX_MODELS) return -1;
    
    EnterCriticalSection(&router->cs);
    
    ModelInfo* m = &router->models[router->model_count];
    memset(m, 0, sizeof(ModelInfo));
    
    wcsncpy_s(m->name, 64, name, _TRUNCATE);
    wcsncpy_s(m->display_name, 128, display_name ? display_name : name, _TRUNCATE);
    m->backend = backend;
    m->capabilities = capabilities;
    m->available = TRUE;
    
    // Default parameters
    m->context_length = 4096;
    m->max_tokens = 512;
    m->temperature = 0.7f;
    m->top_p = 0.9f;
    
    if (model_path_or_endpoint) {
        if (backend == BACKEND_LOCAL_GGUF) {
            wcsncpy_s(m->model_path, MAX_PATH, model_path_or_endpoint, _TRUNCATE);
        } else {
            wcsncpy_s(m->endpoint, 256, model_path_or_endpoint, _TRUNCATE);
        }
    }
    
    int index = router->model_count++;
    
    LeaveCriticalSection(&router->cs);
    return index;
}

__declspec(dllexport)
void ModelRouter_SetModelApiKey(ModelRouter* router, int model_index, const wchar_t* api_key) {
    if (!router || model_index < 0 || model_index >= router->model_count) return;
    wcsncpy_s(router->models[model_index].api_key, 256, api_key, _TRUNCATE);
}

__declspec(dllexport)
void ModelRouter_SetModelPort(ModelRouter* router, int model_index, WORD port) {
    if (!router || model_index < 0 || model_index >= router->model_count) return;
    router->models[model_index].port = port;
}

__declspec(dllexport)
void ModelRouter_SetModelParams(ModelRouter* router, int model_index, 
                                 int context_len, int max_tokens, float temp, float top_p) {
    if (!router || model_index < 0 || model_index >= router->model_count) return;
    
    ModelInfo* m = &router->models[model_index];
    m->context_length = context_len;
    m->max_tokens = max_tokens;
    m->temperature = temp;
    m->top_p = top_p;
}

// ============================================================================
// BACKEND REGISTRATION
// ============================================================================

__declspec(dllexport)
int ModelRouter_RegisterBackend(
    ModelRouter* router,
    const wchar_t* name,
    BackendType type,
    const wchar_t* endpoint,
    WORD port,
    const wchar_t* api_key
) {
    if (!router || router->backend_count >= MAX_BACKENDS) return -1;
    
    EnterCriticalSection(&router->cs);
    
    Backend* b = &router->backends[router->backend_count];
    memset(b, 0, sizeof(Backend));
    
    wcsncpy_s(b->name, 64, name, _TRUNCATE);
    b->type = type;
    wcsncpy_s(b->endpoint, 256, endpoint, _TRUNCATE);
    b->port = port;
    if (api_key) wcsncpy_s(b->api_key, 256, api_key, _TRUNCATE);
    b->timeout_ms = 30000;  // 30s default
    
    // Create HTTP session for HTTP backends
    if (type != BACKEND_LOCAL_GGUF) {
        b->hSession = WinHttpOpen(
            L"RawrXD-ModelRouter/1.0",
            WINHTTP_ACCESS_TYPE_NO_PROXY,
            NULL, NULL, 0
        );
        
        if (b->hSession) {
            b->hConnect = WinHttpConnect(b->hSession, endpoint, port, 0);
            b->connected = (b->hConnect != NULL);
        }
    }
    
    int index = router->backend_count++;
    
    LeaveCriticalSection(&router->cs);
    return index;
}

// ============================================================================
// INFERENCE ROUTING
// ============================================================================

__declspec(dllexport)
char* ModelRouter_Generate(
    ModelRouter* router,
    const char* prompt,
    int max_tokens,
    int required_capabilities
) {
    if (!router || !prompt) return NULL;
    
    EnterCriticalSection(&router->cs);
    
    // Select model based on strategy
    ModelInfo* model = SelectModel(router, required_capabilities);
    if (!model) {
        LeaveCriticalSection(&router->cs);
        if (router->on_error) {
            router->on_error(L"No available model for required capabilities", router->callback_user_data);
        }
        return NULL;
    }
    
    char* response = NULL;
    LARGE_INTEGER start, end, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    
    // Route to appropriate backend
    switch (model->backend) {
        case BACKEND_LOCAL_GGUF:
            response = CallLocalGGUF(router, model, prompt, max_tokens);
            break;
        case BACKEND_OLLAMA:
            response = CallOllama(router, model, prompt, max_tokens);
            break;
        case BACKEND_OPENAI:
        case BACKEND_ANTHROPIC:
            response = CallOpenAI(router, model, prompt, max_tokens);
            break;
        default:
            break;
    }
    
    QueryPerformanceCounter(&end);
    double latency = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart) * 1000.0;
    
    // Update metrics
    model->request_count++;
    model->avg_latency_ms = (model->avg_latency_ms * (model->request_count - 1) + latency) / model->request_count;
    
    if (!response) {
        model->error_count++;
    } else if (router->on_response) {
        router->on_response(response, router->callback_user_data);
    }
    
    LeaveCriticalSection(&router->cs);
    return response;
}

__declspec(dllexport)
char* ModelRouter_GenerateWithModel(
    ModelRouter* router,
    const wchar_t* model_name,
    const char* prompt,
    int max_tokens
) {
    if (!router || !model_name || !prompt) return NULL;
    
    EnterCriticalSection(&router->cs);
    
    // Find model by name
    ModelInfo* model = NULL;
    for (int i = 0; i < router->model_count; i++) {
        if (wcscmp(router->models[i].name, model_name) == 0) {
            model = &router->models[i];
            break;
        }
    }
    
    if (!model || !model->available) {
        LeaveCriticalSection(&router->cs);
        return NULL;
    }
    
    // Same routing logic
    char* response = NULL;
    switch (model->backend) {
        case BACKEND_LOCAL_GGUF:
            response = CallLocalGGUF(router, model, prompt, max_tokens);
            break;
        case BACKEND_OLLAMA:
            response = CallOllama(router, model, prompt, max_tokens);
            break;
        case BACKEND_OPENAI:
        case BACKEND_ANTHROPIC:
            response = CallOpenAI(router, model, prompt, max_tokens);
            break;
        default:
            break;
    }
    
    model->request_count++;
    if (!response) model->error_count++;
    
    LeaveCriticalSection(&router->cs);
    return response;
}

// ============================================================================
// MODEL SELECTION
// ============================================================================

static ModelInfo* SelectModel(ModelRouter* router, int required_caps) {
    ModelInfo* best = NULL;
    double best_score = -1.0;
    
    for (int i = 0; i < router->model_count; i++) {
        ModelInfo* m = &router->models[i];
        
        // Skip unavailable or capability mismatch
        if (!m->available) continue;
        if (required_caps && (m->capabilities & required_caps) != required_caps) continue;
        
        double score = 0.0;
        
        switch (router->strategy) {
            case ROUTE_AUTO:
            case ROUTE_BY_CAPABILITY:
                // Prefer local models, then by capability match
                score = (m->backend == BACKEND_LOCAL_GGUF) ? 100.0 : 50.0;
                score += __popcnt(m->capabilities & required_caps) * 10.0;
                break;
                
            case ROUTE_BY_LATENCY:
                // Lower latency = higher score (avoid division by zero)
                score = (m->avg_latency_ms > 0) ? (10000.0 / m->avg_latency_ms) : 100.0;
                break;
                
            case ROUTE_BY_COST:
                // Local is free, cloud has cost
                score = (m->backend == BACKEND_LOCAL_GGUF) ? 100.0 :
                        (m->backend == BACKEND_OLLAMA) ? 90.0 : 10.0;
                break;
                
            case ROUTE_ROUND_ROBIN:
                // Return next in sequence
                router->current_model_index = (router->current_model_index + 1) % router->model_count;
                return &router->models[router->current_model_index];
                
            case ROUTE_SPECIFIC:
                // First available match
                return m;
        }
        
        if (score > best_score) {
            best_score = score;
            best = m;
        }
    }
    
    return best;
}

// ============================================================================
// BACKEND IMPLEMENTATIONS
// ============================================================================

static char* CallLocalGGUF(ModelRouter* router, ModelInfo* model, const char* prompt, int max_tokens) {
    if (!router->local_generate) {
        // No local engine configured
        return NULL;
    }
    
    // Convert model path to char
    char model_path[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, model->model_path, -1, model_path, MAX_PATH, NULL, NULL);
    
    return router->local_generate(model_path, prompt, 
        max_tokens > 0 ? max_tokens : model->max_tokens, 
        router->local_engine);
}

static char* CallOllama(ModelRouter* router, ModelInfo* model, const char* prompt, int max_tokens) {
    // Find Ollama backend
    Backend* backend = NULL;
    for (int i = 0; i < router->backend_count; i++) {
        if (router->backends[i].type == BACKEND_OLLAMA && router->backends[i].connected) {
            backend = &router->backends[i];
            break;
        }
    }
    
    if (!backend) return NULL;
    
    // Build JSON request
    char* request_body = (char*)malloc(MAX_PROMPT_SIZE);
    char* escaped_prompt = (char*)malloc(MAX_PROMPT_SIZE);
    
    // JSON-escape prompt
    int j = 0;
    for (int i = 0; prompt[i] && j < MAX_PROMPT_SIZE - 2; i++) {
        char c = prompt[i];
        if (c == '"' || c == '\\') escaped_prompt[j++] = '\\';
        if (c == '\n') { escaped_prompt[j++] = '\\'; escaped_prompt[j++] = 'n'; }
        else if (c == '\r') { escaped_prompt[j++] = '\\'; escaped_prompt[j++] = 'r'; }
        else if (c == '\t') { escaped_prompt[j++] = '\\'; escaped_prompt[j++] = 't'; }
        else escaped_prompt[j++] = c;
    }
    escaped_prompt[j] = '\0';
    
    char model_name[64];
    WideCharToMultiByte(CP_UTF8, 0, model->name, -1, model_name, 64, NULL, NULL);
    
    snprintf(request_body, MAX_PROMPT_SIZE,
        "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false,"
        "\"options\":{\"num_predict\":%d,\"temperature\":%.2f,\"top_p\":%.2f}}",
        model_name, escaped_prompt, 
        max_tokens > 0 ? max_tokens : model->max_tokens,
        model->temperature, model->top_p);
    
    free(escaped_prompt);
    
    // Make HTTP request
    HINTERNET hRequest = WinHttpOpenRequest(
        backend->hConnect, L"POST", L"/api/generate",
        NULL, NULL, NULL, 0);
    
    if (!hRequest) {
        free(request_body);
        return NULL;
    }
    
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json\r\n", -1, WINHTTP_ADDREQ_FLAG_ADD);
    
    DWORD body_len = (DWORD)strlen(request_body);
    if (!WinHttpSendRequest(hRequest, NULL, 0, request_body, body_len, body_len, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        free(request_body);
        return NULL;
    }
    
    free(request_body);
    
    // Read response
    char* response = (char*)malloc(MAX_RESPONSE_SIZE);
    DWORD total_read = 0, bytes_read;
    
    while (WinHttpReadData(hRequest, response + total_read, 
           MAX_RESPONSE_SIZE - total_read - 1, &bytes_read) && bytes_read > 0) {
        total_read += bytes_read;
    }
    response[total_read] = '\0';
    
    WinHttpCloseHandle(hRequest);
    
    // Extract response text from Ollama JSON
    const char* resp_key = "\"response\":\"";
    const char* pos = strstr(response, resp_key);
    if (!pos) {
        free(response);
        return NULL;
    }
    
    pos += strlen(resp_key);
    char* result = (char*)malloc(MAX_RESPONSE_SIZE);
    int k = 0;
    
    while (*pos && *pos != '"' && k < MAX_RESPONSE_SIZE - 1) {
        if (*pos == '\\' && pos[1]) {
            pos++;
            switch (*pos) {
                case 'n': result[k++] = '\n'; break;
                case 'r': result[k++] = '\r'; break;
                case 't': result[k++] = '\t'; break;
                case '"': result[k++] = '"'; break;
                case '\\': result[k++] = '\\'; break;
                default: result[k++] = *pos; break;
            }
        } else {
            result[k++] = *pos;
        }
        pos++;
    }
    result[k] = '\0';
    
    free(response);
    return result;
}

static char* CallOpenAI(ModelRouter* router, ModelInfo* model, const char* prompt, int max_tokens) {
    // Find OpenAI-compatible backend
    Backend* backend = NULL;
    for (int i = 0; i < router->backend_count; i++) {
        if ((router->backends[i].type == BACKEND_OPENAI || 
             router->backends[i].type == BACKEND_ANTHROPIC) && 
            router->backends[i].connected) {
            backend = &router->backends[i];
            break;
        }
    }
    
    if (!backend || !model->api_key[0]) return NULL;
    
    // Build request
    char* request_body = (char*)malloc(MAX_PROMPT_SIZE);
    char* escaped_prompt = (char*)malloc(MAX_PROMPT_SIZE);
    
    int j = 0;
    for (int i = 0; prompt[i] && j < MAX_PROMPT_SIZE - 2; i++) {
        char c = prompt[i];
        if (c == '"' || c == '\\') escaped_prompt[j++] = '\\';
        if (c == '\n') { escaped_prompt[j++] = '\\'; escaped_prompt[j++] = 'n'; }
        else if (c == '\r') { escaped_prompt[j++] = '\\'; escaped_prompt[j++] = 'r'; }
        else if (c == '\t') { escaped_prompt[j++] = '\\'; escaped_prompt[j++] = 't'; }
        else escaped_prompt[j++] = c;
    }
    escaped_prompt[j] = '\0';
    
    char model_name[64];
    WideCharToMultiByte(CP_UTF8, 0, model->name, -1, model_name, 64, NULL, NULL);
    
    snprintf(request_body, MAX_PROMPT_SIZE,
        "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
        "\"max_tokens\":%d,\"temperature\":%.2f}",
        model_name, escaped_prompt,
        max_tokens > 0 ? max_tokens : model->max_tokens,
        model->temperature);
    
    free(escaped_prompt);
    
    // API endpoint depends on backend
    const wchar_t* api_path = (backend->type == BACKEND_ANTHROPIC) ? 
        L"/v1/messages" : L"/v1/chat/completions";
    
    HINTERNET hRequest = WinHttpOpenRequest(
        backend->hConnect, L"POST", api_path,
        NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    
    if (!hRequest) {
        free(request_body);
        return NULL;
    }
    
    // Set headers
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json\r\n", -1, WINHTTP_ADDREQ_FLAG_ADD);
    
    wchar_t auth_header[512];
    if (backend->type == BACKEND_ANTHROPIC) {
        swprintf_s(auth_header, 512, L"x-api-key: %s\r\nanthropic-version: 2023-06-01\r\n", model->api_key);
    } else {
        swprintf_s(auth_header, 512, L"Authorization: Bearer %s\r\n", model->api_key);
    }
    WinHttpAddRequestHeaders(hRequest, auth_header, -1, WINHTTP_ADDREQ_FLAG_ADD);
    
    DWORD body_len = (DWORD)strlen(request_body);
    if (!WinHttpSendRequest(hRequest, NULL, 0, request_body, body_len, body_len, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        free(request_body);
        return NULL;
    }
    
    free(request_body);
    
    // Read response
    char* response = (char*)malloc(MAX_RESPONSE_SIZE);
    DWORD total_read = 0, bytes_read;
    
    while (WinHttpReadData(hRequest, response + total_read, 
           MAX_RESPONSE_SIZE - total_read - 1, &bytes_read) && bytes_read > 0) {
        total_read += bytes_read;
    }
    response[total_read] = '\0';
    
    WinHttpCloseHandle(hRequest);
    
    // Extract content from OpenAI format
    const char* content_key = "\"content\":\"";
    const char* pos = strstr(response, content_key);
    if (!pos) {
        // Try Anthropic format
        content_key = "\"text\":\"";
        pos = strstr(response, content_key);
    }
    
    if (!pos) {
        free(response);
        return NULL;
    }
    
    pos += strlen(content_key);
    char* result = (char*)malloc(MAX_RESPONSE_SIZE);
    int k = 0;
    
    while (*pos && *pos != '"' && k < MAX_RESPONSE_SIZE - 1) {
        if (*pos == '\\' && pos[1]) {
            pos++;
            switch (*pos) {
                case 'n': result[k++] = '\n'; break;
                case 'r': result[k++] = '\r'; break;
                case 't': result[k++] = '\t'; break;
                case '"': result[k++] = '"'; break;
                case '\\': result[k++] = '\\'; break;
                default: result[k++] = *pos; break;
            }
        } else {
            result[k++] = *pos;
        }
        pos++;
    }
    result[k] = '\0';
    
    free(response);
    return result;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

__declspec(dllexport)
void ModelRouter_SetStrategy(ModelRouter* router, RoutingStrategy strategy) {
    if (router) router->strategy = strategy;
}

__declspec(dllexport)
void ModelRouter_SetLocalEngine(
    ModelRouter* router,
    char* (*generate_fn)(const char* model_path, const char* prompt, int max_tokens, void* engine),
    void* engine
) {
    if (!router) return;
    router->local_generate = generate_fn;
    router->local_engine = engine;
}

__declspec(dllexport)
void ModelRouter_SetCallbacks(
    ModelRouter* router,
    void (*on_model_changed)(const wchar_t* name, void* user_data),
    void (*on_response)(const char* response, void* user_data),
    void (*on_stream_chunk)(const char* chunk, void* user_data),
    void (*on_error)(const wchar_t* error, void* user_data),
    void* user_data
) {
    if (!router) return;
    router->on_model_changed = on_model_changed;
    router->on_response = on_response;
    router->on_stream_chunk = on_stream_chunk;
    router->on_error = on_error;
    router->callback_user_data = user_data;
}

// ============================================================================
// MODEL QUERIES
// ============================================================================

__declspec(dllexport)
int ModelRouter_GetModelCount(ModelRouter* router) {
    return router ? router->model_count : 0;
}

__declspec(dllexport)
const ModelInfo* ModelRouter_GetModel(ModelRouter* router, int index) {
    if (!router || index < 0 || index >= router->model_count) return NULL;
    return &router->models[index];
}

__declspec(dllexport)
const ModelInfo* ModelRouter_GetModelByName(ModelRouter* router, const wchar_t* name) {
    if (!router || !name) return NULL;
    
    for (int i = 0; i < router->model_count; i++) {
        if (wcscmp(router->models[i].name, name) == 0) {
            return &router->models[i];
        }
    }
    return NULL;
}

__declspec(dllexport)
void ModelRouter_SetModelAvailable(ModelRouter* router, int index, BOOL available) {
    if (router && index >= 0 && index < router->model_count) {
        router->models[index].available = available;
    }
}

__declspec(dllexport)
void ModelRouter_GetStats(ModelRouter* router, int model_index, 
                           int* request_count, int* error_count, double* avg_latency) {
    if (!router || model_index < 0 || model_index >= router->model_count) return;
    
    ModelInfo* m = &router->models[model_index];
    if (request_count) *request_count = m->request_count;
    if (error_count) *error_count = m->error_count;
    if (avg_latency) *avg_latency = m->avg_latency_ms;
}
