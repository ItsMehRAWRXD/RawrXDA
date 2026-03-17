; Entry point stub for MASM Windows app
; Jumps to WinMain defined elsewhere

.686
.model flat, stdcall
option casemap:none

EXTERN WinMain@16:PROC

.code
PUBLIC WinMainCRTStartup
WinMainCRTStartup PROC
    jmp WinMain@16
WinMainCRTStartup ENDP

END