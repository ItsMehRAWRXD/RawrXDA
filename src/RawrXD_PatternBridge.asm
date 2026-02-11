; ============================================================================
; RawrXD Pattern Bridge - Hybrid AVX-512 + Scalar Implementation
; Fast aligned scan (64 bytes) + accurate unaligned tail scan
; ============================================================================

OPTION CASemap:NONE

PUBLIC ClassifyPattern
PUBLIC InitializePatternEngine
PUBLIC ShutdownPatternEngine
PUBLIC GetPatternStats
PUBLIC DllMain

; Pattern types
PAT_UNKNOWN  EQU 0
PAT_BUG      EQU 1
PAT_FIXME    EQU 2
PAT_XXX      EQU 3
PAT_TODO     EQU 4
PAT_HACK     EQU 5
PAT_REVIEW   EQU 6
PAT_NOTE     EQU 7
PAT_IDEA     EQU 8

; Result struct offsets
RES_TYPE     EQU 0
RES_CONF     EQU 8
RES_LINE     EQU 16
RES_PRIO     EQU 20

.DATA
ALIGN 16

g_TotalCalls    DQ 0
g_AVX512Avail   DB 0

; 8 pattern signatures, 8 bytes each, packed into 64 bytes for ZMM
; Layout: "bug:....", "fixme...", "xxx:....", "todo....", etc. (lowercase for case-folded compare)
g_Patterns:
    DB 62H, 75H, 67H, 3AH, 00H, 00H, 00H, 00H  ; bug:
    DB 66H, 69H, 78H, 6DH, 65H, 3AH, 00H, 00H  ; fixme:
    DB 78H, 78H, 78H, 3AH, 00H, 00H, 00H, 00H  ; xxx:
        DB 74H, 6FH, 64H, 6FH, 3AH, 00H, 00H, 00H  ; todo:
        DB 68H, 61H, 63H, 6BH, 3AH, 00H, 00H, 00H  ; hack:
        DB 72H, 65H, 76H, 69H, 65H, 77H, 3AH, 00H  ; review:
        DB 6EH, 6FH, 74H, 65H, 3AH, 00H, 00H, 00H  ; note:
        DB 69H, 64H, 65H, 61H, 3AH, 00H, 00H, 00H  ; idea:

; Priority table (severity-based: BUG=10, FIXME/XXX=8, etc.)
g_Priorities:
    DB 0, 10, 8, 8, 5, 6, 4, 2, 1  ; Index 0-8 (UNKNOWN, BUG, FIXME, XXX, TODO, HACK, REVIEW, NOTE, IDEA)

; Case folding mask (0x20 for ASCII letters)
ALIGN 16
g_CaseMask:
    DB 64 DUP(20H)

.CODE

;-----------------------------------------------------------------------------
; DetectAVX512
;-----------------------------------------------------------------------------
DetectAVX512 PROC
    PUSH RBX
    MOV EAX, 7
    XOR ECX, ECX
    CPUID
    TEST EBX, 00010000H      ; AVX-512F bit
    JZ @@no_avx512
    XOR ECX, ECX
    XGETBV
    AND EAX, 0E6H            ; Check ZMM state
    CMP EAX, 0E6H
    JNE @@no_avx512
    MOV AL, 1
    POP RBX
    RET
@@no_avx512:
    XOR AL, AL
    POP RBX
    RET
DetectAVX512 ENDP

;-----------------------------------------------------------------------------
; ScanScalar - Byte-by-byte scan for unaligned/short buffers
; RCX = buffer, RDX = length, R8 = result struct
; Returns: RAX = pattern type (0-8)
;-----------------------------------------------------------------------------
ScanScalar PROC
    PUSH RBX
    PUSH R12
    PUSH R13
    
    XOR R12, R12              ; Best match type
    XOR R13, R13              ; Best match position
    
    CMP RDX, 4
    JB @@scalar_done          ; Too short for any pattern
    
    MOV R11, RDX
    SUB R11, 3                ; Last valid start position
    
