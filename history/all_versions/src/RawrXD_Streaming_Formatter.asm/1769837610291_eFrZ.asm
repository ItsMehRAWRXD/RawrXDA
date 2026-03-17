; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Streaming_Formatter.asm
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

includelib \masm64\lib64\kernel32.lib

.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; StreamFormatter_WriteToken
; RCX = completion port, RDX = data, R8 = length
; ═══════════════════════════════════════════════════════════════════════════════
StreamFormatter_WriteToken PROC FRAME
    mov rax, 1
    ret
StreamFormatter_WriteToken ENDP

PUBLIC StreamFormatter_WriteToken

END
