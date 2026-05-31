// rawr_crash_handler.cpp
// Global exception handler to catch and log xtree/xmemory crashes
// Install in WinMain before any other initialization

#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#pragma comment(lib, "dbghelp.lib")

static const char* g_crashLogPath = "rawrxd_crash.log";

static void WriteCrashLog(const char* msg)
{
    FILE* f = nullptr;
    fopen_s(&f, g_crashLogPath, "a");
    if (f) {
        fprintf(f, "[%llu] %s\n", GetTickCount64(), msg);
        fclose(f);
    }
}

static void WriteMiniDump(EXCEPTION_POINTERS* pExceptionInfo)
{
    HANDLE hFile = CreateFileA("rawrxd_crash.dmp", GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    MINIDUMP_EXCEPTION_INFORMATION mdei = {};
    mdei.ThreadId = GetCurrentThreadId();
    mdei.ExceptionPointers = pExceptionInfo;
    mdei.ClientPointers = TRUE;

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                      MiniDumpNormal, &mdei, nullptr, nullptr);
    CloseHandle(hFile);
}

static LONG WINAPI RawrGlobalCrashHandler(EXCEPTION_POINTERS* pExceptionInfo)
{
    char msg[512];
    DWORD code = pExceptionInfo ? pExceptionInfo->ExceptionRecord->ExceptionCode : 0;
    snprintf(msg, sizeof(msg),
             "--- CRITICAL EXCEPTION ---\n"
             "Code: 0x%08X\n"
             "Address: 0x%p\n"
             "Info[0]: 0x%llX  Info[1]: 0x%llX\n",
             code,
             pExceptionInfo ? pExceptionInfo->ExceptionRecord->ExceptionAddress : nullptr,
             pExceptionInfo ? pExceptionInfo->ExceptionRecord->ExceptionInformation[0] : 0,
             pExceptionInfo ? pExceptionInfo->ExceptionRecord->ExceptionInformation[1] : 0);

    WriteCrashLog(msg);
    WriteMiniDump(pExceptionInfo);

    // Show a message box with the crash info
    MessageBoxA(nullptr, msg, "RawrXD Engine Error", MB_ICONERROR | MB_OK);

    return EXCEPTION_EXECUTE_HANDLER;
}

extern "C" void RawrInstallCrashHandler()
{
    SetUnhandledExceptionFilter(RawrGlobalCrashHandler);
    WriteCrashLog("Crash handler installed");
}
