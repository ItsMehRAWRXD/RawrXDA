// RawrXD File Browser - Pure Win32 (No Qt)
// Replaces: file_browser.cpp
// Directory tree navigation and file management

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
#include <shlobj.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// ============================================================================
// CONFIGURATION
// ============================================================================
#define MAX_FILES           8192
#define MAX_PATH_DEPTH      64
#define MAX_FILTER_LENGTH   256

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef enum {
    FTYPE_UNKNOWN = 0,
    FTYPE_DIRECTORY,
    FTYPE_SOURCE_C,
    FTYPE_SOURCE_CPP,
    FTYPE_SOURCE_H,
    FTYPE_SOURCE_PY,
    FTYPE_SOURCE_JS,
    FTYPE_SOURCE_TS,
    FTYPE_SOURCE_RS,
    FTYPE_SOURCE_GO,
    FTYPE_SOURCE_ASM,
    FTYPE_CONFIG_JSON,
    FTYPE_CONFIG_XML,
    FTYPE_CONFIG_YAML,
    FTYPE_CONFIG_INI,
    FTYPE_TEXT,
    FTYPE_MARKDOWN,
    FTYPE_BINARY,
    FTYPE_EXECUTABLE,
    FTYPE_LIBRARY,
    FTYPE_IMAGE
} BrowserFileType;

typedef struct {
    wchar_t name[MAX_PATH];
    wchar_t full_path[MAX_PATH];
    BrowserFileType type;
    DWORD attributes;
    LARGE_INTEGER size;
    FILETIME modified_time;
    int depth;
    BOOL expanded;
    int parent_index;
    int child_count;
} FileEntry;

typedef struct {
    wchar_t root_path[MAX_PATH];
    FileEntry* files;
    int file_count;
    int file_capacity;
    wchar_t include_patterns[16][MAX_FILTER_LENGTH];
    wchar_t exclude_patterns[16][MAX_FILTER_LENGTH];
    int include_count;
    int exclude_count;
    BOOL show_hidden;
    int selected_index;
    int* multi_selection;
    int multi_selection_count;
    HANDLE hDirWatch;
    HANDLE hWatchThread;
    volatile BOOL watch_active;
    void (*on_file_selected)(const wchar_t* path, BrowserFileType type, void* user_data);
    void (*on_file_double_clicked)(const wchar_t* path, BrowserFileType type, void* user_data);
    void (*on_files_changed)(void* user_data);
    void* callback_user_data;
    CRITICAL_SECTION cs;
} FileBrowser;

// Forward declarations
static void ScanDirectory(FileBrowser* browser, const wchar_t* dir, int parent_index, int depth);
static BrowserFileType DetermineFileType(const wchar_t* name, DWORD attributes);
static BOOL MatchesFilter(FileBrowser* browser, const wchar_t* name, DWORD attributes);
__declspec(dllexport) void FileBrowser_Refresh(FileBrowser* browser);

// ============================================================================
// BROWSER CREATION
// ============================================================================

__declspec(dllexport)
FileBrowser* FileBrowser_Create(void) {
    FileBrowser* browser = (FileBrowser*)calloc(1, sizeof(FileBrowser));
    if (!browser) return NULL;
    
    InitializeCriticalSection(&browser->cs);
    browser->file_capacity = MAX_FILES;
    browser->files = (FileEntry*)calloc(browser->file_capacity, sizeof(FileEntry));
    browser->multi_selection = (int*)calloc(MAX_FILES, sizeof(int));
    
    wcscpy_s(browser->exclude_patterns[0], MAX_FILTER_LENGTH, L".git");
    wcscpy_s(browser->exclude_patterns[1], MAX_FILTER_LENGTH, L"node_modules");
    wcscpy_s(browser->exclude_patterns[2], MAX_FILTER_LENGTH, L"__pycache__");
    wcscpy_s(browser->exclude_patterns[3], MAX_FILTER_LENGTH, L".vs");
    wcscpy_s(browser->exclude_patterns[4], MAX_FILTER_LENGTH, L"*.obj");
    wcscpy_s(browser->exclude_patterns[5], MAX_FILTER_LENGTH, L"*.pdb");
    browser->exclude_count = 6;
    browser->selected_index = -1;
    
    return browser;
}

