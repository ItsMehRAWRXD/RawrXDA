// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ nvme_thermal_sidecar_clean.cpp - Clean C++ NVMe Thermal Sidecar             ║
// ║                                                                              ║
// ║ Purpose: Standalone sidecar providing real NVMe temperature telemetry       ║
// ║          via IOCTL_STORAGE_QUERY_PROPERTY, published to Local\ MMF          ║
// ║                                                                              ║
// ║ Features:                                                                    ║
// ║   - Direct IOCTL_STORAGE_QUERY_PROPERTY (no WMI, no PowerShell)            ║
// ║   - Real Kelvin→Celsius conversion                                         ║
// ║   - Local\ namespace MMF (works without elevation in same session)         ║
// ║   - Can run as standalone process or Windows service                        ║
// ║   - Debug output via OutputDebugString and console                         ║
// ║                                                                              ║
// ║ Build (standalone):                                                          ║
// ║   cl.exe /O2 /EHsc nvme_thermal_sidecar_clean.cpp /Fe:nvme_sidecar.exe     ║
// ║          kernel32.lib advapi32.lib                                          ║
// ║                                                                              ║
// ║ Run as standalone:                                                           ║
// ║   nvme_sidecar.exe                                                          ║
// ║                                                                              ║
// ║ Install as service:                                                          ║
// ║   sc create SovereignNVMeOracle binPath= "C:\path\nvme_sidecar.exe"        ║
// ║      start= auto obj= "NT AUTHORITY\SYSTEM"                                 ║
// ║   sc start SovereignNVMeOracle                                              ║
// ║                                                                              ║
// ║ Author: RawrXD IDE Team                                                      ║
// ║ Version: 2.0.0                                                               ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <winioctl.h>
#include <ntddstor.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

namespace Config {
    // MMF name (Local namespace - no SeCreateGlobalPrivilege required)
    constexpr const char* kMapName = "Local\\SOVEREIGN_NVME_TEMPS";
    
    // Magic signature "SOVE" in little-endian
    constexpr uint32_t kSignature = 0x45564F53;
    constexpr uint32_t kVersion = 1;
    
    // Drive configuration
    constexpr int kDriveIds[] = {0, 1, 2, 4, 5};
    constexpr int kDriveCount = sizeof(kDriveIds) / sizeof(kDriveIds[0]);
    constexpr int kMaxDrives = 16;
    
    // Polling interval
    constexpr DWORD kPollIntervalMs = 500;
    
    // Invalid temperature sentinel
    constexpr int32_t kInvalidTemp = -1;
    
    // Service name
    constexpr const char* kServiceName = "SovereignNVMeOracle";
}

// ═══════════════════════════════════════════════════════════════════════════════
// MMF Layout Structure (MUST MATCH MASM AND READER)
// ═══════════════════════════════════════════════════════════════════════════════
// Offset  Size   Field
// 0x00    u32    Signature  "SOVE" = 0x45564F53
// 0x04    u32    Version    (1)
// 0x08    u32    DriveCount
// 0x0C    u32    Reserved
// 0x10    i32[16] Temps     (64 bytes)
// 0x50    i32[16] Wear      (64 bytes)
// 0x90    u64    Timestamp  (ms since boot)
// ═══════════════════════════════════════════════════════════════════════════════

#pragma pack(push, 1)
struct SovereignThermalData {
    uint32_t signature;      // 0x00: "SOVE" = 0x45564F53
    uint32_t version;        // 0x04: Schema version (1)
    uint32_t driveCount;     // 0x08: Number of active drives
    uint32_t reserved;       // 0x0C: Reserved (0)
    int32_t  temps[16];      // 0x10: Temperatures in Celsius (-1 = invalid)
    int32_t  wear[16];       // 0x50: Wear level percentage (-1 = invalid)
    uint64_t timestampMs;    // 0x90: GetTickCount64() value
};
#pragma pack(pop)

static_assert(offsetof(SovereignThermalData, signature) == 0x00, "Layout error");
static_assert(offsetof(SovereignThermalData, version) == 0x04, "Layout error");
static_assert(offsetof(SovereignThermalData, driveCount) == 0x08, "Layout error");
static_assert(offsetof(SovereignThermalData, reserved) == 0x0C, "Layout error");
static_assert(offsetof(SovereignThermalData, temps) == 0x10, "Layout error");
static_assert(offsetof(SovereignThermalData, wear) == 0x50, "Layout error");
static_assert(offsetof(SovereignThermalData, timestampMs) == 0x90, "Layout error");
static_assert(sizeof(SovereignThermalData) == 152, "Layout size mismatch");

