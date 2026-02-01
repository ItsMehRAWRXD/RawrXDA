; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Streaming_Formatter.asm
; Stub implementation for Streaming Formatter
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

.CODE

StreamFormatter_WriteToken PROC FRAME
    ; RCX = Context, RDX = TokenData, R8 = Length
    mov rax, 1
    ret
StreamFormatter_WriteToken ENDP

PUBLIC StreamFormatter_WriteToken

END