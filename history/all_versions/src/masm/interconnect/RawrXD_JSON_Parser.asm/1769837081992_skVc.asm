; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_JSON_Parser.asm
; Stub implementation for JSON Parser
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

.CODE

; Add any exports if needed later
JsonParser_Parse PROC FRAME
    mov rax, 1
    ret
JsonParser_Parse ENDP

PUBLIC JsonParser_Parse

END