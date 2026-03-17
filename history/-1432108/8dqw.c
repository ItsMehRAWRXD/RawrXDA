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
    FILE_TYPE_UNKNOWN = 0,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_SOURCE_C,
    FILE_TYPE_SOURCE_CPP,
    FILE_TYPE_SOURCE_H,
    FILE_TYPE_SOURCE_PY,
    FILE_TYPE_SOURCE_JS,
    FILE_TYPE_SOURCE_TS,
    FILE_TYPE_SOURCE_RS,
    FILE_TYPE_SOURCE_GO,
    FILE_TYPE_SOURCE_ASM,
    FILE_TYPE_CONFIG_JSON,
    FILE_TYPE_CONFIG_XML,
    FILE_TYPE_CONFIG_YAML,
    FILE_TYPE_CONFIG_INI,
    FILE_TYPE_TEXT,
    FILE_TYPE_MARKDOWN,
    FILE_TYPE_BINARY,
    FILE_TYPE_EXECUTABLE,
    FILE_TYPE_LIBRARY,
    FILE_TYPE_IMAGE
} FileType;

typedef struct {
    wchar_t name[MAX_PATH];
    wchar_t full_path[MAX_PATH];
    FileType type;
    DWORD attributes;
    LARGE_INTEGER size;
    FILETIME modified_time;
    int depth;
    BOOL expanded;      // For directories
    int parent_index;   // Index of parent directory (-1 for root)
    int child_count;
} FileEntry;

typedef struct {
    // Root directory
    wchar_t root_path[MAX_PATH];
    
    // File list (flat, but hierarchical via parent_index)
    FileEntry* files;
    int file_count;
    int file_capacity;
    
    // Filters
    wchar_t include_patterns[16][MAX_FILTER_LENGTH];
    wchar_t exclude_patterns[16][MAX_FILTER_LENGTH];
    int include_count;
    int exclude_count;
    BOOL show_hidden;
    BOOL show_ignored;
    
    // Selection
    int selected_index;
    int* multi_selection;
    int multi_selection_count;
    
    // Watch for changes
    HANDLE hDirWatch;
    HANDLE hWatchThread;
    volatile BOOL watch_active;
    
    // Callbacks
    void (*on_file_selected)(const wchar_t* path, FileType type, void* user_data);
    void (*on_file_double_clicked)(const wchar_t* path, FileType type, void* user_data);
    void (*on_files_changed)(void* user_data);
    void* callback_user_data;
    
    CRITICAL_SECTION cs;
} FileBrowser;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
static void ScanDirectory(FileBrowser* browser, const wchar_t* dir, int parent_index, int depth);
static FileType DetermineFileType(const wchar_t* name, DWORD attributes);
static BOOL MatchesFilter(FileBrowser* browser, const wchar_t* name, DWORD attributes);
static DWORD WINAPI DirectoryWatchThread(LPVOID param);

// ============================================================================
// BROWSER CREATION
// ============================================================================

__declspec(dllexport)
FileBrowser* FileBrowser_Create(void) {
    FileBrowser* browser = (FileBrowser*)calloc(1, sizeof(FileBrowser));
    if (!browser) return NULL;
    
    InitializeCriticalSection(&browser->cs);
    
    // Allocate file array
    browser->file_capacity = MAX_FILES;
    browser->files = (FileEntry*)calloc(browser->file_capacity, sizeof(FileEntry));
    browser->multi_selection = (int*)calloc(MAX_FILES, sizeof(int));
    
    // Default filters - exclude build artifacts
    wcscpy_s(browser->exclude_patterns[0], MAX_FILTER_LENGTH, L".git");
    wcscpy_s(browser->exclude_patterns[1], MAX_FILTER_LENGTH, L"node_modules");
    wcscpy_s(browser->exclude_patterns[2], MAX_FILTER_LENGTH, L"__pycache__");
    wcscpy_s(browser->exclude_patterns[3], MAX_FILTER_LENGTH, L".vs");
    wcscpy_s(browser->exclude_patterns[4], MAX_FILTER_LENGTH, L"build");
    wcscpy_s(browser->exclude_patterns[5], MAX_FILTER_LENGTH, L"Debug");
    wcscpy_s(browser->exclude_patterns[6], MAX_FILTER_LENGTH, L"Release");
    wcscpy_s(browser->exclude_patterns[7], MAX_FILTER_LENGTH, L"*.obj");
    wcscpy_s(browser->exclude_patterns[8], MAX_FILTER_LENGTH, L"*.pdb");
    browser->exclude_count = 9;
    
    browser->selected_index = -1;
    
    return browser;
}

