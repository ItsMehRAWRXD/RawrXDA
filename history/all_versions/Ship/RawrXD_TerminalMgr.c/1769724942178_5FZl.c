// RawrXD Terminal Manager - Pure Win32 (No Qt)
// Replaces: terminal_pool.cpp
// Multiple terminal sessions with CreateProcess + ConPTY

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
#define MAX_TERMINALS       16
#define TERMINAL_BUFFER_SIZE (64 * 1024)  // 64KB per terminal
#define MAX_COMMAND_LENGTH  4096

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef enum {
    TERM_STATE_IDLE = 0,
    TERM_STATE_RUNNING,
    TERM_STATE_FINISHED,
    TERM_STATE_ERROR
} TerminalState;

typedef struct {
    int id;
    wchar_t name[64];
    wchar_t shell_path[MAX_PATH];
    wchar_t working_dir[MAX_PATH];
    
    // Process handles
    HANDLE hProcess;
    HANDLE hThread;
    HANDLE hStdinWrite;
    HANDLE hStdoutRead;
    HANDLE hReaderThread;
    DWORD dwPid;
    
    // State
    TerminalState state;
    DWORD exit_code;
    volatile BOOL stop_requested;
    
    // Output buffer (circular)
    char* output_buffer;
    int output_head;
    int output_tail;
    int output_size;
    
    // Command history
    wchar_t* history[256];
    int history_count;
    int history_pos;
    
    CRITICAL_SECTION cs;
} Terminal;

typedef struct {
    Terminal terminals[MAX_TERMINALS];
    int terminal_count;
    int active_terminal;
    
    // Default shell
    wchar_t default_shell[MAX_PATH];
    wchar_t default_working_dir[MAX_PATH];
    
    // Callbacks
    void (*on_output)(int terminal_id, const char* data, int length, void* user_data);
    void (*on_state_changed)(int terminal_id, TerminalState state, void* user_data);
    void (*on_exit)(int terminal_id, DWORD exit_code, void* user_data);
    void* callback_user_data;
    
    CRITICAL_SECTION cs;
} TerminalManager;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
static DWORD WINAPI TerminalReaderThread(LPVOID param);
static void WriteToOutputBuffer(Terminal* term, const char* data, int length);

// ============================================================================
// MANAGER CREATION
// ============================================================================

__declspec(dllexport)
TerminalManager* TerminalMgr_Create(void) {
    TerminalManager* mgr = (TerminalManager*)calloc(1, sizeof(TerminalManager));
    if (!mgr) return NULL;
    
    InitializeCriticalSection(&mgr->cs);
    mgr->active_terminal = -1;
    
    // Default to cmd.exe or pwsh.exe
    wchar_t pwsh_path[MAX_PATH];
    ExpandEnvironmentStringsW(L"%ProgramFiles%\\PowerShell\\7\\pwsh.exe", pwsh_path, MAX_PATH);
    
    if (GetFileAttributesW(pwsh_path) != INVALID_FILE_ATTRIBUTES) {
        wcscpy_s(mgr->default_shell, MAX_PATH, pwsh_path);
    } else {
        wcscpy_s(mgr->default_shell, MAX_PATH, L"cmd.exe");
    }
    
    // Default working directory
    GetCurrentDirectoryW(MAX_PATH, mgr->default_working_dir);
    
    return mgr;
}

__declspec(dllexport)
void TerminalMgr_Destroy(TerminalManager* mgr) {
    if (!mgr) return;
    
    // Close all terminals
    for (int i = 0; i < mgr->terminal_count; i++) {
        TerminalMgr_CloseTerminal(mgr, mgr->terminals[i].id);
    }
    
    DeleteCriticalSection(&mgr->cs);
    free(mgr);
}

// ============================================================================
// TERMINAL MANAGEMENT
// ============================================================================