// ═══════════════════════════════════════════════════════════════════════════════
// Global State
// ═══════════════════════════════════════════════════════════════════════════════

namespace State {
    std::atomic<bool> g_running{true};
    std::atomic<bool> g_isService{false};
    
    HANDLE g_hMapFile = nullptr;
    SovereignThermalData* g_pData = nullptr;
    SERVICE_STATUS_HANDLE g_hServiceStatus = nullptr;
    SERVICE_STATUS g_serviceStatus = {};
}

// ═══════════════════════════════════════════════════════════════════════════════
// Debug Logging
// ═══════════════════════════════════════════════════════════════════════════════

void DebugLog(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Output to debugger
    OutputDebugStringA(buffer);
    
    // Also output to console if not running as service
    if (!State::g_isService.load()) {
        printf("%s", buffer);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// NVMe Temperature Query via IOCTL
// ═══════════════════════════════════════════════════════════════════════════════

int32_t GetNvmeTemperatureIoctl(int driveId) {
    char path[64] = {};
    snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%d", driveId);
    
    // Open the physical drive
    // Note: Requires GENERIC_READ at minimum, but GENERIC_READ|GENERIC_WRITE
    // is more reliable for IOCTL_STORAGE_QUERY_PROPERTY
    HANDLE hDrive = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    
    if (hDrive == INVALID_HANDLE_VALUE) {
        // Try with just GENERIC_READ as fallback
        hDrive = CreateFileA(
            path,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );
        
        if (hDrive == INVALID_HANDLE_VALUE) {
            DebugLog("[Sidecar] Drive %d: CreateFile failed (error %lu)\n", 
                     driveId, GetLastError());
            return Config::kInvalidTemp;
        }
    }
    
    // Prepare the query
    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceTemperatureProperty;
    query.QueryType = PropertyStandardQuery;
    
    // Buffer for the response
    BYTE buffer[512] = {};
    DWORD bytesReturned = 0;
    
    BOOL ok = DeviceIoControl(
        hDrive,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &query,
        sizeof(query),
        buffer,
        sizeof(buffer),
        &bytesReturned,
        nullptr
    );
    
    CloseHandle(hDrive);
    
    if (!ok) {
        DebugLog("[Sidecar] Drive %d: IOCTL failed (error %lu)\n", 
                 driveId, GetLastError());
        return Config::kInvalidTemp;
    }
    
    if (bytesReturned < sizeof(STORAGE_TEMPERATURE_DATA_DESCRIPTOR)) {
        DebugLog("[Sidecar] Drive %d: Insufficient bytes returned (%lu)\n", 
                 driveId, bytesReturned);
        return Config::kInvalidTemp;
    }
    
    auto* desc = reinterpret_cast<STORAGE_TEMPERATURE_DATA_DESCRIPTOR*>(buffer);
    
    if (desc->InfoCount == 0) {
        DebugLog("[Sidecar] Drive %d: No temperature info available\n", driveId);
        return Config::kInvalidTemp;
    }
    
    // Get temperature from first info entry
    // Temperature is in Kelvin * 10 (0.1 Kelvin units)
    // Some drives report in Celsius directly - we need to detect which
    const STORAGE_TEMPERATURE_INFO& info = desc->TemperatureInfo[0];
    SHORT rawTemp = info.Temperature;
    
    if (rawTemp == 0 || rawTemp == -1) {
        DebugLog("[Sidecar] Drive %d: Invalid raw temperature (%d)\n", 
                 driveId, rawTemp);
        return Config::kInvalidTemp;
    }
    
    int32_t tempCelsius;
    
    // Check if this looks like a Kelvin value (typically > 2500 for room temp)
    // or a direct Celsius value (typically < 100)
    if (rawTemp > 2500) {
        // Convert from 0.1 Kelvin to Celsius: (temp - 2731) / 10
        // 2731 = 273.15 * 10 (0°C in 0.1K units)
        tempCelsius = (rawTemp - 2731) / 10;
    } else if (rawTemp > 0 && rawTemp < 200) {
        // Already in Celsius (some drives report this way)
        tempCelsius = rawTemp;
    } else {
        DebugLog("[Sidecar] Drive %d: Unexpected temperature value (%d)\n", 
                 driveId, rawTemp);
        return Config::kInvalidTemp;
    }
    
    // Sanity check
    if (tempCelsius < -40 || tempCelsius > 150) {
        DebugLog("[Sidecar] Drive %d: Temperature out of range (%d°C)\n", 
                 driveId, tempCelsius);
        return Config::kInvalidTemp;
    }
    
    DebugLog("[Sidecar] Drive %d: %d°C (raw=%d)\n", driveId, tempCelsius, rawTemp);
    return tempCelsius;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MMF Management
// ═══════════════════════════════════════════════════════════════════════════════

bool InitMMF() {
    // Create the memory-mapped file
    State::g_hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,           // Pagefile-backed
        nullptr,                        // Default security
        PAGE_READWRITE,                 // Read/Write access
        0,                              // Size high
        sizeof(SovereignThermalData),   // Size low
        Config::kMapName                // Name
    );
    
    if (!State::g_hMapFile) {
        DWORD err = GetLastError();
        DebugLog("[Sidecar] FATAL: CreateFileMappingA failed (error %lu)\n", err);
        return false;
    }
    
    bool alreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);
    DebugLog("[Sidecar] MMF %s. Handle = 0x%p\n", 
             alreadyExists ? "opened (already exists)" : "created", 
             State::g_hMapFile);
    
    // Map a view
    State::g_pData = reinterpret_cast<SovereignThermalData*>(
        MapViewOfFile(
            State::g_hMapFile,
            FILE_MAP_WRITE,
            0, 0,
            sizeof(SovereignThermalData)
        )
    );
    
    if (!State::g_pData) {
        DWORD err = GetLastError();
        DebugLog("[Sidecar] FATAL: MapViewOfFile failed (error %lu)\n", err);
        CloseHandle(State::g_hMapFile);
        State::g_hMapFile = nullptr;
        return false;
    }
    
    DebugLog("[Sidecar] View mapped at 0x%p\n", State::g_pData);
    
    // Initialize header
    State::g_pData->signature = Config::kSignature;
    State::g_pData->version = Config::kVersion;
    State::g_pData->driveCount = Config::kDriveCount;
    State::g_pData->reserved = 0;
    
    // Initialize temps and wear to -1 (invalid)
    for (int i = 0; i < Config::kMaxDrives; i++) {
        State::g_pData->temps[i] = Config::kInvalidTemp;
        State::g_pData->wear[i] = Config::kInvalidTemp;
    }
    
    State::g_pData->timestampMs = GetTickCount64();
    
    return true;
}

void CleanupMMF() {
    if (State::g_pData) {
        UnmapViewOfFile(State::g_pData);
        State::g_pData = nullptr;
    }
    
    if (State::g_hMapFile) {
        CloseHandle(State::g_hMapFile);
        State::g_hMapFile = nullptr;
    }
    
    DebugLog("[Sidecar] MMF cleaned up\n");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Main Polling Loop
// ═══════════════════════════════════════════════════════════════════════════════

void UpdateDrives() {
    if (!State::g_pData) return;
    
    for (int i = 0; i < Config::kDriveCount; i++) {
        int driveId = Config::kDriveIds[i];
        int32_t temp = GetNvmeTemperatureIoctl(driveId);
        State::g_pData->temps[i] = temp;
        State::g_pData->wear[i] = Config::kInvalidTemp; // Not implemented yet
    }
    
    State::g_pData->timestampMs = GetTickCount64();
}

void MainLoop() {
    DebugLog("[Sidecar] Entering main loop (poll every %lu ms)\n", 
             Config::kPollIntervalMs);
    
    while (State::g_running.load()) {
        UpdateDrives();
        Sleep(Config::kPollIntervalMs);
    }
    
    DebugLog("[Sidecar] Main loop exited\n");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Windows Service Support
// ═══════════════════════════════════════════════════════════════════════════════

DWORD WINAPI ServiceCtrlHandler(DWORD dwControl, DWORD dwEventType, 
                                 LPVOID lpEventData, LPVOID lpContext) {
    (void)dwEventType;
    (void)lpEventData;
    (void)lpContext;
    
    switch (dwControl) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            DebugLog("[Sidecar] Received stop/shutdown control\n");
            State::g_running.store(false);
            
            State::g_serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            State::g_serviceStatus.dwCheckPoint = 1;
            State::g_serviceStatus.dwWaitHint = 3000;
            SetServiceStatus(State::g_hServiceStatus, &State::g_serviceStatus);
            return NO_ERROR;
            
        case SERVICE_CONTROL_INTERROGATE:
            SetServiceStatus(State::g_hServiceStatus, &State::g_serviceStatus);
            return NO_ERROR;
            
        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

void WINAPI ServiceMain(DWORD argc, LPSTR* argv) {
    (void)argc;
    (void)argv;
    
    DebugLog("[Sidecar] ServiceMain called\n");
    
    // Register service control handler
    State::g_hServiceStatus = RegisterServiceCtrlHandlerExA(
        Config::kServiceName,
        ServiceCtrlHandler,
        nullptr
    );
    
    if (!State::g_hServiceStatus) {
        DebugLog("[Sidecar] FATAL: RegisterServiceCtrlHandlerEx failed\n");
        return;
    }
    
    // Report starting
    ZeroMemory(&State::g_serviceStatus, sizeof(State::g_serviceStatus));
    State::g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    State::g_serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    State::g_serviceStatus.dwControlsAccepted = 0;
    State::g_serviceStatus.dwWin32ExitCode = NO_ERROR;
    State::g_serviceStatus.dwCheckPoint = 1;
    State::g_serviceStatus.dwWaitHint = 3000;
    SetServiceStatus(State::g_hServiceStatus, &State::g_serviceStatus);
    
    // Initialize MMF
    if (!InitMMF()) {
        State::g_serviceStatus.dwCurrentState = SERVICE_STOPPED;
        State::g_serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        State::g_serviceStatus.dwServiceSpecificExitCode = 1;
        SetServiceStatus(State::g_hServiceStatus, &State::g_serviceStatus);
        return;
    }
    
    // Report running
    State::g_serviceStatus.dwCurrentState = SERVICE_RUNNING;
    State::g_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    State::g_serviceStatus.dwCheckPoint = 0;
    State::g_serviceStatus.dwWaitHint = 0;
    SetServiceStatus(State::g_hServiceStatus, &State::g_serviceStatus);
    
    // Main loop
    MainLoop();
    
    // Cleanup
    CleanupMMF();
    
    // Report stopped
    State::g_serviceStatus.dwCurrentState = SERVICE_STOPPED;
    State::g_serviceStatus.dwControlsAccepted = 0;
    State::g_serviceStatus.dwCheckPoint = 0;
    State::g_serviceStatus.dwWaitHint = 0;
    SetServiceStatus(State::g_hServiceStatus, &State::g_serviceStatus);
}

bool TryStartAsService() {
    SERVICE_TABLE_ENTRYA serviceTable[] = {
        { const_cast<char*>(Config::kServiceName), ServiceMain },
        { nullptr, nullptr }
    };
    
    // This will block if we're running as a service
    // and return FALSE with ERROR_FAILED_SERVICE_CONTROLLER_CONNECT if not
    if (StartServiceCtrlDispatcherA(serviceTable)) {
        return true; // Running as service
    }
    
    DWORD err = GetLastError();
    if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
        return false; // Not running as service, run standalone
    }
    
    // Some other error
    DebugLog("[Sidecar] StartServiceCtrlDispatcher failed (error %lu)\n", err);
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Standalone Mode (Console)
// ═══════════════════════════════════════════════════════════════════════════════

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            printf("\n[Sidecar] Shutdown signal received\n");
            State::g_running.store(false);
            return TRUE;
        default:
            return FALSE;
    }
}

void RunStandalone() {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     SOVEREIGN NVMe Thermal Sidecar v2.0.0                    ║\n");
    printf("║     Running in standalone mode (Ctrl+C to stop)             ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("[Sidecar] Map Name: %s\n", Config::kMapName);
    printf("[Sidecar] Drives: ");
    for (int i = 0; i < Config::kDriveCount; i++) {
        printf("%d", Config::kDriveIds[i]);
        if (i < Config::kDriveCount - 1) printf(", ");
    }
    printf("\n");
    printf("[Sidecar] Poll Interval: %lu ms\n\n", Config::kPollIntervalMs);
    
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    
    if (!InitMMF()) {
        fprintf(stderr, "[Sidecar] FATAL: Failed to initialize MMF\n");
        return;
    }
    
    MainLoop();
    
    CleanupMMF();
    
    printf("[Sidecar] Shutdown complete\n");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Entry Point
// ═══════════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    // Check for --standalone flag
    bool forceStandalone = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--standalone") == 0 || 
            strcmp(argv[i], "-s") == 0) {
            forceStandalone = true;
            break;
        }
    }
    
    if (!forceStandalone) {
        // Try to run as a Windows service
        State::g_isService.store(true);
        if (TryStartAsService()) {
            return 0;
        }
        State::g_isService.store(false);
    }
    
    // Run in standalone mode
    RunStandalone();
    return 0;
}
