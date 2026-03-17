// RawrXD Agentic Engine - Pure Win32/C++ (No Qt)
// Consolidates: agentic_engine.cpp, agentic_executor.cpp, plan_orchestrator.cpp
// Provides autonomous AI task execution with model inference

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
#include <wchar.h>
#include <time.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

// ============================================================================
// FORWARD DECLARATIONS FOR INFERENCE DLL
// ============================================================================
typedef struct ModelContext ModelContext;
typedef ModelContext* (*LoadModelFn)(const wchar_t* path);
typedef void (*UnloadModelFn)(ModelContext* ctx);
typedef int (*ForwardPassFn)(ModelContext* ctx, int token, int pos);
typedef int (*SampleNextFn)(float* logits, int n_vocab, float temp, float top_p, int top_k);

// ============================================================================
// CONFIGURATION
// ============================================================================
#define MAX_PROMPT_LEN      65536
#define MAX_RESPONSE_LEN    32768
#define MAX_STEPS           64
#define MAX_TOOLS           32
#define MAX_MEMORY_ENTRIES  256
#define MAX_CONTEXT_TOKENS  4096

// ============================================================================
// DATA STRUCTURES
// ============================================================================

// Task step for agentic execution
typedef struct {
    wchar_t description[512];
    wchar_t action_type[64];    // create_file, run_command, generate_code, etc.
    wchar_t target_path[MAX_PATH];
    wchar_t parameters[1024];
    BOOL completed;
    BOOL success;
    wchar_t error_msg[256];
} TaskStep;

// Plan for multi-step execution
typedef struct {
    wchar_t goal[1024];
    TaskStep steps[MAX_STEPS];
    int n_steps;
    int current_step;
    BOOL active;
    BOOL completed;
    int success_count;
} ExecutionPlan;

// Memory entry for context
typedef struct {
    wchar_t key[128];
    wchar_t value[4096];
    uint64_t timestamp;
} MemoryEntry;

// Tool definition
typedef struct {
    wchar_t name[64];
    wchar_t description[256];
    BOOL (*execute)(const wchar_t* params, wchar_t* output, int output_len);
} Tool;

// Main engine state
typedef struct {
    // Model state
    HMODULE hInferenceDLL;
    ModelContext* model;
    LoadModelFn pfnLoadModel;
    UnloadModelFn pfnUnloadModel;
    ForwardPassFn pfnForward;
    SampleNextFn pfnSample;
    BOOL model_loaded;
    wchar_t model_path[MAX_PATH];
    
    // Generation parameters
    float temperature;
    float top_p;
    int top_k;
    int max_tokens;
    int context_size;
    
    // Execution state
    ExecutionPlan current_plan;
    BOOL executing;
    
    // Memory/context
    MemoryEntry memory[MAX_MEMORY_ENTRIES];
    int n_memory;
    
    // Tools
    Tool tools[MAX_TOOLS];
    int n_tools;
    
    // Callbacks
    void (*on_token)(const wchar_t* token, void* user_data);
    void (*on_step_complete)(int step, BOOL success, void* user_data);
    void (*on_plan_complete)(BOOL success, void* user_data);
    void* callback_user_data;
    
    // Statistics
    uint64_t total_tokens_generated;
    uint64_t total_requests;
    uint64_t successful_requests;
    
    CRITICAL_SECTION cs;  // Thread safety
} AgenticEngine;

// ============================================================================
// GLOBAL ENGINE INSTANCE
// ============================================================================
static AgenticEngine* g_Engine = NULL;

// ============================================================================
// ENGINE INITIALIZATION
// ============================================================================

__declspec(dllexport)
AgenticEngine* AgenticEngine_Create(void) {
    AgenticEngine* engine = (AgenticEngine*)calloc(1, sizeof(AgenticEngine));
    if (!engine) return NULL;
    
    InitializeCriticalSection(&engine->cs);
    
    // Default parameters
    engine->temperature = 0.8f;
    engine->top_p = 0.95f;
    engine->top_k = 40;
    engine->max_tokens = 512;
    engine->context_size = 4096;
    
    g_Engine = engine;
    return engine;
}

