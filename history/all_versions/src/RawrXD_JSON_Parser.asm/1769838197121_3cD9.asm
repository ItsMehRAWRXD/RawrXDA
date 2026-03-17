; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_JSON_Parser.asm
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
; OPTION WIN64:3

include windows.inc
include kernel32.inc

includelib kernel32.lib

.CODE

; Dummy entry to ensure object file creation
JsonParser_Init PROC FRAME
    mov rax, 1
    ret
JsonParser_Init ENDP

PUBLIC JsonParser_Init

END
