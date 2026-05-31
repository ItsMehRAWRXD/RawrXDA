// startup_sentinels.cpp
// CRT-level entry probe: fires before HeadlessIDE::initialize and main_win32 logic
// This confirms whether the process reaches user-mode initialization at all

#include <windows.h>
#include <cstdio>
#include <ctime>

// Static constructor - runs during CRT initialization, before main/WinMain
class CRTEntrySentinel
{
public:
    CRTEntrySentinel()
    {
        logCRTEntry();
    }

private:
    void logCRTEntry()
    {
        DWORD pid = GetCurrentProcessId();
        SYSTEMTIME st;
        GetSystemTime(&st);

        // Format: [HH:MM:SS.mmm] PID=XXXXX CRT_ENTRY_REACHED
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                 "[%02d:%02d:%02d.%03d] PID=%u CRT_ENTRY_REACHED\n",
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                 pid);

        // Log to stderr (visible in console/OutputDebugString)
        fputs(buffer, stderr);
        fflush(stderr);

        // Also log to file
        FILE* traceFile = nullptr;
        errno_t err = fopen_s(&traceFile, "startup_crt_entry.log", "a");
        if (err == 0 && traceFile)
        {
            fputs(buffer, traceFile);
            fclose(traceFile);
        }

        // Output to debugger
        OutputDebugStringA(buffer);
    }
};

// Global instance - ctor runs during CRT startup
static CRTEntrySentinel g_crt_entry_probe;

// ============================================================================
// STATIC INIT COMPLETION SENTINEL
// Fires AFTER all other static constructors complete
// If this fires: failure is NOT during static init (failure is later, in WinMain)
// If this doesn't fire: failure IS during static init (other global ctors)
// ============================================================================
namespace
{
    class StaticInitCompleteSentinel
    {
    public:
        StaticInitCompleteSentinel()
        {
            DWORD pid = GetCurrentProcessId();
            SYSTEMTIME st;
            GetSystemTime(&st);

            char buffer[256];
            snprintf(buffer, sizeof(buffer),
                     "[%02d:%02d:%02d.%03d] PID=%u STATIC_INIT_COMPLETE_OK\n",
                     st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                     pid);

            // Log to stderr
            fputs(buffer, stderr);
            fflush(stderr);

            // Log to file
            FILE* completeLog = nullptr;
            errno_t err = fopen_s(&completeLog, "startup_static_init_complete.log", "a");
            if (err == 0 && completeLog)
            {
                fputs(buffer, completeLog);
                fclose(completeLog);
            }

            // Output to debugger
            OutputDebugStringA(buffer);
        }
    };

    // Place this AFTER CRTEntrySentinel and other module statics
    // It will be one of the last global ctors to run
    static StaticInitCompleteSentinel g_static_init_complete_probe;
}
