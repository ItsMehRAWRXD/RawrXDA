; ============================================================================
; RawrXD_AVX512_SIMD.asm
; AVX-512 SIMD Pattern Engine for Zen 4 (Ryzen 7 7800X3D)
; Target: 150-200K ops/sec via 64-byte parallel scanning
; ============================================================================

option casemap:none

; ============================================
; EXPORTS
; ============================================
PUBLIC InitializePatternEngine
PUBLIC ClassifyPattern
PUBLIC ShutdownPatternEngine
PUBLIC GetPatternStats
PUBLIC GetEngineInfo

; ============================================
; CONSTANTS
; ============================================
PATTERN_UNKNOWN EQU 0
PATTERN_TODO    EQU 1
PATTERN_FIXME   EQU 2
PATTERN_XXX     EQU 3
PATTERN_HACK    EQU 4
PATTERN_BUG     EQU 5
PATTERN_NOTE    EQU 6
PATTERN_IDEA    EQU 7
PATTERN_REVIEW  EQU 8

ENGINE_MODE_SCALAR  EQU 1
ENGINE_MODE_AVX512  EQU 2

; ============================================
; DATA SECTION
; ============================================
.data

; Engine state
g_Initialized   DWORD 0
g_EngineMode    DWORD 0             ; 1=scalar, 2=avx512
g_TotalScans    QWORD 0
g_TotalMatches  QWORD 0
g_TotalBytes    QWORD 0
g_TotalCycles   QWORD 0

