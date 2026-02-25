#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

// RawrXD MASM CLI - both ways (MASM + NASM), x86 + x64
// Assembles both MASM and NASM sources; auto-detects format and bitness.
// Integrates with Win32 IDE's split-pane terminal.

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
    wprintf(L"RawrXD MASM CLI v1.1 (both ways: MASM + NASM, x86 + x64)\n");
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
    wprintf(L"  asm FILE          - Assemble FILE.asm (MASM or NASM, x86 or x64)\n");
    wprintf(L"  check FILE        - Syntax check (auto-detects format & bitness)\n");
    wprintf(L"  info FILE         - Show file information\n");
    wprintf(L"  path              - Display assembler search paths\n");
    wprintf(L"  exit              - Exit CLI\n");
    wprintf(L"\nBoth ways: MASM (ml/ml64) and NASM; x86 and x64.\n");
    wprintf(L"  NASM: section .text, bits 32/64, default rel -> -f win32 or -f win64\n");
    wprintf(L"  MASM: .model/.code, option casemap, invoke -> ml.exe (x86) or ml64 (x64)\n");
    wprintf(L"\nExamples:\n");
    wprintf(L"  asm Titan_Kernel.asm        (MASM x64 -> ml64)\n");
    wprintf(L"  asm camellia-assembly.asm   (NASM x86 -> nasm -f win32)\n\n");
}

void print_version() {
    wprintf(L"RawrXD MASM CLI v1.1\n");
    wprintf(L"Both ways: MASM (ml + ml64) and NASM. Targets: x86 and x64.\n");
    wprintf(L"  MASM x64: ml64.exe  |  MASM x86: ml.exe  |  NASM: nasm.exe (-f win32/win64)\n\n");
}

// Detect assembly format by peeking at file content. Returns 1=MASM, 2=NASM, 0=unknown.
int detect_asm_format(const wchar_t* filename) {
    HANDLE h = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    enum { PEEK_SIZE = 4096 };
    char buf[PEEK_SIZE + 1];
    DWORD read = 0;
    ReadFile(h, buf, PEEK_SIZE, &read, NULL);
    CloseHandle(h);
    if (read == 0) return 0;
    buf[read] = '\0';
    // Normalize to lowercase for comparison
    for (DWORD i = 0; i < read; i++) {
        if (buf[i] >= 'A' && buf[i] <= 'Z') buf[i] += 32;
    }
    int nasm_score = 0, masm_score = 0;
    // NASM signatures
    if (strstr(buf, "section .text")) nasm_score++;
    if (strstr(buf, "section .data")) nasm_score++;
    if (strstr(buf, "section .bss")) nasm_score++;
    if (strstr(buf, "bits 64") || strstr(buf, "bits 32")) nasm_score++;
    if (strstr(buf, "default rel")) nasm_score++;
    if (strstr(buf, "%include")) nasm_score++;
    if (strstr(buf, "[rel ")) nasm_score++;
    // MASM signatures
    if (strstr(buf, ".model")) masm_score++;
    if (strstr(buf, "option casemap")) masm_score++;
    if (strstr(buf, "invoke ")) masm_score++;
    if (strstr(buf, "includelib")) masm_score++;
    if (strstr(buf, ".686") || strstr(buf, ".xmm")) masm_score++;
    if (strstr(buf, "option frame")) masm_score++;
    if (strstr(buf, " proc ") || strstr(buf, " endp ") || strstr(buf, "\tproc ") || strstr(buf, "\tendp ")) masm_score++;
    // .code / .data: MASM uses bare .code/.data; NASM uses "section .*"
    if (strstr(buf, ".code") && !strstr(buf, "section .code")) masm_score++;
    if (strstr(buf, ".data") && !strstr(buf, "section .data")) masm_score++;
    if (nasm_score > masm_score) return 2;
    if (masm_score > nasm_score) return 1;
    return 0;
}