__declspec(dllexport)
int TerminalMgr_CreateTerminal(TerminalManager* mgr, const wchar_t* name, 
                                const wchar_t* shell, const wchar_t* working_dir) {
    if (!mgr || mgr->terminal_count >= MAX_TERMINALS) return -1;
    
    EnterCriticalSection(&mgr->cs);
    
    Terminal* term = &mgr->terminals[mgr->terminal_count];
    memset(term, 0, sizeof(Terminal));
    
    term->id = mgr->terminal_count;
    
    if (name) {
        wcsncpy_s(term->name, 64, name, _TRUNCATE);
    } else {
        swprintf_s(term->name, 64, L"Terminal %d", term->id + 1);
    }
    
    wcsncpy_s(term->shell_path, MAX_PATH, 
        shell ? shell : mgr->default_shell, _TRUNCATE);
    wcsncpy_s(term->working_dir, MAX_PATH, 
        working_dir ? working_dir : mgr->default_working_dir, _TRUNCATE);
    
    // Allocate output buffer
    term->output_buffer = (char*)malloc(TERMINAL_BUFFER_SIZE);
    term->output_size = TERMINAL_BUFFER_SIZE;
    
    InitializeCriticalSection(&term->cs);
    
    term->state = TERM_STATE_IDLE;
    
    int id = mgr->terminal_count++;
    
    if (mgr->active_terminal < 0) {
        mgr->active_terminal = id;
    }
    
    LeaveCriticalSection(&mgr->cs);
    return id;
}

