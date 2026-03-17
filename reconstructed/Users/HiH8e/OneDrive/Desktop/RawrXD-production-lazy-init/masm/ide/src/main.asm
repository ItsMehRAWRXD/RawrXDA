; ============================================================================
; RawrXD Agentic IDE - Pure MASM Implementation
; Main Entry Point
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include ..\include\winapi_min.inc

; Local prototypes to satisfy INVOKE resolution
GetMessage PROTO :DWORD, :DWORD, :DWORD, :DWORD
TranslateMessage PROTO :DWORD
DispatchMessage PROTO :DWORD
PeekMessage PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
Sleep PROTO :DWORD
; Include the comprehensive GUI wiring system
include gui_wiring_system.asm

; Include Qt-like pane system modules
include pane_system_core.asm
include pane_layout_engine.asm
include settings_ui_complete.asm
include pane_serialization.asm
include pane_gguf_integration_bridge.asm

; GUI wiring system functions
extern GUI_InitAllComponents:proc
extern GUI_UpdateLayout:proc
extern GUI_HandleCommand:proc
extern GUI_UpdateStatus:proc
extern GUI_UpdateProgress:proc

; Pane system functions - supports up to 100 customizable panes with docking
extern PANE_SYSTEM_INIT:proc
extern PANE_CREATE:proc
extern PANE_DESTROY:proc
extern PANE_SETPOSITION:proc
extern PANE_SETSTATE:proc
extern PANE_SETZORDER:proc
extern PANE_GETINFO:proc
extern PANE_SETCONSTRAINTS:proc
extern PANE_SETCOLOR:proc
extern PANE_GETCOUNT:proc
extern PANE_ENUMALL:proc

; Layout engine
extern LAYOUT_APPLY_VSCODE:proc
extern LAYOUT_APPLY_WEBSTORM:proc
extern LAYOUT_APPLY_VISUALSTUDIO:proc
extern CURSOR_SETRESIZEH:proc
extern CURSOR_SETRESIZEV:proc
extern CURSOR_SETMOVE:proc
extern CURSOR_SETDEFAULT:proc

; Settings UI
extern SETTINGS_INIT:proc
extern SETTINGS_CREATETAB:proc
extern SETTINGS_APPLYLAYOUT:proc
extern SETTINGS_GETCHATOPTIONS:proc
extern SETTINGS_GETTERMINALOPTIONS:proc
extern SETTINGS_SETCHATMODEL:proc
extern SETTINGS_TOGGLEFEATURE:proc
extern SETTINGS_SAVECONFIGURATION:proc
extern SETTINGS_LOADCONFIGURATION:proc

; Serialization
extern PANE_SERIALIZATION_SAVE:proc
extern PANE_SERIALIZATION_LOAD:proc
extern PANE_SERIALIZATION_RESET:proc

; GGUF Integration
extern GGUF_PANE_INITIALIZE:proc
extern GGUF_PANE_ADDMODEL:proc
extern GGUF_PANE_LOADMODEL:proc
extern GGUF_PANE_CREATEMODELPANE:proc
extern GGUF_PANE_DISPLAYMODELINFO:proc
extern GGUF_PANE_INTEGRATESETTINGS:proc
extern GGUF_PANE_APPLYLAYOUT:proc
extern MagicWand_ShowWishDialog:proc
extern FloatingPanel_Init:proc
extern FloatingPanel_Create:proc
extern FloatingPanel_Cleanup:proc
extern GGUFLoader_Init:proc
extern GGUFLoader_ShowModelDialog:proc
extern GGUFLoader_Cleanup:proc
extern LSPClient_Init:proc
extern LSPClient_Create:proc
extern LSPClient_Cleanup:proc
extern Compression_Init:proc
extern Compression_CompressFile:proc
extern Compression_DecompressFile:proc
extern Deflate_Compress:proc
extern Deflate_Decompress:proc
extern HandlePhase4Command:proc
extern DebugTest_Init:proc
extern DebugTest_Run:proc
extern InitializeLogging:proc
extern ShutdownLogging:proc
extern LogMessage:proc
extern DebugTest_Cleanup:proc
extern ToolRegistry_Init:proc
extern ModelInvoker_Init:proc
extern ModelInvoker_SetEndpoint:proc
extern ActionExecutor_Init:proc
extern ActionExecutor_SetProjectRoot:proc
extern LoopEngine_Init:proc

; Include performance optimization modules
extern PerformanceMonitor_Init:proc

; Phase 3 Test Harness
extern TestHarness_Initialize:proc
extern TestHarness_RegisterTest:proc
extern TestHarness_RunAllTests:proc
extern TestHarness_GenerateReport:proc
extern Test_Explorer_EnumerateDrives:proc
extern Test_Explorer_EnumerateFiles:proc
extern Test_Explorer_PathValidation:proc
extern Test_Logging_SystemActive:proc
extern Test_Logging_FileCreation:proc
extern Test_Perf_MonitorStartup:proc
extern Test_Perf_SamplingCollection:proc
extern Test_Integration_EditorInit:proc

; Consolidated File Explorer
extern FileExplorer_Create:proc
extern FileExplorer_PopulateFolder:proc
extern FileExplorer_EnableDragDrop:proc
extern FileExplorer_StartWatcher:proc
extern FileExplorer_ShowContextMenu:proc
extern FileExplorer_Search:proc
extern FileExplorer_Cleanup:proc
extern PerformanceMonitor_Start:proc
extern PerformanceMonitor_Stop:proc
extern PerformanceMonitor_Cleanup:proc
extern MemoryPool_Init:proc
extern MemoryPool_Cleanup:proc
extern FileEnumeration_Init:proc
extern FileEnumeration_Cleanup:proc
extern FileEnumeration_EnumerateAsync:proc
extern PerformanceOptimizer_Init:proc
extern PerformanceOptimizer_Cleanup:proc
extern PerformanceOptimizer_StartMonitoring:proc
extern PerformanceOptimizer_StopMonitoring:proc
extern PerformanceOptimizer_ApplyOptimizations:proc
extern Compression_GetStatistics:proc
extern Compression_Cleanup:proc
extern PopulateDirectory:proc
extern GetItemPath:proc
extern RefreshFileTree:proc
extern CreateErrorDashboard:proc
extern CloseErrorDashboard:proc
; Perf metrics
extern PerfMetrics_Init:proc
extern PerfMetrics_Update:proc
extern PerfMetrics_SetStatusBar:proc
extern FileExplorer_OnEnumComplete:proc
extern OpenLogViewer:proc
; Hex inspector (Phase 5.5)
extern HexUi_Init:proc
extern HexUi_Show:proc
extern HexUi_SelectAndLoad:proc
extern HexUi_ApplyEdits:proc

; Phase 6 - Performance Optimization
extern PerfOpt_InitializePool:proc
extern PerfOpt_AllocFromPool:proc
extern PerfOpt_FreeToPool:proc
extern PerfOpt_MarkDirty:proc
extern PerfOpt_IsRegionDirty:proc
extern PerfOpt_ClearDirty:proc
extern PerfOpt_GetFPS:proc
extern PerfOpt_UpdateFrameCounter:proc
extern PerfOpt_CacheDirectory:proc
extern PerfOpt_LookupDirCache:proc
extern PerfOpt_GetMemoryUsage:proc
extern PerfOpt_OptimizeEditorBuffer:proc
extern PerfOpt_BatchLLMRequests:proc
extern PerfOpt_Cleanup:proc
; Security
extern Security_Init:proc
; Settings & UI
extern Settings_Init:proc
extern Settings_Show:proc
extern Editor_Init:proc
extern CommandPalette_Init:proc
extern CommandPalette_Show:proc

