// RawrXD Plan Orchestrator - Pure Win32 (No Qt)
// Replaces: plan_orchestrator.cpp
// AI-driven multi-file edit coordination

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE  
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "shlwapi.lib")

// ============================================================================
// CONFIGURATION
// ============================================================================
#define MAX_TASKS           256
#define MAX_FILE_SIZE       (16 * 1024 * 1024)  // 16MB max file
#define MAX_PATH_LENGTH     1024
#define MAX_FILES           512

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef enum {
    EDIT_OP_INSERT = 0,
    EDIT_OP_REPLACE,
    EDIT_OP_DELETE,
    EDIT_OP_RENAME,
    EDIT_OP_CREATE_FILE,
    EDIT_OP_DELETE_FILE
} EditOperation;

typedef enum {
    TASK_STATUS_PENDING = 0,
    TASK_STATUS_IN_PROGRESS,
    TASK_STATUS_COMPLETED,
    TASK_STATUS_FAILED,
    TASK_STATUS_SKIPPED
} TaskStatus;

typedef struct {
    wchar_t file_path[MAX_PATH_LENGTH];
    int start_line;
    int end_line;
    EditOperation operation;
    char* old_text;         // For replace/delete
    char* new_text;         // For insert/replace
    wchar_t description[512];
    int priority;           // Lower = higher priority
    TaskStatus status;
    wchar_t error_message[256];
} EditTask;

typedef struct {
    BOOL success;
    wchar_t plan_description[1024];
    wchar_t* affected_files[MAX_FILES];
    int affected_file_count;
    int estimated_changes;
    EditTask tasks[MAX_TASKS];
    int task_count;
    wchar_t error_message[512];
} PlanningResult;

typedef struct {
    // Workspace
    wchar_t workspace_root[MAX_PATH_LENGTH];
    
    // External components (function pointers)
    char* (*inference_generate)(const char* prompt, int max_tokens, void* engine);
    void* inference_engine;
    
    // File patterns to include/exclude
    wchar_t include_patterns[32][64];
    wchar_t exclude_patterns[32][64];
    int include_count;
    int exclude_count;
    
    // State
    BOOL initialized;
    BOOL plan_in_progress;
    
    // Current plan
    PlanningResult current_plan;
    int current_task_index;
    
    // Undo stack
    EditTask* undo_stack[MAX_TASKS];
    int undo_count;
    
    // Callbacks
    void (*on_plan_started)(const wchar_t* description, void* user_data);
    void (*on_task_completed)(int task_index, int total_tasks, void* user_data);
    void (*on_plan_completed)(BOOL success, const wchar_t* message, void* user_data);
    void (*on_progress)(int percent, const wchar_t* status, void* user_data);
    void* callback_user_data;
    
    CRITICAL_SECTION cs;
} PlanOrchestrator;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
static void GatherContextFiles(PlanOrchestrator* orch, const wchar_t* dir, 
                                wchar_t** out_files, int* count, int max_files);
static char* ReadFileContents(const wchar_t* path, int* out_size);
static BOOL WriteFileContents(const wchar_t* path, const char* content, int size);
static char* BuildPlanningPrompt(PlanOrchestrator* orch, const char* user_prompt, 
                                  wchar_t** context_files, int file_count);
static BOOL ParsePlanJSON(PlanOrchestrator* orch, const char* json, PlanningResult* result);
static BOOL ExecuteEditTask(PlanOrchestrator* orch, EditTask* task);

// ============================================================================
// ORCHESTRATOR CREATION
// ============================================================================

