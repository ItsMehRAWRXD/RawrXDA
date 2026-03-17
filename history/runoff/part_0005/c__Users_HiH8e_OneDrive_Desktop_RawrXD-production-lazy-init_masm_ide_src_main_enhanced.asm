; ============================================================================
; ENHANCED MAIN - Phase 3 Integration with test harness and consolidated explorer
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\gdi32.inc
include \masm32\include\comctl32.inc
include \masm32\include\ole32.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\gdi32.lib
includelib \masm32\lib\comctl32.lib
includelib \masm32\lib\ole32.lib

; External modules with prototypes
extern LogSystem_Initialize:proc
extern LogMessage:proc
extern ShutdownLogging:proc
extern PerformanceMonitor_Init:proc
extern PerformanceMonitor_Start:proc
extern PerformanceMonitor_Stop:proc
extern PerformanceMonitor_Cleanup:proc
extern PerformanceMonitor_Sample:proc
extern PerformanceMonitor_GetStats:proc
extern PerformanceOptimizer_Init:proc
extern PerformanceOptimizer_StartMonitoring:proc
extern PerformanceOptimizer_ApplyOptimizations:proc
extern PerformanceOptimizer_StopMonitoring:proc
extern PerformanceOptimizer_Cleanup:proc
extern FileExplorer_Create:proc
extern FileExplorer_Cleanup:proc
extern TestHarness_Initialize:proc
extern TestHarness_RegisterTest:proc
extern TestHarness_RunAllTests:proc
extern TestHarness_GenerateReport:proc

; Test functions
extern Test_Explorer_EnumerateDrives:proc
extern Test_Explorer_EnumerateFiles:proc
extern Test_Explorer_PathValidation:proc
extern Test_Logging_SystemActive:proc
extern Test_Logging_FileCreation:proc
extern Test_Perf_MonitorStartup:proc
extern Test_Perf_SamplingCollection:proc
extern Test_Integration_EditorInit:proc
extern Test_Integration_UIResponsive:proc

; ==================== CONSTANTS ====================
IDM_FILE_NEW           equ 100
IDM_FILE_OPEN          equ 101
IDM_FILE_SAVE          equ 102
IDM_FILE_EXIT          equ 103
IDM_VIEW_RUN_TESTS     equ 200
IDM_VIEW_LOGS          equ 201
IDM_VIEW_PERFORMANCE   equ 202
IDM_HELP_ABOUT         equ 300

ID_HOTKEY_VIEW_LOGS    equ 1000

; ==================== STRUCTURES ====================
IDE_STATE struct
    hMainWindow        dd ?
    hFileExplorer      dd ?
    hEditor            dd ?
    hStatusBar         dd ?
    hMenuBar           dd ?
    bInitialized       dd ?
IDE_STATE ends

; ==================== DATA ====================
.data
    g_IdeState         IDE_STATE <>
    
    szClassName        db "RawrXDIDEClass", 0
    szWindowTitle      db "RawrXD IDE - Phase 3 Enhanced", 0
    szAboutTitle       db "About RawrXD IDE", 0
    szAboutText        db "RawrXD IDE Phase 3", 13, 10
                       db "Enhanced with:", 13, 10
                       db "• Consolidated File Explorer", 13, 10
                       db "• Test Harness Framework", 13, 10
                       db "• Performance Monitoring", 13, 10
                       db "• Advanced Logging", 0
    
    ; Menu strings
    szMenuFile         db "&File", 0
    szMenuNew          db "&New\tCtrl+N", 0
    szMenuOpen         db "&Open\tCtrl+O", 0
    szMenuSave         db "&Save\tCtrl+S", 0
    szMenuExit         db "E&xit", 0
    szMenuView         db "&View", 0
    szMenuRunTests     db "&Run Tests\tF5", 0
    szMenuLogs         db "&Logs\tCtrl+L", 0
    szMenuPerf         db "&Performance", 0
    szMenuHelp         db "&Help", 0
    szMenuAbout        db "&About", 0
    
    ; Messages
    szStartupMsg       db "RawrXD IDE Phase 3 startup", 0
    szShutdownMsg      db "RawrXD IDE Phase 3 shutdown", 0
    szTestsComplete    db "Test execution complete", 0
    szTestResultTitle  db "Test Results", 0
    
    ; UI strings for OnCreate and handlers
    szStatusClass      db "msctls_statusbar32", 0
    szStatusReady      db "Ready - Phase 3 Enhanced IDE", 0
    szRunningTests     db "Running tests...", 0
    szTestDoneFormat   db "Tests completed in %dms", 0
    szStatusBuffer     db 128 dup(?)
    szLogsMsg          db "Log viewer not yet implemented. Check C:\\RawrXD\\logs\\ide.log", 0
    szLogsTitle        db "Log Viewer", 0
    szPerfFormat       db "Memory Usage: %d bytes", 13, 10, "GDI Objects: %d", 0
    szPerfTitle        db "Performance Monitor", 0
    szPerfBuffer       db 256 dup(?)