@@scalar_loop:
    CMP R13, R11
    JAE @@scalar_done
    
    ; Load 4 bytes and case-fold
    MOV EAX, [RCX + R13]
    MOV R10D, 20202020H
    OR EAX, R10D
    
    ; Check each pattern prefix (little-endian: last char in high byte)
    CMP EAX, 3A677562H        ; "bug:" -> b(62) u(75) g(67) :(3A)
    JE @@found_bug
    CMP EAX, 6D786966H        ; "fixm" -> f(66) i(69) x(78) m(6D)
    JE @@found_fixme
    CMP EAX, 3A787878H        ; "xxx:" -> x(78) x(78) x(78) :(3A)
    JE @@found_xxx
    CMP EAX, 6F646F74H        ; "todo" -> t(74) o(6F) d(64) o(6F)
    JE @@found_todo
    CMP EAX, 6B636168H        ; "hack" -> h(68) a(61) c(63) k(6B)
    JE @@found_hack
    CMP EAX, 69766572H        ; "revi" -> r(72) e(65) v(76) i(69)
    JE @@found_review
    CMP EAX, 65746F6EH        ; "note" -> n(6E) o(6F) t(74) e(65)
    JE @@found_note
    CMP EAX, 61656469H        ; "idea" -> i(69) d(64) e(65) a(61)
    JE @@found_idea
    
    INC R13
    JMP @@scalar_loop
    
@@found_bug:
    MOV R12D, PAT_BUG
    JMP @@scalar_done
@@found_fixme:
    MOV R12D, PAT_FIXME
    JMP @@scalar_done
@@found_xxx:
    MOV R12D, PAT_XXX
    JMP @@scalar_done
@@found_todo:
    MOV R12D, PAT_TODO
    JMP @@scalar_done
@@found_hack:
    MOV R12D, PAT_HACK
    JMP @@scalar_done
@@found_review:
    MOV R12D, PAT_REVIEW
    JMP @@scalar_done
@@found_note:
    MOV R12D, PAT_NOTE
    JMP @@scalar_done
@@found_idea:
    MOV R12D, PAT_IDEA
    
@@scalar_done:
    ; Fallback: detect "bug" prefix even if colon missing
    CMP R12, 0
    JNE @@scalar_return
    CMP RDX, 3
    JB @@scalar_return
    MOV EAX, [RCX]
    MOV R10D, 20202020H
    OR EAX, R10D
    AND EAX, 00FFFFFFH
    CMP EAX, 00677562H        ; "bug" (lowercase)
    JNE @@scalar_return
    MOV R12D, PAT_BUG

@@scalar_return:
    MOV RAX, R12
    
    POP R13
    POP R12
    POP RBX
    RET
ScanScalar ENDP

;-----------------------------------------------------------------------------
; ScanAVX512 - Fast 64-byte aligned scan
; RCX = buffer, RDX = length, R8 = result struct
; Returns: RAX = pattern type (0-8), or 0 if no aligned match
;-----------------------------------------------------------------------------
ScanAVX512 PROC
    PUSH RBX
    PUSH R12
    PUSH R13
    PUSH R14
    PUSH R15
    
    XOR R15, R15              ; Match type (0 = none)
    
    ; Load pattern table and case mask
    VMOVDQU64 ZMM0, ZMMWORD PTR [g_Patterns]
    VMOVDQU64 ZMM1, ZMMWORD PTR [g_CaseMask]
    
    ; Calculate aligned end
    MOV R12, RDX
    AND R12, -64              ; Round down to 64-byte boundary
    
    XOR R13, R13              ; Position counter
    
@@avx_loop:
    CMP R13, R12
    JAE @@avx_done            ; No more aligned blocks
    
    ; Load 64 bytes and case-fold
    VMOVDQU64 ZMM2, ZMMWORD PTR [RCX + R13]
    VPORQ ZMM2, ZMM2, ZMM1
    
    ; Compare against all 8 patterns simultaneously
    VPCMPEQB K0, ZMM2, ZMM0
    
    ; Check if any match
    KTESTW K0, K0
    JZ @@avx_next_block
    
    ; Extract which pattern matched (simplified - finds first)
    KMOVQ R14, K0
    TZCNT R14, R14            ; Find first set bit
    CMP R14, 64
    JAE @@avx_next_block      ; No valid match
    
    ; Calculate pattern index (0-7)
    SHR R14, 3                ; Divide by 8 bytes per pattern
    MOV R15, R14
    INC R15                   ; Convert 0-7 to 1-8
    JMP @@avx_done
    
