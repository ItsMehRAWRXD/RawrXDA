; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_JSON_Parser.asm
; Fast SIMD-accelerated JSON scanner
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
include RawrXD_Defs.inc

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; Json_ParseFast
; RCX = Input String, RDX = Length
; ═══════════════════════════════════════════════════════════════════════════════
Json_ParseFast PROC FRAME
    .endprolog
    ; Stub for JSON parsing
    ; Returns root node or error
    xor rax, rax
    ret
Json_ParseFast ENDP

PUBLIC Json_ParseFast

END