__declspec(dllexport)
void AgenticEngine_Destroy(AgenticEngine* engine) {
    if (!engine) return;
    
    EnterCriticalSection(&engine->cs);
    
    if (engine->model && engine->pfnUnloadModel) {
        engine->pfnUnloadModel(engine->model);
    }
    if (engine->hInferenceDLL) {
        FreeLibrary(engine->hInferenceDLL);
    }
    
    LeaveCriticalSection(&engine->cs);
    DeleteCriticalSection(&engine->cs);
    
    if (g_Engine == engine) g_Engine = NULL;
    free(engine);
}

// ============================================================================
// MODEL LOADING
// ============================================================================

__declspec(dllexport)
BOOL AgenticEngine_LoadModel(AgenticEngine* engine, const wchar_t* model_path) {
    if (!engine || !model_path) return FALSE;
    
    EnterCriticalSection(&engine->cs);
    
    // Load inference DLL if not already loaded
    if (!engine->hInferenceDLL) {
        engine->hInferenceDLL = LoadLibraryW(L"RawrXD_InferenceEngine.dll");
        if (!engine->hInferenceDLL) {
            // Try current directory
            wchar_t dllPath[MAX_PATH];
            GetModuleFileNameW(NULL, dllPath, MAX_PATH);
            wchar_t* slash = wcsrchr(dllPath, L'\\');
            if (slash) {
                wcscpy(slash + 1, L"RawrXD_InferenceEngine.dll");
                engine->hInferenceDLL = LoadLibraryW(dllPath);
            }
        }
        
        if (engine->hInferenceDLL) {
            engine->pfnLoadModel = (LoadModelFn)GetProcAddress(engine->hInferenceDLL, "LoadModel");
            engine->pfnUnloadModel = (UnloadModelFn)GetProcAddress(engine->hInferenceDLL, "UnloadModel");
            engine->pfnForward = (ForwardPassFn)GetProcAddress(engine->hInferenceDLL, "ForwardPass");
            engine->pfnSample = (SampleNextFn)GetProcAddress(engine->hInferenceDLL, "SampleNext");
        }
    }
    
    if (!engine->pfnLoadModel) {
        LeaveCriticalSection(&engine->cs);
        return FALSE;
    }
    
    // Unload existing model
    if (engine->model && engine->pfnUnloadModel) {
        engine->pfnUnloadModel(engine->model);
        engine->model = NULL;
    }
    
    // Load new model
    engine->model = engine->pfnLoadModel(model_path);
    if (engine->model) {
        wcscpy(engine->model_path, model_path);
        engine->model_loaded = TRUE;
    }
    
    LeaveCriticalSection(&engine->cs);
    return engine->model_loaded;
}

__declspec(dllexport)
BOOL AgenticEngine_IsModelLoaded(AgenticEngine* engine) {
    return engine && engine->model_loaded;
}

// ============================================================================
// GENERATION PARAMETERS
// ============================================================================

__declspec(dllexport)
void AgenticEngine_SetTemperature(AgenticEngine* engine, float temp) {
    if (engine) engine->temperature = temp;
}

__declspec(dllexport)
void AgenticEngine_SetTopP(AgenticEngine* engine, float top_p) {
    if (engine) engine->top_p = top_p;
}

__declspec(dllexport)
void AgenticEngine_SetTopK(AgenticEngine* engine, int top_k) {
    if (engine) engine->top_k = top_k;
}

