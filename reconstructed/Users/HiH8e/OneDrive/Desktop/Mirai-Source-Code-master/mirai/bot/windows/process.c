#ifdef _WIN32

#include "windows_process.h"
#include "includes.h"
#include "util.h"
#include <stdio.h>

// Global state for process enumeration
win_process_enum_t g_proc_enum = {INVALID_HANDLE_VALUE, {0}, TRUE};

BOOL win_enum_processes_init(void)
{
    if (g_proc_enum.snapshot != INVALID_HANDLE_VALUE)
        CloseHandle(g_proc_enum.snapshot);
    
    g_proc_enum.snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (g_proc_enum.snapshot == INVALID_HANDLE_VALUE)
        return FALSE;
    
    g_proc_enum.pe32.dwSize = sizeof(PROCESSENTRY32W);
    g_proc_enum.first_call = TRUE;
    
    return TRUE;
}

void win_enum_processes_cleanup(void)
{
    if (g_proc_enum.snapshot != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_proc_enum.snapshot);
        g_proc_enum.snapshot = INVALID_HANDLE_VALUE;
    }
}

BOOL win_get_next_process(DWORD *pid, char *exe_path, size_t path_len)
{
    BOOL result;
    
    if (g_proc_enum.snapshot == INVALID_HANDLE_VALUE)
        return FALSE;
    
    if (g_proc_enum.first_call)
    {
        result = Process32FirstW(g_proc_enum.snapshot, &g_proc_enum.pe32);
        g_proc_enum.first_call = FALSE;
    }
    else
    {
        result = Process32NextW(g_proc_enum.snapshot, &g_proc_enum.pe32);
    }
    
    if (!result)
        return FALSE;
    
    *pid = g_proc_enum.pe32.th32ProcessID;
    
    // Convert wide string to multibyte
    int len = WideCharToMultiByte(CP_UTF8, 0, g_proc_enum.pe32.szExeFile, -1, 
                                  exe_path, (int)path_len, NULL, NULL);
    if (len == 0)
    {
        exe_path[0] = '\0';
        return FALSE;
    }
    
    return TRUE;
}

BOOL win_get_process_info(DWORD pid, char *exe_path, size_t path_len)
{
    HANDLE hProcess;
    DWORD needed;
    WCHAR path_wide[MAX_PATH];
    
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL)
        return FALSE;
    
    if (GetModuleFileNameExW(hProcess, NULL, path_wide, MAX_PATH) == 0)
    {
        CloseHandle(hProcess);
        return FALSE;
    }
    
    CloseHandle(hProcess);
    
    // Convert wide string to multibyte
    int len = WideCharToMultiByte(CP_UTF8, 0, path_wide, -1, 
                                  exe_path, (int)path_len, NULL, NULL);
    if (len == 0)
    {
        exe_path[0] = '\0';
        return FALSE;
    }
    
    return TRUE;
}

BOOL win_kill_process(DWORD pid)
{
    HANDLE hProcess;
    BOOL result;
    
    hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL)
        return FALSE;
    
    result = TerminateProcess(hProcess, 9); // Exit code 9 to match Unix kill -9
    CloseHandle(hProcess);
    
    return result;
}

BOOL win_check_port_listening(USHORT port, DWORD *pid)
{
    PMIB_TCPTABLE_OWNER_PID tcpTable;
    DWORD tableSize = 0;
    DWORD result;
    DWORD i;
    
    // Get the size needed for the TCP table
    result = GetExtendedTcpTable(NULL, &tableSize, FALSE, AF_INET, 
                               TCP_TABLE_OWNER_PID_LISTENER, 0);
    
    if (result != ERROR_INSUFFICIENT_BUFFER)
        return FALSE;
    
    tcpTable = (PMIB_TCPTABLE_OWNER_PID)malloc(tableSize);
    if (tcpTable == NULL)
        return FALSE;
    
    result = GetExtendedTcpTable(tcpTable, &tableSize, FALSE, AF_INET,
                               TCP_TABLE_OWNER_PID_LISTENER, 0);
    
    if (result != NO_ERROR)
    {
        free(tcpTable);
        return FALSE;
    }
    
    // Search for the port
    for (i = 0; i < tcpTable->dwNumEntries; i++)
    {
        if (ntohs((USHORT)tcpTable->table[i].dwLocalPort) == port)
        {
            if (pid)
                *pid = tcpTable->table[i].dwOwningPid;
            free(tcpTable);
            return TRUE;
        }
    }
    
    free(tcpTable);
    return FALSE;
}

BOOL win_kill_process_by_port(USHORT port)
{
    DWORD pid;
    
    if (!win_check_port_listening(port, &pid))
        return FALSE;
    
    if (pid == 0 || pid == GetCurrentProcessId())
        return FALSE;
    
    return win_kill_process(pid);
}

BOOL win_get_current_exe_path(char *path, size_t path_len)
{
    WCHAR path_wide[MAX_PATH];
    
    if (GetModuleFileNameW(NULL, path_wide, MAX_PATH) == 0)
        return FALSE;
    
    // Convert wide string to multibyte
    int len = WideCharToMultiByte(CP_UTF8, 0, path_wide, -1, 
                                  path, (int)path_len, NULL, NULL);
    if (len == 0)
    {
        path[0] = '\0';
        return FALSE;
    }
    
    return TRUE;
}

#endif // _WIN32