__declspec(dllexport)
void FileBrowser_Destroy(FileBrowser* browser) {
    if (!browser) return;
    if (browser->watch_active) {
        browser->watch_active = FALSE;
        if (browser->hDirWatch) FindCloseChangeNotification(browser->hDirWatch);
        if (browser->hWatchThread) { WaitForSingleObject(browser->hWatchThread, 1000); CloseHandle(browser->hWatchThread); }
    }
    if (browser->files) free(browser->files);
    if (browser->multi_selection) free(browser->multi_selection);
    DeleteCriticalSection(&browser->cs);
    free(browser);
}

// ============================================================================
// DIRECTORY OPERATIONS
// ============================================================================

__declspec(dllexport)
BOOL FileBrowser_SetRoot(FileBrowser* browser, const wchar_t* root_path) {
    if (!browser || !root_path) return FALSE;
    
    EnterCriticalSection(&browser->cs);
    DWORD attr = GetFileAttributesW(root_path);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        LeaveCriticalSection(&browser->cs);
        return FALSE;
    }
    
    wcscpy_s(browser->root_path, MAX_PATH, root_path);
    browser->file_count = 0;
    browser->selected_index = -1;
    browser->multi_selection_count = 0;
    ScanDirectory(browser, root_path, -1, 0);
    LeaveCriticalSection(&browser->cs);
    return TRUE;
}

__declspec(dllexport)
void FileBrowser_Refresh(FileBrowser* browser) {
    if (!browser || !browser->root_path[0]) return;
    EnterCriticalSection(&browser->cs);
    browser->file_count = 0;
    ScanDirectory(browser, browser->root_path, -1, 0);
    if (browser->on_files_changed) browser->on_files_changed(browser->callback_user_data);
    LeaveCriticalSection(&browser->cs);
}

static void ScanDirectory(FileBrowser* browser, const wchar_t* dir, int parent_index, int depth) {
    if (depth > MAX_PATH_DEPTH) return;
    
    WIN32_FIND_DATAW fd;
    wchar_t search_path[MAX_PATH];
    swprintf_s(search_path, MAX_PATH, L"%s\\*", dir);
    
    HANDLE hFind = FindFirstFileW(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if (!MatchesFilter(browser, fd.cFileName, fd.dwFileAttributes)) continue;
        if (browser->file_count >= browser->file_capacity) break;
        
        FileEntry* entry = &browser->files[browser->file_count];
        wcscpy_s(entry->name, MAX_PATH, fd.cFileName);
        swprintf_s(entry->full_path, MAX_PATH, L"%s\\%s", dir, fd.cFileName);
        entry->type = DetermineFileType(fd.cFileName, fd.dwFileAttributes);
        entry->attributes = fd.dwFileAttributes;
        entry->size.LowPart = fd.nFileSizeLow;
        entry->size.HighPart = fd.nFileSizeHigh;
        entry->modified_time = fd.ftLastWriteTime;
        entry->depth = depth;
        entry->parent_index = parent_index;
        entry->expanded = FALSE;
        entry->child_count = 0;
        if (parent_index >= 0) browser->files[parent_index].child_count++;
        
        int current_index = browser->file_count++;
        if (entry->type == FTYPE_DIRECTORY && depth == 0) {
            entry->expanded = TRUE;
            ScanDirectory(browser, entry->full_path, current_index, depth + 1);
        }
    } while (FindNextFileW(hFind, &fd));
    
    FindClose(hFind);
}

