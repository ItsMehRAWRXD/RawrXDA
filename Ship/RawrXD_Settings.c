// RawrXD Settings Manager - Pure Win32 (No Qt)
// Replaces: settings.cpp
// INI-based configuration with registry fallback

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE  
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

// ============================================================================
// CONFIGURATION
// ============================================================================
#define MAX_SETTINGS        256
#define MAX_KEY_LENGTH      128
#define MAX_VALUE_LENGTH    4096
#define MAX_SECTION_LENGTH  64

// Registry key for settings
static const wchar_t* REGISTRY_KEY = L"Software\\RawrXD\\IDE";

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef enum {
    SETTING_TYPE_STRING = 0,
    SETTING_TYPE_INT,
    SETTING_TYPE_BOOL,
    SETTING_TYPE_FLOAT,
    SETTING_TYPE_COLOR,
    SETTING_TYPE_BINARY
} SettingType;

typedef struct {
    wchar_t section[MAX_SECTION_LENGTH];
    wchar_t key[MAX_KEY_LENGTH];
    SettingType type;
    union {
        wchar_t str_value[MAX_VALUE_LENGTH];
        int int_value;
        BOOL bool_value;
        float float_value;
        COLORREF color_value;
        struct {
            BYTE* data;
            int size;
        } binary;
    };
    BOOL modified;
} Setting;

typedef struct {
    // Settings storage
    Setting settings[MAX_SETTINGS];
    int setting_count;
    
    // File paths
    wchar_t ini_path[MAX_PATH];
    wchar_t app_data_path[MAX_PATH];
    
    // Options
    BOOL use_registry;
    BOOL auto_save;
    
    // Change tracking
    void (*on_setting_changed)(const wchar_t* section, const wchar_t* key, void* user_data);
    void* callback_user_data;
    
    CRITICAL_SECTION cs;
} SettingsManager;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
__declspec(dllexport) BOOL Settings_Load(SettingsManager* mgr);
__declspec(dllexport) BOOL Settings_Save(SettingsManager* mgr);

// ============================================================================
// DEFAULT SETTINGS
// ============================================================================

typedef struct {
    const wchar_t* section;
    const wchar_t* key;
    SettingType type;
    const wchar_t* default_value;
} DefaultSetting;

