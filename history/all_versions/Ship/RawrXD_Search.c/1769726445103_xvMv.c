// RawrXD Search System - Pure Win32 (No Qt)
// Replaces: search_all_files_widget.cpp, global_search_manager.cpp
// Full-text search across workspace with regex support

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

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "shlwapi.lib")

// ============================================================================
// CONFIGURATION
// ============================================================================
#define MAX_SEARCH_RESULTS  4096
#define MAX_LINE_LENGTH     4096
#define MAX_CONTEXT_LINES   3
#define MAX_SEARCH_THREADS  8

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef struct {
    wchar_t file_path[MAX_PATH];
    int line_number;
    int column;
    int match_length;
    wchar_t line_text[MAX_LINE_LENGTH];
    wchar_t context_before[MAX_LINE_LENGTH];
    wchar_t context_after[MAX_LINE_LENGTH];
} SearchResult;

typedef enum {
    SEARCH_MODE_LITERAL = 0,
    SEARCH_MODE_REGEX,
    SEARCH_MODE_WHOLE_WORD,
    SEARCH_MODE_GLOB
} SearchMode;

typedef struct {
    // Search configuration
    wchar_t query[MAX_LINE_LENGTH];
    wchar_t root_path[MAX_PATH];
    wchar_t file_pattern[MAX_PATH];      // e.g., "*.c;*.cpp;*.h"
    SearchMode mode;
    BOOL case_sensitive;
    BOOL search_in_hidden;
    BOOL search_binary_files;
    
    // Exclusion patterns
    wchar_t exclude_dirs[16][MAX_PATH];
    wchar_t exclude_files[16][MAX_PATH];
    int exclude_dir_count;
    int exclude_file_count;
    
    // Results
    SearchResult* results;
    int result_count;
    int result_capacity;
    
    // Stats
    int files_searched;
    int lines_searched;
    DWORD start_time;
    DWORD elapsed_ms;
    
    // Threading
    volatile BOOL search_active;
    volatile BOOL stop_requested;
    HANDLE hSearchThread;
    
    // Callbacks
    void (*on_result)(const SearchResult* result, void* user_data);
    void (*on_progress)(int files_searched, int matches, void* user_data);
    void (*on_complete)(int total_results, DWORD elapsed_ms, void* user_data);
    void* callback_user_data;
    
    CRITICAL_SECTION cs;
} SearchEngine;

// Forward declarations
static DWORD WINAPI SearchThread(LPVOID param);
static void SearchFile(SearchEngine* engine, const wchar_t* file_path);
static void SearchDirectory(SearchEngine* engine, const wchar_t* dir_path);
static BOOL IsTextFile(const wchar_t* path);
static BOOL MatchesPattern(const wchar_t* text, const wchar_t* pattern, SearchMode mode, BOOL case_sens);

// Exported function forward declarations
__declspec(dllexport) void Search_Stop(SearchEngine* engine);

// ============================================================================
// ENGINE CREATION
// ============================================================================

__declspec(dllexport)
SearchEngine* Search_Create(void) {
    SearchEngine* engine = (SearchEngine*)calloc(1, sizeof(SearchEngine));
    if (!engine) return NULL;
    
    InitializeCriticalSection(&engine->cs);
    
    engine->result_capacity = MAX_SEARCH_RESULTS;
    engine->results = (SearchResult*)calloc(engine->result_capacity, sizeof(SearchResult));
    
    // Default exclusions
    wcscpy_s(engine->exclude_dirs[0], MAX_PATH, L".git");
    wcscpy_s(engine->exclude_dirs[1], MAX_PATH, L"node_modules");
    wcscpy_s(engine->exclude_dirs[2], MAX_PATH, L"__pycache__");
    wcscpy_s(engine->exclude_dirs[3], MAX_PATH, L".vs");
    wcscpy_s(engine->exclude_dirs[4], MAX_PATH, L"Debug");
    wcscpy_s(engine->exclude_dirs[5], MAX_PATH, L"Release");
    engine->exclude_dir_count = 6;
    
    wcscpy_s(engine->exclude_files[0], MAX_PATH, L"*.exe");
    wcscpy_s(engine->exclude_files[1], MAX_PATH, L"*.dll");
    wcscpy_s(engine->exclude_files[2], MAX_PATH, L"*.obj");
    wcscpy_s(engine->exclude_files[3], MAX_PATH, L"*.pdb");
    engine->exclude_file_count = 4;
    
    wcscpy_s(engine->file_pattern, MAX_PATH, L"*.*");
    
    return engine;
}