; Prototypes
CreateWindowExA PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
DefWindowProcA PROTO :DWORD,:DWORD,:DWORD,:DWORD
RegisterClassExA PROTO :DWORD
GetMessageA PROTO :DWORD,:DWORD,:DWORD,:DWORD
TranslateMessage PROTO :DWORD
DispatchMessageA PROTO :DWORD
PostQuitMessage PROTO :DWORD
LoadCursorA PROTO :DWORD,:DWORD
LoadIconA PROTO :DWORD,:DWORD
CreateMenu PROTO
CreatePopupMenu PROTO
AppendMenuA PROTO :DWORD,:DWORD,:DWORD,:DWORD
SetMenu PROTO :DWORD,:DWORD
MessageBoxA PROTO :DWORD,:DWORD,:DWORD,:DWORD
wsprintfA PROTO C :VARARG
CoInitialize PROTO :DWORD
CoUninitialize PROTO
ShowWindow PROTO :DWORD,:DWORD
UpdateWindow PROTO :DWORD
GetClientRect PROTO :DWORD,:DWORD
GetWindowRect PROTO :DWORD,:DWORD
MoveWindow PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
SendMessageA PROTO :DWORD,:DWORD,:DWORD,:DWORD
GetTickCount PROTO
GetModuleHandleA PROTO :DWORD
ExitProcess PROTO :DWORD

.data?
    hInstance          dd ?
    msg                MSG <>
    wc                 WNDCLASSEX <>
    szTestReport       db 4096 dup(?)

; ==================== CODE ====================
.code