__declspec(dllexport)
BOOL TerminalMgr_StartTerminal(TerminalManager* mgr, int terminal_id) {
    if (!mgr || terminal_id < 0 || terminal_id >= mgr->terminal_count) return FALSE;
    
    Terminal* term = &mgr->terminals[terminal_id];
    
    if (term->state == TERM_STATE_RUNNING) return TRUE;  // Already running
    
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hStdinRead, hStdoutWrite;
    
    // Create pipes for stdin/stdout
    if (!CreatePipe(&hStdinRead, &term->hStdinWrite, &sa, 0)) return FALSE;
    if (!CreatePipe(&term->hStdoutRead, &hStdoutWrite, &sa, 0)) {
        CloseHandle(hStdinRead);
        CloseHandle(term->hStdinWrite);
        return FALSE;
    }
    
    // Don't inherit the write end of stdin or read end of stdout
    SetHandleInformation(term->hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(term->hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    
    // Start the shell process
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    
    // Build command line
    wchar_t cmdline[MAX_PATH + 32];
    wcscpy_s(cmdline, MAX_PATH + 32, term->shell_path);
    
    if (!CreateProcessW(
        NULL,               // Application name (use cmdline)
        cmdline,            // Command line
        NULL,               // Process attributes
        NULL,               // Thread attributes
        TRUE,               // Inherit handles
        CREATE_NO_WINDOW,   // Creation flags
        NULL,               // Environment
        term->working_dir,  // Current directory
        &si,                // Startup info
        &pi                 // Process info
    )) {
        CloseHandle(hStdinRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(term->hStdinWrite);
        CloseHandle(term->hStdoutRead);
        term->state = TERM_STATE_ERROR;
        return FALSE;
    }
    
    // Close unneeded handles (child has duplicates)
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
    
    term->hProcess = pi.hProcess;
    term->hThread = pi.hThread;
    term->dwPid = pi.dwProcessId;
    term->state = TERM_STATE_RUNNING;
    term->stop_requested = FALSE;
    
    // Start reader thread
    term->hReaderThread = CreateThread(NULL, 0, TerminalReaderThread, term, 0, NULL);
    
    if (mgr->on_state_changed) {
        mgr->on_state_changed(terminal_id, TERM_STATE_RUNNING, mgr->callback_user_data);
    }
    
    return TRUE;
}

__declspec(dllexport)
void TerminalMgr_CloseTerminal(TerminalManager* mgr, int terminal_id) {
    if (!mgr || terminal_id < 0 || terminal_id >= mgr->terminal_count) return;
    
    Terminal* term = &mgr->terminals[terminal_id];
    
    if (term->state == TERM_STATE_RUNNING) {
        term->stop_requested = TRUE;
        
        // Try graceful termination first
        TerminateProcess(term->hProcess, 0);
        WaitForSingleObject(term->hProcess, 1000);
    }
    
    // Close handles
    if (term->hStdinWrite) CloseHandle(term->hStdinWrite);
    if (term->hStdoutRead) CloseHandle(term->hStdoutRead);
    if (term->hProcess) CloseHandle(term->hProcess);
    if (term->hThread) CloseHandle(term->hThread);
    if (term->hReaderThread) {
        WaitForSingleObject(term->hReaderThread, 500);
        CloseHandle(term->hReaderThread);
    }
    
    // Free buffers
    if (term->output_buffer) free(term->output_buffer);
    
    // Free history
    for (int i = 0; i < term->history_count; i++) {
        if (term->history[i]) free(term->history[i]);
    }
    
    DeleteCriticalSection(&term->cs);
    
    term->state = TERM_STATE_IDLE;
    term->hProcess = NULL;
    term->hStdinWrite = NULL;
    term->hStdoutRead = NULL;
}

// ============================================================================
// INPUT / OUTPUT
// ============================================================================

__declspec(dllexport)
BOOL TerminalMgr_SendInput(TerminalManager* mgr, int terminal_id, const char* input, int length) {
    if (!mgr || terminal_id < 0 || terminal_id >= mgr->terminal_count) return FALSE;
    
    Terminal* term = &mgr->terminals[terminal_id];
    
    if (term->state != TERM_STATE_RUNNING || !term->hStdinWrite) return FALSE;
    
    DWORD written;
    return WriteFile(term->hStdinWrite, input, length, &written, NULL) && written == (DWORD)length;
}

__declspec(dllexport)
BOOL TerminalMgr_SendCommand(TerminalManager* mgr, int terminal_id, const wchar_t* command) {
    if (!mgr || !command) return FALSE;
    
    Terminal* term = &mgr->terminals[terminal_id];
    
    // Convert to UTF-8
    char cmd_utf8[MAX_COMMAND_LENGTH];
    int len = WideCharToMultiByte(CP_UTF8, 0, command, -1, cmd_utf8, MAX_COMMAND_LENGTH - 2, NULL, NULL);
    if (len <= 0) return FALSE;
    
    // Append newline
    cmd_utf8[len - 1] = '\n';
    cmd_utf8[len] = '\0';
    
    // Add to history
    if (term->history_count < 256) {
        term->history[term->history_count++] = _wcsdup(command);
    }
    term->history_pos = term->history_count;
    
    return TerminalMgr_SendInput(mgr, terminal_id, cmd_utf8, len);
}

__declspec(dllexport)
int TerminalMgr_ReadOutput(TerminalManager* mgr, int terminal_id, char* buffer, int max_length) {
    if (!mgr || terminal_id < 0 || terminal_id >= mgr->terminal_count) return 0;
    
    Terminal* term = &mgr->terminals[terminal_id];
    
    EnterCriticalSection(&term->cs);
    
    int available = 0;
    if (term->output_head >= term->output_tail) {
        available = term->output_head - term->output_tail;
    } else {
        available = term->output_size - term->output_tail + term->output_head;
    }
    
    int to_read = (available < max_length) ? available : max_length;
    int read = 0;
    
    while (read < to_read) {
        buffer[read++] = term->output_buffer[term->output_tail];
        term->output_tail = (term->output_tail + 1) % term->output_size;
    }
    
    LeaveCriticalSection(&term->cs);
    return read;
}

__declspec(dllexport)
const char* TerminalMgr_GetOutputBuffer(TerminalManager* mgr, int terminal_id, int* length) {
    if (!mgr || terminal_id < 0 || terminal_id >= mgr->terminal_count) return NULL;
    
    Terminal* term = &mgr->terminals[terminal_id];
    
    if (length) {
        if (term->output_head >= term->output_tail) {
            *length = term->output_head - term->output_tail;
        } else {
            *length = term->output_size - term->output_tail + term->output_head;
        }
    }
    
    return term->output_buffer;
}

// ============================================================================
// READER THREAD
// ============================================================================

static DWORD WINAPI TerminalReaderThread(LPVOID param) {
    Terminal* term = (Terminal*)param;
    char buffer[4096];
    DWORD bytes_read;
    
    while (!term->stop_requested) {
        // Check if process is still running
        DWORD exit_code;
        if (!GetExitCodeProcess(term->hProcess, &exit_code) || exit_code != STILL_ACTIVE) {
            term->exit_code = exit_code;
            term->state = TERM_STATE_FINISHED;
            break;
        }
        
        // Check for available data
        DWORD available = 0;
        if (!PeekNamedPipe(term->hStdoutRead, NULL, 0, NULL, &available, NULL) || available == 0) {
            Sleep(10);  // Small sleep to avoid busy-waiting
            continue;
        }
        
        // Read data
        if (ReadFile(term->hStdoutRead, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
            WriteToOutputBuffer(term, buffer, bytes_read);
        }
    }
    
    return 0;
}

static void WriteToOutputBuffer(Terminal* term, const char* data, int length) {
    EnterCriticalSection(&term->cs);
    
    for (int i = 0; i < length; i++) {
        term->output_buffer[term->output_head] = data[i];
        term->output_head = (term->output_head + 1) % term->output_size;
        
        // If we've caught up with tail, advance tail (discard oldest)
        if (term->output_head == term->output_tail) {
            term->output_tail = (term->output_tail + 1) % term->output_size;
        }
    }
    
    LeaveCriticalSection(&term->cs);
}

// ============================================================================
// HISTORY NAVIGATION
// ============================================================================

__declspec(dllexport)
const wchar_t* TerminalMgr_GetPrevCommand(TerminalManager* mgr, int terminal_id) {
    if (!mgr || terminal_id < 0 || terminal_id >= mgr->terminal_count) return NULL;
    
    Terminal* term = &mgr->terminals[terminal_id];
    
    if (term->history_count == 0 || term->history_pos <= 0) return NULL;
    
    term->history_pos--;
    return term->history[term->history_pos];
}

__declspec(dllexport)
const wchar_t* TerminalMgr_GetNextCommand(TerminalManager* mgr, int terminal_id) {
    if (!mgr || terminal_id < 0 || terminal_id >= mgr->terminal_count) return NULL;
    
    Terminal* term = &mgr->terminals[terminal_id];
    
    if (term->history_pos >= term->history_count - 1) return NULL;
    
    term->history_pos++;
    return term->history[term->history_pos];
}

// ============================================================================
// QUERIES
// ============================================================================

__declspec(dllexport)
int TerminalMgr_GetTerminalCount(TerminalManager* mgr) {
    return mgr ? mgr->terminal_count : 0;
}

__declspec(dllexport)
int TerminalMgr_GetActiveTerminal(TerminalManager* mgr) {
    return mgr ? mgr->active_terminal : -1;
}

__declspec(dllexport)
void TerminalMgr_SetActiveTerminal(TerminalManager* mgr, int terminal_id) {
    if (mgr && terminal_id >= 0 && terminal_id < mgr->terminal_count) {
        mgr->active_terminal = terminal_id;
    }
}

__declspec(dllexport)
TerminalState TerminalMgr_GetState(TerminalManager* mgr, int terminal_id) {
    if (!mgr || terminal_id < 0 || terminal_id >= mgr->terminal_count) return TERM_STATE_ERROR;
    return mgr->terminals[terminal_id].state;
}

__declspec(dllexport)
const wchar_t* TerminalMgr_GetName(TerminalManager* mgr, int terminal_id) {
    if (!mgr || terminal_id < 0 || terminal_id >= mgr->terminal_count) return NULL;
    return mgr->terminals[terminal_id].name;
}

__declspec(dllexport)
const wchar_t* TerminalMgr_GetWorkingDir(TerminalManager* mgr, int terminal_id) {
    if (!mgr || terminal_id < 0 || terminal_id >= mgr->terminal_count) return NULL;
    return mgr->terminals[terminal_id].working_dir;
}

__declspec(dllexport)
void TerminalMgr_SetWorkingDir(TerminalManager* mgr, int terminal_id, const wchar_t* dir) {
    if (!mgr || terminal_id < 0 || terminal_id >= mgr->terminal_count || !dir) return;
    wcsncpy_s(mgr->terminals[terminal_id].working_dir, MAX_PATH, dir, _TRUNCATE);
}

// ============================================================================
// CONFIGURATION
// ============================================================================

__declspec(dllexport)
void TerminalMgr_SetDefaultShell(TerminalManager* mgr, const wchar_t* shell_path) {
    if (mgr && shell_path) {
        wcsncpy_s(mgr->default_shell, MAX_PATH, shell_path, _TRUNCATE);
    }
}

__declspec(dllexport)
void TerminalMgr_SetDefaultWorkingDir(TerminalManager* mgr, const wchar_t* dir) {
    if (mgr && dir) {
        wcsncpy_s(mgr->default_working_dir, MAX_PATH, dir, _TRUNCATE);
    }
}

__declspec(dllexport)
void TerminalMgr_SetCallbacks(
    TerminalManager* mgr,
    void (*on_output)(int terminal_id, const char* data, int length, void* user_data),
    void (*on_state_changed)(int terminal_id, TerminalState state, void* user_data),
    void (*on_exit)(int terminal_id, DWORD exit_code, void* user_data),
    void* user_data
) {
    if (!mgr) return;
    mgr->on_output = on_output;
    mgr->on_state_changed = on_state_changed;
    mgr->on_exit = on_exit;
    mgr->callback_user_data = user_data;
}