__declspec(dllexport)
void FileBrowser_Destroy(FileBrowser* browser) {
    if (!browser) return;
    
    // Stop directory watch
    if (browser->watch_active) {
        browser->watch_active = FALSE;
        if (browser->hDirWatch) {
            FindCloseChangeNotification(browser->hDirWatch);
        }
        if (browser->hWatchThread) {
            WaitForSingleObject(browser->hWatchThread, 1000);
            CloseHandle(browser->hWatchThread);
        }
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
    
    // Verify directory exists
    DWORD attr = GetFileAttributesW(root_path);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        LeaveCriticalSection(&browser->cs);
        return FALSE;
    }
    
    wcscpy_s(browser->root_path, MAX_PATH, root_path);
    browser->file_count = 0;
    browser->selected_index = -1;
    browser->multi_selection_count = 0;
    
    // Scan directory
    ScanDirectory(browser, root_path, -1, 0);
    
    LeaveCriticalSection(&browser->cs);
    return TRUE;
}

__declspec(dllexport)
void FileBrowser_Refresh(FileBrowser* browser) {
    if (!browser || !browser->root_path[0]) return;
    
    EnterCriticalSection(&browser->cs);
    
    // Remember expanded directories
    wchar_t expanded_dirs[256][MAX_PATH];
    int expanded_count = 0;
    
    for (int i = 0; i < browser->file_count && expanded_count < 256; i++) {
        if (browser->files[i].type == FILE_TYPE_DIRECTORY && browser->files[i].expanded) {
            wcscpy_s(expanded_dirs[expanded_count++], MAX_PATH, browser->files[i].full_path);
        }
    }
    
    // Rescan
    browser->file_count = 0;
    ScanDirectory(browser, browser->root_path, -1, 0);
    
    // Restore expanded state
    for (int i = 0; i < browser->file_count; i++) {
        if (browser->files[i].type == FILE_TYPE_DIRECTORY) {
            for (int j = 0; j < expanded_count; j++) {
                if (wcscmp(browser->files[i].full_path, expanded_dirs[j]) == 0) {
                    browser->files[i].expanded = TRUE;
                    break;
                }
            }
        }
    }
    
    if (browser->on_files_changed) {
        browser->on_files_changed(browser->callback_user_data);
    }
    
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
        // Skip . and ..
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        
        // Check filters
        if (!MatchesFilter(browser, fd.cFileName, fd.dwFileAttributes))
            continue;
        
        if (browser->file_count >= browser->file_capacity)
            break;
        
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
        
        // Update parent's child count
        if (parent_index >= 0) {
            browser->files[parent_index].child_count++;
        }
        
        int current_index = browser->file_count++;
        
        // Recursively scan subdirectories (only if expanded or first level)
        if (entry->type == FILE_TYPE_DIRECTORY && depth == 0) {
            entry->expanded = TRUE;
            ScanDirectory(browser, entry->full_path, current_index, depth + 1);
        }
        
    } while (FindNextFileW(hFind, &fd));
    
    FindClose(hFind);
}