// Detect target bitness by peeking at file. Returns 32 or 64. Default 64 if unclear.
int detect_asm_bits(const wchar_t* filename) {
    HANDLE h = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 64;
    enum { PEEK_SIZE = 4096 };
    char buf[PEEK_SIZE + 1];
    DWORD read = 0;
    ReadFile(h, buf, PEEK_SIZE, &read, NULL);
    CloseHandle(h);
    if (read == 0) return 64;
    buf[read] = '\0';
    for (DWORD i = 0; i < read; i++) {
        if (buf[i] >= 'A' && buf[i] <= 'Z') buf[i] += 32;
    }
    /* Explicit 32-bit: NASM "bits 32", MASM .model flat, .386, stdcall, etc. */
    if (strstr(buf, "bits 32")) return 32;
    if (strstr(buf, ".model flat") || strstr(buf, ".model small") || strstr(buf, ".model medium")) return 32;
    if (strstr(buf, ".386") || strstr(buf, ".486")) return 32;
    if (strstr(buf, "stdcall") && !strstr(buf, "win64")) return 32;
    /* Explicit 64-bit */
    if (strstr(buf, "bits 64")) return 64;
    if (strstr(buf, "win64") || strstr(buf, "option win64")) return 64;
    /* Default: 64 for MASM .code/.data without .model (common for ml64); else 64 */
    return 64;
}

// Find assembler: type 1 = ML64 (MASM x64), type 2 = NASM, type 3 = ML (MASM x86). Fills path, returns 1 if found.
int find_assembler_ex(int type, wchar_t* path, size_t pathlen) {
    if (type == 1) {
        if (SearchPathW(NULL, L"ml64.exe", NULL, (DWORD)pathlen, path, NULL) > 0) return 1;
    } else if (type == 2) {
        if (SearchPathW(NULL, L"nasm.exe", NULL, (DWORD)pathlen, path, NULL) > 0) return 1;
    } else if (type == 3) {
        if (SearchPathW(NULL, L"ml.exe", NULL, (DWORD)pathlen, path, NULL) > 0) return 1;
    }
    return 0;
}

int find_assembler(wchar_t* path, size_t pathlen) {
    if (find_assembler_ex(1, path, pathlen)) {
        wprintf(L"Found: ml64.exe at %s\n", path);
        return 1;
    }
    if (find_assembler_ex(2, path, pathlen)) {
        wprintf(L"Found: nasm.exe at %s\n", path);
        return 2;
    }
    if (find_assembler_ex(3, path, pathlen)) {
        wprintf(L"Found: ml.exe at %s\n", path);
        return 3;
    }
    return 0;
}