static BrowserFileType DetermineFileType(const wchar_t* name, DWORD attributes) {
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) return FTYPE_DIRECTORY;
    
    const wchar_t* ext = PathFindExtensionW(name);
    if (!ext || !*ext) return FTYPE_TEXT;
    
    if (_wcsicmp(ext, L".c") == 0) return FTYPE_SOURCE_C;
    if (_wcsicmp(ext, L".cpp") == 0 || _wcsicmp(ext, L".cc") == 0) return FTYPE_SOURCE_CPP;
    if (_wcsicmp(ext, L".h") == 0 || _wcsicmp(ext, L".hpp") == 0) return FTYPE_SOURCE_H;
    if (_wcsicmp(ext, L".py") == 0) return FTYPE_SOURCE_PY;
    if (_wcsicmp(ext, L".js") == 0) return FTYPE_SOURCE_JS;
    if (_wcsicmp(ext, L".ts") == 0) return FTYPE_SOURCE_TS;
    if (_wcsicmp(ext, L".rs") == 0) return FTYPE_SOURCE_RS;
    if (_wcsicmp(ext, L".go") == 0) return FTYPE_SOURCE_GO;
    if (_wcsicmp(ext, L".asm") == 0 || _wcsicmp(ext, L".s") == 0) return FTYPE_SOURCE_ASM;
    if (_wcsicmp(ext, L".json") == 0) return FTYPE_CONFIG_JSON;
    if (_wcsicmp(ext, L".xml") == 0) return FTYPE_CONFIG_XML;
    if (_wcsicmp(ext, L".yaml") == 0 || _wcsicmp(ext, L".yml") == 0) return FTYPE_CONFIG_YAML;
    if (_wcsicmp(ext, L".ini") == 0) return FTYPE_CONFIG_INI;
    if (_wcsicmp(ext, L".txt") == 0) return FTYPE_TEXT;
    if (_wcsicmp(ext, L".md") == 0) return FTYPE_MARKDOWN;
    if (_wcsicmp(ext, L".exe") == 0) return FTYPE_EXECUTABLE;
    if (_wcsicmp(ext, L".dll") == 0) return FTYPE_LIBRARY;
    if (_wcsicmp(ext, L".png") == 0 || _wcsicmp(ext, L".jpg") == 0) return FTYPE_IMAGE;
    
    return FTYPE_UNKNOWN;
}

static BOOL MatchesFilter(FileBrowser* browser, const wchar_t* name, DWORD attributes) {
    if (!browser->show_hidden && (attributes & FILE_ATTRIBUTE_HIDDEN)) return FALSE;
    for (int i = 0; i < browser->exclude_count; i++) {
        if (PathMatchSpecW(name, browser->exclude_patterns[i])) return FALSE;
    }
    if (browser->include_count > 0 && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        BOOL matches = FALSE;
        for (int i = 0; i < browser->include_count; i++) {
            if (PathMatchSpecW(name, browser->include_patterns[i])) { matches = TRUE; break; }
        }
        if (!matches) return FALSE;
    }
    return TRUE;
}

// ============================================================================
// SELECTION & QUERIES
// ============================================================================

__declspec(dllexport)
void FileBrowser_SelectIndex(FileBrowser* browser, int index) {
    if (!browser || index < -1 || index >= browser->file_count) return;
    browser->selected_index = index;
    browser->multi_selection_count = 0;
    if (index >= 0 && browser->on_file_selected) {
        browser->on_file_selected(browser->files[index].full_path, browser->files[index].type, browser->callback_user_data);
    }
}

__declspec(dllexport)
int FileBrowser_GetFileCount(FileBrowser* browser) { return browser ? browser->file_count : 0; }

__declspec(dllexport)
const FileEntry* FileBrowser_GetFile(FileBrowser* browser, int index) {
    if (!browser || index < 0 || index >= browser->file_count) return NULL;
    return &browser->files[index];
}

__declspec(dllexport)
int FileBrowser_GetSelectedIndex(FileBrowser* browser) { return browser ? browser->selected_index : -1; }

__declspec(dllexport)
const wchar_t* FileBrowser_GetSelectedPath(FileBrowser* browser) {
    if (!browser || browser->selected_index < 0) return NULL;
    return browser->files[browser->selected_index].full_path;
}

__declspec(dllexport)
const wchar_t* FileBrowser_GetRootPath(FileBrowser* browser) { return browser ? browser->root_path : NULL; }

// ============================================================================
// FILE OPERATIONS
// ============================================================================

