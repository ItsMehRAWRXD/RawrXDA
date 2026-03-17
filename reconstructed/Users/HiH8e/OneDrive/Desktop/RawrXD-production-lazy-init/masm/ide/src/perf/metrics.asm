; ============================================================================
; RawrXD Agentic IDE - Performance Metrics Module
; Real-time FPS, bitrate, and streaming stats display
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; External globals
extern g_hMainWindow:DWORD
extern g_hInstance:DWORD

; ============================================================================
; Constants
; ============================================================================

; Display configuration: 4K @ 540Hz
DISPLAY_WIDTH       equ 3840
DISPLAY_HEIGHT      equ 2160
DISPLAY_REFRESH_HZ  equ 540

; Performance sampling
SAMPLE_INTERVAL_MS  equ 16      ; ~60 samples/sec for smooth display
FPS_HISTORY_SIZE    equ 120     ; 2 seconds of history at 60Hz sampling
BITRATE_HISTORY_SIZE equ 60     ; 1 second of history

; Status bar part indices
SB_PART_FPS         equ 0
SB_PART_BITRATE     equ 1
SB_PART_TOKENS      equ 2
SB_PART_MEMORY      equ 3
SB_PART_STATUS      equ 4

; ============================================================================
; Structures
; ============================================================================

PERF_METRICS struct
    ; Frame timing
    dwFrameCount        dd ?        ; Total frames rendered
    dwFrameTime         dd ?        ; Last frame time (microseconds)
    dwFPS               dd ?        ; Current FPS
    dwFPSAvg            dd ?        ; Average FPS over history
    dwFPSMin            dd ?        ; Minimum FPS in history
    dwFPSMax            dd ?        ; Maximum FPS in history
    
    ; Bitrate tracking
    qwBytesRead         dq ?        ; Total bytes read from model
    qwBytesWritten      dq ?        ; Total bytes written/processed
    dwBitrateMbps       dd ?        ; Current bitrate in Mbps
    dwBitrateAvg        dd ?        ; Average bitrate
    
    ; Token generation
    dwTokensGenerated   dd ?        ; Total tokens generated
    dwTokensPerSec      dd ?        ; Tokens per second
    dwTokenLatencyUs    dd ?        ; Token generation latency (microseconds)
    
    ; Memory usage
    dwMemoryUsedMB      dd ?        ; Memory used in MB
    dwMemoryPeakMB      dd ?        ; Peak memory usage in MB
    dwVRAMUsedMB        dd ?        ; VRAM used (if GPU)
    
    ; Timing
    qwStartTime         dq ?        ; Session start time (QPC)
    qwLastSampleTime    dq ?        ; Last sample time (QPC)
    qwPerfFrequency     dq ?        ; QPC frequency
    
    ; History buffers (pointers)
    pFPSHistory         dd ?        ; Pointer to FPS history buffer
    pBitrateHistory     dd ?        ; Pointer to bitrate history buffer
    dwHistoryIndex      dd ?        ; Current history index
PERF_METRICS ends

DISPLAY_CONFIG struct
    dwWidth             dd ?        ; Display width
    dwHeight            dd ?        ; Display height
    dwRefreshHz         dd ?        ; Refresh rate in Hz
    dwBitsPerPixel      dd ?        ; Color depth
    bFullscreen         dd ?        ; Fullscreen mode flag
    bVSync              dd ?        ; VSync enabled
    bHighRefresh        dd ?        ; High refresh rate mode
DISPLAY_CONFIG ends

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    ; Format strings for display
    szFPSFormat         db "FPS: %d (avg: %d)", 0
    szBitrateFormat     db "Bitrate: %d.%d Mbps", 0
    szTokensFormat      db "Tokens: %d/s", 0
    szMemoryFormat      db "Mem: %d MB", 0
    szStatusReady       db "Ready", 0
    szStatusLoading     db "Loading model...", 0
    szStatusStreaming   db "Streaming...", 0
    szStatusIdle        db "Idle", 0
    
    ; Display mode strings
    szDisplayMode       db "4K @ 540Hz", 0
    szDisplayConfig     db "Resolution: %dx%d @ %dHz", 0
    
    ; Timer ID
    TIMER_PERF_UPDATE   equ 1001
    
    ; Global metrics instance
    g_PerfMetrics       PERF_METRICS <>
    g_DisplayConfig     DISPLAY_CONFIG <>
    
    ; Status bar handle
    g_hStatusBar        dd 0
    
    ; Initialized flag
    g_bPerfInitialized  dd 0