__declspec(dllexport)
void AgenticEngine_SetMaxTokens(AgenticEngine* engine, int max_tokens) {
    if (engine) engine->max_tokens = max_tokens;
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

__declspec(dllexport)
void AgenticEngine_SetMemory(AgenticEngine* engine, const wchar_t* key, const wchar_t* value) {
    if (!engine || !key || !value) return;
    
    EnterCriticalSection(&engine->cs);
    
    // Check if key exists
    for (int i = 0; i < engine->n_memory; i++) {
        if (wcscmp(engine->memory[i].key, key) == 0) {
            wcsncpy(engine->memory[i].value, value, 4095);
            engine->memory[i].timestamp = GetTickCount64();
            LeaveCriticalSection(&engine->cs);
            return;
        }
    }
    
    // Add new entry
    if (engine->n_memory < MAX_MEMORY_ENTRIES) {
        wcsncpy(engine->memory[engine->n_memory].key, key, 127);
        wcsncpy(engine->memory[engine->n_memory].value, value, 4095);
        engine->memory[engine->n_memory].timestamp = GetTickCount64();
        engine->n_memory++;
    }
    
    LeaveCriticalSection(&engine->cs);
}

__declspec(dllexport)
const wchar_t* AgenticEngine_GetMemory(AgenticEngine* engine, const wchar_t* key) {
    if (!engine || !key) return NULL;
    
    for (int i = 0; i < engine->n_memory; i++) {
        if (wcscmp(engine->memory[i].key, key) == 0) {
            return engine->memory[i].value;
        }
    }
    return NULL;
}

__declspec(dllexport)
void AgenticEngine_ClearMemory(AgenticEngine* engine) {
    if (!engine) return;
    EnterCriticalSection(&engine->cs);
    engine->n_memory = 0;
    LeaveCriticalSection(&engine->cs);
}

// ============================================================================
// TOOL REGISTRATION
// ============================================================================

__declspec(dllexport)
BOOL AgenticEngine_RegisterTool(AgenticEngine* engine, const wchar_t* name, 
                                 const wchar_t* description,
                                 BOOL (*execute)(const wchar_t*, wchar_t*, int)) {
    if (!engine || !name || !execute) return FALSE;
    if (engine->n_tools >= MAX_TOOLS) return FALSE;
    
    EnterCriticalSection(&engine->cs);
    
    wcsncpy(engine->tools[engine->n_tools].name, name, 63);
    if (description) {
        wcsncpy(engine->tools[engine->n_tools].description, description, 255);
    }
    engine->tools[engine->n_tools].execute = execute;
    engine->n_tools++;
    
    LeaveCriticalSection(&engine->cs);
    return TRUE;
}

// ============================================================================
// BUILT-IN TOOLS
// ============================================================================

static BOOL Tool_CreateFile(const wchar_t* params, wchar_t* output, int output_len) {
    // params format: "path|content"
    wchar_t path[MAX_PATH];
    const wchar_t* content = NULL;
    
    const wchar_t* sep = wcschr(params, L'|');
    if (sep) {
        int pathLen = (int)(sep - params);
        wcsncpy(path, params, pathLen);
        path[pathLen] = 0;
        content = sep + 1;
    } else {
        wcscpy(path, params);
        content = L"";
    }
    
    // Create directory if needed
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash) {
        *lastSlash = 0;
        CreateDirectoryW(path, NULL);
        *lastSlash = L'\\';
    }
    
    FILE* f = _wfopen(path, L"wb");
    if (!f) {
        swprintf(output, output_len, L"Error: Could not create file %s", path);
        return FALSE;
    }
    
    // Write content as UTF-8
    int utf8len = WideCharToMultiByte(CP_UTF8, 0, content, -1, NULL, 0, NULL, NULL);
    char* utf8 = (char*)malloc(utf8len);
    WideCharToMultiByte(CP_UTF8, 0, content, -1, utf8, utf8len, NULL, NULL);
    fwrite(utf8, 1, utf8len - 1, f);
    free(utf8);
    fclose(f);
    
    swprintf(output, output_len, L"Created file: %s", path);
    return TRUE;
}

static BOOL Tool_RunCommand(const wchar_t* params, wchar_t* output, int output_len) {
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hReadPipe, hWritePipe;
    
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        wcscpy(output, L"Error: Could not create pipe");
        return FALSE;
    }
    
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    
    PROCESS_INFORMATION pi;
    wchar_t cmdline[4096];
    swprintf(cmdline, 4096, L"cmd.exe /c %s", params);
    
    if (!CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        wcscpy(output, L"Error: Could not start process");
        return FALSE;
    }
    
    CloseHandle(hWritePipe);
    
    // Read output
    char buffer[4096];
    DWORD bytesRead;
    int totalLen = 0;
    output[0] = 0;
    
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = 0;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
        if (totalLen + wlen < output_len) {
            MultiByteToWideChar(CP_UTF8, 0, buffer, -1, output + totalLen, wlen);
            totalLen += wlen - 1;
        }
    }
    
    CloseHandle(hReadPipe);
    
    WaitForSingleObject(pi.hProcess, 10000);
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return (exitCode == 0);
}

