// nvme_oracle_host_standalone.cpp - Self-contained Service (No DLL dependency)
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>

#pragma comment(lib, "advapi32.lib")

#define MMF_NAME "Global\\SOVEREIGN_NVME_TEMPS"
#define SIGNATURE 0x534F5645

SERVICE_STATUS g_Status = {0};
SERVICE_STATUS_HANDLE g_hStatus = NULL;
HANDLE g_StopEvent = NULL;

void ReportStatus(DWORD state, DWORD exitCode = 0, DWORD waitHint = 0) {
    static DWORD checkPoint = 1;
    g_Status.dwCurrentState = state;
    g_Status.dwWin32ExitCode = exitCode;
    g_Status.dwWaitHint = waitHint;
    g_Status.dwCheckPoint = (state == SERVICE_RUNNING || state == SERVICE_STOPPED) ? 0 : checkPoint++;
    SetServiceStatus(g_hStatus, &g_Status);
}

// Inline temperature query - no DLL needed
int QueryNVMeTemp(int driveId) {
    char path[32];
    sprintf_s(path, "\\\\.\\PhysicalDrive%d", driveId);
    
    HANDLE hDrive = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hDrive == INVALID_HANDLE_VALUE) return -1;
    
    STORAGE_PROPERTY_QUERY query = {0};
    query.PropertyId = StorageDeviceTemperatureProperty;
    query.QueryType = PropertyStandardQuery;
    
    BYTE buffer[512] = {0};
    DWORD bytesReturned = 0;
    
    BOOL result = DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query), buffer, sizeof(buffer), &bytesReturned, NULL);
    
    CloseHandle(hDrive);
    
    if (!result || bytesReturned < 20) return -1;
    
    // Use the SDK structure directly
    STORAGE_TEMPERATURE_DATA_DESCRIPTOR* pDesc = (STORAGE_TEMPERATURE_DATA_DESCRIPTOR*)buffer;
    
    if (pDesc->InfoCount == 0) return -1;
    
    // Get temperature from first sensor
    SHORT temp = pDesc->TemperatureInfo[0].Temperature;
    
    if (temp < -50 || temp > 150) return -1;
    
    return temp;
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

    // Initialize MMF with explicit security descriptor
    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;
    
    HANDLE hMMF = CreateFileMappingA(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, 256, MMF_NAME);
    void* pBuf = NULL;
    if (hMMF) {
        pBuf = MapViewOfFile(hMMF, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (pBuf) {
            unsigned int* pData = (unsigned int*)pBuf;
            pData[0] = SIGNATURE;
            pData[1] = 1;
            pData[2] = 5;
            for(int i = 0; i < 32; i++) ((int*)pBuf)[4 + i] = -1;
        }
    }

    ReportStatus(SERVICE_RUNNING);

    while (WaitForSingleObject(g_StopEvent, 1000) == WAIT_TIMEOUT) {
        if (pBuf) {
            int* pTemps = (int*)pBuf + 4;
            for (int i = 0; i < 5; i++) {
                pTemps[i] = QueryNVMeTemp(i);
            }
            *(unsigned long long*)((char*)pBuf + 144) = GetTickCount64();
        }
    }

    if (pBuf) UnmapViewOfFile(pBuf);
    if (hMMF) CloseHandle(hMMF);
    CloseHandle(g_StopEvent);
    
    ReportStatus(SERVICE_STOPPED);
}

int main() {
    SERVICE_TABLE_ENTRYA table[] = {
        {(LPSTR)"SovereignNVMeOracle", ServiceMain},
        {NULL, NULL}
    };

    if (!StartServiceCtrlDispatcherA(table)) {
        printf("Sovereign NVMe Oracle - Standalone Test Mode\n");
        for (int i = 0; i < 6; i++) {
            printf("Drive %d: %d C\n", i, QueryNVMeTemp(i));
        }
    }
    return 0;
}