static const DefaultSetting g_defaults[] = {
    // Editor settings
    { L"Editor", L"FontFamily", SETTING_TYPE_STRING, L"Consolas" },
    { L"Editor", L"FontSize", SETTING_TYPE_INT, L"12" },
    { L"Editor", L"TabSize", SETTING_TYPE_INT, L"4" },
    { L"Editor", L"UseSpaces", SETTING_TYPE_BOOL, L"true" },
    { L"Editor", L"WordWrap", SETTING_TYPE_BOOL, L"false" },
    { L"Editor", L"ShowLineNumbers", SETTING_TYPE_BOOL, L"true" },
    { L"Editor", L"HighlightCurrentLine", SETTING_TYPE_BOOL, L"true" },
    { L"Editor", L"AutoIndent", SETTING_TYPE_BOOL, L"true" },
    
    // Theme settings
    { L"Theme", L"Name", SETTING_TYPE_STRING, L"Dark" },
    { L"Theme", L"BackgroundColor", SETTING_TYPE_COLOR, L"1E1E1E" },
    { L"Theme", L"ForegroundColor", SETTING_TYPE_COLOR, L"D4D4D4" },
    { L"Theme", L"SelectionColor", SETTING_TYPE_COLOR, L"264F78" },
    { L"Theme", L"GutterColor", SETTING_TYPE_COLOR, L"252526" },
    { L"Theme", L"CommentColor", SETTING_TYPE_COLOR, L"6A9955" },
    { L"Theme", L"KeywordColor", SETTING_TYPE_COLOR, L"569CD6" },
    { L"Theme", L"StringColor", SETTING_TYPE_COLOR, L"CE9178" },
    
    // AI/Completion settings
    { L"AI", L"Enabled", SETTING_TYPE_BOOL, L"true" },
    { L"AI", L"ModelEndpoint", SETTING_TYPE_STRING, L"localhost" },
    { L"AI", L"ModelPort", SETTING_TYPE_INT, L"11434" },
    { L"AI", L"ModelName", SETTING_TYPE_STRING, L"codellama" },
    { L"AI", L"MaxTokens", SETTING_TYPE_INT, L"256" },
    { L"AI", L"Temperature", SETTING_TYPE_FLOAT, L"0.2" },
    { L"AI", L"AutoComplete", SETTING_TYPE_BOOL, L"true" },
    { L"AI", L"GhostText", SETTING_TYPE_BOOL, L"true" },
    { L"AI", L"CompletionDelay", SETTING_TYPE_INT, L"150" },
    
    // LSP settings
    { L"LSP", L"Enabled", SETTING_TYPE_BOOL, L"true" },
    { L"LSP", L"CLanguageServer", SETTING_TYPE_STRING, L"clangd" },
    { L"LSP", L"PythonServer", SETTING_TYPE_STRING, L"pylsp" },
    { L"LSP", L"TypeScriptServer", SETTING_TYPE_STRING, L"typescript-language-server" },
    
    // Window settings
    { L"Window", L"Width", SETTING_TYPE_INT, L"1280" },
    { L"Window", L"Height", SETTING_TYPE_INT, L"720" },
    { L"Window", L"X", SETTING_TYPE_INT, L"-1" },
    { L"Window", L"Y", SETTING_TYPE_INT, L"-1" },
    { L"Window", L"Maximized", SETTING_TYPE_BOOL, L"false" },
    { L"Window", L"SidebarWidth", SETTING_TYPE_INT, L"250" },
    { L"Window", L"TerminalHeight", SETTING_TYPE_INT, L"200" },
    
    // Recent files
    { L"Recent", L"MaxFiles", SETTING_TYPE_INT, L"20" },
    { L"Recent", L"LastWorkspace", SETTING_TYPE_STRING, L"" },
    
    // Terminal settings
    { L"Terminal", L"Shell", SETTING_TYPE_STRING, L"cmd.exe" },
    { L"Terminal", L"FontFamily", SETTING_TYPE_STRING, L"Consolas" },
    { L"Terminal", L"FontSize", SETTING_TYPE_INT, L"11" },
    
    { NULL, NULL, SETTING_TYPE_STRING, NULL }  // Terminator
};

// ============================================================================
// CREATION / DESTRUCTION
// ============================================================================

__declspec(dllexport)
SettingsManager* Settings_Create(void) {
    SettingsManager* mgr = (SettingsManager*)calloc(1, sizeof(SettingsManager));
    if (!mgr) return NULL;
    
    InitializeCriticalSection(&mgr->cs);
    mgr->auto_save = TRUE;
    
    // Get AppData path
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, mgr->app_data_path))) {
        PathAppendW(mgr->app_data_path, L"RawrXD");
        CreateDirectoryW(mgr->app_data_path, NULL);
        
        // Build INI path
        wcscpy_s(mgr->ini_path, MAX_PATH, mgr->app_data_path);
        PathAppendW(mgr->ini_path, L"settings.ini");
    }
    
    // Load defaults
    for (int i = 0; g_defaults[i].key != NULL; i++) {
        Setting* s = &mgr->settings[mgr->setting_count++];
        wcscpy_s(s->section, MAX_SECTION_LENGTH, g_defaults[i].section);
        wcscpy_s(s->key, MAX_KEY_LENGTH, g_defaults[i].key);
        s->type = g_defaults[i].type;
        
        switch (s->type) {
            case SETTING_TYPE_STRING:
                wcscpy_s(s->str_value, MAX_VALUE_LENGTH, g_defaults[i].default_value);
                break;
            case SETTING_TYPE_INT:
                s->int_value = _wtoi(g_defaults[i].default_value);
                break;
            case SETTING_TYPE_BOOL:
                s->bool_value = (wcscmp(g_defaults[i].default_value, L"true") == 0);
                break;
            case SETTING_TYPE_FLOAT:
                s->float_value = (float)_wtof(g_defaults[i].default_value);
                break;
            case SETTING_TYPE_COLOR: {
                wchar_t* end;
                s->color_value = wcstoul(g_defaults[i].default_value, &end, 16);
                // Convert RGB to BGR for Win32
                s->color_value = 
                    ((s->color_value & 0xFF0000) >> 16) |
                    (s->color_value & 0x00FF00) |
                    ((s->color_value & 0x0000FF) << 16);
                break;
            }
            default:
                break;
        }
    }
    
    // Load saved settings (overrides defaults)
    Settings_Load(mgr);
    
    return mgr;
}