WM_SOCKET_NOTIFY equ WM_USER + 100
WM_AGENTIC_COMPLETE equ WM_USER + 101
WM_TOOL_EXECUTE equ WM_USER + 102
WM_FILE_ENUM_COMPLETE equ WM_USER + 510
MAX_TABS equ 32
TIMER_PERF_UPDATE equ 1001
PERF_TIMER_INTERVAL equ 16
SB_SETPARTS equ (WM_USER + 4)
ID_HOTKEY_VIEW_LOGS equ 51001
MOD_CONTROL equ 0002h

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    szClassName         db "RawrXDAgenticIDE", 0
    szWindowTitle       db "RawrXD Agentic IDE (Pure MASM)", 0
    
    ; Configuration
    szProjectRoot       db 260 dup(0)
    szModelEndpoint     db "http://localhost:11434", 0
    szCurrentModel      db "llama2", 0
    
    ; UI State
    bMaximized          dd 0
    bDarkMode           dd 1
    hMainWindow         dd 0
    hStatusBar          dd 0
    hToolBar            dd 0
    hFileTree           dd 0
    hTabControl         dd 0
    hEditor             dd 0
    hTerminal           dd 0
    hChatPanel          dd 0
    hOrchestraPanel     dd 0
    hProgressBar        dd 0
    hErrorDashboard     dd 0
    perf_DrawLogCounter dd 0
    bPerfLoggingEnabled dd 1
    
    ; Agentic Engine Handles
    hModelInvoker       dd 0
    hActionExecutor     dd 0
    hToolRegistry       dd 0
    hLoopEngine         dd 0
    
    ; Font handles
    hMainFont           dd 0
    hEditorFont         dd 0
    hMonoFont           dd 0
    
    ; Performance optimization
    dwFrameStartTick    dd 0
    dwFrameEndTick      dd 0
    dwFrameTimeMs       dd 0
    dwTargetFrameTimeMs dd 16   ; 60 Hz = ~16.67 ms
    qwFrameCount        dq 0
    dwLastFpsUpdateTick dd 0
    dwCurrentFps        dd 0
    dwMinFrameTimeMs    dd 1000
    dwMaxFrameTimeMs    dd 0
    
    ; Memory pooling
    MemoryPool_FileBuffer  dd 0
    MemoryPool_TabBuffer   dd 0
    dwBufferPoolSize       dd 1048576
    dwTabPoolSize          dd 524288
    
    ; Lazy loading
    bLazyLoadEnabled    dd 1
    dwMaxTreeItemsPerUpdate dd 100
    dwPendingItemsCount dd 0
    
    ; Performance metrics
    dwPeakMemoryUsage   dd 0
    dwAverageMemory     dd 0
    qwFileEnumTotalTime dq 0
    dwFileEnumCount     dd 0
    
    ; Colors (Dark Theme)
    clrBackground       dd 001E1E1Eh
    clrForeground       dd 00E0E0E0h
    clrAccent           dd 0007ACCh
    clrSelection        dd 00264F78h
    clrBorder           dd 00404040h
    
    ; Brushes
    hBackgroundBrush    dd 0
    hSelectionBrush     dd 0
    
    ; Metrics
    nTotalExecutions    dd 0
    nSuccessCount       dd 0
    nFailureCount       dd 0
    
    ; UI Strings
    szSegoeUI           db "Segoe UI", 0
    szConsolas          db "Consolas", 0
    szToolbarClass      db "ToolbarWindow32", 0
    szStatusClass       db "msctls_statusbar32", 0
    szTreeViewClass     db "SysTreeView32", 0
    szTabClass          db "SysTabControl32", 0
    szEditClass         db "EDIT", 0
    szToolPanelTitle    db "Tool Panel", 0
    szRegistryTitle     db "Tool Registry", 0
    szLoopEngineTitle   db "Agentic Loop Engine", 0
    szCompressTitle     db "Compression", 0
    szWelcomeTab        db "Welcome", 0
    szChatWelcome       db "Welcome to RawrXD Agentic IDE", 13, 10, "Type your requests here...", 13, 10, 0
    szOrchestraStatus   db "Orchestra Panel - Multi-Agent Coordination", 13, 10, "Status: Ready", 13, 10, 0
    szProgressClass     db "msctls_progress32", 0
    szStartupLog        db "IDE started", 0
    szShutdownLog       db "IDE shutting down", 0
    szPaintPerfFmt      db "OnPaint %u us", 0
    szViewLogs          db "View Logs", 0
    szStatusHex         db "Hex inspector ready", 0

    ; Menu IDs
    IDM_FILE_NEW        equ 1001
    IDM_FILE_OPEN       equ 1002
    IDM_FILE_SAVE       equ 1003
    IDM_FILE_SAVE_AS    equ 1004
    IDM_FILE_EXIT       equ 1099
    IDM_AGENTIC_WISH    equ 2001
    IDM_AGENTIC_LOOP    equ 2002
    IDM_TOOLS_REGISTRY  equ 3001
    IDM_TOOLS_LOAD_GGUF equ 3002
    IDM_FILE_COMPRESS_INFO equ 3003
    IDM_VIEW_FLOATING   equ 4001
    IDM_VIEW_REFRESH_TREE equ 4002
    IDM_VIEW_LOGS       equ 4003
    IDM_VIEW_RUN_TESTS  equ 4004
    IDM_VIEW_HEX_INSPECT equ 4005
    IDM_HELP_ABOUT      equ 5001
    
    ; Floating Panel Styles
    FP_STYLE_TOOL       equ 1
    FP_STYLE_MODELESS   equ 2
    
    ; Note: Lazy loading and performance metrics are defined in .data? section below
    
    szAboutText         db "RawrXD Agentic IDE (Pure MASM)", 13, 10
                        db "Version 1.0 - Phase 1 Complete", 13, 10, 13, 10
                        db "A modern IDE with agentic AI assistance", 13, 10
                        db "Built entirely in x86 Assembly (MASM32)", 13, 10, 13, 10
                        db "Features:", 13, 10
                        db "• Multi-tab code editor", 13, 10
                        db "• File tree navigation", 13, 10
                        db "• Agentic AI integration", 13, 10
                        db "• Pure Win32 API implementation", 13, 10, 0
    
.data?
    hInstance           dd ?
    CommandLine         dd ?
    
    ; Window rectangles
    rectClient          RECT <>
    rectFileTree        RECT <>
    rectEditor          RECT <>
    rectTerminal        RECT <>
    rectChat            RECT <>
    
    ; Message structure
    msg                 MSG <>

    public bPerfLoggingEnabled

    ; Memory pooling, lazy loading, perf metrics defined in other modules

; ============================================================================
; CODE SECTION
; ============================================================================

.code

COMMENT @

