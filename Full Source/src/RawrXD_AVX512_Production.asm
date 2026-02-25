; ============================================================================
; RawrXD_AVX512_Production.asm
; Production AVX-512 Pattern Engine with SIMD + Scalar Fallback
; Optimized for Ryzen 7 7800X3D (Zen 4 with AVX-512)
; ============================================================================

; ============================================
; EXPORTS
; ============================================

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

PUBLIC InitializePatternEngine
PUBLIC ClassifyPattern
PUBLIC ShutdownPatternEngine
PUBLIC GetPatternStats
PUBLIC GetEngineInfo

; ============================================
; CONSTANTS
; ============================================
PATTERN_UNKNOWN EQU 0
PATTERN_FIXME   EQU 1
PATTERN_XXX     EQU 2
PATTERN_HACK    EQU 3
PATTERN_BUG     EQU 4
PATTERN_NOTE    EQU 5
PATTERN_IDEA    EQU 6
PATTERN_REVIEW  EQU 7

; AVX-512 detection bits
AVX512F_BIT     EQU 10000h      ; Bit 16 of EBX from CPUID leaf 7
XCR0_ZMM_MASK   EQU 0E6h        ; ZMM + YMM + XMM state bits

; ============================================
; DATA SECTION
; ============================================
.data

; Engine state
g_Initialized   DWORD 0         ; 0=uninit, 1=scalar, 2=avx512
g_AVX512Ready   DWORD 0         ; 1 if AVX-512 available
g_TotalScans    QWORD 0
g_TotalMatches  QWORD 0
g_TotalBytes    QWORD 0
g_TotalCycles   QWORD 0

; Confidence values (IEEE 754 double)
Conf_Critical   QWORD 3FF0000000000000h  ; 1.0
Conf_High       QWORD 3FEE666666666666h  ; 0.95
Conf_Medium     QWORD 3FEB333333333333h  ; 0.85
Conf_Low        QWORD 3FE8000000000000h  ; 0.75
Conf_Zero       QWORD 0                   ; 0.0

; Engine info structure (64 bytes)
EngineInfo:
    ei_Version      DWORD 1             ; +0: Version
    ei_Mode         DWORD 0             ; +4: 1=scalar, 2=avx512
    ei_Patterns     DWORD 8             ; +8: Number of patterns
    ei_Reserved     DWORD 0             ; +12
    ei_TotalScans   QWORD 0             ; +16
    ei_TotalMatches QWORD 0             ; +24
    ei_TotalBytes   QWORD 0             ; +32
    ei_TotalCycles  QWORD 0             ; +40
    ei_AvgNsPerOp   QWORD 0             ; +48
    ei_Padding      QWORD 0             ; +56

; ============================================
; CODE SECTION
; ============================================
.code

; --------------------------------------------
; CheckAVX512Support
; Returns: 1 if AVX-512F supported, 0 otherwise
; --------------------------------------------
CheckAVX512Support PROC
    push rbx
    push rcx
    push rdx
    
    ; Check max CPUID leaf
    xor eax, eax
    cpuid
    cmp eax, 7
    jb no_avx512
    
    ; Check CPUID leaf 7 for AVX-512F
    mov eax, 7
    xor ecx, ecx
    cpuid
    test ebx, AVX512F_BIT
    jz no_avx512
    
    ; Check OS support via XGETBV
    xor ecx, ecx
    xgetbv
    and eax, XCR0_ZMM_MASK
    cmp eax, XCR0_ZMM_MASK
    jne no_avx512
    
    ; AVX-512 supported!
    pop rdx
    pop rcx
    pop rbx
    mov eax, 1
    ret

no_avx512:
    pop rdx
    pop rcx
    pop rbx
    xor eax, eax
    ret
CheckAVX512Support ENDP

; --------------------------------------------
; InitializePatternEngine
; Returns: 0 on success
; --------------------------------------------
InitializePatternEngine PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; Reset counters
    mov qword ptr [g_TotalScans], 0
    mov qword ptr [g_TotalMatches], 0
    mov qword ptr [g_TotalBytes], 0
    mov qword ptr [g_TotalCycles], 0
    
    ; Check for AVX-512
    call CheckAVX512Support
    mov dword ptr [g_AVX512Ready], eax
    
    test eax, eax
    jz scalar_mode
    
    ; AVX-512 mode
    mov dword ptr [g_Initialized], 2
    mov dword ptr [EngineInfo + 4], 2   ; ei_Mode = 2
    jmp init_done
    
scalar_mode:
    mov dword ptr [g_Initialized], 1
    mov dword ptr [EngineInfo + 4], 1   ; ei_Mode = 1
    