.data?
    ; History buffers
    g_FPSHistory        dd FPS_HISTORY_SIZE dup(?)
    g_BitrateHistory    dd BITRATE_HISTORY_SIZE dup(?)
    
    ; Temporary string buffer
    szPerfBuffer        db 256 dup(?)

; ============================================================================
; CODE SECTION
; ============================================================================

.code

public PerfMetrics_Init
public PerfMetrics_Update
public PerfMetrics_GetFPS
public PerfMetrics_GetBitrate
public PerfMetrics_AddBytes
public PerfMetrics_AddToken
public PerfMetrics_SetStatusBar
public DisplayConfig_Init
public DisplayConfig_Apply

; ============================================================================
; PerfMetrics_Init - Initialize performance metrics system
; Returns: TRUE on success
; ============================================================================
PerfMetrics_Init proc
    LOCAL qwFreq:QWORD
    
    ; Get performance counter frequency
    lea eax, qwFreq
    push eax
    call QueryPerformanceFrequency
    
    ; Store frequency
    mov eax, dword ptr qwFreq
    mov dword ptr g_PerfMetrics.qwPerfFrequency, eax
    mov eax, dword ptr qwFreq+4
    mov dword ptr g_PerfMetrics.qwPerfFrequency+4, eax
    
    ; Get start time
    lea eax, g_PerfMetrics.qwStartTime
    push eax
    call QueryPerformanceCounter
    
    ; Initialize history pointers
    lea eax, g_FPSHistory
    mov g_PerfMetrics.pFPSHistory, eax
    lea eax, g_BitrateHistory
    mov g_PerfMetrics.pBitrateHistory, eax
    
    ; Zero out history buffers
    lea edi, g_FPSHistory
    mov ecx, FPS_HISTORY_SIZE
    xor eax, eax
    rep stosd
    
    lea edi, g_BitrateHistory
    mov ecx, BITRATE_HISTORY_SIZE
    xor eax, eax
    rep stosd
    
    ; Initialize metrics
    mov g_PerfMetrics.dwFrameCount, 0
    mov g_PerfMetrics.dwFPS, 0
    mov g_PerfMetrics.dwFPSAvg, 0
    mov g_PerfMetrics.dwFPSMin, 0FFFFFFFFh
    mov g_PerfMetrics.dwFPSMax, 0
    mov g_PerfMetrics.dwBitrateMbps, 0
    mov g_PerfMetrics.dwTokensGenerated, 0
    mov g_PerfMetrics.dwTokensPerSec, 0
    mov g_PerfMetrics.dwHistoryIndex, 0
    
    mov g_bPerfInitialized, 1
    
    mov eax, TRUE
    ret
PerfMetrics_Init endp

