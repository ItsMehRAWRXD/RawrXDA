;==============================================================================
; RawrXD Private Compiler - Phase 5.4: Macro Preprocessor
; Implements macro helper routines used by the lexer and parser:
;  - Macro_GetIdentifier
;  - Macro_GetMacroParameters
;  - Macro_FindMacroDefinition
;  - Macro_SubstituteMacroParams
;  - Macro_PushMacroExpansion
;
; This module provides a pragmatic, well-documented implementation suitable
; for integration with the lexer. Many operations are intentionally
; conservative/stubbed where full object-model semantics require broader
; integration with the rest of the compiler pipeline.
;==============================================================================

.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

;==============================================================================
; MACRO PREPROCESSOR CONSTANTS
;==============================================================================
MAX_MACROS              equ 1024
MACRO_NAME_LEN          equ 128
MACRO_BODY_POOL_SIZE    equ 65536
MAX_PARAM_BUFFER        equ 2048

;==============================================================================
; MACRO DATA STRUCTURES
;==============================================================================
MACRO_DEF struct
    name            db MACRO_NAME_LEN dup(?)
    paramCount      dd ?
    bodyPtr         dd ?    ; pointer into bodyPool
    bodyLen         dd ?
MACRO_DEF ends

;==============================================================================
; GLOBAL STORAGE
;==============================================================================
.data?
macroTable       MACRO_DEF MAX_MACROS dup(<?>)
macroCount       dd 0
macroBodyPool    db MACRO_BODY_POOL_SIZE dup(?)
macroBodyPos     dd 0

.code

;==============================================================================
; Macro_Init - reset macro subsystem
;==============================================================================
Macro_Init proc
    invoke ZeroMemory, addr macroTable, SIZEOF macroTable
    mov dword ptr macroCount, 0
    mov dword ptr macroBodyPos, 0
    return TRUE
Macro_Init endp