__declspec(dllexport)
BOOL FileBrowser_CreateFile(FileBrowser* browser, const wchar_t* name) {
    if (!browser || !name || !browser->root_path[0]) return FALSE;
    wchar_t path[MAX_PATH];
    swprintf_s(path, MAX_PATH, L"%s\\%s", browser->root_path, name);
    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    CloseHandle(hFile);
    FileBrowser_Refresh(browser);
    return TRUE;
}

__declspec(dllexport)
BOOL FileBrowser_CreateDirectory(FileBrowser* browser, const wchar_t* name) {
    if (!browser || !name || !browser->root_path[0]) return FALSE;
    wchar_t path[MAX_PATH];
    swprintf_s(path, MAX_PATH, L"%s\\%s", browser->root_path, name);
    if (!CreateDirectoryW(path, NULL)) return FALSE;
    FileBrowser_Refresh(browser);
    return TRUE;
}

__declspec(dllexport)
BOOL FileBrowser_Delete(FileBrowser* browser, int index) {
    if (!browser || index < 0 || index >= browser->file_count) return FALSE;
    FileEntry* entry = &browser->files[index];
    BOOL result = (entry->type == FTYPE_DIRECTORY) ? RemoveDirectoryW(entry->full_path) : DeleteFileW(entry->full_path);
    if (result) FileBrowser_Refresh(browser);
    return result;
}

__declspec(dllexport)
BOOL FileBrowser_Rename(FileBrowser* browser, int index, const wchar_t* new_name) {
    if (!browser || index < 0 || index >= browser->file_count || !new_name) return FALSE;
    FileEntry* entry = &browser->files[index];
    wchar_t new_path[MAX_PATH];
    wchar_t* last_slash = wcsrchr(entry->full_path, L'\\');
    if (!last_slash) return FALSE;
    size_t dir_len = last_slash - entry->full_path;
    wcsncpy_s(new_path, MAX_PATH, entry->full_path, dir_len);
    new_path[dir_len] = L'\\';
    wcscpy_s(new_path + dir_len + 1, MAX_PATH - dir_len - 1, new_name);
    if (!MoveFileW(entry->full_path, new_path)) return FALSE;
    FileBrowser_Refresh(browser);
    return TRUE;
}

// ============================================================================
// EXPAND/COLLAPSE & CALLBACKS
// ============================================================================

__declspec(dllexport)
void FileBrowser_ExpandDirectory(FileBrowser* browser, int index) {
    if (!browser || index < 0 || index >= browser->file_count) return;
    FileEntry* entry = &browser->files[index];
    if (entry->type != FTYPE_DIRECTORY || entry->expanded) return;
    EnterCriticalSection(&browser->cs);
    entry->expanded = TRUE;
    ScanDirectory(browser, entry->full_path, index, entry->depth + 1);
    LeaveCriticalSection(&browser->cs);
}

__declspec(dllexport)
void FileBrowser_CollapseDirectory(FileBrowser* browser, int index) {
    if (!browser || index < 0 || index >= browser->file_count) return;
    FileEntry* entry = &browser->files[index];
    if (entry->type != FTYPE_DIRECTORY || !entry->expanded) return;
    entry->expanded = FALSE;
    FileBrowser_Refresh(browser);
}

__declspec(dllexport)
void FileBrowser_SetCallbacks(FileBrowser* browser, 
    void (*on_selected)(const wchar_t*, BrowserFileType, void*),
    void (*on_dblclick)(const wchar_t*, BrowserFileType, void*),
    void (*on_changed)(void*), void* user_data) {
    if (!browser) return;
    browser->on_file_selected = on_selected;
    browser->on_file_double_clicked = on_dblclick;
    browser->on_files_changed = on_changed;
    browser->callback_user_data = user_data;
}

__declspec(dllexport)
void FileBrowser_AddExcludePattern(FileBrowser* browser, const wchar_t* pattern) {
    if (!browser || !pattern || browser->exclude_count >= 16) return;
    wcscpy_s(browser->exclude_patterns[browser->exclude_count++], MAX_FILTER_LENGTH, pattern);
}

__declspec(dllexport)
void FileBrowser_SetShowHidden(FileBrowser* browser, BOOL show) {
    if (browser) browser->show_hidden = show;
}