; ============================================================================
; WinMain - Application Entry Point
; ============================================================================
WinMain proc
    LOCAL wc:WNDCLASSEX
    
    ; Get instance handle
    invoke GetModuleHandle, NULL
    mov hInstance, eax

    ; Security baseline
    ; call Security_Init  ; TODO: implement
    
    ; Settings & UI framework
    ; invoke Settings_Init, hInstance  ; TODO: implement
    ; invoke Editor_Init, hInstance  ; TODO: implement
    ; invoke CommandPalette_Init, hInstance  ; TODO: implement
    
    ; Initialize common controls
    invoke InitCommonControls
    ; Initialize memory pooling
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, dwBufferPoolSize
    mov MemoryPool_FileBuffer, eax
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, dwTabPoolSize
    mov MemoryPool_TabBuffer, eax
    
    ; Initialize COM for shell operations
    invoke CoInitializeEx, NULL, COINIT_APARTMENTTHREADED

    ; Initialize logging system
    ; invoke InitializeLogging  ; TODO: define
    ; Optional: log startup
    ; invoke LogMessage, 1, addr szStartupLog  ; TODO: define
    
    ; Create brushes
    invoke CreateSolidBrush, clrBackground
    mov hBackgroundBrush, eax
    invoke CreateSolidBrush, clrSelection
    mov hSelectionBrush, eax
    
    ; Register window class
    mov wc.cbSize, sizeof WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW or CS_DBLCLKS
    mov wc.lpfnWndProc, offset WndProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    push hInstance
    pop wc.hInstance
    invoke LoadIcon, NULL, IDI_APPLICATION
    mov wc.hIcon, eax
    invoke LoadCursor, NULL, IDC_ARROW
    mov wc.hCursor, eax
    push hBackgroundBrush
    pop wc.hbrBackground
    mov wc.lpszMenuName, NULL
    mov wc.lpszClassName, offset szClassName
    mov wc.hIconSm, 0
    
    invoke RegisterClassEx, addr wc
    test eax, eax
    jz @Exit
    
    ; Create main window
    invoke CreateWindowEx, WS_EX_APPWINDOW or WS_EX_WINDOWEDGE,
        addr szClassName,
        addr szWindowTitle,
        WS_OVERLAPPEDWINDOW or WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 800,
        NULL, NULL,
        hInstance, NULL
    
    mov hMainWindow, eax
    test eax, eax
    jz @Exit
    
    ; Show and update window
    invoke ShowWindow, hMainWindow, SW_SHOWNORMAL
    invoke UpdateWindow, hMainWindow
    
    ; Phase 3: Initialize performance monitoring
    invoke PerformanceMonitor_Init
    invoke PerformanceMonitor_Start

    ; Initialize agentic subsystems (stubbed)
    ; call InitializeAgenticEngine

    ; Phase 3: Apply initial performance optimizations
    invoke PerformanceOptimizer_Init
    invoke PerformanceOptimizer_StartMonitoring
    invoke PerformanceOptimizer_ApplyOptimizations
    
    ; Main message loop with frame rate limiting
@@MessageLoop:
    ; Measure frame start time
    invoke GetTickCount
    mov dwFrameStartTick, eax
    
    ; Process all pending messages without blocking
@@ProcessMessages:
    invoke GetMessage, addr msg, NULL, 0, 0
    test eax, eax
    jz @Exit  ; WM_QUIT received
    
    ; Handle message
    invoke TranslateMessage, addr msg
    invoke DispatchMessage, addr msg
    
    ; Check for more messages (non-blocking peek)
    invoke PeekMessage, addr msg, NULL, 0, 0, PM_REMOVE
    test eax, eax
    jnz @ProcessMessages
    
    ; Measure frame end time
    invoke GetTickCount
    mov dwFrameEndTick, eax
    
    ; Calculate frame time
    mov eax, dwFrameEndTick
    sub eax, dwFrameStartTick
    mov dwFrameTimeMs, eax
    
    ; Update performance metrics
    cmp eax, dwMinFrameTimeMs
    jge @NotMinFrame
    mov dwMinFrameTimeMs, eax
@NotMinFrame:
    cmp eax, dwMaxFrameTimeMs
    jle @NotMaxFrame
    mov dwMaxFrameTimeMs, eax
@NotMaxFrame:
    
    ; Frame rate limiting: sleep if frame finished too early
    cmp eax, dwTargetFrameTimeMs
    jge @FrameComplete  ; Frame already took >= target time
    
    ; Calculate sleep time = target - actual
    mov ecx, dwTargetFrameTimeMs
    sub ecx, eax
    invoke Sleep, ecx
    
@FrameComplete:
    ; Update FPS counter every 1000ms
    mov eax, dwFrameStartTick
    sub eax, dwLastFpsUpdateTick
    cmp eax, 1000
    jl @SkipFpsUpdate
    
    ; Calculate FPS = 1000 / average_frame_time
    mov edx, dword ptr qwFrameCount
    test edx, edx
    jz @SkipFpsUpdate
    mov eax, 1000
    xor edx, edx
    mov ecx, dword ptr qwFrameCount
    div ecx
    mov dwCurrentFps, eax
    mov dwLastFpsUpdateTick, dwFrameStartTick
    mov qword ptr qwFrameCount, 0
    
@SkipFpsUpdate:
    ; Increment frame counter (64-bit)
    inc dword ptr qwFrameCount
    adc dword ptr qwFrameCount+4, 0
    
    jmp @MessageLoop
    
@Exit:
    ; Phase 3: Stop monitoring and cleanup performance modules
    invoke PerformanceMonitor_Stop
    invoke PerformanceMonitor_Cleanup
    invoke PerformanceOptimizer_StopMonitoring
    invoke PerformanceOptimizer_Cleanup

    ; Phase 3: Cleanup file explorer
    invoke FileExplorer_Cleanup

    ; Close dashboard window if running
    invoke CloseErrorDashboard

    ; Phase 3: Log shutdown and cleanup logging system
    invoke LogMessage, 1, addr szShutdownLog
    invoke ShutdownLogging
    
    ; Cleanup resources
    invoke DeleteObject, hBackgroundBrush
    invoke DeleteObject, hSelectionBrush
    invoke CoUninitialize
    
    push msg.wParam
    pop eax
    ret
WinMain endp

; ============================================================================
; WndProc - Main Window Procedure
; ============================================================================
WndProc proc hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    
    .if uMsg == WM_CREATE
        call OnCreate
        xor eax, eax
        ret
        
    .elseif uMsg == WM_SIZE
        call OnSize
        xor eax, eax
        ret
        
    .elseif uMsg == WM_COMMAND
        push lParam
        push wParam
        call OnCommand
        xor eax, eax
        ret
        
    .elseif uMsg == WM_NOTIFY
        push lParam
        call OnNotify
        ret
    .elseif uMsg == WM_FILE_ENUM_COMPLETE
        ; Async file enumeration completed: wParam = opHandle
        push lParam
        push wParam
        call FileExplorer_OnEnumComplete
        xor eax, eax
        ret
    
    .elseif uMsg == WM_TIMER
        ; Perf metrics timer - batch updates
        mov eax, wParam
        cmp eax, TIMER_PERF_UPDATE
        jne @notPerfTimer
        ; Update status bar with FPS (batched)
        call UpdateStatusBarFPS
        call PerfMetrics_Update
        xor eax, eax
        ret
@notPerfTimer:
        ; fallthrough
        
    .elseif uMsg == WM_PAINT
        call OnPaint
        ; Batch status bar update after paint
        call UpdateStatusBarFPS
        xor eax, eax
        ret
        
    .elseif uMsg == WM_HOTKEY
        ; Ctrl+L opens logs
        mov eax, wParam
        cmp eax, ID_HOTKEY_VIEW_LOGS
        jne @afterHotkey2
        call OpenLogViewer
        xor eax, eax
        ret
@afterHotkey2:
        
    .elseif uMsg == WM_CTLCOLOREDIT
        push lParam
        push wParam
        call OnCtlColorEdit
        ret
        
    .elseif uMsg == WM_AGENTIC_COMPLETE
        push lParam
        push wParam
        call OnAgenticComplete
        xor eax, eax
        ret
        
    .elseif uMsg == WM_TOOL_EXECUTE
        push lParam
        push wParam
        call OnToolExecute
        xor eax, eax
        ret
        
    .elseif uMsg == WM_CLOSE
        invoke DestroyWindow, hWnd
        xor eax, eax
        ret
        
    .elseif uMsg == WM_DESTROY
        ; kill perf timer
        invoke KillTimer, hMainWindow, TIMER_PERF_UPDATE
        ; unregister hotkey
        invoke UnregisterHotKey, hMainWindow, ID_HOTKEY_VIEW_LOGS
        ; close hex inspector if open
        call HexUi_Destroy
        invoke PostQuitMessage, 0
        xor eax, eax
        ret
        
    .endif
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
WndProc endp