__declspec(dllexport)
PlanOrchestrator* PlanOrch_Create(void) {
    PlanOrchestrator* orch = (PlanOrchestrator*)calloc(1, sizeof(PlanOrchestrator));
    if (!orch) return NULL;
    
    InitializeCriticalSection(&orch->cs);
    
    // Default include patterns (source code)
    wcscpy_s(orch->include_patterns[0], 64, L"*.c");
    wcscpy_s(orch->include_patterns[1], 64, L"*.cpp");
    wcscpy_s(orch->include_patterns[2], 64, L"*.h");
    wcscpy_s(orch->include_patterns[3], 64, L"*.hpp");
    wcscpy_s(orch->include_patterns[4], 64, L"*.py");
    wcscpy_s(orch->include_patterns[5], 64, L"*.js");
    wcscpy_s(orch->include_patterns[6], 64, L"*.ts");
    wcscpy_s(orch->include_patterns[7], 64, L"*.rs");
    wcscpy_s(orch->include_patterns[8], 64, L"*.go");
    wcscpy_s(orch->include_patterns[9], 64, L"*.java");
    orch->include_count = 10;
    
    // Default exclude patterns
    wcscpy_s(orch->exclude_patterns[0], 64, L".git");
    wcscpy_s(orch->exclude_patterns[1], 64, L"node_modules");
    wcscpy_s(orch->exclude_patterns[2], 64, L"build");
    wcscpy_s(orch->exclude_patterns[3], 64, L"*.exe");
    wcscpy_s(orch->exclude_patterns[4], 64, L"*.dll");
    wcscpy_s(orch->exclude_patterns[5], 64, L"*.obj");
    orch->exclude_count = 6;
    
    return orch;
}

__declspec(dllexport)
void PlanOrch_Destroy(PlanOrchestrator* orch) {
    if (!orch) return;
    
    // Free allocated strings in tasks
    for (int i = 0; i < orch->current_plan.task_count; i++) {
        if (orch->current_plan.tasks[i].old_text) 
            free(orch->current_plan.tasks[i].old_text);
        if (orch->current_plan.tasks[i].new_text) 
            free(orch->current_plan.tasks[i].new_text);
    }
    
    for (int i = 0; i < orch->current_plan.affected_file_count; i++) {
        if (orch->current_plan.affected_files[i])
            free(orch->current_plan.affected_files[i]);
    }
    
    DeleteCriticalSection(&orch->cs);
    free(orch);
}

// ============================================================================
// CONFIGURATION
// ============================================================================

__declspec(dllexport)
void PlanOrch_SetWorkspace(PlanOrchestrator* orch, const wchar_t* workspace_root) {
    if (!orch) return;
    wcsncpy_s(orch->workspace_root, MAX_PATH_LENGTH, workspace_root, _TRUNCATE);
}

__declspec(dllexport)
void PlanOrch_SetInferenceEngine(PlanOrchestrator* orch, 
    char* (*generate_fn)(const char* prompt, int max_tokens, void* engine),
    void* engine
) {
    if (!orch) return;
    orch->inference_generate = generate_fn;
    orch->inference_engine = engine;
}

__declspec(dllexport)
void PlanOrch_SetCallbacks(
    PlanOrchestrator* orch,
    void (*on_plan_started)(const wchar_t* desc, void* user_data),
    void (*on_task_completed)(int task_idx, int total, void* user_data),
    void (*on_plan_completed)(BOOL success, const wchar_t* msg, void* user_data),
    void (*on_progress)(int percent, const wchar_t* status, void* user_data),
    void* user_data
) {
    if (!orch) return;
    orch->on_plan_started = on_plan_started;
    orch->on_task_completed = on_task_completed;
    orch->on_plan_completed = on_plan_completed;
    orch->on_progress = on_progress;
    orch->callback_user_data = user_data;
}

// ============================================================================
// PLAN GENERATION
// ============================================================================

