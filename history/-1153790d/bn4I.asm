; ============================================================================
; RawrXD Agentic IDE - Orchestra Panel Implementation Pure MASM
; Tool execution monitoring and control
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
; TODO: Add richedit support when available
; include \masm32\include\richedit.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
; includelib \masm32\lib\riched20.lib

; External declarations for main window, font, instance, and global instance handle
extern g_hMainWindow:DWORD
extern g_hMainFont:DWORD
extern hInstance:DWORD
extern g_hInstance:DWORD
extern szStatusFormat:DWORD
extern szToolOutputFormat:DWORD

; Procedure prototypes for Orchestra functions (forward references)
Orchestra_AppendStatus proto :DWORD
Orchestra_AppendToolOutput proto :DWORD, :DWORD
Orchestra_Start proto
Orchestra_Pause proto
Orchestra_Stop proto
Orchestra_Complete proto

; Include files moved inside .data section (will be placed after .data)

; ============================================================================
; DATA SECTION
; ============================================================================

 .data
    ; Include constants, structures, macros inside data segment
    include constants.inc
    include structures.inc
    include macros.inc

    szOrchestraClass   db "RichEdit20W", 0
    szEmptyTitle       db "", 0
    szButtonClass      db "BUTTON", 0
    szStartButton      db "Start", 0
    szPauseButton      db "Pause", 0
    szStopButton       db "Stop", 0
    szStatusRunning    db "Running", 0
    szStatusPaused     db "Paused", 0
    szStatusStopped    db "Stopped", 0
    szStatusComplete   db "Complete", 0
    szOrchestraWelcome db "Orchestra Panel Ready", 13, 10, 0
    
    ; Orchestra state
    g_hOrchestraPanel  dd 0
    g_hStartButton     dd 0
    g_hPauseButton     dd 0
    g_hStopButton      dd 0
    g_bOrchestraRunning dd 0
    g_bOrchestraPaused dd 0
    ; Additional data strings
    szOrchestraWelcome db "Orchestra Panel Ready", 13, 10
                      db "Use Start/Pause/Stop to control agentic execution.", 13, 10, 13, 10, 0
    szStatusFormat     db "[%d] %s", 13, 10, 0
    szToolOutputFormat db "%s: %s", 0

; ============================================================================
; PROCEDURES
; ============================================================================

.code

; ============================================================================
; CreateOrchestraPanel - Create orchestra panel
; Returns: Panel handle in eax
; ============================================================================
CreateOrchestraPanel proc
    LOCAL dwStyle:DWORD
    LOCAL hPanel:DWORD
    LOCAL hStart:DWORD
    LOCAL hPause:DWORD
    LOCAL hStop:DWORD
    
    ; Create orchestra display (RichEdit)
    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_AUTOVSCROLL or ES_READONLY
    
    CreateWnd szOrchestraClass, szEmptyTitle, dwStyle, 0, 0, 400, 200, g_hMainWindow, IDC_ORCHESTRA
    mov hPanel, eax
    mov g_hOrchestraPanel, eax

    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Set font
    invoke SendMessage, hPanel, WM_SETFONT, g_hMainFont, TRUE
    
    ; Create control buttons
    mov dwStyle, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    
    invoke CreateWindowEx, 0, addr szButtonClass, addr szStartButton, dwStyle, 0, 180, 60, 20, g_hMainWindow, 0, g_hInstance, NULL
    mov hStart, eax
    mov g_hStartButton, eax
    
    invoke CreateWindowEx, 0, addr szButtonClass, addr szPauseButton, dwStyle, 60, 180, 60, 20, g_hMainWindow, 0, g_hInstance, NULL
    mov hPause, eax
    mov g_hPauseButton, eax
    
    invoke CreateWindowEx, 0, addr szButtonClass, addr szStopButton, dwStyle, 120, 180, 60, 20, g_hMainWindow, 0, g_hInstance, NULL
    mov hStop, eax
    mov g_hStopButton, eax
    
    ; Initialize orchestra
    call Orchestra_Initialize
    
    mov eax, hPanel
    ret
CreateOrchestraPanel endp

; ============================================================================
; Orchestra_Initialize - Initialize orchestra panel
; ============================================================================
Orchestra_Initialize proc
    ; Set initial text directly
    invoke SendMessage, g_hOrchestraPanel, WM_SETTEXT, 0, addr szOrchestraWelcome
    
    ret
Orchestra_Initialize endp

