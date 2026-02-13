// nvme_oracle_host.cpp - Bulletproof Service Host
#include <windows.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "advapi32.lib")

// Define the signature and name for the MMF
#define MMF_NAME "Global\\SOVEREIGN_NVME_TEMPS"
#define SIGNATURE 0x534F5645 // 'SOVE'

typedef int (__cdecl *QueryNVMeTempProto)(int driveId);

SERVICE_STATUS g_Status = {0};
SERVICE_STATUS_HANDLE g_hStatus = NULL;
HANDLE g_StopEvent = NULL;
HMODULE g_hQueryDll = NULL;
QueryNVMeTempProto g_QueryProc = NULL;
char g_DllPath[MAX_PATH] = {0};

void ReportStatus(DWORD state, DWORD exitCode = 0, DWORD waitHint = 0) {
    static DWORD checkPoint = 1;
    g_Status.dwCurrentState = state;
    g_Status.dwWin32ExitCode = exitCode;
    g_Status.dwWaitHint = waitHint;
    g_Status.dwCheckPoint = (state == SERVICE_RUNNING || state == SERVICE_STOPPED) ? 0 : checkPoint++;
    SetServiceStatus(g_hStatus, &g_Status);
}

void WINAPI Handler(DWORD ctrl) {
    if (ctrl == SERVICE_CONTROL_STOP) {
        ReportStatus(SERVICE_STOP_PENDING);
        SetEvent(g_StopEvent);
    }
}

void WINAPI ServiceMain(DWORD argc, LPSTR* argv) {
    g_hStatus = RegisterServiceCtrlHandlerA("SovereignNVMeOracle", Handler);
    if (!g_hStatus) return;

    g_Status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_Status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    ReportStatus(SERVICE_START_PENDING, 0, 3000);

    g_StopEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!g_StopEvent) {
        ReportStatus(SERVICE_STOPPED, GetLastError());
        return;
    }

    // Get the directory where the service EXE is located
    DWORD len = GetModuleFileNameA(NULL, g_DllPath, MAX_PATH);
    if (len > 0) {
        // Find last backslash and replace filename with DLL name
        for (int i = len - 1; i >= 0; i--) {
            if (g_DllPath[i] == '\\') {
                g_DllPath[i + 1] = '\0';
                break;
            }
        }
        strcat_s(g_DllPath, MAX_PATH, "nvme_query.dll");
    }

    // Load the MASM DLL from the same directory as the service
    g_hQueryDll = LoadLibraryA(g_DllPath);
    if (g_hQueryDll) {
        g_QueryProc = (QueryNVMeTempProto)GetProcAddress(g_hQueryDll, "QueryNVMeTemp");
    }

    // Initialize MMF with explicit security descriptor for cross-session access
    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE); // NULL DACL = everyone access
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;
    
    HANDLE hMMF = CreateFileMappingA(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, 256, MMF_NAME);
    void* pBuf = NULL;
    if (hMMF) {
        pBuf = MapViewOfFile(hMMF, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (pBuf) {
            // Write Header
            unsigned int* pData = (unsigned int*)pBuf;
            pData[0] = SIGNATURE;
            pData[1] = 1; // version
            pData[2] = 5; // drive count
            
            // Clear Temps
            for(int i=0; i<32; i++) {
                ((int*)pBuf)[4 + i] = -1;
            }
        }
    }

    ReportStatus(SERVICE_RUNNING);

    while (WaitForSingleObject(g_StopEvent, 1000) == WAIT_TIMEOUT) {
        if (pBuf && g_QueryProc) {
            int* pTemps = (int*)pBuf + 4;
            for (int i = 0; i < 5; i++) {
                pTemps[i] = g_QueryProc(i);
            }
            
            // Timestamp at offset 144
            unsigned long long* pTime = (unsigned long long*)((char*)pBuf + 144);
            *pTime = GetTickCount64();
        }
    }

    if (pBuf) UnmapViewOfFile(pBuf);
    if (hMMF) CloseHandle(hMMF);
    if (g_hQueryDll) FreeLibrary(g_hQueryDll);
    CloseHandle(g_StopEvent);
    
    ReportStatus(SERVICE_STOPPED);
}

int main() {
    SERVICE_TABLE_ENTRYA table[] = {
        {(LPSTR)"SovereignNVMeOracle", ServiceMain},
        {NULL, NULL}
    };

    if (!StartServiceCtrlDispatcherA(table)) {
        // Console mode for testing
        printf("Sovereign NVMe Oracle - Standalone Test Mode\n");
        g_hQueryDll = LoadLibraryA("nvme_query.dll");
        if (!g_hQueryDll) {
            printf("Failed to load nvme_query.dll (Error: %lu)\n", GetLastError());
            return 1;
        }
        g_QueryProc = (QueryNVMeTempProto)GetProcAddress(g_hQueryDll, "QueryNVMeTemp");
        if (!g_QueryProc) {
            printf("Failed to find QueryNVMeTemp in DLL\n");
            return 1;
        }

        printf("Querying Drive 0... Temp: %d C\n", g_QueryProc(0));
        FreeLibrary(g_hQueryDll);
    }
    return 0;
}