__declspec(dllexport)
PlanningResult* PlanOrch_GeneratePlan(
    PlanOrchestrator* orch,
    const char* user_prompt,
    const wchar_t** context_files,
    int context_file_count
) {
    if (!orch || !user_prompt) return NULL;
    
    EnterCriticalSection(&orch->cs);
    
    // Reset current plan
    memset(&orch->current_plan, 0, sizeof(PlanningResult));
    
    if (orch->on_plan_started) {
        wchar_t desc[256];
        MultiByteToWideChar(CP_UTF8, 0, user_prompt, -1, desc, 256);
        orch->on_plan_started(desc, orch->callback_user_data);
    }
    
    // Gather context files if not provided
    wchar_t* gathered_files[MAX_FILES] = {0};
    int gathered_count = 0;
    
    if (!context_files || context_file_count == 0) {
        GatherContextFiles(orch, orch->workspace_root, gathered_files, &gathered_count, MAX_FILES);
        context_files = (const wchar_t**)gathered_files;
        context_file_count = gathered_count;
    }
    
    if (orch->on_progress) {
        orch->on_progress(10, L"Gathered context files", orch->callback_user_data);
    }
    
    // Check for inference engine
    if (!orch->inference_generate) {
        // No AI available - create manual plan stub
        orch->current_plan.success = TRUE;
        wcscpy_s(orch->current_plan.plan_description, 1024, 
            L"Manual planning mode (no AI inference engine configured)");
        orch->current_plan.estimated_changes = 0;
        
        LeaveCriticalSection(&orch->cs);
        return &orch->current_plan;
    }
    
    // Build planning prompt
    char* planning_prompt = BuildPlanningPrompt(orch, user_prompt, 
        (wchar_t**)context_files, context_file_count);
    
    if (orch->on_progress) {
        orch->on_progress(20, L"Built planning prompt", orch->callback_user_data);
    }
    
    // Call inference engine
    char* ai_response = orch->inference_generate(
        planning_prompt, 
        4096,  // max tokens
        orch->inference_engine
    );
    
    free(planning_prompt);
    
    if (orch->on_progress) {
        orch->on_progress(60, L"Received AI response", orch->callback_user_data);
    }
    
    if (!ai_response || strlen(ai_response) == 0) {
        orch->current_plan.success = FALSE;
        wcscpy_s(orch->current_plan.error_message, 512, L"No response from inference engine");
        
        if (gathered_count > 0) {
            for (int i = 0; i < gathered_count; i++) free(gathered_files[i]);
        }
        
        LeaveCriticalSection(&orch->cs);
        return &orch->current_plan;
    }
    
    // Parse JSON response
    if (!ParsePlanJSON(orch, ai_response, &orch->current_plan)) {
        orch->current_plan.success = FALSE;
        wcscpy_s(orch->current_plan.error_message, 512, L"Failed to parse AI response");
    }
    
    free(ai_response);
    
    // Copy affected files
    for (int i = 0; i < context_file_count && i < MAX_FILES; i++) {
        orch->current_plan.affected_files[i] = _wcsdup(context_files[i]);
    }
    orch->current_plan.affected_file_count = context_file_count;
    
    if (orch->on_progress) {
        orch->on_progress(100, L"Plan generation complete", orch->callback_user_data);
    }
    
    // Clean up gathered files
    if (gathered_count > 0) {
        for (int i = 0; i < gathered_count; i++) free(gathered_files[i]);
    }
    
    LeaveCriticalSection(&orch->cs);
    return &orch->current_plan;
}

// ============================================================================
// PLAN EXECUTION
// ============================================================================