init_done:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
InitializePatternEngine ENDP

; --------------------------------------------
; ShutdownPatternEngine
; Returns: 0 on success
; --------------------------------------------
ShutdownPatternEngine PROC
    ; Update engine info before shutdown
    mov rax, qword ptr [g_TotalScans]
    mov qword ptr [EngineInfo + 16], rax
    mov rax, qword ptr [g_TotalMatches]
    mov qword ptr [EngineInfo + 24], rax
    mov rax, qword ptr [g_TotalBytes]
    mov qword ptr [EngineInfo + 32], rax
    mov rax, qword ptr [g_TotalCycles]
    mov qword ptr [EngineInfo + 40], rax
    
    ; Calculate avg ns/op (cycles / scans * ~0.3 for 3GHz CPU)
    mov rax, qword ptr [g_TotalCycles]
    mov rcx, qword ptr [g_TotalScans]
    test rcx, rcx
    jz skip_avg
    xor edx, edx
    div rcx
    ; Approximate ns (cycles / 3 for ~3GHz)
    mov rcx, 3
    xor edx, edx
    div rcx
    mov qword ptr [EngineInfo + 48], rax
    
skip_avg:
    mov dword ptr [g_Initialized], 0
    
    ; Clear ZMM state if we used AVX-512
    cmp dword ptr [g_AVX512Ready], 1
    jne no_vzeroupper
    vzeroupper
    
no_vzeroupper:
    xor eax, eax
    ret
ShutdownPatternEngine ENDP

; --------------------------------------------
; GetPatternStats
; Returns: pointer to stats structure
; --------------------------------------------
GetPatternStats PROC
    ; Update stats first
    mov rax, qword ptr [g_TotalScans]
    mov qword ptr [EngineInfo + 16], rax
    mov rax, qword ptr [g_TotalMatches]
    mov qword ptr [EngineInfo + 24], rax
    mov rax, qword ptr [g_TotalBytes]
    mov qword ptr [EngineInfo + 32], rax
    mov rax, qword ptr [g_TotalCycles]
    mov qword ptr [EngineInfo + 40], rax
    
    lea rax, [EngineInfo]
    ret
GetPatternStats ENDP

; --------------------------------------------
; GetEngineInfo
; rcx = output buffer (64 bytes min)
; Returns: 0 on success, -1 on error
; --------------------------------------------
GetEngineInfo PROC
    test rcx, rcx
    jz info_error
    
    ; Update stats
    mov rax, qword ptr [g_TotalScans]
    mov qword ptr [EngineInfo + 16], rax
    mov rax, qword ptr [g_TotalMatches]
    mov qword ptr [EngineInfo + 24], rax
    mov rax, qword ptr [g_TotalBytes]
    mov qword ptr [EngineInfo + 32], rax
    mov rax, qword ptr [g_TotalCycles]
    mov qword ptr [EngineInfo + 40], rax
    
    ; Copy to output buffer (64 bytes)
    push rdi
    push rsi
    mov rdi, rcx
    lea rsi, [EngineInfo]
    mov ecx, 64
    rep movsb
    pop rsi
    pop rdi
    
    xor eax, eax
    ret
    
info_error:
    mov eax, -1
    ret
GetEngineInfo ENDP

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

    ; Read timestamp counter for benchmarking
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov r15, rax            ; r15 = start cycles

    ; Validate inputs
    test rcx, rcx
    jz invalid_input
    test edx, edx
    jz invalid_input
    test r9, r9
    jz invalid_input

    ; Save parameters
    mov rsi, rcx            ; rsi = buffer pointer
    mov r12d, edx           ; r12d = buffer length
    mov r14, r9             ; r14 = confidence output pointer

    ; Initialize result
    xor ebx, ebx            ; ebx = best pattern found (0 = UNKNOWN)

    ; Update stats
    inc qword ptr [g_TotalScans]
    mov eax, r12d
    add qword ptr [g_TotalBytes], rax

    ; Check if AVX-512 available and buffer >= 64 bytes
    cmp dword ptr [g_AVX512Ready], 1
    jne scalar_scan
    cmp r12d, 64
    jb scalar_scan

    ; ========================================
    ; AVX-512 FAST PATH (64-byte chunks)
    ; ========================================
avx512_loop:
    cmp r12d, 64
    jb scalar_scan          ; Finish remainder with scalar

    ; Load 64 bytes (use unaligned for safety)
    vmovdqu64 zmm0, zmmword ptr [rsi]
    
    ; Broadcast pattern first chars for comparison
    ; Check for 'F' (FIXME), 'B' (BUG), 'X' (XXX)
    
