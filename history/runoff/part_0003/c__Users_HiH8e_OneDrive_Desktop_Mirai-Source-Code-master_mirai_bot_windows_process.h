#pragma once

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")

// Windows-specific process and network management
BOOL win_enum_processes_init(void);
void win_enum_processes_cleanup(void);
BOOL win_get_next_process(DWORD *pid, char *exe_path, size_t path_len);
BOOL win_get_process_info(DWORD pid, char *exe_path, size_t path_len);
BOOL win_kill_process(DWORD pid);
BOOL win_kill_process_by_port(USHORT port);
BOOL win_check_port_listening(USHORT port, DWORD *pid);
BOOL win_get_current_exe_path(char *path, size_t path_len);

// Windows process enumeration state
typedef struct {
    HANDLE snapshot;
    PROCESSENTRY32W pe32;
    BOOL first_call;
} win_process_enum_t;

// Global state for process enumeration
extern win_process_enum_t g_proc_enum;

#endif // _WIN32