__declspec(dllexport)
BOOL PlanOrch_ExecutePlan(PlanOrchestrator* orch) {
    if (!orch || !orch->current_plan.success || orch->current_plan.task_count == 0) {
        return FALSE;
    }
    
    EnterCriticalSection(&orch->cs);
    orch->plan_in_progress = TRUE;
    orch->current_task_index = 0;
    
    BOOL all_success = TRUE;
    
    for (int i = 0; i < orch->current_plan.task_count; i++) {
        EditTask* task = &orch->current_plan.tasks[i];
        task->status = TASK_STATUS_IN_PROGRESS;
        
        if (orch->on_progress) {
            wchar_t status[256];
            swprintf_s(status, 256, L"Executing task %d/%d: %s", 
                i + 1, orch->current_plan.task_count, task->description);
            int percent = (i * 100) / orch->current_plan.task_count;
            orch->on_progress(percent, status, orch->callback_user_data);
        }
        
        if (ExecuteEditTask(orch, task)) {
            task->status = TASK_STATUS_COMPLETED;
        } else {
            task->status = TASK_STATUS_FAILED;
            all_success = FALSE;
        }
        
        if (orch->on_task_completed) {
            orch->on_task_completed(i, orch->current_plan.task_count, orch->callback_user_data);
        }
        
        orch->current_task_index = i + 1;
    }
    
    orch->plan_in_progress = FALSE;
    
    if (orch->on_plan_completed) {
        orch->on_plan_completed(all_success, 
            all_success ? L"All tasks completed successfully" : L"Some tasks failed",
            orch->callback_user_data);
    }
    
    LeaveCriticalSection(&orch->cs);
    return all_success;
}

__declspec(dllexport)
BOOL PlanOrch_ExecuteNextTask(PlanOrchestrator* orch) {
    if (!orch || orch->current_task_index >= orch->current_plan.task_count) {
        return FALSE;
    }
    
    EnterCriticalSection(&orch->cs);
    
    EditTask* task = &orch->current_plan.tasks[orch->current_task_index];
    task->status = TASK_STATUS_IN_PROGRESS;
    
    BOOL success = ExecuteEditTask(orch, task);
    task->status = success ? TASK_STATUS_COMPLETED : TASK_STATUS_FAILED;
    
    if (orch->on_task_completed) {
        orch->on_task_completed(orch->current_task_index, 
            orch->current_plan.task_count, orch->callback_user_data);
    }
    
    orch->current_task_index++;
    
    LeaveCriticalSection(&orch->cs);
    return success;
}

