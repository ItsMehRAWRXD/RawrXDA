// masm_orchestration_stubs.cpp
// Stub implementations of MASM orchestration functions when MASM integration is disabled
// These are called from MainWindow and other Qt code

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
    // Stub implementations - no-ops when MASM is not available
    
    void ai_orchestration_install(HWND hWindow) {
        // Stub: do nothing - MASM integration disabled at build time
        (void)hWindow;  // Suppress unused parameter warning
    }
    
    void ai_orchestration_poll() {
        // Stub: do nothing - MASM integration disabled at build time
    }
    
    void ai_orchestration_shutdown() {
        // Stub: do nothing - MASM integration disabled at build time
    }
    
    void ai_orchestration_set_handles(HWND hOutput, HWND hChat) {
        // Stub: do nothing - MASM integration disabled at build time
        (void)hOutput;  // Suppress unused parameter warning
        (void)hChat;    // Suppress unused parameter warning
    }
    
    void ai_orchestration_schedule_task(const char* goal, int priority, bool autoRetry) {
        // Stub: do nothing - MASM integration disabled at build time
        (void)goal;       // Suppress unused parameter warning
        (void)priority;   // Suppress unused parameter warning
        (void)autoRetry;  // Suppress unused parameter warning
    }
}