__declspec(dllexport)
void Settings_Destroy(SettingsManager* mgr) {
    if (!mgr) return;
    
    if (mgr->auto_save) {
        Settings_Save(mgr);
    }
    
    // Free binary data
    for (int i = 0; i < mgr->setting_count; i++) {
        if (mgr->settings[i].type == SETTING_TYPE_BINARY && 
            mgr->settings[i].binary.data) {
            free(mgr->settings[i].binary.data);
        }
    }
    
    DeleteCriticalSection(&mgr->cs);
    free(mgr);
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

static Setting* FindSetting(SettingsManager* mgr, const wchar_t* section, const wchar_t* key) {
    for (int i = 0; i < mgr->setting_count; i++) {
        if (wcscmp(mgr->settings[i].section, section) == 0 &&
            wcscmp(mgr->settings[i].key, key) == 0) {
            return &mgr->settings[i];
        }
    }
    return NULL;
}

static Setting* GetOrCreateSetting(SettingsManager* mgr, const wchar_t* section, 
                                    const wchar_t* key, SettingType type) {
    Setting* s = FindSetting(mgr, section, key);
    if (s) return s;
    
    if (mgr->setting_count >= MAX_SETTINGS) return NULL;
    
    s = &mgr->settings[mgr->setting_count++];
    wcscpy_s(s->section, MAX_SECTION_LENGTH, section);
    wcscpy_s(s->key, MAX_KEY_LENGTH, key);
    s->type = type;
    s->modified = TRUE;
    return s;
}

// ============================================================================
// GETTERS
// ============================================================================

__declspec(dllexport)
const wchar_t* Settings_GetString(SettingsManager* mgr, const wchar_t* section, 
                                   const wchar_t* key, const wchar_t* default_value) {
    if (!mgr) return default_value;
    
    EnterCriticalSection(&mgr->cs);
    Setting* s = FindSetting(mgr, section, key);
    const wchar_t* result = (s && s->type == SETTING_TYPE_STRING) ? s->str_value : default_value;
    LeaveCriticalSection(&mgr->cs);
    return result;
}

__declspec(dllexport)
int Settings_GetInt(SettingsManager* mgr, const wchar_t* section, 
                     const wchar_t* key, int default_value) {
    if (!mgr) return default_value;
    
    EnterCriticalSection(&mgr->cs);
    Setting* s = FindSetting(mgr, section, key);
    int result = (s && s->type == SETTING_TYPE_INT) ? s->int_value : default_value;
    LeaveCriticalSection(&mgr->cs);
    return result;
}

__declspec(dllexport)
BOOL Settings_GetBool(SettingsManager* mgr, const wchar_t* section, 
                       const wchar_t* key, BOOL default_value) {
    if (!mgr) return default_value;
    
    EnterCriticalSection(&mgr->cs);
    Setting* s = FindSetting(mgr, section, key);
    BOOL result = (s && s->type == SETTING_TYPE_BOOL) ? s->bool_value : default_value;
    LeaveCriticalSection(&mgr->cs);
    return result;
}

__declspec(dllexport)
float Settings_GetFloat(SettingsManager* mgr, const wchar_t* section, 
                         const wchar_t* key, float default_value) {
    if (!mgr) return default_value;
    
    EnterCriticalSection(&mgr->cs);
    Setting* s = FindSetting(mgr, section, key);
    float result = (s && s->type == SETTING_TYPE_FLOAT) ? s->float_value : default_value;
    LeaveCriticalSection(&mgr->cs);
    return result;
}

__declspec(dllexport)
COLORREF Settings_GetColor(SettingsManager* mgr, const wchar_t* section, 
                            const wchar_t* key, COLORREF default_value) {
    if (!mgr) return default_value;
    
    EnterCriticalSection(&mgr->cs);
    Setting* s = FindSetting(mgr, section, key);
    COLORREF result = (s && s->type == SETTING_TYPE_COLOR) ? s->color_value : default_value;
    LeaveCriticalSection(&mgr->cs);
    return result;
}

// ============================================================================
// SETTERS
// ============================================================================

__declspec(dllexport)
void Settings_SetString(SettingsManager* mgr, const wchar_t* section, 
                         const wchar_t* key, const wchar_t* value) {
    if (!mgr) return;
    
    EnterCriticalSection(&mgr->cs);
    Setting* s = GetOrCreateSetting(mgr, section, key, SETTING_TYPE_STRING);
    if (s) {
        wcscpy_s(s->str_value, MAX_VALUE_LENGTH, value);
        s->modified = TRUE;
        
        if (mgr->on_setting_changed) {
            mgr->on_setting_changed(section, key, mgr->callback_user_data);
        }
    }
    LeaveCriticalSection(&mgr->cs);
}

__declspec(dllexport)
void Settings_SetInt(SettingsManager* mgr, const wchar_t* section, 
                      const wchar_t* key, int value) {
    if (!mgr) return;
    
    EnterCriticalSection(&mgr->cs);
    Setting* s = GetOrCreateSetting(mgr, section, key, SETTING_TYPE_INT);
    if (s) {
        s->int_value = value;
        s->modified = TRUE;
        
        if (mgr->on_setting_changed) {
            mgr->on_setting_changed(section, key, mgr->callback_user_data);
        }
    }
    LeaveCriticalSection(&mgr->cs);
}

__declspec(dllexport)
void Settings_SetBool(SettingsManager* mgr, const wchar_t* section, 
                       const wchar_t* key, BOOL value) {
    if (!mgr) return;
    
    EnterCriticalSection(&mgr->cs);
    Setting* s = GetOrCreateSetting(mgr, section, key, SETTING_TYPE_BOOL);
    if (s) {
        s->bool_value = value;
        s->modified = TRUE;
        
        if (mgr->on_setting_changed) {
            mgr->on_setting_changed(section, key, mgr->callback_user_data);
        }
    }
    LeaveCriticalSection(&mgr->cs);
}

__declspec(dllexport)
void Settings_SetFloat(SettingsManager* mgr, const wchar_t* section, 
                        const wchar_t* key, float value) {
    if (!mgr) return;
    
    EnterCriticalSection(&mgr->cs);
    Setting* s = GetOrCreateSetting(mgr, section, key, SETTING_TYPE_FLOAT);
    if (s) {
        s->float_value = value;
        s->modified = TRUE;
        
        if (mgr->on_setting_changed) {
            mgr->on_setting_changed(section, key, mgr->callback_user_data);
        }
    }
    LeaveCriticalSection(&mgr->cs);
}

__declspec(dllexport)
void Settings_SetColor(SettingsManager* mgr, const wchar_t* section, 
                        const wchar_t* key, COLORREF value) {
    if (!mgr) return;
    
    EnterCriticalSection(&mgr->cs);
    Setting* s = GetOrCreateSetting(mgr, section, key, SETTING_TYPE_COLOR);
    if (s) {
        s->color_value = value;
        s->modified = TRUE;
        
        if (mgr->on_setting_changed) {
            mgr->on_setting_changed(section, key, mgr->callback_user_data);
        }
    }
    LeaveCriticalSection(&mgr->cs);
}

// ============================================================================
// PERSISTENCE (INI FILE)
// ============================================================================

__declspec(dllexport)
BOOL Settings_Load(SettingsManager* mgr) {
    if (!mgr || !mgr->ini_path[0]) return FALSE;
    
    // Check if INI file exists
    if (GetFileAttributesW(mgr->ini_path) == INVALID_FILE_ATTRIBUTES) {
        return FALSE;  // File doesn't exist, use defaults
    }
    
    EnterCriticalSection(&mgr->cs);
    
    for (int i = 0; i < mgr->setting_count; i++) {
        Setting* s = &mgr->settings[i];
        wchar_t buffer[MAX_VALUE_LENGTH];
        
        DWORD read = GetPrivateProfileStringW(
            s->section, s->key, L"", buffer, MAX_VALUE_LENGTH, mgr->ini_path);
        
        if (read > 0) {
            switch (s->type) {
                case SETTING_TYPE_STRING:
                    wcscpy_s(s->str_value, MAX_VALUE_LENGTH, buffer);
                    break;
                case SETTING_TYPE_INT:
                    s->int_value = _wtoi(buffer);
                    break;
                case SETTING_TYPE_BOOL:
                    s->bool_value = (wcscmp(buffer, L"true") == 0 || wcscmp(buffer, L"1") == 0);
                    break;
                case SETTING_TYPE_FLOAT:
                    s->float_value = (float)_wtof(buffer);
                    break;
                case SETTING_TYPE_COLOR: {
                    wchar_t* end;
                    DWORD rgb = wcstoul(buffer, &end, 16);
                    // Convert RGB to BGR
                    s->color_value = 
                        ((rgb & 0xFF0000) >> 16) |
                        (rgb & 0x00FF00) |
                        ((rgb & 0x0000FF) << 16);
                    break;
                }
                default:
                    break;
            }
            s->modified = FALSE;
        }
    }
    
    LeaveCriticalSection(&mgr->cs);
    return TRUE;
}

__declspec(dllexport)
BOOL Settings_Save(SettingsManager* mgr) {
    if (!mgr || !mgr->ini_path[0]) return FALSE;
    
    EnterCriticalSection(&mgr->cs);
    
    for (int i = 0; i < mgr->setting_count; i++) {
        Setting* s = &mgr->settings[i];
        wchar_t buffer[MAX_VALUE_LENGTH];
        
        switch (s->type) {
            case SETTING_TYPE_STRING:
                wcscpy_s(buffer, MAX_VALUE_LENGTH, s->str_value);
                break;
            case SETTING_TYPE_INT:
                swprintf_s(buffer, MAX_VALUE_LENGTH, L"%d", s->int_value);
                break;
            case SETTING_TYPE_BOOL:
                wcscpy_s(buffer, MAX_VALUE_LENGTH, s->bool_value ? L"true" : L"false");
                break;
            case SETTING_TYPE_FLOAT:
                swprintf_s(buffer, MAX_VALUE_LENGTH, L"%.6f", s->float_value);
                break;
            case SETTING_TYPE_COLOR: {
                // Convert BGR to RGB for storage
                DWORD rgb = 
                    ((s->color_value & 0xFF0000) >> 16) |
                    (s->color_value & 0x00FF00) |
                    ((s->color_value & 0x0000FF) << 16);
                swprintf_s(buffer, MAX_VALUE_LENGTH, L"%06X", rgb);
                break;
            }
            default:
                continue;
        }
        
        WritePrivateProfileStringW(s->section, s->key, buffer, mgr->ini_path);
        s->modified = FALSE;
    }
    
    LeaveCriticalSection(&mgr->cs);
    return TRUE;
}

// ============================================================================
// REGISTRY PERSISTENCE (ALTERNATIVE)
// ============================================================================

__declspec(dllexport)
BOOL Settings_LoadFromRegistry(SettingsManager* mgr) {
    if (!mgr) return FALSE;
    
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return FALSE;
    }
    
    EnterCriticalSection(&mgr->cs);
    
    for (int i = 0; i < mgr->setting_count; i++) {
        Setting* s = &mgr->settings[i];
        
        // Build full key name: Section_Key
        wchar_t value_name[256];
        swprintf_s(value_name, 256, L"%s_%s", s->section, s->key);
        
        DWORD type, size;
        BYTE buffer[MAX_VALUE_LENGTH * 2];
        size = sizeof(buffer);
        
        if (RegQueryValueExW(hKey, value_name, NULL, &type, buffer, &size) == ERROR_SUCCESS) {
            switch (s->type) {
                case SETTING_TYPE_STRING:
                    if (type == REG_SZ) {
                        wcscpy_s(s->str_value, MAX_VALUE_LENGTH, (wchar_t*)buffer);
                    }
                    break;
                case SETTING_TYPE_INT:
                    if (type == REG_DWORD) {
                        s->int_value = *(int*)buffer;
                    }
                    break;
                case SETTING_TYPE_BOOL:
                    if (type == REG_DWORD) {
                        s->bool_value = *(DWORD*)buffer != 0;
                    }
                    break;
                case SETTING_TYPE_FLOAT:
                    if (type == REG_BINARY && size == sizeof(float)) {
                        s->float_value = *(float*)buffer;
                    }
                    break;
                case SETTING_TYPE_COLOR:
                    if (type == REG_DWORD) {
                        s->color_value = *(COLORREF*)buffer;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    
    RegCloseKey(hKey);
    LeaveCriticalSection(&mgr->cs);
    return TRUE;
}

__declspec(dllexport)
BOOL Settings_SaveToRegistry(SettingsManager* mgr) {
    if (!mgr) return FALSE;
    
    HKEY hKey;
    DWORD disp;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, NULL, 
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &disp) != ERROR_SUCCESS) {
        return FALSE;
    }
    
    EnterCriticalSection(&mgr->cs);
    
    for (int i = 0; i < mgr->setting_count; i++) {
        Setting* s = &mgr->settings[i];
        
        wchar_t value_name[256];
        swprintf_s(value_name, 256, L"%s_%s", s->section, s->key);
        
        switch (s->type) {
            case SETTING_TYPE_STRING:
                RegSetValueExW(hKey, value_name, 0, REG_SZ, 
                    (BYTE*)s->str_value, (DWORD)(wcslen(s->str_value) + 1) * sizeof(wchar_t));
                break;
            case SETTING_TYPE_INT:
                RegSetValueExW(hKey, value_name, 0, REG_DWORD, 
                    (BYTE*)&s->int_value, sizeof(DWORD));
                break;
            case SETTING_TYPE_BOOL: {
                DWORD val = s->bool_value ? 1 : 0;
                RegSetValueExW(hKey, value_name, 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
                break;
            }
            case SETTING_TYPE_FLOAT:
                RegSetValueExW(hKey, value_name, 0, REG_BINARY, 
                    (BYTE*)&s->float_value, sizeof(float));
                break;
            case SETTING_TYPE_COLOR:
                RegSetValueExW(hKey, value_name, 0, REG_DWORD, 
                    (BYTE*)&s->color_value, sizeof(COLORREF));
                break;
            default:
                break;
        }
    }
    
    RegCloseKey(hKey);
    LeaveCriticalSection(&mgr->cs);
    return TRUE;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

__declspec(dllexport)
void Settings_SetCallback(SettingsManager* mgr, 
    void (*callback)(const wchar_t* section, const wchar_t* key, void* user_data),
    void* user_data
) {
    if (!mgr) return;
    mgr->on_setting_changed = callback;
    mgr->callback_user_data = user_data;
}

__declspec(dllexport)
void Settings_SetAutoSave(SettingsManager* mgr, BOOL auto_save) {
    if (mgr) mgr->auto_save = auto_save;
}

__declspec(dllexport)
const wchar_t* Settings_GetIniPath(SettingsManager* mgr) {
    return mgr ? mgr->ini_path : NULL;
}

__declspec(dllexport)
void Settings_ResetToDefaults(SettingsManager* mgr) {
    if (!mgr) return;
    
    EnterCriticalSection(&mgr->cs);
    
    for (int i = 0; g_defaults[i].key != NULL && i < mgr->setting_count; i++) {
        Setting* s = FindSetting(mgr, g_defaults[i].section, g_defaults[i].key);
        if (!s) continue;
        
        switch (s->type) {
            case SETTING_TYPE_STRING:
                wcscpy_s(s->str_value, MAX_VALUE_LENGTH, g_defaults[i].default_value);
                break;
            case SETTING_TYPE_INT:
                s->int_value = _wtoi(g_defaults[i].default_value);
                break;
            case SETTING_TYPE_BOOL:
                s->bool_value = (wcscmp(g_defaults[i].default_value, L"true") == 0);
                break;
            case SETTING_TYPE_FLOAT:
                s->float_value = (float)_wtof(g_defaults[i].default_value);
                break;
            case SETTING_TYPE_COLOR: {
                wchar_t* end;
                s->color_value = wcstoul(g_defaults[i].default_value, &end, 16);
                s->color_value = 
                    ((s->color_value & 0xFF0000) >> 16) |
                    (s->color_value & 0x00FF00) |
                    ((s->color_value & 0x0000FF) << 16);
                break;
            }
            default:
                break;
        }
        s->modified = TRUE;
    }
    
    LeaveCriticalSection(&mgr->cs);
}

__declspec(dllexport)
int Settings_Export(SettingsManager* mgr, const wchar_t* export_path) {
    if (!mgr || !export_path) return -1;
    
    // Just copy INI file
    return CopyFileW(mgr->ini_path, export_path, FALSE) ? 0 : -1;
}

__declspec(dllexport)
int Settings_Import(SettingsManager* mgr, const wchar_t* import_path) {
    if (!mgr || !import_path) return -1;
    
    // Copy and reload
    if (!CopyFileW(import_path, mgr->ini_path, FALSE)) {
        return -1;
    }
    
    return Settings_Load(mgr) ? 0 : -1;
}
