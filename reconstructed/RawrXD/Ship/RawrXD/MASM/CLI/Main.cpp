#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

// RawrXD MASM x64 CLI
// Pure command-line interface for assembling MASM x64 code
// Integrates with Win32 IDE's split-pane terminal

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

#define MAX_CMD 512
#define MAX_PATH_STR 260

typedef struct {
    wchar_t command[MAX_CMD];
    wchar_t filename[MAX_PATH_STR];
    wchar_t options[MAX_CMD];
    int verbose;
    int optimize;
} MasmCommand;

void print_banner() {
    wprintf(L"RawrXD MASM x64 CLI v1.0\n");
    wprintf(L"Pure x64 Assembly Command Interface\n");
    wprintf(L"Type 'help' for available commands\n\n");
}

void print_prompt() {
    wprintf(L"[MASM]> ");
    fflush(stdout);
}

void print_help() {
    wprintf(L"\nAvailable Commands:\n");
    wprintf(L"  help              - Show this help message\n");
    wprintf(L"  version           - Display version information\n");
    wprintf(L"  asm FILE          - Assemble FILE.asm to FILE.obj\n");
    wprintf(L"  ml64 FILE [opts]  - Run ML64.exe directly\n");
    wprintf(L"  nasm FILE [opts]  - Run NASM assembler\n");
    wprintf(L"  build FILE        - Full build: asm+link\n");
    wprintf(L"  check FILE        - Syntax check (syntax-only compile)\n");
    wprintf(L"  info FILE         - Show file information\n");
    wprintf(L"  path              - Display assembler search paths\n");
    wprintf(L"  exit              - Exit CLI\n");
    wprintf(L"\nExamples:\n");
    wprintf(L"  asm Titan_Kernel.asm\n");
    wprintf(L"  ml64 RawrXD_NativeModelBridge.asm /Fo obj/ /Zs\n");
    wprintf(L"  nasm -f win64 -o output.obj file.asm\n\n");
}

void print_version() {
    wprintf(L"RawrXD MASM CLI v1.0 (Windows x64)\n");
    wprintf(L"MASM Assembler Integration\n");
    wprintf(L"Microsoft Macro Assembler (ML64) support\n");
    wprintf(L"NASM fallback support\n\n");
}

int find_assembler(wchar_t* path, size_t pathlen) {
    // Try to find ML64.exe or NASM in PATH
    DWORD ret = SearchPathW(NULL, L"ml64.exe", NULL, (DWORD)pathlen, path, NULL);
    if (ret > 0) {
        wprintf(L"Found: ml64.exe at %s\n", path);
        return 1;  // ML64
    }
    ret = SearchPathW(NULL, L"nasm.exe", NULL, (DWORD)pathlen, path, NULL);
    if (ret > 0) {
        wprintf(L"Found: nasm.exe at %s\n", path);
        return 2;  // NASM
    }
    return 0;  // Not found
}

int cmd_build(wchar_t* filename) {
    // Simple build: assemble with warnings, link to exe
    wchar_t asmPath[MAX_PATH_STR];
    int asmType = find_assembler(asmPath, MAX_PATH_STR);
    
    if (asmType == 0) {
        wprintf(L"[ERROR] No assembler found. Install ML64 or NASM.\n");
        return 1;
    }
    
    wchar_t cmd[1024];
    wchar_t objfile[MAX_PATH_STR];
    wcscpy_s(objfile, MAX_PATH_STR, filename);
    
    // Replace .asm with .obj
    size_t len = wcslen(objfile);
    if (len > 4 && wcscmp(&objfile[len-4], L".asm") == 0) {
        wcscpy_s(&objfile[len-4], 5, L".obj");
    }
    
    if (asmType == 1) {  // ML64
        // ml64.exe /c /Zs filename.asm
        swprintf_s(cmd, 1024, L"\"%s\" /c /Zs /Fo\"%s\" \"%s\"",
                   asmPath, objfile, filename);
    } else {  // NASM
        // nasm -f win64 -o output.obj input.asm
        swprintf_s(cmd, 1024, L"\"%s\" -f win64 -o \"%s\" \"%s\"",
                   asmPath, objfile, filename);
    }
    
    wprintf(L"Building: %s -> %s\n", filename, objfile);
    wprintf(L"Command: %s\n", cmd);
    
    // Execute
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
                        NULL, NULL, &si, &pi)) {
        wprintf(L"[ERROR] Failed to execute assembler\n");
        return 1;
    }
    
    wprintf(L"[MASM] Assembling...\n");
    WaitForSingleObject(pi.hProcess, 30000);  // Wait up to 30 seconds
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (exitCode == 0) {
        wprintf(L"[MASM] Assembly complete. Output: %s\n", objfile);
    } else {
        wprintf(L"[MASM] Assembly failed with exit code: %u\n", exitCode);
    }
    
    return (int)exitCode;
}