check_f:
    ; Check for 'F' (FIXME)
    mov eax, 46h
    vpbroadcastb zmm1, eax
    mov eax, 66h
    vpbroadcastb zmm2, eax
    vpcmpeqb k1, zmm0, zmm1
    vpcmpeqb k2, zmm0, zmm2
    korq k1, k1, k2
    
    kortestq k1, k1
    jz check_b
    
    kmovq rax, k1
    tzcnt rcx, rax
    mov eax, r12d
    sub eax, ecx
    cmp eax, 5
    jb check_b
    
    lea rdi, [rsi + rcx]
    call ValidateFIXME
    test eax, eax
    jnz found_fixme

check_b:
    ; Check for 'B' (BUG)
    mov eax, 42h
    vpbroadcastb zmm1, eax
    mov eax, 62h
    vpbroadcastb zmm2, eax
    vpcmpeqb k1, zmm0, zmm1
    vpcmpeqb k2, zmm0, zmm2
    korq k1, k1, k2
    
    kortestq k1, k1
    jz check_x
    
    kmovq rax, k1
    tzcnt rcx, rax
    mov eax, r12d
    sub eax, ecx
    cmp eax, 3
    jb check_x
    
    lea rdi, [rsi + rcx]
    call ValidateBUG
    test eax, eax
    jnz found_bug

check_x:
    ; Check for 'X' (XXX)
    mov eax, 58h
    vpbroadcastb zmm1, eax
    mov eax, 78h
    vpbroadcastb zmm2, eax
    vpcmpeqb k1, zmm0, zmm1
    vpcmpeqb k2, zmm0, zmm2
    korq k1, k1, k2
    
    kortestq k1, k1
    jz avx512_next
    
    kmovq rax, k1
    tzcnt rcx, rax
    mov eax, r12d
    sub eax, ecx
    cmp eax, 3
    jb avx512_next
    
    lea rdi, [rsi + rcx]
    call ValidateXXX
    test eax, eax
    jnz found_xxx

avx512_next:
    add rsi, 64
    sub r12d, 64
    jmp avx512_loop

    ; ========================================
    ; AVX-512 Match handlers
    ; ========================================
found_fixme:
    mov ebx, PATTERN_FIXME
    jmp pattern_found

found_bug:
    mov ebx, PATTERN_BUG
    jmp pattern_found

found_xxx:
    mov ebx, PATTERN_XXX
    jmp pattern_found

    ; ========================================
    ; SCALAR FALLBACK
    ; ========================================
scalar_scan:
    cmp r12d, 3
    jb scan_done

    ; Get current byte and convert to uppercase
    movzx eax, byte ptr [rsi]
    cmp al, 'a'
    jb check_patterns
    cmp al, 'z'
    ja check_patterns
    sub al, 32

check_patterns:
    cmp al, 'F'
    je try_fixme
    cmp al, 'X'
    je try_xxx
    cmp al, 'H'
    je try_hack
    cmp al, 'B'
    je try_bug
    cmp al, 'N'
    je try_note
    cmp al, 'I'
    je try_idea
    cmp al, 'R'
    je try_review
    jmp next_byte

try_fixme:
    cmp r12d, 5
    jb next_byte
    mov rdi, rsi
    call ValidateFIXME
    test eax, eax
    jz next_byte
    mov ebx, PATTERN_FIXME
    jmp pattern_found

try_xxx:
    cmp r12d, 3
    jb next_byte
    mov rdi, rsi
    call ValidateXXX
    test eax, eax
    jz next_byte
    mov ebx, PATTERN_XXX
    jmp pattern_found

try_hack:
    cmp r12d, 4
    jb next_byte
    mov rdi, rsi
    call ValidateHACK
    test eax, eax
    jz next_byte
    mov ebx, PATTERN_HACK
    jmp pattern_found

try_bug:
    cmp r12d, 3
    jb next_byte
    mov rdi, rsi
    call ValidateBUG
    test eax, eax
    jz next_byte
    mov ebx, PATTERN_BUG
    jmp pattern_found

try_note:
    cmp r12d, 4
    jb next_byte
    mov rdi, rsi
    call ValidateNOTE
    test eax, eax
    jz next_byte
    mov ebx, PATTERN_NOTE
    jmp pattern_found

try_idea:
    cmp r12d, 4
    jb next_byte
    mov rdi, rsi
    call ValidateIDEA
    test eax, eax
    jz next_byte
    mov ebx, PATTERN_IDEA
    jmp pattern_found

try_review:
    cmp r12d, 6
    jb next_byte
    mov rdi, rsi
    call ValidateREVIEW
    test eax, eax
    jz next_byte
    mov ebx, PATTERN_REVIEW
    jmp pattern_found