; ============================================================================
; Orchestra_Start - Start orchestra execution
; ============================================================================
Orchestra_Start proc
    mov g_bOrchestraRunning, 1
    mov g_bOrchestraPaused, 0
    
    invoke Orchestra_AppendStatus, addr szStatusRunning
    
    ; Disable start button, enable pause/stop
    invoke EnableWindow, g_hStartButton, FALSE
    invoke EnableWindow, g_hPauseButton, TRUE
    invoke EnableWindow, g_hStopButton, TRUE
    
    ret
Orchestra_Start endp

; ============================================================================
; Orchestra_Pause - Pause orchestra execution
; ============================================================================
Orchestra_Pause proc
    .if g_bOrchestraPaused
        mov g_bOrchestraPaused, 0
        invoke Orchestra_AppendStatus, addr szStatusRunning
    .else
        mov g_bOrchestraPaused, 1
        invoke Orchestra_AppendStatus, addr szStatusPaused
    .endif
    
    ret
Orchestra_Pause endp

; ============================================================================
; Orchestra_Stop - Stop orchestra execution
; ============================================================================
Orchestra_Stop proc
    mov g_bOrchestraRunning, 0
    mov g_bOrchestraPaused, 0
    
    invoke Orchestra_AppendStatus, addr szStatusStopped
    
    ; Enable start button, disable pause/stop
    invoke EnableWindow, g_hStartButton, TRUE
    invoke EnableWindow, g_hPauseButton, FALSE
    invoke EnableWindow, g_hStopButton, FALSE
    
    ret
Orchestra_Stop endp

; ============================================================================
; Orchestra_Complete - Mark orchestra as complete
; ============================================================================
Orchestra_Complete proc
    mov g_bOrchestraRunning, 0
    mov g_bOrchestraPaused, 0
    
    invoke Orchestra_AppendStatus, addr szStatusComplete
    
    ; Enable start button, disable pause/stop
    invoke EnableWindow, g_hStartButton, TRUE
    invoke EnableWindow, g_hPauseButton, FALSE
    invoke EnableWindow, g_hStopButton, FALSE
    
    ret
Orchestra_Complete endp

; ============================================================================
; Orchestra_AppendStatus - Append status message
; Input: pszStatus
; ============================================================================
Orchestra_AppendStatus proc pszStatus:DWORD
    LOCAL szFullMessage db 512 dup(0)
    LOCAL dwTextLength:DWORD
    LOCAL dwCurrentLength:DWORD
    
    ; Build message with timestamp using lstrcpy and lstrcat
    invoke GetTickCount
    ; Build message with timestamp and status
    invoke wsprintfA, addr szFullMessage, addr szStatusFormat, eax, pszStatus
    
    ; Get current text length
    invoke SendMessage, g_hOrchestraPanel, WM_GETTEXTLENGTH, 0, 0
    mov dwCurrentLength, eax
    
    ; Get text to append length
    ; Get length of the full message
    invoke lstrlen, addr szFullMessage
    mov dwTextLength, eax
    
    ; Append text
    invoke SendMessage, g_hOrchestraPanel, EM_SETSEL, dwCurrentLength, dwCurrentLength
    invoke SendMessage, g_hOrchestraPanel, EM_REPLACESEL, FALSE, addr szFullMessage
    
    ; Scroll to bottom
    invoke SendMessage, g_hOrchestraPanel, EM_SCROLL, SB_BOTTOM, 0
    
    ret
Orchestra_AppendStatus endp

; ============================================================================
; Orchestra_AppendToolOutput - Append tool execution output
; Input: pszToolName, pszOutput
; ============================================================================
Orchestra_AppendToolOutput proc pszToolName:DWORD, pszOutput:DWORD
    LOCAL szFullMessage db 1024 dup(0)
    
    invoke wsprintfA, addr szFullMessage, addr szToolOutputFormat, pszToolName, pszOutput
    invoke Orchestra_AppendStatus, addr szFullMessage
    
    ret
Orchestra_AppendToolOutput endp

; ============================================================================
; Orchestra_OnStartButton - Handle start button click
; ============================================================================
Orchestra_OnStartButton proc
    invoke Orchestra_Start
    ret
Orchestra_OnStartButton endp

; ============================================================================
; Orchestra_OnPauseButton - Handle pause button click
; ============================================================================
Orchestra_OnPauseButton proc
    invoke Orchestra_Pause
    ret
Orchestra_OnPauseButton endp

; ============================================================================
; Orchestra_OnStopButton - Handle stop button click
; ============================================================================
Orchestra_OnStopButton proc
    invoke Orchestra_Stop
    ret
Orchestra_OnStopButton endp

; ============================================================================
; Orchestra_Cleanup - Cleanup orchestra resources
; ============================================================================
Orchestra_Cleanup proc
    ret
Orchestra_Cleanup endp

; ============================================================================
end