static FileType DetermineFileType(const wchar_t* name, DWORD attributes) {
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        return FILE_TYPE_DIRECTORY;
    }
    
    const wchar_t* ext = PathFindExtensionW(name);
    if (!ext || !*ext) return FILE_TYPE_TEXT;
    
    // Source files
    if (_wcsicmp(ext, L".c") == 0) return FILE_TYPE_SOURCE_C;
    if (_wcsicmp(ext, L".cpp") == 0 || _wcsicmp(ext, L".cc") == 0 || _wcsicmp(ext, L".cxx") == 0)
        return FILE_TYPE_SOURCE_CPP;
    if (_wcsicmp(ext, L".h") == 0 || _wcsicmp(ext, L".hpp") == 0 || _wcsicmp(ext, L".hxx") == 0)
        return FILE_TYPE_SOURCE_H;
    if (_wcsicmp(ext, L".py") == 0) return FILE_TYPE_SOURCE_PY;
    if (_wcsicmp(ext, L".js") == 0 || _wcsicmp(ext, L".jsx") == 0) return FILE_TYPE_SOURCE_JS;
    if (_wcsicmp(ext, L".ts") == 0 || _wcsicmp(ext, L".tsx") == 0) return FILE_TYPE_SOURCE_TS;
    if (_wcsicmp(ext, L".rs") == 0) return FILE_TYPE_SOURCE_RS;
    if (_wcsicmp(ext, L".go") == 0) return FILE_TYPE_SOURCE_GO;
    if (_wcsicmp(ext, L".asm") == 0 || _wcsicmp(ext, L".s") == 0) return FILE_TYPE_SOURCE_ASM;
    
    // Config files
    if (_wcsicmp(ext, L".json") == 0) return FILE_TYPE_CONFIG_JSON;
    if (_wcsicmp(ext, L".xml") == 0) return FILE_TYPE_CONFIG_XML;
    if (_wcsicmp(ext, L".yaml") == 0 || _wcsicmp(ext, L".yml") == 0) return FILE_TYPE_CONFIG_YAML;
    if (_wcsicmp(ext, L".ini") == 0 || _wcsicmp(ext, L".cfg") == 0) return FILE_TYPE_CONFIG_INI;
    
    // Text files
    if (_wcsicmp(ext, L".txt") == 0 || _wcsicmp(ext, L".log") == 0) return FILE_TYPE_TEXT;
    if (_wcsicmp(ext, L".md") == 0 || _wcsicmp(ext, L".markdown") == 0) return FILE_TYPE_MARKDOWN;
    
    // Binaries
    if (_wcsicmp(ext, L".exe") == 0) return FILE_TYPE_EXECUTABLE;
    if (_wcsicmp(ext, L".dll") == 0 || _wcsicmp(ext, L".so") == 0) return FILE_TYPE_LIBRARY;
    if (_wcsicmp(ext, L".png") == 0 || _wcsicmp(ext, L".jpg") == 0 || 
        _wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".ico") == 0)
        return FILE_TYPE_IMAGE;
    
    return FILE_TYPE_UNKNOWN;
}

static BOOL MatchesFilter(FileBrowser* browser, const wchar_t* name, DWORD attributes) {
    // Check hidden files
    if (!browser->show_hidden && (attributes & FILE_ATTRIBUTE_HIDDEN)) {
        return FALSE;
    }
    
    // Check exclude patterns
    for (int i = 0; i < browser->exclude_count; i++) {
        if (PathMatchSpecW(name, browser->exclude_patterns[i])) {
            return FALSE;
        }
    }
    
    // If include patterns are set, file must match at least one
    if (browser->include_count > 0 && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        BOOL matches = FALSE;
        for (int i = 0; i < browser->include_count; i++) {
            if (PathMatchSpecW(name, browser->include_patterns[i])) {
                matches = TRUE;
                break;
            }
        }
        if (!matches) return FALSE;
    }
    
    return TRUE;
}

// ============================================================================
// SELECTION
// ============================================================================

__declspec(dllexport)
void FileBrowser_SelectIndex(FileBrowser* browser, int index) {
    if (!browser || index < -1 || index >= browser->file_count) return;
    
    browser->selected_index = index;
    browser->multi_selection_count = 0;
    
    if (index >= 0 && browser->on_file_selected) {
        FileEntry* entry = &browser->files[index];
        browser->on_file_selected(entry->full_path, entry->type, browser->callback_user_data);
    }
}

__declspec(dllexport)
void FileBrowser_SelectPath(FileBrowser* browser, const wchar_t* path) {
    if (!browser || !path) return;
    
    for (int i = 0; i < browser->file_count; i++) {
        if (_wcsicmp(browser->files[i].full_path, path) == 0) {
            FileBrowser_SelectIndex(browser, i);
            return;
        }
    }
}

__declspec(dllexport)
void FileBrowser_AddToSelection(FileBrowser* browser, int index) {
    if (!browser || index < 0 || index >= browser->file_count) return;
    
    // Check if already selected
    for (int i = 0; i < browser->multi_selection_count; i++) {
        if (browser->multi_selection[i] == index) return;
    }
    
    browser->multi_selection[browser->multi_selection_count++] = index;
}