; ============================================================================
; WinMain - Application entry point
; ============================================================================
public WinMain
WinMain proc hInst:DWORD, hPrevInst:DWORD, szCmdLine:DWORD, iCmdShow:DWORD
    mov eax, hInst
    mov hInstance, eax
    
    ; Initialize COM
    invoke CoInitialize, NULL
    
    ; Initialize logging first
    invoke LogSystem_Initialize
    invoke LogMessage, 1, addr szStartupMsg
    
    ; Initialize performance monitoring
    invoke PerformanceMonitor_Init
    invoke PerformanceOptimizer_Init
    
    ; Register window class
    mov wc.cbSize, sizeof WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    mov wc.lpfnWndProc, offset WndProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov eax, hInstance
    mov wc.hInstance, eax
    invoke LoadIconA, NULL, IDI_APPLICATION
    mov wc.hIcon, eax
    invoke LoadCursorA, NULL, IDC_ARROW
    mov wc.hCursor, eax
    mov wc.hbrBackground, COLOR_WINDOW + 1
    mov wc.lpszMenuName, NULL
    mov wc.lpszClassName, offset szClassName
    mov wc.hIconSm, 0
    
    invoke RegisterClassExA, addr wc
    test eax, eax
    jz @@Exit
    
    ; Create main window
    invoke CreateWindowExA, WS_EX_APPWINDOW,
        addr szClassName,
        addr szWindowTitle,
        WS_OVERLAPPEDWINDOW or WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1200, 800,
        NULL, NULL,
        hInstance, NULL
    
    test eax, eax
    jz @@Exit
    mov g_IdeState.hMainWindow, eax
    
    ; Create menu
    call CreateMainMenu
    
    ; Show window
    invoke ShowWindow, g_IdeState.hMainWindow, SW_SHOWNORMAL
    invoke UpdateWindow, g_IdeState.hMainWindow
    
    ; Start performance monitoring
    invoke PerformanceMonitor_Start, g_IdeState.hMainWindow
    invoke PerformanceOptimizer_StartMonitoring
    invoke PerformanceOptimizer_ApplyOptimizations
    
    ; Initialize test harness
    invoke TestHarness_Initialize, hInstance
    call RegisterTestCases
    
    ; Message loop
    @@MessageLoop:
        invoke GetMessageA, addr msg, NULL, 0, 0
        cmp eax, 0
        je @@Exit
        cmp eax, -1
        je @@Exit
        
        invoke TranslateMessage, addr msg
        invoke DispatchMessageA, addr msg
        jmp @@MessageLoop
    
    @@Exit:
    ; Cleanup
    invoke PerformanceMonitor_Stop, g_IdeState.hMainWindow
    invoke PerformanceOptimizer_StopMonitoring
    invoke PerformanceMonitor_Cleanup
    invoke PerformanceOptimizer_Cleanup
    invoke FileExplorer_Cleanup
    invoke LogMessage, 1, addr szShutdownMsg
    invoke ShutdownLogging
    invoke CoUninitialize
    
    mov eax, msg.wParam
    ret
WinMain endp

; ============================================================================
; WndProc - Window procedure
; ============================================================================
WndProc proc hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    .if uMsg == WM_CREATE
        call OnCreate
        xor eax, eax
        ret
    .elseif uMsg == WM_COMMAND
        call OnCommand
        xor eax, eax
        ret
    .elseif uMsg == WM_SIZE
        call OnSize
        xor eax, eax
        ret
    .elseif uMsg == WM_TIMER
        call OnTimer
        xor eax, eax
        ret
    .elseif uMsg == WM_CLOSE
        invoke PostQuitMessage, 0
        xor eax, eax
        ret
    .endif
    
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret
WndProc endp

; ============================================================================
; OnCreate - Handle WM_CREATE
; ============================================================================
OnCreate proc
    ; Create file explorer
    invoke FileExplorer_Create, g_IdeState.hMainWindow, hInstance
    mov g_IdeState.hFileExplorer, eax
    
    ; Create status bar
    invoke CreateWindowExA, 0, 
        addr szStatusClass, NULL,
        WS_CHILD or WS_VISIBLE or SBARS_SIZEGRIP,
        0, 0, 0, 0,
        g_IdeState.hMainWindow, NULL, hInstance, NULL
    mov g_IdeState.hStatusBar, eax
    
    ; Set status text
    invoke SendMessageA, g_IdeState.hStatusBar, SB_SETTEXT, 0, addr szStatusReady
    
    mov g_IdeState.bInitialized, 1
    ret
OnCreate endp

; ============================================================================
; OnCommand - Handle WM_COMMAND
; ============================================================================
OnCommand proc uses ebx edi esi
    LOCAL cmdId:DWORD
    
    mov eax, wParam
    and eax, 0FFFFh
    mov cmdId, eax
    
    mov eax, cmdId
    .if eax == IDM_FILE_EXIT
        invoke PostQuitMessage, 0
    .elseif eax == IDM_VIEW_RUN_TESTS
        call OnRunTests
    .elseif eax == IDM_VIEW_LOGS
        call OnViewLogs
    .elseif eax == IDM_VIEW_PERFORMANCE
        call OnViewPerformance
    .elseif eax == IDM_HELP_ABOUT
        call OnAbout
    .endif
    
    ret
OnCommand endp

