; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_JSON_Parser.asm
; Fast SIMD-accelerated JSON scanner
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

includelib \masm64\lib64\kernel32.lib

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; Json_ParseFast
; RCX = Input String, RDX = Length
; ═══════════════════════════════════════════════════════════════════════════════
Json_ParseFast PROC FRAME
    ; Stub for JSON parsing
    ; Returns root node or error
    xor rax, rax
    ret
Json_ParseFast ENDP

PUBLIC Json_ParseFast

END