__declspec(dllexport)
void FileBrowser_ClearSelection(FileBrowser* browser) {
    if (!browser) return;
    browser->selected_index = -1;
    browser->multi_selection_count = 0;
}

// ============================================================================
// EXPAND / COLLAPSE
// ============================================================================

__declspec(dllexport)
void FileBrowser_ExpandDirectory(FileBrowser* browser, int index) {
    if (!browser || index < 0 || index >= browser->file_count) return;
    
    FileEntry* entry = &browser->files[index];
    if (entry->type != FILE_TYPE_DIRECTORY || entry->expanded) return;
    
    EnterCriticalSection(&browser->cs);
    
    entry->expanded = TRUE;
    
    // Scan children
    ScanDirectory(browser, entry->full_path, index, entry->depth + 1);
    
    LeaveCriticalSection(&browser->cs);
}

__declspec(dllexport)
void FileBrowser_CollapseDirectory(FileBrowser* browser, int index) {
    if (!browser || index < 0 || index >= browser->file_count) return;
    
    FileEntry* entry = &browser->files[index];
    if (entry->type != FILE_TYPE_DIRECTORY || !entry->expanded) return;
    
    EnterCriticalSection(&browser->cs);
    
    entry->expanded = FALSE;
    
    // Remove children from list (they have parent_index == index or descendants)
    // This is a simple implementation - mark for removal and compact
    // For now, just refresh
    FileBrowser_Refresh(browser);
    
    LeaveCriticalSection(&browser->cs);
}