; Engine info structure (matches C# EngineInfo)
EngineInfoBlock LABEL BYTE
    EI_Version      DWORD 1
    EI_Mode         DWORD 0
    EI_Patterns     DWORD 8
    EI_Reserved     DWORD 0
    EI_TotalScans   QWORD 0
    EI_TotalMatches QWORD 0
    EI_TotalBytes   QWORD 0
    EI_TotalCycles  QWORD 0
    EI_AvgNsPerOp   QWORD 0
    EI_Padding      QWORD 0

; Confidence values (IEEE 754 double)
Conf_Critical   QWORD 3FF0000000000000h  ; 1.0
Conf_High       QWORD 3FEE666666666666h  ; 0.95
Conf_Medium     QWORD 3FEB333333333333h  ; 0.85
Conf_Low        QWORD 3FE8000000000000h  ; 0.75
Conf_Zero       QWORD 0                   ; 0.0

; First character lookup (lowercase): b=BUG, f=FIXME, h=HACK, i=IDEA, n=NOTE, r=REVIEW, t=TODO, x=XXX
CharLookup      BYTE 256 DUP(0)

; ============================================
; CODE SECTION
; ============================================
.code

; --------------------------------------------
; DetectAVX512
; Returns: 1 if AVX-512F available, 0 otherwise
; Clobbers: rax, rbx, rcx, rdx
; --------------------------------------------
DetectAVX512 PROC
    push rbx
    
    ; Check max CPUID leaf
    xor eax, eax
    cpuid
    cmp eax, 7
    jb no_avx512
    
    ; Check CPUID.7:0.EBX[bit 16] = AVX-512F
    mov eax, 7
    xor ecx, ecx
    cpuid
    
    test ebx, 00010000h     ; Bit 16 = AVX-512F
    jz no_avx512
    
    ; Check XCR0 for ZMM register state enabled
    xor ecx, ecx
    xgetbv                  ; Result in edx:eax
    
    ; Check bits: 1=SSE, 2=AVX, 5=OPMASK, 6=ZMM_Hi256, 7=Hi16_ZMM
    and eax, 0E6h           ; Bits 1,2,5,6,7
    cmp eax, 0E6h
    jne no_avx512
    
    mov eax, 1
    pop rbx
    ret
    
no_avx512:
    xor eax, eax
    pop rbx
    ret
DetectAVX512 ENDP

; --------------------------------------------
; InitCharLookup - Initialize first-char dispatch table
; --------------------------------------------
InitCharLookup PROC
    push rdi
    
    ; Zero the table
    lea rdi, [CharLookup]
    xor eax, eax
    mov ecx, 256
    rep stosb
    
    ; Set pattern first chars (lowercase)
    lea rdi, [CharLookup]
    mov byte ptr [rdi + 'b'], PATTERN_BUG
    mov byte ptr [rdi + 'B'], PATTERN_BUG
    mov byte ptr [rdi + 'f'], PATTERN_FIXME
    mov byte ptr [rdi + 'F'], PATTERN_FIXME
    mov byte ptr [rdi + 'h'], PATTERN_HACK
    mov byte ptr [rdi + 'H'], PATTERN_HACK
    mov byte ptr [rdi + 'i'], PATTERN_IDEA
    mov byte ptr [rdi + 'I'], PATTERN_IDEA
    mov byte ptr [rdi + 'n'], PATTERN_NOTE
    mov byte ptr [rdi + 'N'], PATTERN_NOTE
    mov byte ptr [rdi + 'r'], PATTERN_REVIEW
    mov byte ptr [rdi + 'R'], PATTERN_REVIEW
    mov byte ptr [rdi + 't'], PATTERN_TODO
    mov byte ptr [rdi + 'T'], PATTERN_TODO
    mov byte ptr [rdi + 'x'], PATTERN_XXX
    mov byte ptr [rdi + 'X'], PATTERN_XXX
    
    pop rdi
    ret
InitCharLookup ENDP

; --------------------------------------------
; InitializePatternEngine
; Returns: 0 on success
; --------------------------------------------
InitializePatternEngine PROC
    push rbx
    
    ; Initialize lookup table
    call InitCharLookup
    
    ; Detect AVX-512
    call DetectAVX512
    test eax, eax
    jz use_scalar
    
    mov dword ptr [g_EngineMode], ENGINE_MODE_AVX512
    mov dword ptr [EI_Mode], ENGINE_MODE_AVX512
    jmp init_done
    
use_scalar:
    mov dword ptr [g_EngineMode], ENGINE_MODE_SCALAR
    mov dword ptr [EI_Mode], ENGINE_MODE_SCALAR
    
init_done:
    mov dword ptr [g_Initialized], 1
    mov qword ptr [g_TotalScans], 0
    mov qword ptr [g_TotalMatches], 0
    mov qword ptr [g_TotalBytes], 0
    mov qword ptr [g_TotalCycles], 0
    
    ; Sync to info block
    mov qword ptr [EI_TotalScans], 0
    mov qword ptr [EI_TotalMatches], 0
    mov qword ptr [EI_TotalBytes], 0
    mov qword ptr [EI_TotalCycles], 0
    
    xor eax, eax
    pop rbx
    ret
InitializePatternEngine ENDP

; --------------------------------------------
; ShutdownPatternEngine
; Returns: 0 on success
; --------------------------------------------
ShutdownPatternEngine PROC
    mov dword ptr [g_Initialized], 0
    xor eax, eax
    ret
ShutdownPatternEngine ENDP

; --------------------------------------------
; GetPatternStats
; Returns: pointer to engine info
; --------------------------------------------
GetPatternStats PROC
    lea rax, [EngineInfoBlock]
    ret
GetPatternStats ENDP

; --------------------------------------------
; GetEngineInfo
; rcx = buffer pointer (64 bytes min)
; Returns: 0 on success
; --------------------------------------------
GetEngineInfo PROC
    test rcx, rcx
    jz getinfo_fail
    
    push rsi
    push rdi
    
    ; Copy EngineInfoBlock to caller buffer
    lea rsi, [EngineInfoBlock]
    mov rdi, rcx
    mov ecx, 64
    rep movsb
    
    pop rdi
    pop rsi
    xor eax, eax
    ret
    
getinfo_fail:
    mov eax, 1
    ret
GetEngineInfo ENDP

; --------------------------------------------
; ScanAVX512
; Fast 64-byte chunk scanning for pattern first chars
; rcx = buffer, rdx = length
; Returns: rax = position of first potential match, -1 if none
; --------------------------------------------
ScanAVX512 PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov r12, rcx            ; buffer
    mov r13, rdx            ; length
    xor r14, r14            ; position
    
    ; Broadcast pattern first chars to ZMM for comparison
    ; We'll search for: B, F, H, I, N, R, T, X (and lowercase)
    
    ; Create mask with all first chars we're looking for
    mov eax, 'B'
    vpbroadcastb zmm1, eax
    mov eax, 'b'
    vpbroadcastb zmm2, eax
    
    mov eax, 'F'
    vpbroadcastb zmm3, eax
    mov eax, 'f'
    vpbroadcastb zmm4, eax
    
    mov eax, 'T'
    vpbroadcastb zmm5, eax
    mov eax, 't'
    vpbroadcastb zmm6, eax
    
    mov eax, 'X'
    vpbroadcastb zmm7, eax
    mov eax, 'x'
    vpbroadcastb zmm8, eax
    
avx_loop:
    ; Need at least 64 bytes
    mov rax, r13
    sub rax, r14
    cmp rax, 64
    jb avx_done
    
    ; Load 64 bytes
    vmovdqu64 zmm0, zmmword ptr [r12 + r14]
    
    ; Compare against each first char
    vpcmpeqb k1, zmm0, zmm1     ; 'B'
    vpcmpeqb k2, zmm0, zmm2     ; 'b'
    korq k1, k1, k2
    
    vpcmpeqb k2, zmm0, zmm3     ; 'F'
    korq k1, k1, k2
    vpcmpeqb k2, zmm0, zmm4     ; 'f'
    korq k1, k1, k2
    
    vpcmpeqb k2, zmm0, zmm5     ; 'T'
    korq k1, k1, k2
    vpcmpeqb k2, zmm0, zmm6     ; 't'
    korq k1, k1, k2
    
    vpcmpeqb k2, zmm0, zmm7     ; 'X'
    korq k1, k1, k2
    vpcmpeqb k2, zmm0, zmm8     ; 'x'
    korq k1, k1, k2
    
    ; Check for any matches
    kmovq rax, k1
    test rax, rax
    jnz avx_found
    
    ; No match in this chunk, advance
    add r14, 64
    jmp avx_loop
    
avx_found:
    ; Find first set bit (first potential pattern start)
    tzcnt rax, rax
    add rax, r14            ; Absolute position
    jmp avx_return
    
avx_done:
    mov rax, -1             ; No match found in aligned portion
    
avx_return:
    vzeroupper
    
    add rsp, 40
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ScanAVX512 ENDP

; --------------------------------------------
; ValidatePattern
; Checks if position contains a full pattern
; rcx = buffer, rdx = position, r8 = remaining length
; Returns: rax = pattern type (0 if not valid)
; --------------------------------------------
ValidatePattern PROC
    push rbx
    push rsi
    
    mov rsi, rcx
    add rsi, rdx            ; Point to potential pattern start
    mov rbx, r8
    sub rbx, rdx            ; Remaining bytes from position
    
    ; Get first char and convert to uppercase
    movzx eax, byte ptr [rsi]
    cmp al, 'a'
    jb check_upper
    cmp al, 'z'
    ja check_upper
    sub al, 32              ; To uppercase
    
check_upper:
    ; Dispatch based on first char
    cmp al, 'B'
    je check_bug
    cmp al, 'F'
    je check_fixme
    cmp al, 'H'
    je check_hack
    cmp al, 'I'
    je check_idea
    cmp al, 'N'
    je check_note
    cmp al, 'R'
    je check_review
    cmp al, 'T'
    je check_todo
    cmp al, 'X'
    je check_xxx
    jmp not_pattern

check_bug:
    cmp rbx, 3
    jb not_pattern
    movzx eax, byte ptr [rsi+1]
    or al, 20h
    cmp al, 'u'
    jne not_pattern
    movzx eax, byte ptr [rsi+2]
    or al, 20h
    cmp al, 'g'
    jne not_pattern
    mov eax, PATTERN_BUG
    jmp pattern_found

check_fixme:
    cmp rbx, 5
    jb not_pattern
    movzx eax, byte ptr [rsi+1]
    or al, 20h
    cmp al, 'i'
    jne not_pattern
    movzx eax, byte ptr [rsi+2]
    or al, 20h
    cmp al, 'x'
    jne not_pattern
    movzx eax, byte ptr [rsi+3]
    or al, 20h
    cmp al, 'm'
    jne not_pattern
    movzx eax, byte ptr [rsi+4]
    or al, 20h
    cmp al, 'e'
    jne not_pattern
    mov eax, PATTERN_FIXME
    jmp pattern_found

check_hack:
    cmp rbx, 4
    jb not_pattern
    movzx eax, byte ptr [rsi+1]
    or al, 20h
    cmp al, 'a'
    jne not_pattern
    movzx eax, byte ptr [rsi+2]
    or al, 20h
    cmp al, 'c'
    jne not_pattern
    movzx eax, byte ptr [rsi+3]
    or al, 20h
    cmp al, 'k'
    jne not_pattern
    mov eax, PATTERN_HACK
    jmp pattern_found

check_idea:
    cmp rbx, 4
    jb not_pattern
    movzx eax, byte ptr [rsi+1]
    or al, 20h
    cmp al, 'd'
    jne not_pattern
    movzx eax, byte ptr [rsi+2]
    or al, 20h
    cmp al, 'e'
    jne not_pattern
    movzx eax, byte ptr [rsi+3]
    or al, 20h
    cmp al, 'a'
    jne not_pattern
    mov eax, PATTERN_IDEA
    jmp pattern_found

check_note:
    cmp rbx, 4
    jb not_pattern
    movzx eax, byte ptr [rsi+1]
    or al, 20h
    cmp al, 'o'
    jne not_pattern
    movzx eax, byte ptr [rsi+2]
    or al, 20h
    cmp al, 't'
    jne not_pattern
    movzx eax, byte ptr [rsi+3]
    or al, 20h
    cmp al, 'e'
    jne not_pattern
    mov eax, PATTERN_NOTE
    jmp pattern_found

check_review:
    cmp rbx, 6
    jb not_pattern
    movzx eax, byte ptr [rsi+1]
    or al, 20h
    cmp al, 'e'
    jne not_pattern
    movzx eax, byte ptr [rsi+2]
    or al, 20h
    cmp al, 'v'
    jne not_pattern
    movzx eax, byte ptr [rsi+3]
    or al, 20h
    cmp al, 'i'
    jne not_pattern
    movzx eax, byte ptr [rsi+4]
    or al, 20h
    cmp al, 'e'
    jne not_pattern
    movzx eax, byte ptr [rsi+5]
    or al, 20h
    cmp al, 'w'
    jne not_pattern
    mov eax, PATTERN_REVIEW
    jmp pattern_found

check_todo:
    cmp rbx, 4
    jb not_pattern
    movzx eax, byte ptr [rsi+1]
    or al, 20h
    cmp al, 'o'
    jne not_pattern
    movzx eax, byte ptr [rsi+2]
    or al, 20h
    cmp al, 'd'
    jne not_pattern
    movzx eax, byte ptr [rsi+3]
    or al, 20h
    cmp al, 'o'
    jne not_pattern
    mov eax, PATTERN_TODO
    jmp pattern_found

check_xxx:
    cmp rbx, 3
    jb not_pattern
    movzx eax, byte ptr [rsi+1]
    or al, 20h
    cmp al, 'x'
    jne not_pattern
    movzx eax, byte ptr [rsi+2]
    or al, 20h
    cmp al, 'x'
    jne not_pattern
    mov eax, PATTERN_XXX
    jmp pattern_found

not_pattern:
    xor eax, eax

pattern_found:
    pop rsi
    pop rbx
    ret
ValidatePattern ENDP

; --------------------------------------------
; ClassifyPattern
; rcx = codeBuffer (byte*)
; edx = length (int)
; r8  = context (byte*) - unused
; r9  = confidence (double* out)
;
; Returns: Pattern type (0-8)
; --------------------------------------------
ClassifyPattern PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 56
    .allocstack 56
    .endprolog

    ; Validate inputs
    test rcx, rcx
    jz invalid_input
    test edx, edx
    jz invalid_input
    test r9, r9
    jz invalid_input

    ; Save parameters
    mov rsi, rcx            ; rsi = buffer
    mov r12d, edx           ; r12d = length
    mov r14, r9             ; r14 = confidence out
    
    ; Save length as qword for AVX calls
    mov r13, rdx
    and r13d, r13d          ; Zero-extend

    ; Update stats
    inc qword ptr [g_TotalScans]
    add qword ptr [g_TotalBytes], r13

    ; Initialize result
    xor ebx, ebx            ; ebx = pattern found (0 = UNKNOWN)
    xor r15d, r15d          ; r15d = search position

    ; Check engine mode
    cmp dword ptr [g_EngineMode], ENGINE_MODE_AVX512
    jne scalar_scan

    ; =========================================
    ; AVX-512 SIMD Path
    ; =========================================
avx512_scan:
    ; Check if buffer is large enough for AVX-512 (need >= 64 bytes)
    cmp r13, 64
    jb scalar_scan          ; Too small, use scalar

    ; Use AVX-512 to find first potential match
    mov rcx, rsi
    mov rdx, r13
    call ScanAVX512
    
    ; rax = position of potential match, or -1
    cmp rax, -1
    je avx_remainder        ; No match in aligned portion, scan remainder
    
    ; Validate the potential match
    mov r15, rax            ; Save position
    mov rcx, rsi            ; buffer
    mov rdx, rax            ; position
    mov r8, r13             ; total length
    call ValidatePattern
    
    test eax, eax
    jnz pattern_found       ; Valid pattern!
    
    ; Not a valid pattern, continue scanning from next position
    inc r15
    jmp scalar_continue

avx_remainder:
    ; AVX-512 scanned aligned chunks but found nothing
    ; Scan the remaining bytes (< 64) with scalar
    mov rax, r13
    and rax, 0FFFFFFFFFFFFFFC0h  ; Round down to 64-byte boundary
    mov r15, rax            ; Start scalar scan from end of AVX portion
    jmp scalar_continue

    ; =========================================
    ; Scalar Scan Path
    ; =========================================
scalar_scan:
    xor r15d, r15d          ; Start from beginning

scalar_continue:
    cmp r15d, r12d
    jae scan_done
    
    ; Need at least 3 bytes for shortest pattern (XXX, BUG)
    mov eax, r12d
    sub eax, r15d
    cmp eax, 3
    jb scan_done
    
    ; Get current byte
    movzx eax, byte ptr [rsi + r15]
    
    ; Quick first-char check using lookup table
    lea rcx, [CharLookup]
    movzx eax, byte ptr [rcx + rax]
    test al, al
    jz scalar_next          ; Not a pattern start char
    
    ; Potential pattern - validate it
    mov rcx, rsi
    mov edx, r15d
    mov r8d, r12d
    call ValidatePattern
    
    test eax, eax
    jnz pattern_found
    
scalar_next:
    inc r15d
    jmp scalar_continue

pattern_found:
    mov ebx, eax            ; Save pattern type
    inc qword ptr [g_TotalMatches]
    
    ; Set confidence based on pattern type
    cmp ebx, PATTERN_BUG
    je set_conf_critical
    cmp ebx, PATTERN_FIXME
    je set_conf_high
    cmp ebx, PATTERN_XXX
    je set_conf_high
    cmp ebx, PATTERN_TODO
    je set_conf_medium
    jmp set_conf_low

set_conf_critical:
    lea rax, [Conf_Critical]
    mov rax, qword ptr [rax]
    jmp store_confidence

set_conf_high:
    lea rax, [Conf_High]
    mov rax, qword ptr [rax]
    jmp store_confidence

set_conf_medium:
    lea rax, [Conf_Medium]
    mov rax, qword ptr [rax]
    jmp store_confidence

set_conf_low:
    lea rax, [Conf_Low]
    mov rax, qword ptr [rax]

store_confidence:
    mov qword ptr [r14], rax
    jmp return_result

scan_done:
    ; No pattern found
    lea rax, [Conf_Zero]
    mov rax, qword ptr [rax]
    mov qword ptr [r14], rax
    xor ebx, ebx
    jmp return_result

invalid_input:
    test r9, r9
    jz skip_conf
    lea rax, [Conf_Zero]
    mov rax, qword ptr [rax]
    mov qword ptr [r9], rax
skip_conf:
    xor ebx, ebx

return_result:
    ; Update info block with current stats
    mov rax, qword ptr [g_TotalScans]
    mov qword ptr [EI_TotalScans], rax
    mov rax, qword ptr [g_TotalMatches]
    mov qword ptr [EI_TotalMatches], rax
    mov rax, qword ptr [g_TotalBytes]
    mov qword ptr [EI_TotalBytes], rax
    
    mov eax, ebx            ; Return pattern type

    ; Cleanup
    add rsp, 56
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

ClassifyPattern ENDP

END