static BOOL Tool_ReadFile(const wchar_t* params, wchar_t* output, int output_len) {
    FILE* f = _wfopen(params, L"rb");
    if (!f) {
        swprintf(output, output_len, L"Error: Could not read file %s", params);
        return FALSE;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // Limit size
    if (size > 32768) size = 32768;
    
    char* content = (char*)malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = 0;
    fclose(f);
    
    MultiByteToWideChar(CP_UTF8, 0, content, -1, output, output_len);
    free(content);
    
    return TRUE;
}

static BOOL Tool_ListDirectory(const wchar_t* params, wchar_t* output, int output_len) {
    wchar_t searchPath[MAX_PATH];
    swprintf(searchPath, MAX_PATH, L"%s\\*", params);
    
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        swprintf(output, output_len, L"Error: Could not list directory %s", params);
        return FALSE;
    }
    
    int pos = 0;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        
        int written = swprintf(output + pos, output_len - pos, L"%s%s\n",
            (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? L"[DIR] " : L"      ",
            fd.cFileName);
        if (written > 0) pos += written;
        else break;
    } while (FindNextFileW(hFind, &fd));
    
    FindClose(hFind);
    return TRUE;
}

__declspec(dllexport)
void AgenticEngine_RegisterBuiltinTools(AgenticEngine* engine) {
    if (!engine) return;
    
    AgenticEngine_RegisterTool(engine, L"create_file", L"Create a file with content", Tool_CreateFile);
    AgenticEngine_RegisterTool(engine, L"run_command", L"Run a shell command", Tool_RunCommand);
    AgenticEngine_RegisterTool(engine, L"read_file", L"Read file contents", Tool_ReadFile);
    AgenticEngine_RegisterTool(engine, L"list_dir", L"List directory contents", Tool_ListDirectory);
}

// ============================================================================
// PLAN CREATION AND EXECUTION
// ============================================================================

__declspec(dllexport)
BOOL AgenticEngine_CreatePlan(AgenticEngine* engine, const wchar_t* goal) {
    if (!engine || !goal) return FALSE;
    
    EnterCriticalSection(&engine->cs);
    
    // Reset current plan
    memset(&engine->current_plan, 0, sizeof(ExecutionPlan));
    wcsncpy(engine->current_plan.goal, goal, 1023);
    engine->current_plan.active = TRUE;
    
    // For now, create a simple single-step plan
    // TODO: Use model to decompose into multiple steps
    wcsncpy(engine->current_plan.steps[0].description, goal, 511);
    wcscpy(engine->current_plan.steps[0].action_type, L"generate");
    engine->current_plan.n_steps = 1;
    
    LeaveCriticalSection(&engine->cs);
    return TRUE;
}