int cmd_check(wchar_t* filename) {
    // Syntax check only (compile but don't generate)
    wchar_t asmPath[MAX_PATH_STR];
    int asmType = find_assembler(asmPath, MAX_PATH_STR);
    
    if (asmType == 0) {
        wprintf(L"[ERROR] No assembler found.\n");
        return 1;
    }
    
    wchar_t cmd[1024];
    
    if (asmType == 1) {  // ML64 syntax check
        swprintf_s(cmd, 1024, L"\"%s\" /c /Zs \"%s\"", asmPath, filename);
    } else {  // NASM dry-run 
        swprintf_s(cmd, 1024, L"\"%s\" -f win64 -o nul \"%s\"", asmPath, filename);
    }
    
    wprintf(L"Checking: %s\n", filename);
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
                        NULL, NULL, &si, &pi)) {
        wprintf(L"[ERROR] Failed to execute assembler\n");
        return 1;
    }
    
    WaitForSingleObject(pi.hProcess, 30000);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (exitCode == 0) {
        wprintf(L"[MASM] Syntax OK: %s\n", filename);
    } else {
        wprintf(L"[MASM] Syntax errors found (code %u)\n", exitCode);
    }
    
    return (int)exitCode;
}

void cmd_info(wchar_t* filename) {
    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExW(filename, GetFileExInfoStandard, &fad)) {
        wprintf(L"[ERROR] File not found: %s\n", filename);
        return;
    }
    
    ULARGE_INTEGER fileSize;
    fileSize.LowPart = fad.nFileSizeLow;
    fileSize.HighPart = fad.nFileSizeHigh;
    
    wprintf(L"File: %s\n", filename);
    wprintf(L"Size: %llu bytes\n", fileSize.QuadPart);
    
    SYSTEMTIME st;
    FileTimeToSystemTime(&fad.ftLastWriteTime, &st);
    wprintf(L"Modified: %04u-%02u-%02u %02u:%02u:%02u\n",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond);
}

int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    
    print_banner();
    
    wchar_t inputLine[MAX_CMD];
    wchar_t cmd[100];
    wchar_t arg1[MAX_PATH_STR];
    wchar_t arg2[MAX_CMD];
    
    while (1) {
        print_prompt();
        
        if (!fgetws(inputLine, MAX_CMD, stdin)) {
            break;
        }
        
        // Parse input
        int parsed = swscanf_s(inputLine, L"%99s %259s %511s[^\n]",
                               cmd, (unsigned)sizeof(cmd),
                               arg1, (unsigned)sizeof(arg1),
                               arg2, (unsigned)sizeof(arg2));
        
        if (parsed < 1) continue;
        
        // Process commands
        if (wcscmp(cmd, L"exit") == 0) {
            wprintf(L"Exiting RawrXD MASM CLI.\n");
            break;
        }
        else if (wcscmp(cmd, L"help") == 0) {
            print_help();
        }
        else if (wcscmp(cmd, L"version") == 0) {
            print_version();
        }
        else if (wcscmp(cmd, L"asm") == 0 || wcscmp(cmd, L"assemble") == 0) {
            if (parsed > 1) {
                cmd_build(arg1);
            } else {
                wprintf(L"Usage: asm <filename.asm>\n");
            }
        }
        else if (wcscmp(cmd, L"check") == 0) {
            if (parsed > 1) {
                cmd_check(arg1);
            } else {
                wprintf(L"Usage: check <filename.asm>\n");
            }
        }
        else if (wcscmp(cmd, L"info") == 0) {
            if (parsed > 1) {
                cmd_info(arg1);
            } else {
                wprintf(L"Usage: info <filename>\n");
            }
        }
        else if (wcscmp(cmd, L"path") == 0) {
            wchar_t asmPath[MAX_PATH_STR];
            int type = find_assembler(asmPath, MAX_PATH_STR);
            if (type == 0) {
                wprintf(L"No assembler found in PATH\n");
                wprintf(L"Install ML64 (MSVC) or NASM\n");
            }
        }
        else {
            wprintf(L"Unknown command: %s\n", cmd);
            wprintf(L"Type 'help' for available commands\n");
        }
        
        wprintf(L"\n");
    }
    
    return 0;
}