; ============================================================================
; PerfMetrics_Update - Update metrics (call each frame or on timer)
; ============================================================================
PerfMetrics_Update proc
    LOCAL qwNow:QWORD
    LOCAL qwDelta:QWORD
    LOCAL dwElapsedUs:DWORD
    LOCAL dwFPS:DWORD
    
    cmp g_bPerfInitialized, 0
    je @done
    
    ; Get current time
    lea eax, qwNow
    push eax
    call QueryPerformanceCounter
    
    ; Calculate delta from last sample
    mov eax, dword ptr qwNow
    sub eax, dword ptr g_PerfMetrics.qwLastSampleTime
    mov dword ptr qwDelta, eax
    mov eax, dword ptr qwNow+4
    sbb eax, dword ptr g_PerfMetrics.qwLastSampleTime+4
    mov dword ptr qwDelta+4, eax
    
    ; Convert to microseconds: (delta * 1000000) / freq
    ; Simplified: assume delta fits in 32 bits for typical frame times
    mov eax, dword ptr qwDelta
    mov edx, 1000000
    mul edx
    mov ecx, dword ptr g_PerfMetrics.qwPerfFrequency
    cmp ecx, 0
    je @skipFPS
    div ecx
    mov dwElapsedUs, eax
    mov g_PerfMetrics.dwFrameTime, eax
    
    ; Calculate FPS: 1000000 / elapsedUs
    cmp dwElapsedUs, 0
    je @skipFPS
    mov eax, 1000000
    xor edx, edx
    div dwElapsedUs
    mov dwFPS, eax
    mov g_PerfMetrics.dwFPS, eax
    
    ; Update FPS history
    mov ecx, g_PerfMetrics.dwHistoryIndex
    mov eax, g_PerfMetrics.pFPSHistory
    mov edx, dwFPS
    mov [eax + ecx*4], edx
    
    ; Update min/max
    cmp edx, g_PerfMetrics.dwFPSMin
    jae @checkMax
    mov g_PerfMetrics.dwFPSMin, edx
@checkMax:
    cmp edx, g_PerfMetrics.dwFPSMax
    jbe @calcAvg
    mov g_PerfMetrics.dwFPSMax, edx
    
@calcAvg:
    ; Calculate average FPS from history
    xor eax, eax
    xor ecx, ecx
    mov esi, g_PerfMetrics.pFPSHistory
@avgLoop:
    cmp ecx, FPS_HISTORY_SIZE
    jge @avgDone
    add eax, [esi + ecx*4]
    inc ecx
    jmp @avgLoop
@avgDone:
    xor edx, edx
    mov ecx, FPS_HISTORY_SIZE
    div ecx
    mov g_PerfMetrics.dwFPSAvg, eax
    
@skipFPS:
    ; Update history index
    inc g_PerfMetrics.dwHistoryIndex
    cmp g_PerfMetrics.dwHistoryIndex, FPS_HISTORY_SIZE
    jl @noWrap
    mov g_PerfMetrics.dwHistoryIndex, 0
@noWrap:
    
    ; Store current time as last sample
    mov eax, dword ptr qwNow
    mov dword ptr g_PerfMetrics.qwLastSampleTime, eax
    mov eax, dword ptr qwNow+4
    mov dword ptr g_PerfMetrics.qwLastSampleTime+4, eax
    
    ; Increment frame count
    inc g_PerfMetrics.dwFrameCount
    
    ; Update status bar if available
    cmp g_hStatusBar, 0
    je @done
    call UpdateStatusBar
    
@done:
    ret
PerfMetrics_Update endp

; ============================================================================
; PerfMetrics_GetFPS - Get current FPS
; Returns: FPS in eax
; ============================================================================
PerfMetrics_GetFPS proc
    mov eax, g_PerfMetrics.dwFPS
    ret
PerfMetrics_GetFPS endp

; ============================================================================
; PerfMetrics_GetBitrate - Get current bitrate in Mbps
; Returns: Bitrate in eax
; ============================================================================
PerfMetrics_GetBitrate proc
    mov eax, g_PerfMetrics.dwBitrateMbps
    ret
PerfMetrics_GetBitrate endp

; ============================================================================
; PerfMetrics_AddBytes - Add bytes to bitrate tracking
; Input: dwBytes - number of bytes transferred
; ============================================================================
PerfMetrics_AddBytes proc dwBytes:DWORD
    ; Add to total bytes
    mov eax, dwBytes
    add dword ptr g_PerfMetrics.qwBytesRead, eax
    adc dword ptr g_PerfMetrics.qwBytesRead+4, 0
    
    ; Update bitrate (simplified: bytes * 8 / 1000000 for Mbps)
    ; This is approximate; real implementation would use time delta
    mov eax, dwBytes
    shl eax, 3          ; bytes to bits
    mov ecx, 125000     ; divide by 125000 to get Mbps (approx)
    xor edx, edx
    div ecx
    add g_PerfMetrics.dwBitrateMbps, eax
    
    ret
