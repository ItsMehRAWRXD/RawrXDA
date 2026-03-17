// RawrXD_IDE_Complete.cpp
// Complete RawrXD IDE Application with AI Integration
// Links: IDE_MainWindow.cpp + AI_Integration.cpp + Assembly code

#include <windows.h>
#include <stdio.h>
#include "RawrXD_TextEditor.h"

// Forward declarations from IDE_MainWindow.cpp
extern HWND IDE_CreateMainWindow(HINSTANCE hInstance, int nCmdShow);
extern int IDE_MessageLoop(HACCEL hAccelerators);
extern HACCEL IDE_SetupAccelerators();
extern void IDE_UpdateStatusBar(const char* message);
extern void IDE_UpdateStatusBarPosition();

// Forward declarations from AI_Integration.cpp
extern void AI_InitializeEngine(RawrXDTextEditor* pEditor, HWND hStatusWindow);
extern void AI_TriggerCompletion(int max_tokens);
extern bool AI_IsBusy();
extern void AI_ShutdownEngine();

// Forward declarations from IDE_MainWindow.cpp (globals)
extern RawrXDTextEditor* g_pEditor;
extern HWND g_hMainWindow;
extern HWND g_hStatusBar;

// ============================================================================
// APPLICATION INITIALIZATION
// ============================================================================

bool APP_Initialize(HINSTANCE hInstance) {
    // Initialize assembly library (if in DLL form)
    // This would call DllMain or LibMain if using .lib
    
    printf("[INFO] Initializing RawrXD IDE\n");
    printf("[INFO] Assembly modules linked\n");
    
    return true;
}

// ============================================================================
// APPLICATION STARTUP
// ============================================================================

int APP_Run(HINSTANCE hInstance, HINSTANCE hPrevInstance,
            LPSTR lpCmdLine, int nCmdShow) {
    
    printf("=== RawrXD IDE Startup ===\n");
    printf("Version: 1.0 (x64 MASM)\n");
    printf("Date: March 12, 2026\n");
    printf("\n");
    
    // Initialize application
    if (!APP_Initialize(hInstance)) {
        MessageBoxA(NULL, "Failed to initialize application", "Error", MB_ICONERROR);
        return 1;
    }
    
    printf("[INIT] Creating main window...\n");
    
    // Setup accelerators BEFORE creating window
    HACCEL hAccel = IDE_SetupAccelerators();
    if (!hAccel) {
        MessageBoxA(NULL, "Failed to setup accelerators", "Error", MB_ICONERROR);
        return 1;
    }
    printf("[INIT] Accelerator table created\n");
    
    // Create main IDE window
    HWND hMainWindow = IDE_CreateMainWindow(hInstance, nCmdShow);
    if (!hMainWindow) {
        MessageBoxA(NULL, "Failed to create main window", "Error", MB_ICONERROR);
        return 1;
    }
    printf("[INIT] Main window created (HWND: %p)\n", hMainWindow);
    
    // Wait a moment for window to be ready
    Sleep(100);
    
    // Initialize AI engine (after editor exists)
    printf("[INIT] Initializing AI completion engine...\n");
    if (g_pEditor && g_hStatusBar) {
        AI_InitializeEngine(g_pEditor, g_hStatusBar);
        printf("[INIT] AI engine initialized\n");
    } else {
        printf("[WARN] Editor or status bar not ready for AI\n");
    }
    
    printf("\n=== Application Ready ===\n");
    printf("Window: HWND %p\n", hMainWindow);
    printf("Editor: %p\n", g_pEditor);
    printf("Status Bar: %p\n", g_hStatusBar);
    printf("\nControls:\n");
    printf("  Ctrl+O = Open file\n");
    printf("  Ctrl+S = Save file\n");
    printf("  Ctrl+X = Cut\n");
    printf("  Ctrl+C = Copy\n");
    printf("  Ctrl+V = Paste\n");
    printf("  Tools > AI Completion = Trigger AI\n");
    printf("  Ctrl+Q = Exit\n");
    printf("\n");
    
    // Run message loop with accelerators
    printf("[RUN] Entering message loop...\n");
    int exitCode = IDE_MessageLoop(hAccel);
    
    // Cleanup
    printf("\n[SHUTDOWN] Cleaning up...\n");
    
    AI_ShutdownEngine();
    printf("[SHUTDOWN] AI engine shut down\n");
    
    if (hAccel) {
        DestroyAcceleratorTable(hAccel);
        printf("[SHUTDOWN] Accelerators destroyed\n");
    }
    
    printf("[SHUTDOWN] Exit code: %d\n", exitCode);
    printf("=== RawrXD IDE Closed ===\n");
    
    return exitCode;
}

// ============================================================================
// ENTRY POINT
// ============================================================================

int WINAPI WinMainA(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nCmdShow) {
    return APP_Run(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}

// ============================================================================
// CONSOLE DEBUGGING SUPPORT (Optional)
// ============================================================================

#ifdef _DEBUG

// Redirect console output to debug window
void DEBUG_AllocateConsole() {
    AllocConsole();
    FILE* pFile = NULL;
    freopen_s(&pFile, "CONOUT$", "w", stdout);
    freopen_s(&pFile, "CONOUT$", "w", stderr);
    
    printf("=== RawrXD IDE Debug Console ===\n");
    printf("Process ID: %d\n", GetCurrentProcessId());
    printf("Thread ID: %d\n", GetCurrentThreadId());
    printf("\n");
}

#endif