int cmd_build(wchar_t* filename) {
    wchar_t asmPath[MAX_PATH_STR];
    int asmType = 0;
    int format = detect_asm_format(filename);
    int bits = detect_asm_bits(filename);
    if (format == 2) {
        if (find_assembler_ex(2, asmPath, MAX_PATH_STR)) asmType = 2;
        else if (bits == 64 && find_assembler_ex(1, asmPath, MAX_PATH_STR)) { asmType = 1; wprintf(L"[WARN] NASM format detected but only ML64 found. Using ML64 (may fail).\n"); }
        else if (bits == 32 && find_assembler_ex(3, asmPath, MAX_PATH_STR)) { asmType = 3; wprintf(L"[WARN] NASM format detected but only ML found. Using ML (may fail).\n"); }
    } else if (format == 1) {
        if (bits == 32) {
            if (find_assembler_ex(3, asmPath, MAX_PATH_STR)) asmType = 3;
            else if (find_assembler_ex(1, asmPath, MAX_PATH_STR)) { asmType = 1; wprintf(L"[WARN] MASM x86 detected but only ML64 found. Using ML64 (may fail).\n"); }
        } else {
            if (find_assembler_ex(1, asmPath, MAX_PATH_STR)) asmType = 1;
            else if (find_assembler_ex(3, asmPath, MAX_PATH_STR)) { asmType = 3; wprintf(L"[WARN] MASM x64 detected but only ML found. Using ML (may fail).\n"); }
        }
        if (asmType == 0 && find_assembler_ex(2, asmPath, MAX_PATH_STR)) { asmType = 2; wprintf(L"[WARN] MASM format detected but only NASM found. Using NASM (may fail).\n"); }
    }
    if (asmType == 0) asmType = find_assembler(asmPath, MAX_PATH_STR);
    if (asmType == 0) {
        wprintf(L"[ERROR] No assembler found. Install ml.exe, ml64.exe, or nasm.exe.\n");
        return 1;
    }
    const wchar_t* asmName = (asmType == 1) ? L"ml64" : (asmType == 3) ? L"ml" : L"nasm";
    if (format != 0 || bits != 64) wprintf(L"Detected: %s, %s-bit -> using %s\n", format == 2 ? L"NASM" : format == 1 ? L"MASM" : L"auto", bits == 32 ? L"32" : L"64", asmName);
    wchar_t cmd[1024];
    wchar_t objfile[MAX_PATH_STR];
    wcscpy_s(objfile, MAX_PATH_STR, filename);
    size_t len = wcslen(objfile);
    if (len > 4 && wcscmp(&objfile[len-4], L".asm") == 0) {
        wcscpy_s(&objfile[len-4], 5, L".obj");
    }
    if (asmType == 1) {
        swprintf_s(cmd, 1024, L"\"%s\" /c /Zs /Fo\"%s\" \"%s\"",
                   asmPath, objfile, filename);
    } else if (asmType == 3) {
        swprintf_s(cmd, 1024, L"\"%s\" /c /Zs /Fo\"%s\" \"%s\"",
                   asmPath, objfile, filename);
    } else {
        const wchar_t* nfmt = (bits == 32) ? L"win32" : L"win64";
        swprintf_s(cmd, 1024, L"\"%s\" -f %s -o \"%s\" \"%s\"",
                   asmPath, nfmt, objfile, filename);
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
    wchar_t asmPath[MAX_PATH_STR];
    int asmType = 0;
    int format = detect_asm_format(filename);
    int bits = detect_asm_bits(filename);
    if (format == 2) {
        if (find_assembler_ex(2, asmPath, MAX_PATH_STR)) asmType = 2;
        else if (bits == 64 && find_assembler_ex(1, asmPath, MAX_PATH_STR)) asmType = 1;
        else if (bits == 32 && find_assembler_ex(3, asmPath, MAX_PATH_STR)) asmType = 3;
    } else if (format == 1) {
        if (bits == 32 && find_assembler_ex(3, asmPath, MAX_PATH_STR)) asmType = 3;
        else if (find_assembler_ex(1, asmPath, MAX_PATH_STR)) asmType = 1;
        else if (find_assembler_ex(2, asmPath, MAX_PATH_STR)) asmType = 2;
    }
    if (asmType == 0) asmType = find_assembler(asmPath, MAX_PATH_STR);
    if (asmType == 0) {
        wprintf(L"[ERROR] No assembler found.\n");
        return 1;
    }
    wchar_t cmd[1024];
    wchar_t tmpObj[MAX_PATH_STR] = { 0 };
    if (asmType == 1) {
        swprintf_s(cmd, 1024, L"\"%s\" /c /Zs \"%s\"", asmPath, filename);
    } else if (asmType == 3) {
        swprintf_s(cmd, 1024, L"\"%s\" /c /Zs \"%s\"", asmPath, filename);
    } else {
        const wchar_t* nfmt = (bits == 32) ? L"win32" : L"win64";
        wchar_t tmpDir[MAX_PATH_STR];
        if (GetTempPathW(MAX_PATH_STR, tmpDir)) {
            wcscpy_s(tmpObj, MAX_PATH_STR, tmpDir);
            wcscat_s(tmpObj, MAX_PATH_STR, L"rawrxd_nasm_check.obj");
            swprintf_s(cmd, 1024, L"\"%s\" -f %s -o \"%s\" \"%s\"", asmPath, nfmt, tmpObj, filename);
        } else {
            swprintf_s(cmd, 1024, L"\"%s\" -f %s -o \"%s\" \"%s\"", asmPath, nfmt, L"nasm_check.obj", filename);
        }
    }
    wprintf(L"Checking: %s (%s, %s-bit)\n", filename, format == 2 ? L"NASM" : format == 1 ? L"MASM" : L"auto", bits == 32 ? L"32" : L"64");
    
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
    if (tmpObj[0]) DeleteFileW(tmpObj);
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
            int any = 0;
            if (find_assembler_ex(1, asmPath, MAX_PATH_STR)) { wprintf(L"ml64.exe (MASM x64): %s\n", asmPath); any = 1; }
            if (find_assembler_ex(3, asmPath, MAX_PATH_STR)) { wprintf(L"ml.exe   (MASM x86): %s\n", asmPath); any = 1; }
            if (find_assembler_ex(2, asmPath, MAX_PATH_STR)) { wprintf(L"nasm.exe (NASM):     %s\n", asmPath); any = 1; }
            if (!any) {
                wprintf(L"No assembler found in PATH. Install ml.exe, ml64.exe (MSVC), or nasm.exe.\n");
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
