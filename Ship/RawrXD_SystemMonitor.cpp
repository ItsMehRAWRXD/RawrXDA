// RawrXD System Monitor - Pure Win32/C++ Implementation
// System-wide status, health, and performance monitoring

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <vector>

// ============================================================================
// MONITOR STRUCTURES
// ============================================================================

struct SystemHealth {
    int cpu_usage;
    int memory_usage;
    int disk_usage;
    int active_threads;
    int error_count;
    DWORD uptime_ms;
};

struct ComponentStatus {
    wchar_t name[128];
    BOOL operational;
    int error_count;
    DWORD uptime_ms;
};

// ============================================================================
// SYSTEM MONITOR
// ============================================================================

class SystemMonitor {
private:
    CRITICAL_SECTION m_cs;
    std::vector<ComponentStatus> m_components;
    DWORD m_startTime;
    int m_totalErrors;
    int m_cpuUsage;
    int m_memoryUsage;
    
public:
    SystemMonitor()
        : m_startTime(GetTickCount()),
          m_totalErrors(0),
          m_cpuUsage(0),
          m_memoryUsage(0) {
        InitializeCriticalSection(&m_cs);
    }
    
    ~SystemMonitor() {
        Shutdown();
        DeleteCriticalSection(&m_cs);
    }
    
    void Initialize() {
        EnterCriticalSection(&m_cs);
        m_components.clear();
        m_startTime = GetTickCount();
        m_totalErrors = 0;
        LeaveCriticalSection(&m_cs);
        OutputDebugStringW(L"[SystemMonitor] Initialized\n");
    }
    
    void Shutdown() {
        EnterCriticalSection(&m_cs);
        m_components.clear();
        LeaveCriticalSection(&m_cs);
    }
    
    // Register component
    BOOL RegisterComponent(const wchar_t* name) {
        if (!name) return FALSE;
        
        EnterCriticalSection(&m_cs);
        
        ComponentStatus comp;
        wcscpy_s(comp.name, 128, name);
        comp.operational = TRUE;
        comp.error_count = 0;
        comp.uptime_ms = 0;
        
        m_components.push_back(comp);
        
        LeaveCriticalSection(&m_cs);
        
        wchar_t buf[256];
        swprintf_s(buf, 256, L"[SystemMonitor] Component registered: %s\n", name);
        OutputDebugStringW(buf);
        
        return TRUE;
    }
    
    // Report component error
    BOOL ReportComponentError(const wchar_t* name) {
        if (!name) return FALSE;
        
        EnterCriticalSection(&m_cs);
        
        BOOL found = FALSE;
        for (auto& comp : m_components) {
            if (wcscmp(comp.name, name) == 0) {
                comp.error_count++;
                m_totalErrors++;
                found = TRUE;
                break;
            }
        }
        
        LeaveCriticalSection(&m_cs);
        return found;
    }
    
    // Update component status
    BOOL UpdateComponentStatus(const wchar_t* name, BOOL operational) {
        if (!name) return FALSE;
        
        EnterCriticalSection(&m_cs);
        
        BOOL found = FALSE;
        for (auto& comp : m_components) {
            if (wcscmp(comp.name, name) == 0) {
                comp.operational = operational;
                found = TRUE;
                break;
            }
        }
        
        LeaveCriticalSection(&m_cs);
        return found;
    }
    
    // Get system health
    SystemHealth GetHealth() {
        SystemHealth health;
        health.cpu_usage = m_cpuUsage;
        health.memory_usage = m_memoryUsage;
        health.disk_usage = 0;
        health.active_threads = 0;
        health.error_count = m_totalErrors;
        health.uptime_ms = GetTickCount() - m_startTime;
        
        EnterCriticalSection(&m_cs);
        health.active_threads = (int)m_components.size();
        LeaveCriticalSection(&m_cs);
        
        return health;
    }
    
    // Get component status
    std::vector<ComponentStatus> GetComponentStatuses() {
        EnterCriticalSection(&m_cs);
        std::vector<ComponentStatus> result = m_components;
        LeaveCriticalSection(&m_cs);
        return result;
    }
    
    // Check overall health
    BOOL IsHealthy() {
        SystemHealth health = GetHealth();
        return (health.error_count < 5);
    }
    
    // Get diagnostic report
    const wchar_t* GetDiagnostics() {
        static wchar_t report[1024];
        SystemHealth health = GetHealth();
        
        EnterCriticalSection(&m_cs);
        int operational_count = 0;
        for (const auto& comp : m_components) {
            if (comp.operational) operational_count++;
        }
        LeaveCriticalSection(&m_cs);
        
        swprintf_s(report, 1024,
            L"SystemMonitor: Components=%d Operational=%d Errors=%d CPU=%d%% Mem=%d%% Uptime=%lu ms",
            (int)m_components.size(), operational_count, health.error_count,
            health.cpu_usage, health.memory_usage, health.uptime_ms);
        
        return report;
    }
};

// ============================================================================
// C INTERFACE
// ============================================================================

extern "C" {

__declspec(dllexport) SystemMonitor* __stdcall CreateSystemMonitor(void) {
    return new SystemMonitor();
}

__declspec(dllexport) void __stdcall DestroySystemMonitor(SystemMonitor* mon) {
    if (mon) delete mon;
}

__declspec(dllexport) void __stdcall SystemMonitor_Initialize(SystemMonitor* mon) {
    if (mon) mon->Initialize();
}

__declspec(dllexport) void __stdcall SystemMonitor_Shutdown(SystemMonitor* mon) {
    if (mon) mon->Shutdown();
}

__declspec(dllexport) BOOL __stdcall SystemMonitor_RegisterComponent(SystemMonitor* mon,
    const wchar_t* name) {
    return mon ? mon->RegisterComponent(name) : FALSE;
}

__declspec(dllexport) BOOL __stdcall SystemMonitor_ReportError(SystemMonitor* mon,
    const wchar_t* name) {
    return mon ? mon->ReportComponentError(name) : FALSE;
}

__declspec(dllexport) BOOL __stdcall SystemMonitor_UpdateStatus(SystemMonitor* mon,
    const wchar_t* name, BOOL operational) {
    return mon ? mon->UpdateComponentStatus(name, operational) : FALSE;
}

__declspec(dllexport) BOOL __stdcall SystemMonitor_IsHealthy(SystemMonitor* mon) {
    return mon ? mon->IsHealthy() : FALSE;
}

__declspec(dllexport) const wchar_t* __stdcall SystemMonitor_GetDiagnostics(SystemMonitor* mon) {
    return mon ? mon->GetDiagnostics() : L"Not initialized";
}

} // extern "C"

// ============================================================================
// DLL ENTRY
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringW(L"[RawrXD_SystemMonitor] DLL loaded\n");
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringW(L"[RawrXD_SystemMonitor] DLL unloaded\n");
        break;
    }
    return TRUE;
}