__declspec(dllexport)
void FileBrowser_ToggleDirectory(FileBrowser* browser, int index) {
    if (!browser || index < 0 || index >= browser->file_count) return;
    
    if (browser->files[index].expanded) {
        FileBrowser_CollapseDirectory(browser, index);
    } else {
        FileBrowser_ExpandDirectory(browser, index);
    }
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

__declspec(dllexport)
BOOL FileBrowser_CreateFile(FileBrowser* browser, const wchar_t* name) {
    if (!browser || !name || !browser->root_path[0]) return FALSE;
    
    wchar_t path[MAX_PATH];
    if (browser->selected_index >= 0 && 
        browser->files[browser->selected_index].type == FILE_TYPE_DIRECTORY) {
        swprintf_s(path, MAX_PATH, L"%s\\%s", 
            browser->files[browser->selected_index].full_path, name);
    } else {
        swprintf_s(path, MAX_PATH, L"%s\\%s", browser->root_path, name);
    }
    
    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, 
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    
    CloseHandle(hFile);
    FileBrowser_Refresh(browser);
    return TRUE;
}

__declspec(dllexport)
BOOL FileBrowser_CreateDirectory(FileBrowser* browser, const wchar_t* name) {
    if (!browser || !name || !browser->root_path[0]) return FALSE;
    
    wchar_t path[MAX_PATH];
    if (browser->selected_index >= 0 && 
        browser->files[browser->selected_index].type == FILE_TYPE_DIRECTORY) {
        swprintf_s(path, MAX_PATH, L"%s\\%s", 
            browser->files[browser->selected_index].full_path, name);
    } else {
        swprintf_s(path, MAX_PATH, L"%s\\%s", browser->root_path, name);
    }
    
    if (!CreateDirectoryW(path, NULL)) return FALSE;
    
    FileBrowser_Refresh(browser);
    return TRUE;
}

__declspec(dllexport)
BOOL FileBrowser_Delete(FileBrowser* browser, int index) {
    if (!browser || index < 0 || index >= browser->file_count) return FALSE;
    
    FileEntry* entry = &browser->files[index];
    BOOL result;
    
    if (entry->type == FILE_TYPE_DIRECTORY) {
        // Use SHFileOperation for recursive delete
        SHFILEOPSTRUCTW fo = { 0 };
        wchar_t from[MAX_PATH + 1] = { 0 };
        wcscpy_s(from, MAX_PATH, entry->full_path);
        fo.wFunc = FO_DELETE;
        fo.pFrom = from;
        fo.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI;
        result = (SHFileOperationW(&fo) == 0);
    } else {
        result = DeleteFileW(entry->full_path);
    }
    
    if (result) {
        FileBrowser_Refresh(browser);
    }
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
// QUERIES
// ============================================================================

__declspec(dllexport)
int FileBrowser_GetFileCount(FileBrowser* browser) {
    return browser ? browser->file_count : 0;
}

__declspec(dllexport)
const FileEntry* FileBrowser_GetFile(FileBrowser* browser, int index) {
    if (!browser || index < 0 || index >= browser->file_count) return NULL;
    return &browser->files[index];
}

__declspec(dllexport)
int FileBrowser_GetSelectedIndex(FileBrowser* browser) {
    return browser ? browser->selected_index : -1;
}

__declspec(dllexport)
const wchar_t* FileBrowser_GetSelectedPath(FileBrowser* browser) {
    if (!browser || browser->selected_index < 0) return NULL;
    return browser->files[browser->selected_index].full_path;
}

__declspec(dllexport)
const wchar_t* FileBrowser_GetRootPath(FileBrowser* browser) {
    return browser ? browser->root_path : NULL;
}

// ============================================================================
// FILTERING
// ============================================================================

__declspec(dllexport)
void FileBrowser_AddIncludePattern(FileBrowser* browser, const wchar_t* pattern) {
    if (!browser || !pattern || browser->include_count >= 16) return;
    wcscpy_s(browser->include_patterns[browser->include_count++], MAX_FILTER_LENGTH, pattern);
}

__declspec(dllexport)
void FileBrowser_AddExcludePattern(FileBrowser* browser, const wchar_t* pattern) {
    if (!browser || !pattern || browser->exclude_count >= 16) return;
    wcscpy_s(browser->exclude_patterns[browser->exclude_count++], MAX_FILTER_LENGTH, pattern);
}

__declspec(dllexport)
void FileBrowser_ClearFilters(FileBrowser* browser) {
    if (!browser) return;
    browser->include_count = 0;
    browser->exclude_count = 0;
}

__declspec(dllexport)
void FileBrowser_SetShowHidden(FileBrowser* browser, BOOL show) {
    if (browser) browser->show_hidden = show;
}

// ============================================================================
// DIRECTORY WATCHING
// ============================================================================

__declspec(dllexport)
void FileBrowser_StartWatch(FileBrowser* browser) {
    if (!browser || browser->watch_active || !browser->root_path[0]) return;
    
    browser->hDirWatch = FindFirstChangeNotificationW(
        browser->root_path,
        TRUE,  // Watch subtree
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
        FILE_NOTIFY_CHANGE_LAST_WRITE
    );
    
    if (browser->hDirWatch == INVALID_HANDLE_VALUE) return;
    
    browser->watch_active = TRUE;
    browser->hWatchThread = CreateThread(NULL, 0, DirectoryWatchThread, browser, 0, NULL);
}

__declspec(dllexport)
void FileBrowser_StopWatch(FileBrowser* browser) {
    if (!browser || !browser->watch_active) return;
    
    browser->watch_active = FALSE;
    if (browser->hDirWatch) {
        FindCloseChangeNotification(browser->hDirWatch);
        browser->hDirWatch = NULL;
    }
}

static DWORD WINAPI DirectoryWatchThread(LPVOID param) {
    FileBrowser* browser = (FileBrowser*)param;
    
    while (browser->watch_active) {
        DWORD result = WaitForSingleObject(browser->hDirWatch, 1000);
        
        if (result == WAIT_OBJECT_0 && browser->watch_active) {
            FileBrowser_Refresh(browser);
            FindNextChangeNotification(browser->hDirWatch);
        }
    }
    
    return 0;
}

// ============================================================================
// CALLBACKS
// ============================================================================

__declspec(dllexport)
void FileBrowser_SetCallbacks(
    FileBrowser* browser,
    void (*on_file_selected)(const wchar_t* path, FileType type, void* user_data),
    void (*on_file_double_clicked)(const wchar_t* path, FileType type, void* user_data),
    void (*on_files_changed)(void* user_data),
    void* user_data
) {
    if (!browser) return;
    browser->on_file_selected = on_file_selected;
    browser->on_file_double_clicked = on_file_double_clicked;
    browser->on_files_changed = on_files_changed;
    browser->callback_user_data = user_data;
}