@@avx_next_block:
    ADD R13, 64
    JMP @@avx_loop
    
@@avx_done:
    VZEROUPPER                ; Clear AVX-512 state
    
    MOV RAX, R15
    
    POP R15
    POP R14
    POP R13
    POP R12
    POP RBX
    RET
ScanAVX512 ENDP

;-----------------------------------------------------------------------------
; ClassifyPattern - Main entry point (hybrid)
; RCX = buffer, RDX = length, R8 = result struct
;-----------------------------------------------------------------------------
ClassifyPattern PROC EXPORT
    PUSH RBX
    PUSH R12
    PUSH R13
    
    INC g_TotalCalls
    
    ; Zero result struct
    XOR RAX, RAX
    MOV [R8 + RES_TYPE], RAX
    MOV [R8 + RES_CONF], RAX
    MOV DWORD PTR [R8 + RES_LINE], 0
    MOV DWORD PTR [R8 + RES_PRIO], 0
    
    ; Check AVX-512 availability (once)
    CMP g_AVX512Avail, 0
    JNE @@check_done
    CALL DetectAVX512
    MOV g_AVX512Avail, AL
@@check_done:
    
    ; Try AVX-512 fast path first (if available and buffer large enough)
    CMP g_AVX512Avail, 0
    JE @@use_scalar
    CMP RDX, 64
    JB @@use_scalar
    
    CALL ScanAVX512
    CMP RAX, 0
    JNE @@found_match         ; AVX-512 found a match
    
    ; AVX-512 didn't find match in aligned portion, try scalar on tail
    ; Calculate tail start
    MOV R12, RDX
    AND R12, -64              ; Aligned length
    CMP R12, RDX
    JAE @@use_scalar          ; No tail, do full scalar
    
    ; Scan tail with scalar
    ADD RCX, R12              ; Advance buffer to tail
    SUB RDX, R12              ; Reduce length to tail size
    CALL ScanScalar
    JMP @@store_result
    
@@use_scalar:
    ; Full scalar scan
    CALL ScanScalar
    
@@found_match:
    ; RAX contains pattern type (1-8)
    
@@store_result:
    ; Store result
    MOV [R8 + RES_TYPE], RAX
    
    ; Set confidence (1.0 if match, 0.0 if none)
    CMP RAX, 0
    JE @@no_confidence
    MOV R10, 3FF0000000000000H  ; 1.0 in IEEE 754
    MOV [R8 + RES_CONF], R10
    MOV DWORD PTR [R8 + RES_LINE], 1
    MOV R10, RAX                    ; preserve pattern type before lookup
    LEA R11, g_Priorities
    MOVZX R10D, BYTE PTR [R11 + R10]
    MOV DWORD PTR [R8 + RES_PRIO], R10D
    JMP @@done
    
@@no_confidence:
    MOV QWORD PTR [R8 + RES_CONF], 0
    MOV DWORD PTR [R8 + RES_PRIO], 0
    
@@done:
    XOR EAX, EAX              ; Return 0 = success
    POP R13
    POP R12
    POP RBX
    RET
ClassifyPattern ENDP

InitializePatternEngine PROC EXPORT
    CALL DetectAVX512
    MOV g_AVX512Avail, AL
    XOR EAX, EAX
    RET
InitializePatternEngine ENDP

ShutdownPatternEngine PROC EXPORT
    XOR EAX, EAX
    RET
ShutdownPatternEngine ENDP

GetPatternStats PROC EXPORT
    MOV RAX, g_TotalCalls
    RET
GetPatternStats ENDP

; Minimal DLL entry point to satisfy loader
DllMain PROC EXPORT
    MOV EAX, 1
    RET
DllMain ENDP

END