__declspec(dllexport)
void Search_Destroy(SearchEngine* engine) {
    if (!engine) return;
    
    Search_Stop(engine);
    
    if (engine->results) free(engine->results);
    DeleteCriticalSection(&engine->cs);
    free(engine);
}

// ============================================================================
// SEARCH OPERATIONS
// ============================================================================

__declspec(dllexport)
BOOL Search_Start(SearchEngine* engine, const wchar_t* query, const wchar_t* root_path) {
    if (!engine || !query || !root_path) return FALSE;
    if (engine->search_active) return FALSE;
    
    // Verify directory exists
    DWORD attr = GetFileAttributesW(root_path);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return FALSE;
    }
    
    EnterCriticalSection(&engine->cs);
    
    wcscpy_s(engine->query, MAX_LINE_LENGTH, query);
    wcscpy_s(engine->root_path, MAX_PATH, root_path);
    engine->result_count = 0;
    engine->files_searched = 0;
    engine->lines_searched = 0;
    engine->stop_requested = FALSE;
    engine->search_active = TRUE;
    engine->start_time = GetTickCount();
    
    engine->hSearchThread = CreateThread(NULL, 0, SearchThread, engine, 0, NULL);
    
    LeaveCriticalSection(&engine->cs);
    return TRUE;
}

__declspec(dllexport)
void Search_Stop(SearchEngine* engine) {
    if (!engine) return;
    
    engine->stop_requested = TRUE;
    
    if (engine->hSearchThread) {
        WaitForSingleObject(engine->hSearchThread, 5000);
        CloseHandle(engine->hSearchThread);
        engine->hSearchThread = NULL;
    }
    
    engine->search_active = FALSE;
}

__declspec(dllexport)
BOOL Search_IsActive(SearchEngine* engine) {
    return engine ? engine->search_active : FALSE;
}

static DWORD WINAPI SearchThread(LPVOID param) {
    SearchEngine* engine = (SearchEngine*)param;
    
    SearchDirectory(engine, engine->root_path);
    
    engine->elapsed_ms = GetTickCount() - engine->start_time;
    engine->search_active = FALSE;
    
    if (engine->on_complete) {
        engine->on_complete(engine->result_count, engine->elapsed_ms, engine->callback_user_data);
    }
    
    return 0;
}

static void SearchDirectory(SearchEngine* engine, const wchar_t* dir_path) {
    if (engine->stop_requested) return;
    
    WIN32_FIND_DATAW fd;
    wchar_t search_path[MAX_PATH];
    swprintf_s(search_path, MAX_PATH, L"%s\\*", dir_path);
    
    HANDLE hFind = FindFirstFileW(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    do {
        if (engine->stop_requested) break;
        
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        
        wchar_t full_path[MAX_PATH];
        swprintf_s(full_path, MAX_PATH, L"%s\\%s", dir_path, fd.cFileName);
        
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Check directory exclusions
            BOOL excluded = FALSE;
            for (int i = 0; i < engine->exclude_dir_count; i++) {
                if (_wcsicmp(fd.cFileName, engine->exclude_dirs[i]) == 0) {
                    excluded = TRUE;
                    break;
                }
            }
            
            if (!excluded && (engine->search_in_hidden || !(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))) {
                SearchDirectory(engine, full_path);
            }
        } else {
            // Check file pattern
            if (!PathMatchSpecW(fd.cFileName, engine->file_pattern))
                continue;
            
            // Check file exclusions
            BOOL excluded = FALSE;
            for (int i = 0; i < engine->exclude_file_count; i++) {
                if (PathMatchSpecW(fd.cFileName, engine->exclude_files[i])) {
                    excluded = TRUE;
                    break;
                }
            }
            
            if (!excluded) {
                // Check if it's a text file (unless searching binaries)
                if (engine->search_binary_files || IsTextFile(full_path)) {
                    SearchFile(engine, full_path);
                }
            }
        }
    } while (FindNextFileW(hFind, &fd));
    
    FindClose(hFind);
}