PerfMetrics_AddBytes endp

; ============================================================================
; PerfMetrics_AddToken - Record token generation
; Input: dwLatencyUs - token generation latency in microseconds
; ============================================================================
PerfMetrics_AddToken proc dwLatencyUs:DWORD
    inc g_PerfMetrics.dwTokensGenerated
    
    ; Update latency (moving average)
    mov eax, g_PerfMetrics.dwTokenLatencyUs
    add eax, dwLatencyUs
    shr eax, 1          ; Simple average
    mov g_PerfMetrics.dwTokenLatencyUs, eax
    
    ; Calculate tokens per second
    cmp dwLatencyUs, 0
    je @done
    mov eax, 1000000
    xor edx, edx
    div dwLatencyUs
    mov g_PerfMetrics.dwTokensPerSec, eax
    
@done:
    ret
PerfMetrics_AddToken endp

; ============================================================================
; PerfMetrics_SetStatusBar - Set status bar handle for updates
; Input: hStatusBar - handle to status bar control
; ============================================================================
PerfMetrics_SetStatusBar proc hStatusBar:DWORD
    mov eax, hStatusBar
    mov g_hStatusBar, eax
    ret
PerfMetrics_SetStatusBar endp

; ============================================================================
; UpdateStatusBar - Internal: Update status bar with current metrics
; ============================================================================
UpdateStatusBar proc
    LOCAL szTemp[64]:BYTE
    
    ; Update FPS part
    lea eax, szTemp
    push g_PerfMetrics.dwFPSAvg
    push g_PerfMetrics.dwFPS
    push offset szFPSFormat
    push eax
    call wsprintfA
    add esp, 16
    
    lea eax, szTemp
    push eax
    push SB_PART_FPS
    push 0
    push g_hStatusBar
    call SendMessageA
    
    ; Update bitrate part
    lea eax, szTemp
    ; Calculate decimal part (tenths)
    mov ecx, g_PerfMetrics.dwBitrateMbps
    mov edx, ecx
    and edx, 0Fh        ; Get lower 4 bits as decimal approx
    shr ecx, 4          ; Integer part
    push edx
    push ecx
    push offset szBitrateFormat
    push eax
    call wsprintfA
    add esp, 16
    
    lea eax, szTemp
    push eax
    push SB_PART_BITRATE
    push 0
    push g_hStatusBar
    call SendMessageA
    
    ; Update tokens part
    lea eax, szTemp
    push g_PerfMetrics.dwTokensPerSec
    push offset szTokensFormat
    push eax
    call wsprintfA
    add esp, 12
    
    lea eax, szTemp
    push eax
    push SB_PART_TOKENS
    push 0
    push g_hStatusBar
    call SendMessageA
    
    ; Update memory part
    lea eax, szTemp
    push g_PerfMetrics.dwMemoryUsedMB
    push offset szMemoryFormat
    push eax
    call wsprintfA
    add esp, 12
    
    lea eax, szTemp
    push eax
    push SB_PART_MEMORY
    push 0
    push g_hStatusBar
    call SendMessageA
    
    ret
UpdateStatusBar endp

; ============================================================================
; DisplayConfig_Init - Initialize display configuration for 4K @ 540Hz
; ============================================================================
DisplayConfig_Init proc
    ; Set up 4K @ 540Hz configuration
    mov g_DisplayConfig.dwWidth, DISPLAY_WIDTH
    mov g_DisplayConfig.dwHeight, DISPLAY_HEIGHT
    mov g_DisplayConfig.dwRefreshHz, DISPLAY_REFRESH_HZ
    mov g_DisplayConfig.dwBitsPerPixel, 32
    mov g_DisplayConfig.bFullscreen, FALSE
    mov g_DisplayConfig.bVSync, FALSE      ; Disable VSync for max refresh
    mov g_DisplayConfig.bHighRefresh, TRUE
    
    mov eax, TRUE
    ret
DisplayConfig_Init endp