; ============================================================================
; OnSize - Handle WM_SIZE
; ============================================================================
OnSize proc
    LOCAL clientRect:RECT
    LOCAL statusHeight:DWORD
    
    ; Get client area
    invoke GetClientRect, g_IdeState.hMainWindow, addr clientRect
    
    ; Resize status bar
    invoke SendMessageA, g_IdeState.hStatusBar, WM_SIZE, 0, 0
    
    ; Get status bar height
    invoke GetWindowRect, g_IdeState.hStatusBar, addr statusHeight
    mov eax, statusHeight
    ; statusHeight = bottom - top, but we'll use fixed 25
    mov statusHeight, 25
    
    ; Resize file explorer
    mov eax, clientRect.bottom
    sub eax, statusHeight
    invoke MoveWindow, g_IdeState.hFileExplorer, 0, 0, 300, eax, TRUE
    
    ret
OnSize endp

; ============================================================================
; OnTimer - Handle WM_TIMER
; ============================================================================
OnTimer proc
    ; Take performance sample
    invoke PerformanceMonitor_Sample
    ret
OnTimer endp

; ============================================================================
; OnRunTests - Run test harness
; ============================================================================
OnRunTests proc
    LOCAL dwStartTime:DWORD
    LOCAL dwElapsed:DWORD
    
    ; Update status
    invoke SendMessageA, g_IdeState.hStatusBar, SB_SETTEXT, 0, addr szRunningTests
    
    ; Get start time
    invoke GetTickCount
    mov dwStartTime, eax
    
    ; Run all tests
    invoke TestHarness_RunAllTests
    
    ; Calculate elapsed time
    invoke GetTickCount
    sub eax, dwStartTime
    mov dwElapsed, eax
    
    ; Generate report
    invoke TestHarness_GenerateReport, addr szTestReport, sizeof szTestReport
    
    ; Show results
    invoke MessageBoxA, g_IdeState.hMainWindow, addr szTestReport, 
           addr szTestResultTitle, MB_OK or MB_ICONINFORMATION
    
    ; Log completion
    invoke LogMessage, 1, addr szTestsComplete
    
    ; Update status with elapsed time
    invoke wsprintfA, addr szStatusBuffer, addr szTestDoneFormat, dwElapsed
    invoke SendMessageA, g_IdeState.hStatusBar, SB_SETTEXT, 0, addr szStatusBuffer
    
    ret
OnRunTests endp

; ============================================================================
; OnViewLogs - Show log viewer (placeholder)
; ============================================================================
OnViewLogs proc
    invoke MessageBoxA, g_IdeState.hMainWindow, addr szLogsMsg, 
           addr szLogsTitle, MB_OK or MB_ICONINFORMATION
    ret
OnViewLogs endp

; ============================================================================
; OnViewPerformance - Show performance monitor (placeholder)
; ============================================================================
OnViewPerformance proc
    LOCAL dwMemory:DWORD
    LOCAL dwGdi:DWORD
    
    invoke PerformanceMonitor_GetStats, addr dwMemory, NULL, addr dwGdi
    
    invoke wsprintfA, addr szPerfBuffer, addr szPerfFormat, dwMemory, dwGdi
    invoke MessageBoxA, g_IdeState.hMainWindow, addr szPerfBuffer,
           addr szPerfTitle, MB_OK or MB_ICONINFORMATION
    ret
OnViewPerformance endp

; ============================================================================
; OnAbout - Show about dialog
; ============================================================================
OnAbout proc
    invoke MessageBoxA, g_IdeState.hMainWindow, addr szAboutText,
           addr szAboutTitle, MB_OK or MB_ICONINFORMATION
    ret
OnAbout endp