__declspec(dllexport)
BOOL AgenticEngine_ExecutePlan(AgenticEngine* engine) {
    if (!engine || !engine->current_plan.active) return FALSE;
    
    EnterCriticalSection(&engine->cs);
    engine->executing = TRUE;
    
    for (int i = 0; i < engine->current_plan.n_steps; i++) {
        TaskStep* step = &engine->current_plan.steps[i];
        engine->current_plan.current_step = i;
        
        // Execute based on action type
        BOOL success = FALSE;
        wchar_t output[4096] = {0};
        
        if (wcscmp(step->action_type, L"create_file") == 0) {
            wchar_t params[8192];
            swprintf(params, 8192, L"%s|%s", step->target_path, step->parameters);
            success = Tool_CreateFile(params, output, 4096);
        } else if (wcscmp(step->action_type, L"run_command") == 0) {
            success = Tool_RunCommand(step->parameters, output, 4096);
        } else if (wcscmp(step->action_type, L"read_file") == 0) {
            success = Tool_ReadFile(step->target_path, output, 4096);
        } else if (wcscmp(step->action_type, L"generate") == 0) {
            // TODO: Generate using model
            success = TRUE;
        } else {
            // Try registered tools
            for (int t = 0; t < engine->n_tools; t++) {
                if (wcscmp(engine->tools[t].name, step->action_type) == 0) {
                    success = engine->tools[t].execute(step->parameters, output, 4096);
                    break;
                }
            }
        }
        
        step->completed = TRUE;
        step->success = success;
        if (!success) {
            wcsncpy(step->error_msg, output, 255);
        }
        
        if (success) engine->current_plan.success_count++;
        
        // Callback
        if (engine->on_step_complete) {
            LeaveCriticalSection(&engine->cs);
            engine->on_step_complete(i, success, engine->callback_user_data);
            EnterCriticalSection(&engine->cs);
        }
    }
    
    engine->current_plan.completed = TRUE;
    engine->executing = FALSE;
    
    BOOL overall_success = (engine->current_plan.success_count == engine->current_plan.n_steps);
    
    if (engine->on_plan_complete) {
        LeaveCriticalSection(&engine->cs);
        engine->on_plan_complete(overall_success, engine->callback_user_data);
        EnterCriticalSection(&engine->cs);
    }
    
    engine->total_requests++;
    if (overall_success) engine->successful_requests++;
    
    LeaveCriticalSection(&engine->cs);
    return overall_success;
}

// ============================================================================
// SIMPLE CHAT INTERFACE
// ============================================================================

__declspec(dllexport)
BOOL AgenticEngine_Chat(AgenticEngine* engine, const wchar_t* user_message, 
                        wchar_t* response, int response_len) {
    if (!engine || !user_message || !response) return FALSE;
    
    // Store in memory
    AgenticEngine_SetMemory(engine, L"last_user_message", user_message);
    
    if (!engine->model_loaded) {
        wcscpy(response, L"[No model loaded. Please load a GGUF model first.]");
        return FALSE;
    }
    
    // TODO: Implement actual model inference
    // For now, return a placeholder
    swprintf(response, response_len, 
        L"[Model: %s]\n"
        L"Processing: \"%s\"\n\n"
        L"This is a placeholder response. Wire up the full inference pipeline for real generation.",
        wcsrchr(engine->model_path, L'\\') ? wcsrchr(engine->model_path, L'\\') + 1 : engine->model_path,
        user_message);
    
    return TRUE;
}

// ============================================================================
// CALLBACKS
// ============================================================================

__declspec(dllexport)
void AgenticEngine_SetTokenCallback(AgenticEngine* engine, 
                                    void (*callback)(const wchar_t*, void*), 
                                    void* user_data) {
    if (engine) {
        engine->on_token = callback;
        engine->callback_user_data = user_data;
    }
}

__declspec(dllexport)
void AgenticEngine_SetStepCallback(AgenticEngine* engine,
                                   void (*callback)(int, BOOL, void*),
                                   void* user_data) {
    if (engine) {
        engine->on_step_complete = callback;
        engine->callback_user_data = user_data;
    }
}

__declspec(dllexport)
void AgenticEngine_SetPlanCallback(AgenticEngine* engine,
                                   void (*callback)(BOOL, void*),
                                   void* user_data) {
    if (engine) {
        engine->on_plan_complete = callback;
        engine->callback_user_data = user_data;
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

__declspec(dllexport)
void AgenticEngine_GetStats(AgenticEngine* engine, 
                            uint64_t* total_tokens,
                            uint64_t* total_requests,
                            uint64_t* successful_requests) {
    if (!engine) return;
    if (total_tokens) *total_tokens = engine->total_tokens_generated;
    if (total_requests) *total_requests = engine->total_requests;
    if (successful_requests) *successful_requests = engine->successful_requests;
}

// ============================================================================
// DLL ENTRY
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        break;
    case DLL_PROCESS_DETACH:
        if (g_Engine) {
            AgenticEngine_Destroy(g_Engine);
        }
        break;
    }
    return TRUE;
}