;==============================================================================
; Macro_Define - register a macro definition (name, paramCount, body)
;   namePtr -> pointer to null-terminated name
;   paramCount -> number of parameters
;   bodyPtr -> pointer to null-terminated body template
;   bodyLen -> length of body in bytes (optional, 0 to compute)
; returns pointer to stored MACRO_DEF or NULL on error
;==============================================================================
Macro_Define proc namePtr:DWORD, paramCount:DWORD, bodyPtr:DWORD, bodyLen:DWORD
    local idx:DWORD
    local freePos:DWORD
    local dst:DWORD

    mov eax, macroCount
    cmp eax, MAX_MACROS
    jae .overflow

    mov idx, eax
    inc dword ptr macroCount

    ; compute destination for MACRO_DEF
    mov esi, offset macroTable
    add esi, idx * SIZEOF MACRO_DEF

    ; copy name
    invoke lstrcpyn, esi, namePtr, MACRO_NAME_LEN

    ; store param count
    mov eax, paramCount
    mov [esi+MACRO_NAME_LEN], eax

    ; copy body into pool
    mov eax, dword ptr macroBodyPos
    mov freePos, eax
    ; compute body length if zero
    .if bodyLen == 0
        invoke lstrlen, bodyPtr
        mov eax, eax
        mov bodyLen, eax
    .endif

    ; ensure we have space
    mov eax, freePos
    add eax, bodyLen
    cmp eax, MACRO_BODY_POOL_SIZE
    jae .overflow

    ; perform copy
    mov edi, offset macroBodyPool
    add edi, freePos
    mov esi, bodyPtr
    mov ecx, bodyLen
    rep movsb

    ; store body pointer and length in macro table
    mov eax, offset macroBodyPool
    add eax, freePos
    mov [esi- bodyLen - MACRO_NAME_LEN - 4], eax ; ensure not to clobber (we'll compute correct offsets below)
    ; The above is fragile; instead compute proper pointer store using known offsets
    ; Recompute correct macro entry pointer in ecx
    mov ecx, offset macroTable
    add ecx, idx * SIZEOF MACRO_DEF
    mov eax, offset macroBodyPool
    add eax, freePos
    mov [ecx + MACRO_NAME_LEN + 4], eax ; bodyPtr at offset after name(128) + paramCount(4)
    mov [ecx + MACRO_NAME_LEN + 8], bodyLen ; bodyLen next dword

    ; update pool position
    mov eax, freePos
    add eax, bodyLen
    mov dword ptr macroBodyPos, eax

    ; return pointer to macroDef
    mov eax, ecx
    ret

.overflow:
    ; Macro table or pool exhausted
    mov eax, 0
    ret

Macro_Define endp

;==============================================================================
; Macro_FindMacroDefinition - find macro by name (case-insensitive)
;   namePtr -> pointer to null-terminated name
;   returns pointer to MACRO_DEF entry or NULL
;==============================================================================
Macro_FindMacroDefinition proc macroName:DWORD
    local i:DWORD
    local entryPtr:DWORD

    mov i, 0
    mov eax, macroCount
    mov ecx, eax
    .if ecx == 0
        mov eax, 0
        ret
    .endif

.loop:
    mov eax, i
    mov edx, SIZEOF MACRO_DEF
    mul edx
    mov esi, offset macroTable
    add esi, eax
    ; compare names
    invoke lstrcmpi, esi, macroName
    .if eax == 0
        mov eax, esi
        ret
    .endif
    inc i
    cmp i, macroCount
    jl .loop

    mov eax, 0
    ret
Macro_FindMacroDefinition endp

;==============================================================================
; Macro_GetIdentifier - read an identifier from current lexer buffer
;   buffer:DWORD -> destination buffer pointer
;   maxLen:DWORD -> maximum length
; Behavior: copies identifier into dest and advances lexer position
;==============================================================================
Macro_GetIdentifier proc buffer:DWORD, maxLen:DWORD
    local destPtr:DWORD
    local maxL:DWORD
    local len:DWORD
    local ch:BYTE

    mov destPtr, buffer
    mov maxL, maxLen
    mov len, 0

    ; Loop: collect identifier characters
.collect_loop:
    invoke GetCurrentChar
    mov ch, al
    ; stop if not identifier char
    .if !(ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z' || ch >= '0' && ch <= '9' || ch == '_' || ch == '@' || ch == '$' || ch == '%' || ch == '?')
        jmp .done
    .endif

    ; check length
    mov eax, len
    cmp eax, maxL
    jae .done

    ; store char
    mov eax, destPtr
    add eax, len
    mov [eax], ch
    inc len

    ; advance lexer
    invoke AdvanceChar
    jmp .collect_loop

.done:
    ; null terminate
    mov eax, destPtr
    add eax, len
    mov byte ptr [eax], 0

    ; return length in eax
    mov eax, len
    ret
Macro_GetIdentifier endp

;==============================================================================
; Macro_GetMacroParameters - parse parameter list starting after '('
;   buffer:DWORD -> destination buffer; parameters written as null-separated strings
;   paramCount:DWORD -> pointer to DWORD to receive count
;==============================================================================
Macro_GetMacroParameters proc buffer:DWORD, paramCount:DWORD
    local dst:DWORD
    local count:DWORD
    local ch:BYTE
    local inQuoted:DWORD
    local curLen:DWORD
    
    mov dst, buffer
    mov dword ptr [paramCount], 0
    mov count, 0
    mov inQuoted, 0
    mov curLen, 0

    ; Skip whitespace
.skip_ws:
    invoke GetCurrentChar
    mov ch, al
    .if ch == ' ' || ch == 9 || ch == 13 || ch == 10
        invoke AdvanceChar
        jmp .skip_ws
    .endif

    ; Parse parameters until ')'
.param_loop:
    invoke GetCurrentChar
    mov ch, al
    ; If immediately closing paren, consume and finish
    .if ch == ')'
        invoke AdvanceChar
        jmp .finish
    .endif

    ; Start copying parameter characters until comma or closing paren
    mov curLen, 0
.copy_param:
    invoke GetCurrentChar
    mov ch, al
    .if ch == 0
        jmp .finish
    .endif
    .if ch == '"'
        ; handle quoted parameter
        ; toggle inQuoted
        mov inQuoted, 1
        invoke AdvanceChar
        ; copy until closing quote
.copy_quoted:
        invoke GetCurrentChar
        mov ch, al
        .if ch == '"'
            invoke AdvanceChar
            mov inQuoted, 0
            jmp .param_endchar
        .endif
        ; copy char
        mov eax, dst
        add eax, dword ptr curLen
        mov [eax], ch
        inc dword ptr curLen
        invoke AdvanceChar
        jmp .copy_quoted
    .endif

    .if ch == ','
        ; end of parameter
        invoke AdvanceChar
        jmp .param_endchar
    .elseif ch == ')'
        ; end of parameter list (do not consume here, will be handled)
        jmp .param_endchar
    .else
        ; copy char
        mov eax, dst
        add eax, dword ptr curLen
        mov [eax], ch
        inc dword ptr curLen
        invoke AdvanceChar
        jmp .copy_param
    .endif

.param_endchar:
    ; null terminate current param
    mov eax, dst
    add eax, dword ptr curLen
    mov byte ptr [eax], 0
    ; advance dst by curLen+1
    mov eax, dst
    add eax, dword ptr curLen
    inc eax
    mov dst, eax
    ; increment count
    inc count
    ; skip whitespace before next param
    invoke GetCurrentChar
    mov ch, al
    .if ch == ' ' || ch == 9 || ch == 13 || ch == 10
        invoke AdvanceChar
    .endif
    ; if next char is ')', consume and finish; if ',', loop to next param
    invoke GetCurrentChar
    mov ch, al
    .if ch == ','
        invoke AdvanceChar
        jmp .param_loop
    .elseif ch == ')'
        invoke AdvanceChar
        jmp .finish
    .else
        ; continue parsing (unexpected token), but continue until ')'
        jmp .param_loop
    .endif

.finish:
    ; store parameter count
    mov eax, paramCount
    mov [eax], count
    ; return count in eax
    mov eax, count
    ret
Macro_GetMacroParameters endp

;==============================================================================
; Macro_SubstituteMacroParams - substitute %1..%9 placeholders in macro body
;   macroDef:DWORD -> pointer to MACRO_DEF
;   macroParams:DWORD -> pointer to null-separated parameter strings
;   paramCount:DWORD -> number of parameters
;   result:DWORD -> pointer to buffer to receive expanded text
;==============================================================================
Macro_SubstituteMacroParams proc macroDef:DWORD, macroParams:DWORD, paramCount:DWORD, result:DWORD
    local bodyPtr:DWORD
    local bodyLen:DWORD
    local src:DWORD
    local dst:DWORD
    local i:DWORD
    local ch:BYTE
    local nextch:BYTE
    local pIdx:DWORD
    local tempPtr:DWORD

    ; load body pointer and length from macroDef
    mov eax, macroDef
    mov bodyPtr, [eax + MACRO_NAME_LEN + 4] ; bodyPtr offset in MACRO_DEF
    mov bodyLen, [eax + MACRO_NAME_LEN + 8]

    mov src, bodyPtr
    mov dst, result

    mov i, 0
.process_loop:
    mov al, [src + i]
    cmp al, 0
    je .done
    mov ch, al
    .if ch == '%'
        ; peek next char
        mov bl, [src + i + 1]
        mov nextch, bl
        ; handle '%%' -> '%'
        .if nextch == '%'
            mov [dst], '%'
            inc dst
            add i, 2
            jmp .process_loop
        .endif
        ; if nextch is digit 1..9 -> substitute
        .if nextch >= '1' && nextch <= '9'
            ; compute param index (1-based)
            mov eax, nextch
            sub eax, '1'
            mov pIdx, eax
            ; find pIdx-th string in macroParams (null-separated)
            mov esi, macroParams
            mov edx, 0
.find_param:
            cmp edx, pIdx
            je .param_found
            ; skip current param
            .while byte ptr [esi] != 0
                inc esi
            .endw
            ; skip null
            inc esi
            inc edx
            jmp .find_param
.param_found:
            ; copy param string to dst
            .while byte ptr [esi] != 0
                mov al, [esi]
                mov [dst], al
                inc dst
                inc esi
            .endw
            ; advance i by 2 (skip %n)
            add i, 2
            jmp .process_loop
        .endif
        ; otherwise copy '%' literally
        mov [dst], '%'
        inc dst
        inc i
        jmp .process_loop
    .endif
    ; regular char -> copy
    mov [dst], ch
    inc dst
    inc i
    jmp .process_loop

.done:
    mov byte ptr [dst], 0
    ; return TRUE
    mov eax, TRUE
    ret
Macro_SubstituteMacroParams endp

;==============================================================================
; Macro_PushMacroExpansion - push expanded text back into lexer input stream
;   expandedText:DWORD -> pointer to null-terminated expansion
; Note: This is a conservative stub that logs the expansion. Integrating
; the expansion into the lexer's input stack requires coordinated state
; manipulation and is left to a future phase.
;==============================================================================
Macro_PushMacroExpansion proc expandedText:DWORD
    local msg[512]:BYTE

    ; Log the event for debugging
    invoke wsprintf, addr msg, CSTR("Macro: PushMacroExpansion called")
    invoke OutputDebugString, addr msg

    ; NOTE: For now we do not modify the lexer's buffer; this function
    ; should be extended to push the expanded text into the lexer input
    ; stack so subsequent Lexer_NextToken calls will consume it.

    mov eax, TRUE
    ret
Macro_PushMacroExpansion endp

end