next_byte:
    inc rsi
    dec r12d
    jmp scalar_scan

pattern_found:
    inc qword ptr [g_TotalMatches]

    ; Set confidence based on pattern
    cmp ebx, PATTERN_BUG
    je set_conf_critical
    cmp ebx, PATTERN_FIXME
    je set_conf_high
    cmp ebx, PATTERN_XXX
    je set_conf_high
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
    mov eax, ebx
    jmp cleanup

scan_done:
    lea rax, [Conf_Zero]
    mov rax, qword ptr [rax]
    mov qword ptr [r14], rax
    xor eax, eax
    jmp cleanup

invalid_input:
    test r9, r9
    jz skip_conf_clear
    lea rax, [Conf_Zero]
    mov rax, qword ptr [rax]
    mov qword ptr [r9], rax
skip_conf_clear:
    xor eax, eax

cleanup:
    ; Calculate elapsed cycles
    push rax
    rdtsc
    shl rdx, 32
    or rax, rdx
    sub rax, r15
    add qword ptr [g_TotalCycles], rax
    pop rax

    ; vzeroupper if we used AVX-512
    cmp dword ptr [g_AVX512Ready], 1
    jne skip_vzeroupper
    vzeroupper
skip_vzeroupper:

    add rsp, 56
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; ========================================
; Pattern Validators (case-insensitive)
; rdi = pointer to check
; Returns: 1 if match, 0 if not
; ========================================

ValidateFIXME:
    movzx eax, byte ptr [rdi+1]
    or al, 20h
    cmp al, 'i'
    jne val_fail
    movzx eax, byte ptr [rdi+2]
    or al, 20h
    cmp al, 'x'
    jne val_fail
    movzx eax, byte ptr [rdi+3]
    or al, 20h
    cmp al, 'm'
    jne val_fail
    movzx eax, byte ptr [rdi+4]
    or al, 20h
    cmp al, 'e'
    jne val_fail
    mov eax, 1
    ret

ValidateXXX:
    movzx eax, byte ptr [rdi+1]
    or al, 20h
    cmp al, 'x'
    jne val_fail
    movzx eax, byte ptr [rdi+2]
    or al, 20h
    cmp al, 'x'
    jne val_fail
    mov eax, 1
    ret

ValidateHACK:
    movzx eax, byte ptr [rdi+1]
    or al, 20h
    cmp al, 'a'
    jne val_fail
    movzx eax, byte ptr [rdi+2]
    or al, 20h
    cmp al, 'c'
    jne val_fail
    movzx eax, byte ptr [rdi+3]
    or al, 20h
    cmp al, 'k'
    jne val_fail
    mov eax, 1
    ret

ValidateBUG:
    movzx eax, byte ptr [rdi+1]
    or al, 20h
    cmp al, 'u'
    jne val_fail
    movzx eax, byte ptr [rdi+2]
    or al, 20h
    cmp al, 'g'
    jne val_fail
    mov eax, 1
    ret

ValidateNOTE:
    movzx eax, byte ptr [rdi+1]
    or al, 20h
    cmp al, 'o'
    jne val_fail
    movzx eax, byte ptr [rdi+2]
    or al, 20h
    cmp al, 't'
    jne val_fail
    movzx eax, byte ptr [rdi+3]
    or al, 20h
    cmp al, 'e'
    jne val_fail
    mov eax, 1
    ret

ValidateIDEA:
    movzx eax, byte ptr [rdi+1]
    or al, 20h
    cmp al, 'd'
    jne val_fail
    movzx eax, byte ptr [rdi+2]
    or al, 20h
    cmp al, 'e'
    jne val_fail
    movzx eax, byte ptr [rdi+3]
    or al, 20h
    cmp al, 'a'
    jne val_fail
    mov eax, 1
    ret

ValidateREVIEW:
    movzx eax, byte ptr [rdi+1]
    or al, 20h
    cmp al, 'e'
    jne val_fail
    movzx eax, byte ptr [rdi+2]
    or al, 20h
    cmp al, 'v'
    jne val_fail
    movzx eax, byte ptr [rdi+3]
    or al, 20h
    cmp al, 'i'
    jne val_fail
    movzx eax, byte ptr [rdi+4]
    or al, 20h
    cmp al, 'e'
    jne val_fail
    movzx eax, byte ptr [rdi+5]
    or al, 20h
    cmp al, 'w'
    jne val_fail
    mov eax, 1
    ret

val_fail:
    xor eax, eax
    ret

ClassifyPattern ENDP

END