; ============================================================================
; CreateMainMenu - Create application menu
; ============================================================================
CreateMainMenu proc
    LOCAL hMenu:DWORD
    LOCAL hFileMenu:DWORD
    LOCAL hViewMenu:DWORD
    LOCAL hHelpMenu:DWORD
    
    ; Create main menu
    invoke CreateMenu
    mov hMenu, eax
    
    ; Create File menu
    invoke CreatePopupMenu
    mov hFileMenu, eax
    invoke AppendMenuA, hFileMenu, MF_STRING, IDM_FILE_NEW, addr szMenuNew
    invoke AppendMenuA, hFileMenu, MF_STRING, IDM_FILE_OPEN, addr szMenuOpen
    invoke AppendMenuA, hFileMenu, MF_STRING, IDM_FILE_SAVE, addr szMenuSave
    invoke AppendMenuA, hFileMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenuA, hFileMenu, MF_STRING, IDM_FILE_EXIT, addr szMenuExit
    invoke AppendMenuA, hMenu, MF_POPUP, hFileMenu, addr szMenuFile
    
    ; Create View menu
    invoke CreatePopupMenu
    mov hViewMenu, eax
    invoke AppendMenuA, hViewMenu, MF_STRING, IDM_VIEW_RUN_TESTS, addr szMenuRunTests
    invoke AppendMenuA, hViewMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenuA, hViewMenu, MF_STRING, IDM_VIEW_LOGS, addr szMenuLogs
    invoke AppendMenuA, hViewMenu, MF_STRING, IDM_VIEW_PERFORMANCE, addr szMenuPerf
    invoke AppendMenuA, hMenu, MF_POPUP, hViewMenu, addr szMenuView
    
    ; Create Help menu
    invoke CreatePopupMenu
    mov hHelpMenu, eax
    invoke AppendMenuA, hHelpMenu, MF_STRING, IDM_HELP_ABOUT, addr szMenuAbout
    invoke AppendMenuA, hMenu, MF_POPUP, hHelpMenu, addr szMenuHelp
    
    ; Set menu
    invoke SetMenu, g_IdeState.hMainWindow, hMenu
    mov g_IdeState.hMenuBar, hMenu
    
    ret
CreateMainMenu endp

; ============================================================================
; RegisterTestCases - Register all test cases with the harness
; ============================================================================
RegisterTestCases proc
    ; Explorer tests
    invoke TestHarness_RegisterTest, addr szTestDriveEnum, 0, addr Test_Explorer_EnumerateDrives
    invoke TestHarness_RegisterTest, addr szTestFileEnum, 0, addr Test_Explorer_EnumerateFiles
    invoke TestHarness_RegisterTest, addr szTestPathValid, 0, addr Test_Explorer_PathValidation
    
    ; Logging tests
    invoke TestHarness_RegisterTest, addr szTestLogActive, 1, addr Test_Logging_SystemActive
    invoke TestHarness_RegisterTest, addr szTestLogFile, 1, addr Test_Logging_FileCreation
    
    ; Performance tests
    invoke TestHarness_RegisterTest, addr szTestPerfStart, 2, addr Test_Perf_MonitorStartup
    invoke TestHarness_RegisterTest, addr szTestPerfSample, 2, addr Test_Perf_SamplingCollection
    
    ; Integration tests
    invoke TestHarness_RegisterTest, addr szTestEditorInit, 3, addr Test_Integration_EditorInit
    invoke TestHarness_RegisterTest, addr szTestUIResp, 3, addr Test_Integration_UIResponsive
    
    ret
    
; Test names
szTestDriveEnum  db "Drive Enumeration", 0
szTestFileEnum   db "File Enumeration", 0
szTestPathValid  db "Path Validation", 0
szTestLogActive  db "Logging System Active", 0
szTestLogFile    db "Log File Creation", 0
szTestPerfStart  db "Performance Monitor Startup", 0
szTestPerfSample db "Performance Sampling", 0
szTestEditorInit db "Editor Initialization", 0
szTestUIResp     db "UI Responsiveness", 0
RegisterTestCases endp

; Entry point
public _mainCRTStartup
_mainCRTStartup proc
    invoke GetModuleHandleA, NULL
    push SW_SHOWNORMAL
    push 0
    push 0
    push eax
    call WinMain
    
    push eax
    call ExitProcess
_mainCRTStartup endp

end _mainCRTStartup