; ============================================================================
; OnCreate - Handle WM_CREATE - NOW WITH Qt-LIKE PANE SYSTEM, SETTINGS, AND GGUF
; ============================================================================
OnCreate proc
    ; Use the comprehensive GUI wiring system
    invoke GUI_InitAllComponents, hMainWindow, hInstance

    ; Initialize Qt-like pane system (supports up to 100 panes)
    invoke PANE_SYSTEM_INIT
    
    ; Initialize settings system
    invoke SETTINGS_INIT
    
    ; Initialize GGUF integration
    invoke GGUF_PANE_INITIALIZE
    
    ; Apply default VS Code-style layout
    invoke LAYOUT_APPLY_VSCODE, hMainWindow, 1280, 800

    ; Get handles from GUI system (they're now properly created)
    ; The GUI_InitAllComponents function sets all the global handles

    ; Launch error dashboard for live log visibility
    invoke CreateErrorDashboard
    mov hErrorDashboard, eax

    ; Load project directory
    call LoadProjectRoot

    ret
OnCreate endp

; ============================================================================
; OnSize - Handle WM_SIZE - NOW FULLY WIRED
; ============================================================================
OnSize proc
    ; Use the GUI layout system for proper component positioning
    invoke GUI_UpdateLayout
    ret
OnSize endp

; ============================================================================
; OnToolsCompress - Handle compression tool button
; ============================================================================
OnToolsCompress proc
    LOCAL szStats[512]:BYTE
    
    ; Get compression statistics
    invoke Compression_GetStatistics, addr szStats, 512
    
    ; Show statistics in a message box
    invoke MessageBoxA, hMainWindow, addr szStats, addr szCompressTitle, MB_OK or MB_ICONINFORMATION
    
    ret
OnToolsCompress endp

; ============================================================================
; OnCommand - Handle WM_COMMAND - NOW FULLY WIRED
; ============================================================================
OnCommand proc wParam:DWORD, lParam:DWORD
    ; First try the GUI command handler
    invoke GUI_HandleCommand, wParam, lParam
    .if eax == TRUE
        ret  ; Command was handled by GUI system
    .endif

    ; Fall back to original command handling for any remaining commands
    LOCAL notifyCode:WORD
    LOCAL controlID:WORD

    ; Extract notification code and control ID
    mov eax, wParam
    shr eax, 16
    mov notifyCode, ax
    mov eax, wParam
    and eax, 0FFFFh
    mov controlID, ax

    ; Phase 4 AI menu commands (delegate first)
    invoke HandlePhase4Command, wParam, lParam
    .if eax != 0
        ret
    .endif

    ; Check control ID for remaining commands
    .if controlID == IDM_FILE_EXIT
        invoke SendMessage, hMainWindow, WM_CLOSE, 0, 0
    .elseif controlID == IDM_FILE_SAVE_AS
        call OnFileSaveAs
    .elseif controlID == IDM_VIEW_FLOATING
        call OnToggleFloatingPanel
    .elseif controlID == IDM_VIEW_REFRESH_TREE
        call OnRefreshFileTree
    .elseif controlID == IDM_VIEW_LOGS
        call OpenLogViewer
    .elseif controlID == IDM_VIEW_HEX_INSPECT
        call OnShowHexInspector
    .elseif controlID == IDM_VIEW_RUN_TESTS
        call OnRunTests
    .elseif controlID == IDM_HELP_ABOUT
        call OnHelpAbout
    .endif

    ret
OnCommand endp

; ============================================================================
; OnNotify - Handle WM_NOTIFY
; ============================================================================
OnNotify proc lParam:DWORD
    LOCAL pnmhdr:DWORD
    
    mov eax, lParam
    mov pnmhdr, eax
    
    ; Get notification code
    mov eax, pnmhdr
    assume eax:ptr NMHDR
    mov ecx, [eax].code
    assume eax:nothing
    
    ; Handle notifications
    .if ecx == TCN_SELCHANGE
        call OnTabChange
    .elseif ecx == TVN_SELCHANGED
        call OnTreeSelChange
    .elseif ecx == TVN_ITEMEXPANDING
        push lParam
        call OnTreeItemExpanding
    .endif
    
    xor eax, eax
    ret
OnNotify endp

; ============================================================================
; OnPaint - Handle WM_PAINT
; ============================================================================
OnPaint proc
    LOCAL ps:PAINTSTRUCT
    LOCAL hdc:DWORD
    LOCAL qwStart:QWORD
    LOCAL qwEnd:QWORD
    LOCAL qwFreq:QWORD
    LOCAL us:DWORD
    LOCAL szBuf[96]:BYTE
    
    ; Optionally measure paint duration every 64th paint
    inc perf_DrawLogCounter
    mov eax, perf_DrawLogCounter
    and eax, 3Fh
    cmp eax, 0
    jne @noTiming
    ; start timing
    lea eax, qwStart
    push eax
    call QueryPerformanceCounter
    lea eax, qwFreq
    push eax
    call QueryPerformanceFrequency
@noTiming:

    push OFFSET ps
    push hMainWindow
    call BeginPaint
    mov hdc, eax
    
    ; Custom painting if needed
    
    push OFFSET ps
    push hMainWindow
    call EndPaint

    ; stop timing and log if sampled
    mov eax, perf_DrawLogCounter
    and eax, 3Fh
    cmp eax, 0
    jne @skipLog
    lea eax, qwEnd
    push eax
    call QueryPerformanceCounter
    ; compute us = (end-start)*1e6 / freq (32-bit low parts)
    mov eax, dword ptr qwEnd
    sub eax, dword ptr qwStart
    mov edx, 1000000
    mul edx
    mov ecx, dword ptr qwFreq
    cmp ecx, 0
    je @skipLog
    xor edx, edx
    div ecx
    mov us, eax
    ; format and log (guarded)
    cmp bPerfLoggingEnabled, 0
    je @skipLog
    push us
    push OFFSET szPaintPerfFmt
    lea eax, szBuf
    push eax
    call wsprintfA
    add esp, 12
    invoke LogMessage, 1, addr szBuf
@skipLog:
    ret
OnPaint endp

; ============================================================================
; OnCtlColorEdit - Handle edit control colors
; ============================================================================
OnCtlColorEdit proc wParam:DWORD, lParam:DWORD
    LOCAL hdc:DWORD
    
    mov eax, wParam
    mov hdc, eax
    
    push clrForeground
    push hdc
    call SetTextColor
    push clrBackground
    push hdc
    call SetBkColor
    push hBackgroundBrush
    pop eax
    ret
OnCtlColorEdit endp

; ============================================================================
; InitializeAgenticEngine - Initialize all agentic components
; ============================================================================
InitializeAgenticEngine proc
    ; Initialize performance optimization modules
    call PerformanceOptimizer_Init
    call PerformanceOptimizer_StartMonitoring
    
    ; Initialize tool registry
    call ToolRegistry_Init
    mov hToolRegistry, eax
    
    ; Initialize model invoker
    call ModelInvoker_Init
    mov hModelInvoker, eax
    
    ; Initialize action executor
    call ActionExecutor_Init
    mov hActionExecutor, eax
    
    ; Initialize autonomous loop engine
    call LoopEngine_Init
    mov hLoopEngine, eax
    
    ; Initialize floating panel system
    call FloatingPanel_Init
    test eax, eax
    jz @Exit
    
    ; Initialize GGUF loader
    call GGUFLoader_Init
    test eax, eax
    jz @Exit
    
    ; Initialize LSP client
    call LSPClient_Init
    test eax, eax
    jz @Exit
    
    ; Initialize compression module
    call Compression_Init
    test eax, eax
    jz @Exit
    
    ; Configure connections
    invoke ModelInvoker_SetEndpoint, addr szModelEndpoint
    invoke ActionExecutor_SetProjectRoot, addr szProjectRoot
    
    mov eax, 1  ; Success
@Exit:
    ret
InitializeAgenticEngine endp

; ============================================================================
; Stub procedures - NOW PROVIDED BY stubs.asm module
; Comment out to avoid redefinition errors
; ============================================================================

; ============================================================================
; InitializeToolbar - Create and setup toolbar - NOW FULLY WIRED
; ============================================================================
InitializeToolbar proc
    invoke CreateToolbar
    mov hToolBar, eax
    ret
InitializeToolbar endp
;     ret
; CreateChatPanel endp
; 
; CreateOrchestraPanel proc
;     ret
; CreateOrchestraPanel endp
; 
; CreateProgressPanel proc
;     ret
; CreateProgressPanel endp
@
; End of commented UI functions - provided by stubs.asm

LoadProjectRoot proc
    LOCAL buffer[MAX_PATH]:BYTE
    
    ; Get current directory as project root
    invoke GetCurrentDirectory, MAX_PATH, addr buffer
    test eax, eax
    jz @Exit
    
    ; Copy to project root
    invoke lstrcpy, addr szProjectRoot, addr buffer
    
    ; Update status bar
    invoke SendMessage, hStatusBar, SB_SETTEXT, 0, addr buffer
    
    ; Rebuild the file tree for the selected root
    call RefreshFileTree
    
@Exit:
    ret
LoadProjectRoot endp

OnFileNew proc
    LOCAL szNewFile[32]:BYTE
    LOCAL tci:TC_ITEM
    LOCAL tabCount:DWORD
    
    ; Clear editor
    invoke SetWindowText, hEditor, NULL
    
    ; Create new tab
    invoke SendMessage, hTabControl, TCM_GETITEMCOUNT, 0, 0
    mov tabCount, eax
    
    ; Reset buffer slot for the new tab index
    cmp tabCount, MAX_TABS
    jae @SkipTabInit
    mov ecx, tabCount
    imul ecx, 4
    mov eax, offset TabBuffers
    add eax, ecx
    mov dword ptr [eax], 0
@SkipTabInit:
    
    invoke wsprintf, addr szNewFile, addr szNewTabFmt, tabCount
    
    mov tci.imask, TCIF_TEXT
    lea eax, szNewFile
    mov tci.pszText, eax
    invoke SendMessage, hTabControl, TCM_INSERTITEM, tabCount, addr tci
    
    ; Select new tab
    invoke SendMessage, hTabControl, TCM_SETCURSEL, tabCount, 0
    
    ; Update status
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr szStatusNew
    
    ret
    
szNewTabFmt db "Untitled-%d", 0
szStatusNew db "New file created", 0
OnFileNew endp

OnFileOpen proc
    LOCAL ofn:OPENFILENAME
    LOCAL hFile:HANDLE
    LOCAL fileSize:DWORD
    LOCAL bytesRead:DWORD
    LOCAL buffer:DWORD
    
    ; Clear filename buffer
    mov byte ptr [szFileName], 0
    
    ; Setup OPENFILENAME structure
    mov ofn.lStructSize, sizeof OPENFILENAME
    push hMainWindow
    pop ofn.hwndOwner
    push hInstance
    pop ofn.hInstance
    lea eax, szFileFilter
    mov ofn.lpstrFilter, eax
    mov ofn.lpstrCustomFilter, NULL
    mov ofn.nMaxCustFilter, 0
    mov ofn.nFilterIndex, 1
    lea eax, szFileName
    mov ofn.lpstrFile, eax
    mov ofn.nMaxFile, 260
    lea eax, szFileTitle
    mov ofn.lpstrFileTitle, eax
    mov ofn.nMaxFileTitle, 260
    mov ofn.lpstrInitialDir, NULL
    mov ofn.lpstrTitle, NULL
    mov ofn.Flags, OFN_FILEMUSTEXIST or OFN_PATHMUSTEXIST
    mov ofn.lpstrDefExt, NULL
    
    ; Show open dialog
    invoke GetOpenFileName, addr ofn
    test eax, eax
    jz @Exit
    
    ; Open file
    invoke CreateFile, addr szFileName, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @Exit
    mov hFile, eax
    
    ; Get file size
    invoke GetFileSize, hFile, NULL
    mov fileSize, eax
    
    ; Allocate buffer
    add eax, 1  ; null terminator
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, eax
    test eax, eax
    jz @CloseFile
    mov buffer, eax
    
    ; Read file
    invoke ReadFile, hFile, buffer, fileSize, addr bytesRead, NULL
    
    ; Set editor text
    invoke SetWindowText, hEditor, buffer
    
    ; Free buffer
    invoke GlobalFree, buffer
    
    ; Update status
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr szFileName
    
@CloseFile:
    invoke CloseHandle, hFile
    
@Exit:
    ret
OnFileOpen endp

OnFileSave proc
    LOCAL ofn:OPENFILENAME
    LOCAL hFile:HANDLE
    LOCAL textLen:DWORD
    LOCAL bytesWritten:DWORD
    LOCAL buffer:DWORD
    
    ; Check if filename exists
    cmp byte ptr [szFileName], 0
    jne @HasFilename
    
    ; Setup OPENFILENAME structure for Save As
    mov ofn.lStructSize, sizeof OPENFILENAME
    push hMainWindow
    pop ofn.hwndOwner
    push hInstance
    pop ofn.hInstance
    lea eax, szFileFilter
    mov ofn.lpstrFilter, eax
    mov ofn.lpstrCustomFilter, NULL
    mov ofn.nMaxCustFilter, 0
    mov ofn.nFilterIndex, 1
    lea eax, szFileName
    mov ofn.lpstrFile, eax
    mov ofn.nMaxFile, 260
    lea eax, szFileTitle
    mov ofn.lpstrFileTitle, eax
    mov ofn.nMaxFileTitle, 260
    mov ofn.lpstrInitialDir, NULL
    mov ofn.lpstrTitle, NULL
    mov ofn.Flags, OFN_OVERWRITEPROMPT
    mov ofn.lpstrDefExt, NULL
    
    ; Show save dialog
    invoke GetSaveFileName, addr ofn
    test eax, eax
    jz @Exit
    
@HasFilename:
    ; Get editor text length
    invoke GetWindowTextLength, hEditor
    mov textLen, eax
    add eax, 1
    
    ; Allocate buffer
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, eax
    test eax, eax
    jz @Exit
    mov buffer, eax
    
    ; Get editor text
    invoke GetWindowText, hEditor, buffer, textLen
    
    ; Create/open file
    invoke CreateFile, addr szFileName, GENERIC_WRITE, 0,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @FreeBuffer
    mov hFile, eax
    
    ; Write file
    invoke WriteFile, hFile, buffer, textLen, addr bytesWritten, NULL
    
    ; Close file
    invoke CloseHandle, hFile
    
    ; Update status
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr szStatusSaved
    
@FreeBuffer:
    invoke GlobalFree, buffer
    
@Exit:
    ret
    
szStatusSaved db "File saved successfully", 0
OnFileSave endp

OnAgenticWish proc
    ; Show magic wand wish dialog
    invoke MagicWand_ShowWishDialog
    ret
OnAgenticWish endp

OnAgenticLoop proc
    LOCAL pPanel:DWORD
    
    ; Show loop engine panel
    invoke FloatingPanel_Create, addr szLoopEngineTitle, FP_STYLE_MODELESS,
        150, 150, 400, 500, hMainWindow, NULL
    mov pPanel, eax
    test eax, eax
    jz @Exit
    
    invoke FloatingPanel_Show, pPanel, TRUE
    
    ; Update status
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr szStatusLoop
    
@Exit:
    ret
    
szStatusLoop db "Agentic loop started", 0
OnAgenticLoop endp

OnShowToolRegistry proc
    LOCAL pPanel:DWORD
    
    ; Show tool registry panel
    invoke FloatingPanel_Create, addr szRegistryTitle, FP_STYLE_TOOL,
        200, 100, 500, 600, hMainWindow, hToolRegistry
    mov pPanel, eax
    test eax, eax
    jz @Exit
    
    invoke FloatingPanel_Show, pPanel, TRUE
    
    ; Update status
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr szStatusRegistry
    
@Exit:
    ret
    
szStatusRegistry db "Tool Registry opened", 0
OnShowToolRegistry endp

OnShowHexInspector proc
    ; Bring up the hex inspector and optionally load a file
    invoke HexUi_Init, hInstance
    invoke HexUi_Show, hMainWindow
    invoke HexUi_SelectAndLoad
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr szStatusHex
    ret
OnShowHexInspector endp

OnToggleFloatingPanel proc
    LOCAL pPanel:DWORD
    LOCAL rect:RECT
    LOCAL width:DWORD
    LOCAL height:DWORD
    LOCAL x:DWORD
    LOCAL y:DWORD
    
    ; Create a sample floating panel for demonstration
    mov width, 300
    mov height, 400
    mov x, 100
    mov y, 100
    
    invoke FloatingPanel_Create, addr szToolPanelTitle, FP_STYLE_TOOL, 
        x, y, width, height, hMainWindow, NULL
    mov pPanel, eax
    test eax, eax
    jz @Exit
    
    ; Show the panel
    invoke FloatingPanel_Show, pPanel, TRUE
    
@Exit:
    ret
OnToggleFloatingPanel endp

OnLoadGGUFModel proc
    ; Show GGUF model loading dialog
    invoke GGUFLoader_ShowModelDialog
    ret
OnLoadGGUFModel endp

; ============================================================================
; OnRunTests - Run Phase 3 test harness
; ============================================================================
OnRunTests proc
    LOCAL szTestReport[4096]:BYTE
    LOCAL dwResult:DWORD
    
    ; Update status
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr szStatusRunningTests
    
    ; Initialize test harness
    invoke TestHarness_Initialize, hInstance
    
    ; Register all tests
    invoke TestHarness_RegisterTest, addr szTest1Name, 1, offset Test_Explorer_EnumerateDrives
    invoke TestHarness_RegisterTest, addr szTest2Name, 1, offset Test_Explorer_EnumerateFiles
    invoke TestHarness_RegisterTest, addr szTest3Name, 1, offset Test_Explorer_PathValidation
    invoke TestHarness_RegisterTest, addr szTest4Name, 3, offset Test_Logging_SystemActive
    invoke TestHarness_RegisterTest, addr szTest5Name, 3, offset Test_Logging_FileCreation
    invoke TestHarness_RegisterTest, addr szTest6Name, 4, offset Test_Perf_MonitorStartup
    invoke TestHarness_RegisterTest, addr szTest7Name, 4, offset Test_Perf_SamplingCollection
    invoke TestHarness_RegisterTest, addr szTest8Name, 5, offset Test_Integration_EditorInit
    
    ; Run all tests
    invoke TestHarness_RunAllTests
    mov dwResult, eax
    
    ; Generate report
    lea eax, szTestReport
    invoke TestHarness_GenerateReport, eax, 4096
    
    ; Show results in message box
    invoke MessageBoxA, hMainWindow, addr szTestReport, addr szTestResultTitle, MB_OK or MB_ICONINFORMATION
    
    ; Update status
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr szStatusTestsComplete
    
    ret

szStatusRunningTests db "Running Phase 3 tests...", 0
szStatusTestsComplete db "Tests complete", 0
szTestResultTitle db "Phase 3 Test Results", 0

; Test names
szTest1Name db "Drive Enumeration", 0
szTest2Name db "File Enumeration", 0
szTest3Name db "Path Validation", 0
szTest4Name db "Logging System Active", 0
szTest5Name db "Log File Creation", 0
szTest6Name db "Performance Monitor Startup", 0
szTest7Name db "Sampling Collection", 0
szTest8Name db "Editor Initialization", 0

OnRunTests endp

; ============================================================================
; AllocateTabBuffer - Get pre-allocated buffer from pool for tab
; ============================================================================
AllocateTabBuffer proc tabIndex:DWORD
    LOCAL bufferOffset:DWORD
    LOCAL bufferSize:DWORD
    
    ; Tab buffer size = pool_size / MAX_TABS = 524288 / 32 = 16384 bytes per tab
    mov eax, dwTabPoolSize
    mov ecx, MAX_TABS
    xor edx, edx
    div ecx
    mov bufferSize, eax
    
    ; Calculate offset: tabIndex * buffer_size
    mov eax, tabIndex
    imul eax, bufferSize
    mov bufferOffset, eax
    
    ; Return pointer: pool_base + offset
    mov eax, MemoryPool_TabBuffer
    add eax, bufferOffset
    
    ret
AllocateTabBuffer endp

; ============================================================================
; UseFileBufferPool - Get reference to file buffer pool (avoid malloc)
; ============================================================================
UseFileBufferPool proc
    ; Return pointer to pre-allocated file buffer
    mov eax, MemoryPool_FileBuffer
    ret
UseFileBufferPool endp

; ============================================================================
; OnTabChange - Enhanced with pool-based buffers
    
    ; Remember previous tab and persist its contents
    mov eax, dwCurrentTab
    mov prevIndex, eax
    cmp eax, MAX_TABS
    jae @SkipSave
    
    ; Compute slot pointer for previous tab
    mov ecx, eax
    imul ecx, 4
    mov edx, offset TabBuffers
    add edx, ecx
    
    ; Free any prior buffer
    mov eax, [edx]
    test eax, eax
    jz @SaveText
    push eax
    call GlobalFree
@SaveText:
    ; Capture current editor text
    invoke GetWindowTextLength, hEditor
    mov textLen, eax
    inc textLen
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, textLen
    mov pBuffer, eax
    test eax, eax
    jz @SkipSave
    invoke GetWindowText, hEditor, pBuffer, textLen
    mov [edx], pBuffer

@SkipSave:
    ; Get new tab selection
    invoke SendMessage, hTabControl, TCM_GETCURSEL, 0, 0
    mov tabIndex, eax
    
    ; Update status bar
    invoke wsprintf, addr buffer, addr szTabChangeFmt, tabIndex
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr buffer
    
    ; Load buffer for selected tab (or clear)
    mov dwCurrentTab, tabIndex
    cmp tabIndex, MAX_TABS
    jae @Exit
    mov ecx, tabIndex
    imul ecx, 4
    mov eax, offset TabBuffers
    add eax, ecx
    mov eax, [eax]
    test eax, eax
    jz @ClearEditor
    invoke SetWindowText, hEditor, eax
    jmp @Exit

@ClearEditor:
    invoke SetWindowText, hEditor, addr szEmptyText

@Exit:
    ret
    
szTabChangeFmt db "Tab %d selected", 0
OnTabChange endp

OnTreeSelChange proc
    LOCAL hItem:DWORD
    LOCAL tvi:TV_ITEM
    LOCAL buffer[MAX_PATH]:BYTE
    LOCAL szFullPath[MAX_PATH]:BYTE
    LOCAL dwAttrs:DWORD
    LOCAL hFile:HANDLE
    LOCAL fileSize:DWORD
    LOCAL bytesRead:DWORD
    LOCAL pFileBuffer:DWORD
    
    ; Get selected item
    invoke SendMessage, hFileTree, TVM_GETNEXTITEM, TVGN_CARET, 0
    mov hItem, eax
    test eax, eax
    jz @Exit
    
    ; Get item text
    mov tvi.imask, TVIF_TEXT
    mov tvi.hItem, eax
    lea eax, buffer
    mov tvi.pszText, eax
    mov tvi.cchTextMax, MAX_PATH
    invoke SendMessage, hFileTree, TVM_GETITEM, 0, addr tvi
    
    ; Update status bar with the item label
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr buffer
    
    ; Resolve the full path for this tree item
    push MAX_PATH
    lea ecx, szFullPath
    push ecx
    push hItem
    call GetItemPath
    
    ; Skip directories
    invoke GetFileAttributes, addr szFullPath
    mov dwAttrs, eax
    cmp eax, 0FFFFFFFFh
    je @Exit
    test eax, FILE_ATTRIBUTE_DIRECTORY
    jnz @Exit
    
    ; Load file contents into the editor
    invoke CreateFile, addr szFullPath, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @Exit
    mov hFile, eax
    
    invoke GetFileSize, hFile, NULL
    mov fileSize, eax
    add eax, 1
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, eax
    mov pFileBuffer, eax
    test eax, eax
    jz @CloseFile
    
    invoke ReadFile, hFile, pFileBuffer, fileSize, addr bytesRead, NULL
    invoke SetWindowText, hEditor, pFileBuffer
    invoke lstrcpy, addr szFileName, addr szFullPath
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr szFullPath
    invoke GlobalFree, pFileBuffer
    
@CloseFile:
    invoke CloseHandle, hFile
    
@Exit:
    ret
OnTreeSelChange endp

; ============================================================================
; OnTreeItemExpanding - Handle tree item expansion
; ============================================================================
OnTreeItemExpanding proc pNMHDR:DWORD
    LOCAL pnmtv:DWORD
    LOCAL szPath[MAX_PATH]:BYTE
    
    mov eax, pNMHDR
    mov pnmtv, eax
    
    ; Check if expanding (not collapsing)
    mov eax, pnmtv
    assume eax:ptr NMTREEVIEW
    test [eax].action, TVE_EXPAND
    jz @Exit
    
    ; Get the item handle
    mov eax, [eax].itemNew.hItem
    assume eax:nothing
    
    ; Get full path for this item
    push MAX_PATH
    lea ecx, szPath
    push ecx
    push eax  ; hItem
    call GetItemPath
    
    ; Start async enumeration for this folder
    lea eax, szPath
    push eax                ; pszPath
    mov eax, pnmtv
    assume eax:ptr NMTREEVIEW
    push [eax].itemNew.hItem ; hParent
    push hFileTree           ; hTreeControl
    call FileEnumeration_EnumerateAsync
    assume eax:nothing
    
@Exit:
    xor eax, eax  ; Allow expansion
    ret
OnTreeItemExpanding endp

OnAgenticComplete proc wParam:DWORD, lParam:DWORD
    LOCAL buffer[256]:BYTE
    
    ; Update success/failure counts
    test wParam, wParam
    jz @Failure
    
    inc nSuccessCount
    invoke wsprintf, addr buffer, addr szAgenticSuccess, nSuccessCount
    jmp @UpdateStatus
    
@Failure:
    inc nFailureCount
    invoke wsprintf, addr buffer, addr szAgenticFailure, nFailureCount
    
@UpdateStatus:
    ; Update progress bar
    inc nTotalExecutions
    invoke SendMessage, hProgressBar, PBM_SETPOS, nTotalExecutions, 0
    
    ; Update status bar
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr buffer
    
    ; Append to orchestra panel
    invoke SendMessage, hOrchestraPanel, EM_REPLACESEL, FALSE, addr buffer
    
    ret
    
szAgenticSuccess db "Success: %d completions", 13, 10, 0
szAgenticFailure db "Failure: %d errors", 13, 10, 0
OnAgenticComplete endp

OnToolExecute proc wParam:DWORD, lParam:DWORD
    LOCAL buffer[256]:BYTE
    LOCAL toolID:DWORD
    
    mov eax, wParam
    mov toolID, eax
    
    ; Format tool execution message
    invoke wsprintf, addr buffer, addr szToolExecuteFmt, toolID
    
    ; Display in orchestra panel
    invoke SendMessage, hOrchestraPanel, EM_REPLACESEL, FALSE, addr buffer
    
    ; Update status
    invoke SendMessage, hStatusBar, SB_SETTEXT, 1, addr buffer
    
    ; Execute a small set of built-in tool hooks
    .if toolID == IDM_VIEW_REFRESH_TREE
        call RefreshFileTree
    .elseif toolID == IDM_VIEW_HEX_INSPECT
        call OnShowHexInspector
    .elseif toolID == IDM_TOOLS_COMPRESS || toolID == IDM_FILE_COMPRESS_INFO
        call OnToolsCompress
    .elseif toolID == IDM_VIEW_FLOATING
        call OnToggleFloatingPanel
    .elseif toolID == IDM_AGENTIC_WISH
        call OnAgenticWish
    .elseif toolID == IDM_AGENTIC_LOOP
        call OnAgenticLoop
    .endif

    ; Log the execution for diagnostics
    invoke LogMessage, 1, addr buffer
    
    ret
    
szToolExecuteFmt db "Executing tool ID: %d", 13, 10, 0
OnToolExecute endp

OnFileCompressInfo proc
    LOCAL szStats[512]:BYTE
    
    ; Get compression statistics
    invoke Compression_GetStatistics, addr szStats, 512
    
    ; Show statistics in a message box
    invoke MessageBox, hMainWindow, addr szStats, addr szCompressTitle, MB_OK or MB_ICONINFORMATION
    
    ret
OnFileCompressInfo endp

; ============================================================================
; OptimizedFileEnumeration_CachedEnum - Batch enumerate with caching
; ============================================================================
OptimizedFileEnumeration_CachedEnum proc pszPath:DWORD, hParentItem:DWORD, hTreeControl:DWORD
    LOCAL hFindFile:HANDLE
    LOCAL findData:WIN32_FIND_DATA
    LOCAL itemCount:DWORD
    LOCAL qwStartTime:QWORD
    
    ; Start performance measurement
    invoke QueryPerformanceCounter
    mov dword ptr qwStartTime, eax
    mov dword ptr [qwStartTime + 4], edx
    
    ; Open directory with FindFileEx
    lea eax, findData
    invoke FindFirstFileEx, pszPath, FindExInfoBasic, addr findData, 
        FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH
    cmp eax, INVALID_HANDLE_VALUE
    je @Exit
    mov hFindFile, eax
    
    mov itemCount, 0
    
@@EnumLoop:
    ; Load max items per update (lazy loading)
    cmp itemCount, dwMaxTreeItemsPerUpdate
    jge @FinishEnum
    
    ; Check if folder or file
    test findData.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY
    
    ; Skip "." and ".."
    lea eax, findData.cFileName
    cmp byte ptr [eax], '.'
    je @NextFile
    
    ; Add to tree view
    mov eax, itemCount
    inc itemCount
    
    ; Continue enumeration
@NextFile:
    invoke FindNextFile, hFindFile, addr findData
    test eax, eax
    jnz @EnumLoop
    
@FinishEnum:
    invoke FindClose, hFindFile
    
    ; Log enumeration time
    invoke QueryPerformanceCounter
    ; Time = current - start (simplified, would need freq division)
    inc dwFileEnumCount
    
@Exit:
    ret
OptimizedFileEnumeration_CachedEnum endp

; ============================================================================
; UpdateStatusBarFPS - Batch status bar update with FPS
; ============================================================================
UpdateStatusBarFPS proc
    LOCAL szFpsText[32]:BYTE
    
    ; Only update every 500ms to reduce message overhead
    mov eax, dwLastFpsUpdateTick
    cmp eax, 0
    jne @CheckInterval
    
    mov dwLastFpsUpdateTick, dwFrameStartTick
    jmp @Update
    
@CheckInterval:
    mov eax, dwFrameStartTick
    sub eax, dwLastFpsUpdateTick
    cmp eax, 500
    jl @Exit
    
    mov dwLastFpsUpdateTick, dwFrameStartTick
    
@Update:
    ; Format FPS string
    invoke wsprintf, addr szFpsText, addr szFpsFmt, dwCurrentFps
    
    ; Update status bar part 0 (FPS)
    invoke SendMessage, hStatusBar, SB_SETTEXT, 0, addr szFpsText
    
@Exit:
    ret
UpdateStatusBarFPS endp

; ============================================================================
; SetupStatusBarParts - Configure status bar parts (FPS, bitrate, tokens, mem, status)
; ============================================================================
SetupStatusBarParts proc
    LOCAL parts[5]:DWORD
    
    ; Static widths for first four, last stretches
    mov parts[0*4], 120
    mov parts[1*4], 260
    mov parts[2*4], 360
    mov parts[3*4], 500
    mov parts[4*4], -1
    
    invoke SendMessage, hStatusBar, SB_SETPARTS, 5, addr parts
    ret
SetupStatusBarParts endp

; ============================================================================
; OnTreeItemExpandingLazy - Lazy-load tree items on-demand
; ============================================================================
OnTreeItemExpandingLazy proc pNMHDR:DWORD
    LOCAL pnmtv:DWORD
    LOCAL szPath[MAX_PATH]:BYTE
    LOCAL qwStart:QWORD
    
    mov eax, pNMHDR
    mov pnmtv, eax
    
    ; Check if lazy loading enabled
    cmp bLazyLoadEnabled, 0
    je @StandardExpand
    
    ; Check if expanding
    mov eax, pnmtv
    assume eax:ptr NMTREEVIEW
    test [eax].action, TVE_EXPAND
    jz @Exit
    assume eax:nothing
    
    ; Start timing
    invoke QueryPerformanceCounter
    mov dword ptr qwStart, eax
    mov dword ptr [qwStart + 4], edx
    
    ; Get item path
    mov eax, pnmtv
    assume eax:ptr NMTREEVIEW
    mov eax, [eax].itemNew.hItem
    assume eax:nothing
    
    push MAX_PATH
    lea ecx, szPath
    push ecx
    push eax
    call GetItemPath
    
    ; Start limited enumeration (max 100 items initially)
    lea eax, szPath
    push eax
    mov eax, pnmtv
    assume eax:ptr NMTREEVIEW
    push [eax].itemNew.hItem
    assume eax:nothing
    push hFileTree
    call OptimizedFileEnumeration_CachedEnum
    
    ; Log timing
    invoke QueryPerformanceCounter
    ; Would subtract start time here
    
@Exit:
    xor eax, eax
    ret
    
@StandardExpand:
    ; Fall back to standard enumeration
    push pNMHDR
    call OnTreeItemExpanding
    ret
OnTreeItemExpandingLazy endp

; ============================================================================
; FastStringCopy - Optimized string copy (vs lstrcpy)
; ============================================================================
FastStringCopy proc pszDest:DWORD, pszSrc:DWORD
    mov esi, pszSrc
    mov edi, pszDest
    
@@CopyLoop:
    lodsb
    stosb
    test al, al
    jnz @CopyLoop
    
    ret
FastStringCopy endp

; ============================================================================
; FastStringLength - Get string length (vs lstrlen)
; ============================================================================
FastStringLength proc pszStr:DWORD
    mov esi, pszStr
    xor ecx, ecx
    
@@LenLoop:
    mov al, [esi + ecx]
    test al, al
    jz @LenDone
    inc ecx
    cmp ecx, MAX_PATH
    jl @LenLoop
    
@LenDone:
    mov eax, ecx
    ret
FastStringLength endp

; ============================================================================
; FastStringConcat - Optimized string concatenation
; ============================================================================
FastStringConcat proc pszDest:DWORD, pszSrc:DWORD
    ; Find end of destination
    mov esi, pszDest
    xor ecx, ecx
    
@@FindEnd:
    mov al, [esi + ecx]
    test al, al
    jz @FoundEnd
    inc ecx
    cmp ecx, MAX_PATH
    jl @FindEnd
    
@FoundEnd:
    ; Append source
    mov edi, pszDest
    add edi, ecx
    mov esi, pszSrc
    
@@AppendLoop:
    lodsb
    stosb
    test al, al
    jnz @AppendLoop
    
    ret
FastStringConcat endp

; ============================================================================
; Program Entry Point
; ============================================================================

OnRefreshFileTree proc
    call RefreshFileTree
    ret
OnRefreshFileTree endp

OnFileSaveAs proc
    ; Force a Save As dialog even when a filename already exists
    mov byte ptr [szFileName], 0
    call OnFileSave
    ret
OnFileSaveAs endp

OnHelpAbout proc
    invoke MessageBox, hMainWindow, addr szAboutText, addr szAboutTitle, MB_OK or MB_ICONINFORMATION
    ret
OnHelpAbout endp

@

; ============================================================================
; Minimal stubbed WinMain/WndProc to allow successful build with stubs
; ============================================================================

.code

WndProc proc hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    .if uMsg == WM_DESTROY
        invoke PostQuitMessage, 0
        xor eax, eax
        ret
    .endif
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
WndProc endp

WinMain proc
    LOCAL wc:WNDCLASSEX
    LOCAL msg:MSG

    invoke GetModuleHandle, NULL
    mov wc.hInstance, eax
    mov wc.cbSize, sizeof WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    mov wc.lpfnWndProc, OFFSET WndProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    invoke LoadIcon, NULL, IDI_APPLICATION
    mov wc.hIcon, eax
    invoke LoadCursor, NULL, IDC_ARROW
    mov wc.hCursor, eax
    invoke GetStockObject, WHITE_BRUSH
    mov wc.hbrBackground, eax
    mov wc.lpszMenuName, NULL
    mov wc.lpszClassName, OFFSET szClassName
    mov wc.hIconSm, 0
    invoke RegisterClassEx, ADDR wc

    invoke CreateWindowEx, 0, OFFSET szClassName, OFFSET szMainWindowTitle,
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
            NULL, NULL, wc.hInstance, NULL
    mov hMainWindow, eax
    invoke ShowWindow, hMainWindow, SW_SHOWDEFAULT
    invoke UpdateWindow, hMainWindow

    .while TRUE
        invoke GetMessage, ADDR msg, NULL, 0, 0
        test eax, eax
        jz @exit
        invoke TranslateMessage, ADDR msg
        invoke DispatchMessage, ADDR msg
    .endw

@exit:
    mov eax, msg.wParam
    ret
WinMain endp

start:
    call WinMain
    invoke ExitProcess, eax

end start
