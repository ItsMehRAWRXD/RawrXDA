// SovereignNVMeOracle.cpp - C++ SCM host, calls MASM DLL
#include <windows.h>
#include <stdio.h>
#pragma comment(lib, "advapi32.lib")

typedef int (__stdcall *PFN_QueryTemp)(int driveId);

SERVICE_STATUS g_Status = {0};
SERVICE_STATUS_HANDLE g_hStatus = NULL;
BOOL g_Running = TRUE;
HANDLE g_hMMF = NULL;
LPVOID g_pView = NULL;

#define MMF_NAME "Local\\SOVEREIGN_NVME_TEMPS"
#define MMF_SIZE 256

void WINAPI Handler(DWORD ctrl) {
    if (ctrl == SERVICE_CONTROL_STOP) {
        g_Status.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_hStatus, &g_Status);
        g_Running = FALSE;
    }
}

BOOL InitMMF() {
    g_hMMF = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, MMF_SIZE, MMF_NAME);
    if (!g_hMMF) return FALSE;
    g_pView = MapViewOfFile(g_hMMF, FILE_MAP_ALL_ACCESS, 0, 0, MMF_SIZE);
    if (!g_pView) return FALSE;
    
    // Init header
    DWORD* p = (DWORD*)g_pView;
    p[0] = 0x534F5645;  // "SOVE"
    p[1] = 1;           // Version
    p[2] = 5;           // Count
    p[3] = 0;           // Reserved
    
    // Clear temps/wear to -1
    for (int i = 0; i < 16; i++) {
        ((int*)g_pView)[4 + i] = -1;      // Temps at 0x10
        ((int*)g_pView)[20 + i] = -1;     // Wear at 0x50
    }
    return TRUE;
}

void WINAPI ServiceMain(DWORD, LPTSTR*) {
    // Register with SCM immediately
    g_hStatus = RegisterServiceCtrlHandlerA("SovereignNVMeOracle", Handler);
    if (!g_hStatus) return;
    
    g_Status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_Status.dwCurrentState = SERVICE_START_PENDING;
    g_Status.dwWaitHint = 5000;
    SetServiceStatus(g_hStatus, &g_Status);
    
    // Load MASM DLL
    HMODULE hMod = LoadLibraryA("nvme_query.dll");
    if (!hMod) {
        g_Status.dwCurrentState = SERVICE_STOPPED;
        g_Status.dwWin32ExitCode = ERROR_DLL_NOT_FOUND;
        SetServiceStatus(g_hStatus, &g_Status);
        return;
    }
    
    PFN_QueryTemp QueryTemp = (PFN_QueryTemp)GetProcAddress(hMod, "QueryNVMeTemp");
    if (!QueryTemp) {
        g_Status.dwCurrentState = SERVICE_STOPPED;
        g_Status.dwWin32ExitCode = ERROR_PROC_NOT_FOUND;
        SetServiceStatus(g_hStatus, &g_Status);
        return;
    }
    
    // Init MMF
    if (!InitMMF()) {
        g_Status.dwCurrentState = SERVICE_STOPPED;
        g_Status.dwWin32ExitCode = GetLastError();
        SetServiceStatus(g_hStatus, &g_Status);
        return;
    }
    
    // Now running
    g_Status.dwCurrentState = SERVICE_RUNNING;
    g_Status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_Status.dwWaitHint = 0;
    SetServiceStatus(g_hStatus, &g_Status);
    
    // Main loop
    while (g_Running) {
        for (int i = 0; i < 5; i++) {
            int temp = QueryTemp(i);
            ((int*)g_pView)[4 + i] = temp;  // Store temp
        }
        *(ULONGLONG*)((BYTE*)g_pView + 144) = GetTickCount64();
        Sleep(500);
    }
    
    // Cleanup
    UnmapViewOfFile(g_pView);
    CloseHandle(g_hMMF);
    FreeLibrary(hMod);
    
    g_Status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_hStatus, &g_Status);
}

int main() {
    SERVICE_TABLE_ENTRYA table[] = {
        {"SovereignNVMeOracle", ServiceMain},
        {NULL, NULL}
    };
    StartServiceCtrlDispatcherA(table);
    return 0;
}
    
    g_Status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_hStatus, &g_Status);
}

int main() {
    SERVICE_TABLE_ENTRYA table[] = {
        {"SovereignNVMeOracle", ServiceMain},
        {NULL, NULL}
    };
    StartServiceCtrlDispatcherA(table);
    return 0;
}