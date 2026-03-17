; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_JSON_Parser.asm
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

includelib \masm64\lib64\kernel32.lib

.CODE

; Dummy entry to ensure object file creation
JsonParser_Init PROC FRAME
    mov rax, 1
    ret
JsonParser_Init ENDP

PUBLIC JsonParser_Init

END