static void SearchFile(SearchEngine* engine, const wchar_t* file_path) {
    HANDLE hFile = CreateFileW(file_path, GENERIC_READ, FILE_SHARE_READ, NULL, 
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    
    DWORD file_size = GetFileSize(hFile, NULL);
    if (file_size == 0 || file_size > 50 * 1024 * 1024) {  // Skip files > 50MB
        CloseHandle(hFile);
        return;
    }
    
    char* buffer = (char*)malloc(file_size + 1);
    DWORD bytes_read;
    if (!ReadFile(hFile, buffer, file_size, &bytes_read, NULL)) {
        free(buffer);
        CloseHandle(hFile);
        return;
    }
    buffer[bytes_read] = '\0';
    CloseHandle(hFile);
    
    engine->files_searched++;
    
    // Convert query to UTF-8 for comparison
    char query_utf8[MAX_LINE_LENGTH];
    WideCharToMultiByte(CP_UTF8, 0, engine->query, -1, query_utf8, MAX_LINE_LENGTH, NULL, NULL);
    
    // Search line by line
    int line_num = 1;
    char* line_start = buffer;
    char* p = buffer;
    
    char prev_lines[MAX_CONTEXT_LINES][MAX_LINE_LENGTH] = {0};
    int prev_line_idx = 0;
    
    while (*p && !engine->stop_requested) {
        // Find end of line
        char* line_end = p;
        while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;
        
        char saved = *line_end;
        *line_end = '\0';
        
        engine->lines_searched++;
        
        // Search for query in this line
        char* match = engine->case_sensitive ? 
            strstr(p, query_utf8) : 
            StrStrIA(p, query_utf8);
        
        if (match && engine->result_count < engine->result_capacity) {
            EnterCriticalSection(&engine->cs);
            
            SearchResult* result = &engine->results[engine->result_count];
            wcscpy_s(result->file_path, MAX_PATH, file_path);
            result->line_number = line_num;
            result->column = (int)(match - p) + 1;
            result->match_length = (int)strlen(query_utf8);
            
            // Convert line to wide string
            MultiByteToWideChar(CP_UTF8, 0, p, -1, result->line_text, MAX_LINE_LENGTH);
            
            // Context before (previous lines)
            result->context_before[0] = L'\0';
            for (int i = 0; i < MAX_CONTEXT_LINES; i++) {
                int idx = (prev_line_idx - MAX_CONTEXT_LINES + i + MAX_CONTEXT_LINES) % MAX_CONTEXT_LINES;
                if (prev_lines[idx][0]) {
                    wchar_t wide_line[MAX_LINE_LENGTH];
                    MultiByteToWideChar(CP_UTF8, 0, prev_lines[idx], -1, wide_line, MAX_LINE_LENGTH);
                    wcscat_s(result->context_before, MAX_LINE_LENGTH, wide_line);
                    wcscat_s(result->context_before, MAX_LINE_LENGTH, L"\n");
                }
            }
            
            engine->result_count++;
            
            LeaveCriticalSection(&engine->cs);
            
            if (engine->on_result) {
                engine->on_result(result, engine->callback_user_data);
            }
        }
        
        // Store line for context
        strncpy_s(prev_lines[prev_line_idx], MAX_LINE_LENGTH, p, _TRUNCATE);
        prev_line_idx = (prev_line_idx + 1) % MAX_CONTEXT_LINES;
        
        *line_end = saved;
        
        // Move to next line
        p = line_end;
        if (*p == '\r') p++;
        if (*p == '\n') p++;
        line_num++;
    }
    
    free(buffer);
    
    // Report progress
    if (engine->on_progress && (engine->files_searched % 10 == 0)) {
        engine->on_progress(engine->files_searched, engine->result_count, engine->callback_user_data);
    }
}

static BOOL IsTextFile(const wchar_t* path) {
    // Check by extension first
    const wchar_t* ext = PathFindExtensionW(path);
    if (ext) {
        const wchar_t* text_exts[] = {
            L".c", L".cpp", L".cc", L".cxx", L".h", L".hpp", L".hxx",
            L".py", L".js", L".ts", L".jsx", L".tsx", L".rs", L".go",
            L".java", L".cs", L".rb", L".php", L".pl", L".sh", L".bat",
            L".ps1", L".asm", L".s", L".json", L".xml", L".yaml", L".yml",
            L".toml", L".ini", L".cfg", L".conf", L".txt", L".md", L".rst",
            L".html", L".htm", L".css", L".scss", L".less", L".sql",
            L".cmake", L".make", L".mk", L".dockerfile", NULL
        };
        
        for (int i = 0; text_exts[i]; i++) {
            if (_wcsicmp(ext, text_exts[i]) == 0) return TRUE;
        }
    }
    
    // Check file content (first 4KB)
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, 
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    
    char buffer[4096];
    DWORD bytes_read;
    BOOL result = TRUE;
    
    if (ReadFile(hFile, buffer, sizeof(buffer), &bytes_read, NULL)) {
        // Check for null bytes (likely binary)
        for (DWORD i = 0; i < bytes_read; i++) {
            if (buffer[i] == '\0') {
                result = FALSE;
                break;
            }
        }
    }
    
    CloseHandle(hFile);
    return result;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

__declspec(dllexport)
void Search_SetMode(SearchEngine* engine, SearchMode mode) {
    if (engine) engine->mode = mode;
}

__declspec(dllexport)
void Search_SetCaseSensitive(SearchEngine* engine, BOOL case_sensitive) {
    if (engine) engine->case_sensitive = case_sensitive;
}

__declspec(dllexport)
void Search_SetFilePattern(SearchEngine* engine, const wchar_t* pattern) {
    if (engine && pattern) wcscpy_s(engine->file_pattern, MAX_PATH, pattern);
}

__declspec(dllexport)
void Search_AddExcludeDir(SearchEngine* engine, const wchar_t* dir) {
    if (!engine || !dir || engine->exclude_dir_count >= 16) return;
    wcscpy_s(engine->exclude_dirs[engine->exclude_dir_count++], MAX_PATH, dir);
}

__declspec(dllexport)
void Search_AddExcludeFile(SearchEngine* engine, const wchar_t* pattern) {
    if (!engine || !pattern || engine->exclude_file_count >= 16) return;
    wcscpy_s(engine->exclude_files[engine->exclude_file_count++], MAX_PATH, pattern);
}

__declspec(dllexport)
void Search_SetSearchBinary(SearchEngine* engine, BOOL search_binary) {
    if (engine) engine->search_binary_files = search_binary;
}

__declspec(dllexport)
void Search_SetSearchHidden(SearchEngine* engine, BOOL search_hidden) {
    if (engine) engine->search_in_hidden = search_hidden;
}

// ============================================================================
// RESULTS
// ============================================================================

__declspec(dllexport)
int Search_GetResultCount(SearchEngine* engine) {
    return engine ? engine->result_count : 0;
}

__declspec(dllexport)
const SearchResult* Search_GetResult(SearchEngine* engine, int index) {
    if (!engine || index < 0 || index >= engine->result_count) return NULL;
    return &engine->results[index];
}

__declspec(dllexport)
void Search_ClearResults(SearchEngine* engine) {
    if (!engine) return;
    EnterCriticalSection(&engine->cs);
    engine->result_count = 0;
    LeaveCriticalSection(&engine->cs);
}

__declspec(dllexport)
void Search_GetStats(SearchEngine* engine, int* files_searched, int* lines_searched, DWORD* elapsed_ms) {
    if (!engine) return;
    if (files_searched) *files_searched = engine->files_searched;
    if (lines_searched) *lines_searched = engine->lines_searched;
    if (elapsed_ms) *elapsed_ms = engine->search_active ? 
        (GetTickCount() - engine->start_time) : engine->elapsed_ms;
}

// ============================================================================
// CALLBACKS
// ============================================================================

__declspec(dllexport)
void Search_SetCallbacks(
    SearchEngine* engine,
    void (*on_result)(const SearchResult* result, void* user_data),
    void (*on_progress)(int files, int matches, void* user_data),
    void (*on_complete)(int total, DWORD elapsed, void* user_data),
    void* user_data
) {
    if (!engine) return;
    engine->on_result = on_result;
    engine->on_progress = on_progress;
    engine->on_complete = on_complete;
    engine->callback_user_data = user_data;
}

// ============================================================================
// REPLACE OPERATIONS
// ============================================================================

__declspec(dllexport)
int Search_ReplaceInFile(const wchar_t* file_path, const wchar_t* search, 
                          const wchar_t* replace, BOOL case_sensitive) {
    HANDLE hFile = CreateFileW(file_path, GENERIC_READ, FILE_SHARE_READ, NULL, 
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return -1;
    
    DWORD file_size = GetFileSize(hFile, NULL);
    if (file_size == 0) {
        CloseHandle(hFile);
        return 0;
    }
    
    char* buffer = (char*)malloc(file_size + 1);
    DWORD bytes_read;
    if (!ReadFile(hFile, buffer, file_size, &bytes_read, NULL)) {
        free(buffer);
        CloseHandle(hFile);
        return -1;
    }
    buffer[bytes_read] = '\0';
    CloseHandle(hFile);
    
    // Convert to wide string for processing
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
    wchar_t* wide_buffer = (wchar_t*)malloc(wide_len * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wide_buffer, wide_len);
    free(buffer);
    
    // Perform replacements
    int search_len = (int)wcslen(search);
    int replace_len = (int)wcslen(replace);
    int count = 0;
    
    // Count replacements first
    wchar_t* pos = wide_buffer;
    while ((pos = case_sensitive ? wcsstr(pos, search) : StrStrIW(pos, search)) != NULL) {
        count++;
        pos += search_len;
    }
    
    if (count == 0) {
        free(wide_buffer);
        return 0;
    }
    
    // Build result string
    int new_len = wide_len + count * (replace_len - search_len);
    wchar_t* result = (wchar_t*)malloc(new_len * sizeof(wchar_t));
    wchar_t* dst = result;
    wchar_t* src = wide_buffer;
    
    while (*src) {
        wchar_t* match = case_sensitive ? wcsstr(src, search) : StrStrIW(src, search);
        if (match == src) {
            wcscpy(dst, replace);
            dst += replace_len;
            src += search_len;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = L'\0';
    
    free(wide_buffer);
    
    // Convert back to UTF-8
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, result, -1, NULL, 0, NULL, NULL);
    buffer = (char*)malloc(utf8_len);
    WideCharToMultiByte(CP_UTF8, 0, result, -1, buffer, utf8_len, NULL, NULL);
    free(result);
    
    // Write back to file
    hFile = CreateFileW(file_path, GENERIC_WRITE, 0, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        free(buffer);
        return -1;
    }
    
    DWORD written;
    WriteFile(hFile, buffer, utf8_len - 1, &written, NULL);  // -1 to exclude null terminator
    CloseHandle(hFile);
    free(buffer);
    
    return count;
}
