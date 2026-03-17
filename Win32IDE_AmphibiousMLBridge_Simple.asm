; ==============================================================================
; Win32IDE_AmphibiousMLBridge_Simple.asm
; Simplified bridge - remove complex dependencies
; ==============================================================================

.code

; Exported
public Win32IDE_InitializeML
public Win32IDE_StartInference  
public Win32IDE_StreamTokenToEditor
public Win32IDE_CommitTelemetry
public Win32IDE_CancelInference

; External
EXTERN SendMessageA@16:proc
EXTERN SetWindowTextA@8:proc
EXTERN GetWindowTextA@12:proc

; Constants
EM_SETSEL = 0xB1h
EM_REPLACESEL = 0x194h

; ============================================================================
; Win32IDE_InitializeML
; rcx = editor HWND, rdx = status bar HWND, r8 = model path
; ============================================================================
Win32IDE_InitializeML PROC
    xor eax, eax
    ret
Win32IDE_InitializeML ENDP

; ============================================================================
; Win32IDE_StartInference
; rcx = editor, rdx = code, r8 = prompt, r9 = output
; ============================================================================
Win32IDE_StartInference PROC
    xor eax, eax
    ret
Win32IDE_StartInference ENDP

; ============================================================================
; Win32IDE_StreamTokenToEditor
; rcx = editor, rdx = token, r8 = len, r9 = isDone
; ============================================================================
Win32IDE_StreamTokenToEditor PROC
    ; rdx = token to insert
    mov r12, rcx              ; r12 = hwnd
    
    ; Move to end: SendMessageA(hwnd, EM_SETSEL, -1, -1)
    mov rcx, r12
    mov rdx, 0xB1h            ; EM_SETSEL
    mov r8, -1
    mov r9, -1
    call SendMessageA@16
    
    ; Insert: SendMessageA(hwnd, EM_REPLACESEL, 0, token)
    mov rcx, r12
    mov rdx, 0x194h           ; EM_REPLACESEL
    xor r8, r8
    mov r9, [rsp + 32]        ; token from stack
    call SendMessageA@16
    
    xor eax, eax
    ret
Win32IDE_StreamTokenToEditor ENDP

; ============================================================================
; Win32IDE_CommitTelemetry
; rcx = filepath, rdx = tokencount, r8 = duration, r9 = success
; ============================================================================
Win32IDE_CommitTelemetry PROC
    xor eax, eax
    ret
Win32IDE_CommitTelemetry ENDP

; ============================================================================
; Win32IDE_CancelInference
; rcx = editor hwnd, rdx = original text
; ============================================================================
Win32IDE_CancelInference PROC
    mov r12, rcx
    mov rcx, r12
    mov rdx, rdx              ; rdx = original text
    call SetWindowTextA@8
    xor eax, eax
    ret
Win32IDE_CancelInference ENDP

.end
