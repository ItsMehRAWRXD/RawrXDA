/*
 * main_qt.cpp - MIGRATED TO WIN32
 * 
 * Previously used: Qt framework (QApplication, QMainWindow, etc.)
 * Now uses: Win32 API + RawrXD DLL components
 * 
 * This is the main entry point that initializes:
 * 1. Foundation orchestrator (manages all 32 components)
 * 2. Exception handler with recovery
 * 3. Configuration management via Win32 Registry
 * 4. Main window via Win32 CreateWindowEx
 * 5. Message loop via Win32 GetMessage/DispatchMessage
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

// Type definitions for our DLL exports
typedef void* (__stdcall *PFN_CreateFoundation)();
typedef bool (__stdcall *PFN_FoundationInit)(void*, const wchar_t*);
typedef bool (__stdcall *PFN_FoundationIsReady)(void*);
typedef void (__stdcall *PFN_FoundationPrintStatus)(void*);
typedef void (__stdcall *PFN_FoundationShutdown)(void*);

typedef void* (__stdcall *PFN_CreateMainWindow)(const wchar_t* title, int width, int height);
typedef void (__stdcall *PFN_ShowMainWindow)(void*);
typedef bool (__stdcall *PFN_ProcessMessages)(void*);
typedef void (__stdcall *PFN_DestroyMainWindow)(void*);

// Forward declarations
HMODULE g_hFoundation = nullptr;
HMODULE g_hMainWindow = nullptr;
void* g_pFoundation = nullptr;
void* g_pMainWindow = nullptr;

bool InitializeFoundation()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    
    // Get directory containing exe
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = L'\0';
    
    // Change to that directory (where DLLs are located)
    SetCurrentDirectoryW(exePath);
    
    // Load Foundation orchestrator
    g_hFoundation = LoadLibraryW(L"RawrXD_Foundation_Integration.dll");
    if (!g_hFoundation) {
        wprintf(L"FATAL: Failed to load Foundation DLL (error: 0x%X)\n", GetLastError());
        return false;
    }
    
    // Get Foundation factory function
    PFN_CreateFoundation pfnCreate = (PFN_CreateFoundation)
        GetProcAddress(g_hFoundation, "CreateFoundation");
    if (!pfnCreate) {
        wprintf(L"FATAL: Cannot find CreateFoundation export\n");
        FreeLibrary(g_hFoundation);
        return false;
    }
    
    // Create Foundation instance
    g_pFoundation = pfnCreate();
    if (!g_pFoundation) {
        wprintf(L"FATAL: CreateFoundation returned null\n");
        FreeLibrary(g_hFoundation);
        return false;
    }
    
    // Get Initialize function
    PFN_FoundationInit pfnInit = (PFN_FoundationInit)
        GetProcAddress(g_hFoundation, "Foundation_Initialize");
    if (!pfnInit) {
        wprintf(L"FATAL: Cannot find Foundation_Initialize export\n");
        FreeLibrary(g_hFoundation);
        return false;
    }
    
    // Initialize all 32 components
    wprintf(L"[RawrXD] Initializing Foundation with 32 components...\n");
    if (!pfnInit(g_pFoundation, exePath)) {
        wprintf(L"ERROR: Foundation initialization failed\n");
        
        // Try to print status for diagnostics
        PFN_FoundationPrintStatus pfnStatus = (PFN_FoundationPrintStatus)
            GetProcAddress(g_hFoundation, "Foundation_PrintStatus");
        if (pfnStatus) {
            wprintf(L"[DEBUG] Foundation Status Report:\n");
            pfnStatus(g_pFoundation);
        }
        
        FreeLibrary(g_hFoundation);
        return false;
    }
    
    // Verify ready state
    PFN_FoundationIsReady pfnIsReady = (PFN_FoundationIsReady)
        GetProcAddress(g_hFoundation, "Foundation_IsReady");
    if (pfnIsReady && !pfnIsReady(g_pFoundation)) {
        wprintf(L"WARNING: Foundation reports not ready (some components may have failed)\n");
        // Continue anyway - non-critical components may have failed
    }
    
    wprintf(L"✓ Foundation initialized successfully\n");
    return true;
}

bool InitializeMainWindow()
{
    // Load MainWindow_Win32 DLL
    g_hMainWindow = LoadLibraryW(L"RawrXD_MainWindow_Win32.dll");
    if (!g_hMainWindow) {
        wprintf(L"ERROR: Failed to load MainWindow_Win32 DLL (error: 0x%X)\n", GetLastError());
        return false;
    }
    
    // Get factory function
    PFN_CreateMainWindow pfnCreateWindow = (PFN_CreateMainWindow)
        GetProcAddress(g_hMainWindow, "CreateMainWindow");
    if (!pfnCreateWindow) {
        wprintf(L"ERROR: Cannot find CreateMainWindow export\n");
        FreeLibrary(g_hMainWindow);
        return false;
    }
    
    // Create main window
    g_pMainWindow = pfnCreateWindow(L"RawrXD IDE v1.0", 1280, 800);
    if (!g_pMainWindow) {
        wprintf(L"ERROR: CreateMainWindow failed\n");
        FreeLibrary(g_hMainWindow);
        return false;
    }
    
    wprintf(L"✓ Main window created successfully\n");
    return true;
}

void RunMessageLoop()
{
    if (!g_hMainWindow || !g_pMainWindow) {
        wprintf(L"ERROR: Main window not initialized\n");
        return;
    }
    
    PFN_ShowMainWindow pfnShow = (PFN_ShowMainWindow)
        GetProcAddress(g_hMainWindow, "ShowMainWindow");
    PFN_ProcessMessages pfnProcess = (PFN_ProcessMessages)
        GetProcAddress(g_hMainWindow, "ProcessMessages");
    
    if (!pfnShow || !pfnProcess) {
        wprintf(L"ERROR: Cannot resolve window functions\n");
        return;
    }
    
    // Show the window
    pfnShow(g_pMainWindow);
    
    wprintf(L"[RawrXD] Entering message loop...\n");
    
    // Main message loop
    while (pfnProcess(g_pMainWindow)) {
        // Window is processing messages
        // ProcessMessages returns false when window closes
        Sleep(10);  // Prevent busy-waiting
    }
    
    wprintf(L"[RawrXD] Message loop exited, shutting down...\n");
}

void Cleanup()
{
    // Destroy main window
    if (g_hMainWindow && g_pMainWindow) {
        PFN_DestroyMainWindow pfnDestroy = (PFN_DestroyMainWindow)
            GetProcAddress(g_hMainWindow, "DestroyMainWindow");
        if (pfnDestroy) {
            pfnDestroy(g_pMainWindow);
        }
        FreeLibrary(g_hMainWindow);
        g_hMainWindow = nullptr;
        g_pMainWindow = nullptr;
    }
    
    // Shutdown Foundation
    if (g_hFoundation && g_pFoundation) {
        PFN_FoundationShutdown pfnShutdown = (PFN_FoundationShutdown)
            GetProcAddress(g_hFoundation, "Foundation_Shutdown");
        if (pfnShutdown) {
            pfnShutdown(g_pFoundation);
        }
        FreeLibrary(g_hFoundation);
        g_hFoundation = nullptr;
        g_pFoundation = nullptr;
    }
    
    wprintf(L"[RawrXD] Cleanup complete\n");
}

int main(int argc, char* argv[])
{
    wprintf(L"\n");
    wprintf(L"╔═══════════════════════════════════════════════════════════╗\n");
    wprintf(L"║           RawrXD Autonomous Agentic IDE v1.0             ║\n");
    wprintf(L"║            Zero Qt | Win32 Native | 32 Components        ║\n");
    wprintf(L"╚═══════════════════════════════════════════════════════════╝\n");
    wprintf(L"\n");
    
    // Initialize Foundation (all 32 components)
    if (!InitializeFoundation()) {
        wprintf(L"\nFATAL: Cannot initialize Foundation layer\n");
        return 1;
    }
    
    // Initialize main window
    if (!InitializeMainWindow()) {
        wprintf(L"\nWARNING: Cannot initialize main window, but Foundation is ready\n");
        wprintf(L"You can still use CLI mode or headless operation\n");
        // Don't fail here - system can run without GUI
    }
    
    // Run message loop
    RunMessageLoop();
    
    // Cleanup
    Cleanup();
    
    wprintf(L"\n✓ RawrXD exited cleanly\n\n");
    return 0;
}