__declspec(dllexport)
void PlanOrch_AbortPlan(PlanOrchestrator* orch) {
    if (!orch) return;
    
    EnterCriticalSection(&orch->cs);
    orch->plan_in_progress = FALSE;
    
    // Mark remaining tasks as skipped
    for (int i = orch->current_task_index; i < orch->current_plan.task_count; i++) {
        orch->current_plan.tasks[i].status = TASK_STATUS_SKIPPED;
    }
    
    LeaveCriticalSection(&orch->cs);
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

static void GatherContextFiles(PlanOrchestrator* orch, const wchar_t* dir, 
                                wchar_t** out_files, int* count, int max_files) {
    WIN32_FIND_DATAW fd;
    wchar_t search_path[MAX_PATH_LENGTH];
    swprintf_s(search_path, MAX_PATH_LENGTH, L"%s\\*", dir);
    
    HANDLE hFind = FindFirstFileW(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    do {
        if (*count >= max_files) break;
        
        // Skip . and ..
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        
        // Check exclude patterns
        BOOL excluded = FALSE;
        for (int i = 0; i < orch->exclude_count; i++) {
            if (PathMatchSpecW(fd.cFileName, orch->exclude_patterns[i])) {
                excluded = TRUE;
                break;
            }
        }
        if (excluded) continue;
        
        wchar_t full_path[MAX_PATH_LENGTH];
        swprintf_s(full_path, MAX_PATH_LENGTH, L"%s\\%s", dir, fd.cFileName);
        
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recurse into directory
            GatherContextFiles(orch, full_path, out_files, count, max_files);
        } else {
            // Check include patterns
            for (int i = 0; i < orch->include_count; i++) {
                if (PathMatchSpecW(fd.cFileName, orch->include_patterns[i])) {
                    out_files[*count] = _wcsdup(full_path);
                    (*count)++;
                    break;
                }
            }
        }
    } while (FindNextFileW(hFind, &fd));
    
    FindClose(hFind);
}

static char* ReadFileContents(const wchar_t* path, int* out_size) {
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return NULL;
    
    DWORD file_size = GetFileSize(hFile, NULL);
    if (file_size > MAX_FILE_SIZE) {
        CloseHandle(hFile);
        return NULL;
    }
    
    char* buffer = (char*)malloc(file_size + 1);
    DWORD bytes_read;
    ReadFile(hFile, buffer, file_size, &bytes_read, NULL);
    buffer[bytes_read] = '\0';
    
    if (out_size) *out_size = (int)bytes_read;
    
    CloseHandle(hFile);
    return buffer;
}

static BOOL WriteFileContents(const wchar_t* path, const char* content, int size) {
    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    
    DWORD bytes_written;
    BOOL result = WriteFile(hFile, content, size, &bytes_written, NULL);
    
    CloseHandle(hFile);
    return result && bytes_written == (DWORD)size;
}

static char* BuildPlanningPrompt(PlanOrchestrator* orch, const char* user_prompt, 
                                  wchar_t** context_files, int file_count) {
    // Allocate large buffer for prompt
    char* prompt = (char*)malloc(MAX_CONTEXT_SIZE);
    int pos = 0;
    
    pos += snprintf(prompt + pos, MAX_CONTEXT_SIZE - pos,
        "You are a code refactoring assistant. Generate a structured plan.\n\n"
        "TASK: %s\n\n"
        "AFFECTED FILES:\n", user_prompt);
    
    // Add file summaries (first 100 lines of each)
    for (int i = 0; i < file_count && i < 10; i++) {  // Max 10 files in context
        char path_utf8[MAX_PATH_LENGTH];
        WideCharToMultiByte(CP_UTF8, 0, context_files[i], -1, path_utf8, MAX_PATH_LENGTH, NULL, NULL);
        
        pos += snprintf(prompt + pos, MAX_CONTEXT_SIZE - pos, "\n--- %s ---\n", path_utf8);
        
        int file_size;
        char* contents = ReadFileContents(context_files[i], &file_size);
        if (contents) {
            // Take first 2000 chars or so
            int take = file_size > 2000 ? 2000 : file_size;
            if (pos + take < MAX_CONTEXT_SIZE - 1000) {
                memcpy(prompt + pos, contents, take);
                pos += take;
                if (file_size > 2000) {
                    pos += snprintf(prompt + pos, MAX_CONTEXT_SIZE - pos, "\n... (truncated)\n");
                }
            }
            free(contents);
        }
    }
    
    pos += snprintf(prompt + pos, MAX_CONTEXT_SIZE - pos,
        "\n\nReturn a JSON object with:\n"
        "{\n"
        "  \"success\": true,\n"
        "  \"planDescription\": \"...\",\n"
        "  \"estimatedChanges\": N,\n"
        "  \"tasks\": [\n"
        "    {\"filePath\":\"...\", \"startLine\":0, \"endLine\":0, "
        "\"operation\":\"insert|replace|delete\", \"newText\":\"...\", \"description\":\"...\"}\n"
        "  ]\n"
        "}\n");
    
    prompt[pos] = '\0';
    return prompt;
}

static BOOL ParsePlanJSON(PlanOrchestrator* orch, const char* json, PlanningResult* result) {
    // Simple JSON parsing (no external library)
    const char* success_key = "\"success\"";
    const char* desc_key = "\"planDescription\"";
    const char* tasks_key = "\"tasks\"";
    const char* est_key = "\"estimatedChanges\"";
    
    // Check success
    const char* success_pos = strstr(json, success_key);
    if (success_pos) {
        success_pos += strlen(success_key);
        while (*success_pos == ':' || *success_pos == ' ') success_pos++;
        result->success = (strncmp(success_pos, "true", 4) == 0);
    }
    
    // Get plan description
    const char* desc_pos = strstr(json, desc_key);
    if (desc_pos) {
        desc_pos = strchr(desc_pos, ':');
        if (desc_pos) {
            desc_pos++;
            while (*desc_pos == ' ' || *desc_pos == '"') desc_pos++;
            char desc_buf[1024];
            int i = 0;
            while (*desc_pos && *desc_pos != '"' && i < 1023) {
                desc_buf[i++] = *desc_pos++;
            }
            desc_buf[i] = '\0';
            MultiByteToWideChar(CP_UTF8, 0, desc_buf, -1, result->plan_description, 1024);
        }
    }
    
    // Get estimated changes
    const char* est_pos = strstr(json, est_key);
    if (est_pos) {
        est_pos = strchr(est_pos, ':');
        if (est_pos) {
            result->estimated_changes = atoi(est_pos + 1);
        }
    }
    
    // Parse tasks array (simplified)
    const char* tasks_pos = strstr(json, tasks_key);
    if (tasks_pos) {
        tasks_pos = strchr(tasks_pos, '[');
        if (tasks_pos) {
            tasks_pos++;
            
            // Find each task object
            while (*tasks_pos && result->task_count < MAX_TASKS) {
                const char* obj_start = strchr(tasks_pos, '{');
                if (!obj_start) break;
                
                const char* obj_end = strchr(obj_start, '}');
                if (!obj_end) break;
                
                EditTask* task = &result->tasks[result->task_count];
                memset(task, 0, sizeof(EditTask));
                
                // Parse file path
                const char* fp = strstr(obj_start, "\"filePath\"");
                if (fp && fp < obj_end) {
                    fp = strchr(fp, ':') + 1;
                    while (*fp == ' ' || *fp == '"') fp++;
                    char path_buf[MAX_PATH_LENGTH];
                    int i = 0;
                    while (*fp && *fp != '"' && i < MAX_PATH_LENGTH - 1) {
                        path_buf[i++] = *fp++;
                    }
                    path_buf[i] = '\0';
                    MultiByteToWideChar(CP_UTF8, 0, path_buf, -1, task->file_path, MAX_PATH_LENGTH);
                }
                
                // Parse operation
                const char* op = strstr(obj_start, "\"operation\"");
                if (op && op < obj_end) {
                    op = strchr(op, ':') + 1;
                    while (*op == ' ' || *op == '"') op++;
                    if (strncmp(op, "insert", 6) == 0) task->operation = EDIT_OP_INSERT;
                    else if (strncmp(op, "replace", 7) == 0) task->operation = EDIT_OP_REPLACE;
                    else if (strncmp(op, "delete", 6) == 0) task->operation = EDIT_OP_DELETE;
                }
                
                // Parse start/end lines
                const char* sl = strstr(obj_start, "\"startLine\"");
                if (sl && sl < obj_end) task->start_line = atoi(strchr(sl, ':') + 1);
                
                const char* el = strstr(obj_start, "\"endLine\"");
                if (el && el < obj_end) task->end_line = atoi(strchr(el, ':') + 1);
                
                // Parse description
                const char* desc = strstr(obj_start, "\"description\"");
                if (desc && desc < obj_end) {
                    desc = strchr(desc, ':') + 1;
                    while (*desc == ' ' || *desc == '"') desc++;
                    char desc_buf[512];
                    int i = 0;
                    while (*desc && *desc != '"' && i < 511) {
                        desc_buf[i++] = *desc++;
                    }
                    desc_buf[i] = '\0';
                    MultiByteToWideChar(CP_UTF8, 0, desc_buf, -1, task->description, 512);
                }
                
                // Parse newText
                const char* nt = strstr(obj_start, "\"newText\"");
                if (nt && nt < obj_end) {
                    nt = strchr(nt, ':') + 1;
                    while (*nt == ' ' || *nt == '"') nt++;
                    const char* nt_end = nt;
                    while (*nt_end && *nt_end != '"') nt_end++;
                    int len = (int)(nt_end - nt);
                    task->new_text = (char*)malloc(len + 1);
                    memcpy(task->new_text, nt, len);
                    task->new_text[len] = '\0';
                }
                
                task->status = TASK_STATUS_PENDING;
                result->task_count++;
                tasks_pos = obj_end + 1;
            }
        }
    }
    
    return result->task_count > 0 || result->success;
}

static BOOL ExecuteEditTask(PlanOrchestrator* orch, EditTask* task) {
    switch (task->operation) {
        case EDIT_OP_INSERT:
        case EDIT_OP_REPLACE: {
            // Read file
            int file_size;
            char* content = ReadFileContents(task->file_path, &file_size);
            if (!content) {
                wcscpy_s(task->error_message, 256, L"Failed to read file");
                return FALSE;
            }
            
            // Find line position
            int line = 1;
            char* line_start = content;
            while (line < task->start_line && *line_start) {
                if (*line_start++ == '\n') line++;
            }
            
            // For insert, find end of line; for replace, find end line
            char* line_end = line_start;
            int target_line = (task->operation == EDIT_OP_INSERT) ? 
                task->start_line : task->end_line;
            while (line < target_line && *line_end) {
                if (*line_end++ == '\n') line++;
            }
            while (*line_end && *line_end != '\n') line_end++;
            if (*line_end == '\n') line_end++;
            
            // Build new content
            int new_text_len = task->new_text ? (int)strlen(task->new_text) : 0;
            int before_len = (int)(line_start - content);
            int after_len = file_size - (int)(line_end - content);
            
            char* new_content = (char*)malloc(before_len + new_text_len + after_len + 2);
            memcpy(new_content, content, before_len);
            if (task->new_text) {
                memcpy(new_content + before_len, task->new_text, new_text_len);
            }
            new_content[before_len + new_text_len] = '\n';
            memcpy(new_content + before_len + new_text_len + 1, line_end, after_len);
            
            int new_size = before_len + new_text_len + 1 + after_len;
            
            BOOL result = WriteFileContents(task->file_path, new_content, new_size);
            
            free(content);
            free(new_content);
            
            if (!result) {
                wcscpy_s(task->error_message, 256, L"Failed to write file");
            }
            return result;
        }
        
        case EDIT_OP_DELETE: {
            // Similar to replace but with empty new_text
            int file_size;
            char* content = ReadFileContents(task->file_path, &file_size);
            if (!content) return FALSE;
            
            int line = 1;
            char* line_start = content;
            while (line < task->start_line && *line_start) {
                if (*line_start++ == '\n') line++;
            }
            
            char* line_end = line_start;
            while (line <= task->end_line && *line_end) {
                if (*line_end++ == '\n') line++;
            }
            
            int before_len = (int)(line_start - content);
            int after_len = file_size - (int)(line_end - content);
            
            char* new_content = (char*)malloc(before_len + after_len + 1);
            memcpy(new_content, content, before_len);
            memcpy(new_content + before_len, line_end, after_len);
            
            BOOL result = WriteFileContents(task->file_path, new_content, before_len + after_len);
            
            free(content);
            free(new_content);
            return result;
        }
        
        case EDIT_OP_CREATE_FILE: {
            return WriteFileContents(task->file_path, 
                task->new_text ? task->new_text : "", 
                task->new_text ? (int)strlen(task->new_text) : 0);
        }
        
        case EDIT_OP_DELETE_FILE: {
            return DeleteFileW(task->file_path);
        }
        
        default:
            return FALSE;
    }
}

// ============================================================================
// STATUS QUERIES
// ============================================================================

__declspec(dllexport)
PlanningResult* PlanOrch_GetCurrentPlan(PlanOrchestrator* orch) {
    return orch ? &orch->current_plan : NULL;
}

__declspec(dllexport)
int PlanOrch_GetProgress(PlanOrchestrator* orch) {
    if (!orch || orch->current_plan.task_count == 0) return 0;
    return (orch->current_task_index * 100) / orch->current_plan.task_count;
}

__declspec(dllexport)
BOOL PlanOrch_IsPlanInProgress(PlanOrchestrator* orch) {
    return orch ? orch->plan_in_progress : FALSE;
}