; ============================================================================
; DisplayConfig_Apply - Apply display configuration to window
; Input: hWnd - window handle
; Returns: TRUE on success
; ============================================================================
DisplayConfig_Apply proc hWnd:DWORD
    LOCAL dm:DEVMODE
    LOCAL rc:RECT
    LOCAL dwStyle:DWORD
    LOCAL dwExStyle:DWORD
    
    ; Initialize DEVMODE
    lea edi, dm
    mov ecx, sizeof DEVMODE
    xor eax, eax
    rep stosb
    
    mov dm.dmSize, sizeof DEVMODE
    
    ; Set display mode
    mov eax, g_DisplayConfig.dwWidth
    mov dm.dmPelsWidth, eax
    mov eax, g_DisplayConfig.dwHeight
    mov dm.dmPelsHeight, eax
    mov eax, g_DisplayConfig.dwBitsPerPixel
    mov dm.dmBitsPerPel, eax
    mov eax, g_DisplayConfig.dwRefreshHz
    mov dm.dmDisplayFrequency, eax
    
    mov dm.dmFields, DM_PELSWIDTH or DM_PELSHEIGHT or DM_BITSPERPEL or DM_DISPLAYFREQUENCY
    
    ; Check if fullscreen mode requested
    cmp g_DisplayConfig.bFullscreen, TRUE
    jne @windowed
    
    ; Try to change display settings
    lea eax, dm
    push CDS_FULLSCREEN
    push eax
    call ChangeDisplaySettingsA
    cmp eax, DISP_CHANGE_SUCCESSFUL
    jne @windowed
    
    ; Set window style for fullscreen
    mov dwStyle, WS_POPUP or WS_VISIBLE
    mov dwExStyle, WS_EX_TOPMOST
    
    push dwStyle
    push GWL_STYLE
    push hWnd
    call SetWindowLongA
    
    push dwExStyle
    push GWL_EXSTYLE
    push hWnd
    call SetWindowLongA
    
    ; Position window at 0,0 with full resolution
    push SWP_SHOWWINDOW
    push 0
    push g_DisplayConfig.dwHeight
    push g_DisplayConfig.dwWidth
    push 0
    push 0
    push 0
    push hWnd
    call SetWindowPos
    
    jmp @done
    
@windowed:
    ; Windowed mode: set window size to match resolution
    mov rc.left, 0
    mov rc.top, 0
    mov eax, g_DisplayConfig.dwWidth
    mov rc.right, eax
    mov eax, g_DisplayConfig.dwHeight
    mov rc.bottom, eax
    
    ; Adjust for window borders
    push FALSE
    push WS_OVERLAPPEDWINDOW
    lea eax, rc
    push eax
    call AdjustWindowRect
    
    ; Calculate window dimensions
    mov eax, rc.right
    sub eax, rc.left
    mov ecx, eax            ; width
    mov eax, rc.bottom
    sub eax, rc.top
    mov edx, eax            ; height
    
    ; Center on screen
    push SM_CXSCREEN
    call GetSystemMetrics
    sub eax, ecx
    shr eax, 1
    mov rc.left, eax
    
    push SM_CYSCREEN
    call GetSystemMetrics
    sub eax, edx
    shr eax, 1
    mov rc.top, eax
    
    ; Set window position and size
    push SWP_SHOWWINDOW
    push 0
    mov eax, rc.bottom
    sub eax, rc.top
    push eax                ; height
    mov eax, rc.right
    sub eax, rc.left
    push eax                ; width
    push rc.top
    push rc.left
    push 0
    push hWnd
    call SetWindowPos
    
@done:
    mov eax, TRUE
    ret
DisplayConfig_Apply endp

; ============================================================================
; DisplayConfig_GetInfo - Get display config info string
; Input: pszBuffer - output buffer
; Returns: pointer to buffer
; ============================================================================
DisplayConfig_GetInfo proc pszBuffer:DWORD
    push g_DisplayConfig.dwRefreshHz
    push g_DisplayConfig.dwHeight
    push g_DisplayConfig.dwWidth
    push offset szDisplayConfig
    push pszBuffer
    call wsprintfA
    add esp, 20
    
    mov eax, pszBuffer
    ret
DisplayConfig_GetInfo endp

end
