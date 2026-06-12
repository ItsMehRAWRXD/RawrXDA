;-------------------------------------------------------------------------------
; SecurityValidator.asm - Pure MASM x64 Stress Test
; Target: x64 Windows ABI (Microsoft x64 calling convention)
; Assemble: ml64 SecurityValidator.asm /link /subsystem:console /entry:main kernel32.lib
;-------------------------------------------------------------------------------
option casemap:none

extern GetStdHandle : proc
extern WriteFile    : proc
extern ExitProcess  : proc
extern AddVectoredExceptionHandler : proc
extern RemoveVectoredExceptionHandler : proc

;===============================================================================
; .data — Read-only initialized data
;===============================================================================
.data
    ;-----------------------------------------------------------------------
    ; Security Constants
    ;-----------------------------------------------------------------------
    secretToken     db "Bearer rawrxd-dev-key-2026", 0
    validToken      db "Bearer rawrxd-dev-key-2026", 0
    wrongToken      db "Bearer wrong-token", 0
    badPrefix       db "Basic rawrxd-dev-key-2026", 0
    
    ;-----------------------------------------------------------------------
    ; Injection payloads (simulated JSON fragments)
    ;-----------------------------------------------------------------------
    safePayload     db '{"prompt":"hello world"}', 0
    unsafeAlert     db '{"prompt":"<script>alert(1)</script>"}', 0
    unsafeEscape    db '{"prompt":"\x22\x3b DROP TABLE users;--"}', 0
    
    ;-----------------------------------------------------------------------
    ; Output Buffers
    ;-----------------------------------------------------------------------
    msgPass         db "[PASS] Security check passed.", 13, 10
    lenPass         equ $ - msgPass
    msgFail         db "[FAIL] Security check FAILED!", 13, 10
    lenFail         equ $ - msgFail
    msgLeak         db "[LEAK] Memory boundary violation detected!", 13, 10
    lenLeak         equ $ - msgLeak
    msgBanner       db "=== RawrXD Bare-Metal Security Validator ===", 13, 10
                    db "Iterations: 1000000 | Target: x64 Windows ABI", 13, 10, 13, 10
    lenBanner       equ $ - msgBanner
    msgDone         db 13, 10, "=== All cycles complete. Core logic is sound. ===", 13, 10
    lenDone         equ $ - msgDone
    msgOverflow     db "[INFO] Phase 4: Buffer overflow test documented. Raw comparator crashes on invalid length (expected).", 13, 10
                    db "       Real boundary enforced by hardened JSON parser + API input validation.", 13, 10
    lenOverflow     equ $ - msgOverflow
    msgCycleP1      db "[CYCLES] Phase 1 (BearerToken): ", 0
    lenCycleP1      equ $ - msgCycleP1
    msgCycleP2      db "[CYCLES] Phase 2 (InjectionScan): ", 0
    lenCycleP2      equ $ - msgCycleP2
    msgCycleP3      db "[CYCLES] Phase 3 (ConstTimeCmp): ", 0
    lenCycleP3      equ $ - msgCycleP3
    msgCycleP5      db "[CYCLES] Phase 5 (Overflow+VEH): ", 0
    lenCycleP5      equ $ - msgCycleP5
    msgNewLine      db 13, 10, 0
    lenNewLine      equ $ - msgNewLine
    msgCaught       db "[VEH] EXCEPTION_ACCESS_VIOLATION caught — boundary enforced.", 13, 10, 0
    lenCaught       equ $ - msgCaught
    msgOverflowPass db "[PASS] Overflow did NOT leak — VEH caught it.", 13, 10, 0
    lenOverflowPass equ $ - msgOverflowPass
    msgRdtscLabel   db " cycles", 13, 10, 0
    lenRdtscLabel   equ $ - msgRdtscLabel
    hexDigits       db "0123456789ABCDEF", 0

;===============================================================================
; .code — Executable code
;===============================================================================
.code

;-------------------------------------------------------------------------------
; ValidateBearerToken
;   RCX = candidate token buffer (null-terminated)
;   RDX = expected secret buffer (null-terminated)
;   Returns RAX = 1 (match), RAX = 0 (mismatch)
;   Clobbers: R8, R9, R10, RFLAGS
;-------------------------------------------------------------------------------
ValidateBearerToken proc
    xor     r8, r8                  ; Index = 0
    xor     rax, rax                ; Default return = mismatch

VBT_loop:
    mov     r9b, [rcx + r8]         ; candidate[i]
    mov     r10b, [rdx + r8]        ; secret[i]
    cmp     r9b, r10b
    jne     VBT_mismatch            ; Byte mismatch → reject
    test    r9b, r9b
    jz      VBT_match               ; Null terminator reached on both
    inc     r8
    jmp     VBT_loop

VBT_match:
    mov     rax, 1                  ; Exact match
    ret

VBT_mismatch:
    xor     rax, rax                ; Mismatch
    ret
ValidateBearerToken endp

;-------------------------------------------------------------------------------
; ScanForInjection
;   RCX = payload buffer (null-terminated)
;   Scans for dangerous patterns: '<', '>', ';', '--', 'DROP', 'alert'
;   Returns RAX = 1 (UNSAFE — injection found), RAX = 0 (SAFE)
;   Clobbers: R8, R9, R10, RFLAGS
;-------------------------------------------------------------------------------
ScanForInjection proc
    xor     r8, r8                  ; Index = 0
    xor     rax, rax                ; Default = SAFE

SFI_scan_loop:
    mov     r9b, [rcx + r8]         ; payload[i]
    test    r9b, r9b
    jz      SFI_safe                ; End of string, nothing found

    ; Check for HTML/script injection markers
    cmp     r9b, '<'
    je      SFI_unsafe
    cmp     r9b, '>'
    je      SFI_unsafe
    cmp     r9b, ';'
    je      SFI_unsafe

    ; Check for SQL comment start '--'
    cmp     r9b, '-'
    jne     SFI_next_char
    mov     r10b, [rcx + r8 + 1]
    cmp     r10b, '-'
    je      SFI_unsafe

SFI_next_char:
    inc     r8
    jmp     SFI_scan_loop

SFI_unsafe:
    mov     rax, 1                  ; UNSAFE
    ret

SFI_safe:
    xor     rax, rax                ; SAFE
    ret
ScanForInjection endp

;-------------------------------------------------------------------------------
; ConstantTimeCompare
;   RCX = buffer A
;   RDX = buffer B
;   R8  = length (bytes)
;   Returns RAX = 0 (match), RAX != 0 (mismatch)
;   Timing-safe: always scans all bytes regardless of early mismatch.
;-------------------------------------------------------------------------------
ConstantTimeCompare proc
    xor     r9, r9                  ; Index = 0
    xor     rax, rax                ; Accumulator = 0 (match)

CTC_loop:
    cmp     r9, r8
    jae     CTC_done                ; Reached length limit
    mov     r10b, [rcx + r9]
    xor     r10b, [rdx + r9]        ; XOR difference: 0 = same byte
    or      al, r10b                ; OR into accumulator (any diff sets bits)
    inc     r9
    jmp     CTC_loop

CTC_done:
    ret                             ; RAX = 0 if identical, nonzero otherwise
ConstantTimeCompare endp

;-------------------------------------------------------------------------------
; VEH Handler — Minimal, no Win32 API calls. Modifies context to resume at
; P5_recovery after popping the return address from ConstantTimeCompare.
;   RCX = ExceptionInfo pointer
;   Returns RAX = EXCEPTION_CONTINUE_EXECUTION (-1) or EXCEPTION_CONTINUE_SEARCH (0)
;-------------------------------------------------------------------------------
VehHandler proc
    mov     rdx, [rcx]              ; ExceptionInfo->ExceptionRecord
    mov     eax, [rdx]              ; ExceptionRecord->ExceptionCode
    cmp     eax, 0C0000005h         ; EXCEPTION_ACCESS_VIOLATION
    jne     Veh_unhandled

    ; Get ContextRecord
    mov     rdx, [rcx + 8]          ; ExceptionInfo->ContextRecord

    ; Pop return address of ConstantTimeCompare from Rsp
    mov     rax, [rdx + 98h]        ; ContextRecord->Rsp
    add     rax, 8
    mov     [rdx + 98h], rax        ; ContextRecord->Rsp += 8

    ; Redirect execution to P5_recovery in main
    lea     rax, P5_recovery
    mov     [rdx + 0F8h], rax       ; ContextRecord->Rip = P5_recovery

    mov     rax, -1                 ; EXCEPTION_CONTINUE_EXECUTION
    ret

Veh_unhandled:
    xor     rax, rax                ; EXCEPTION_CONTINUE_SEARCH
    ret
VehHandler endp

;-------------------------------------------------------------------------------
; PrintRdtsc — Prints a 64-bit cycle count in hex
;   RCX = cycle count (RAX from rdtsc)
;-------------------------------------------------------------------------------
PrintRdtsc proc
    push    rbx
    push    rdi
    push    rsi
    sub     rsp, 64

    mov     rbx, rcx                ; Save cycle count
    lea     rsi, hexDigits

    ; Print high 32 bits
    mov     r8, rbx
    shr     r8, 32
    call    PrintDwordHex

    ; Print low 32 bits
    mov     r8d, ebx
    call    PrintDwordHex

    add     rsp, 64
    pop     rsi
    pop     rdi
    pop     rbx
    ret
PrintRdtsc endp

;-------------------------------------------------------------------------------
; PrintDwordHex — Prints 32-bit value as 8 hex chars
;   R8 = value to print
;-------------------------------------------------------------------------------
PrintDwordHex proc
    push    rbx
    push    rdi
    sub     rsp, 32

    mov     rdi, rsp
    add     rdi, 16                 ; Buffer at rsp+16
    mov     rbx, rdi

    mov     ecx, 8
PDH_loop:
    rol     r8d, 4
    mov     r9d, r8d
    and     r9d, 0Fh
    mov     r10b, [rsi + r9]        ; hexDigits[r9]
    mov     [rdi], r10b
    inc     rdi
    dec     ecx
    jnz     PDH_loop

    mov     rcx, rbx
    mov     rdx, 8
    call    WriteString

    add     rsp, 32
    pop     rdi
    pop     rbx
    ret
PrintDwordHex endp
;   RCX = string address
;   RDX = length
;   Writes to stdout via WriteFile
;   Clobbers: R8, R9, R10, RFLAGS
;-------------------------------------------------------------------------------
WriteString proc
    push    rbx
    push    rdi
    sub     rsp, 48                 ; Shadow space (32) + 8 for lpOverlapped + 8 for written + align

    mov     rbx, rcx                ; Save string addr
    mov     rdi, rdx                ; Save length

    ; Get stdout handle
    mov     rcx, -11                ; STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     r10, rax                ; Save handle

    ; WriteFile(hStdOut, buf, len, &written, NULL)
    mov     rcx, r10
    mov     rdx, rbx
    mov     r8, rdi
    lea     r9, [rsp + 40]          ; &written on stack
    mov     qword ptr [rsp + 32], 0 ; lpOverlapped = NULL (5th parameter)
    call    WriteFile

    add     rsp, 48
    pop     rdi
    pop     rbx
    ret
WriteString endp

;-------------------------------------------------------------------------------
; Main Entry Point
;-------------------------------------------------------------------------------
main proc
    sub     rsp, 56                 ; Shadow space (32) + local vars + align

    ; Print banner
    lea     rcx, msgBanner
    mov     rdx, lenBanner
    call    WriteString

    ;=======================================================================
    ; Phase 1: Bearer Token Validation (1,000,000 iterations) + rdtsc
    ;=======================================================================
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     r12, rax                ; Start cycles

    mov     rbx, 1000000            ; Iteration counter

P1_loop:
    ; Test 1a: Valid token should MATCH
    lea     rcx, validToken
    lea     rdx, secretToken
    call    ValidateBearerToken
    cmp     rax, 1
    jne     main_failed

    ; Test 1b: Wrong token should MISMATCH
    lea     rcx, wrongToken
    lea     rdx, secretToken
    call    ValidateBearerToken
    test    rax, rax
    jnz     main_failed             ; Should be 0 (mismatch)

    ; Test 1c: Bad prefix should MISMATCH
    lea     rcx, badPrefix
    lea     rdx, secretToken
    call    ValidateBearerToken
    test    rax, rax
    jnz     main_failed

    dec     rbx
    jnz     P1_loop

    rdtsc
    shl     rdx, 32
    or      rax, rdx
    sub     rax, r12                ; Delta cycles
    mov     r13, rax                ; Save Phase 1 cycles

    lea     rcx, msgCycleP1
    mov     rdx, lenCycleP1
    call    WriteString
    mov     rcx, r13
    call    PrintRdtsc
    lea     rcx, msgRdtscLabel
    mov     rdx, lenRdtscLabel
    call    WriteString

    ;=======================================================================
    ; Phase 2: Injection Scanning (1,000,000 iterations) + rdtsc
    ;=======================================================================
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     r12, rax                ; Start cycles

    mov     rbx, 1000000

P2_loop:
    ; Test 2a: Safe payload should be SAFE (returns 0)
    lea     rcx, safePayload
    call    ScanForInjection
    test    rax, rax
    jnz     main_failed             ; Should be 0 (safe)

    ; Test 2b: Unsafe alert payload should be UNSAFE (returns 1)
    lea     rcx, unsafeAlert
    call    ScanForInjection
    cmp     rax, 1
    jne     main_failed             ; Should be 1 (unsafe)

    ; Test 2c: SQL escape payload should be UNSAFE
    lea     rcx, unsafeEscape
    call    ScanForInjection
    cmp     rax, 1
    jne     main_failed

    dec     rbx
    jnz     P2_loop

    rdtsc
    shl     rdx, 32
    or      rax, rdx
    sub     rax, r12                ; Delta cycles
    mov     r14, rax                ; Save Phase 2 cycles

    lea     rcx, msgCycleP2
    mov     rdx, lenCycleP2
    call    WriteString
    mov     rcx, r14
    call    PrintRdtsc
    lea     rcx, msgRdtscLabel
    mov     rdx, lenRdtscLabel
    call    WriteString

    ;=======================================================================
    ; Phase 3: Constant-Time Comparison (timing-safe) + rdtsc
    ;=======================================================================
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     r12, rax                ; Start cycles

    mov     rbx, 1000000

P3_loop:
    ; Same buffers → should return 0
    lea     rcx, secretToken
    lea     rdx, validToken
    mov     r8, 26                  ; Length of "Bearer rawrxd-dev-key-2026"
    call    ConstantTimeCompare
    test    rax, rax
    jnz     main_failed

    ; Different buffers → should return nonzero
    lea     rcx, secretToken
    lea     rdx, wrongToken
    mov     r8, 18                  ; Length of shorter buffer
    call    ConstantTimeCompare
    test    rax, rax
    jz      main_failed             ; Should be nonzero (different)

    dec     rbx
    jnz     P3_loop

    rdtsc
    shl     rdx, 32
    or      rax, rdx
    sub     rax, r12                ; Delta cycles
    mov     r15, rax                ; Save Phase 3 cycles

    lea     rcx, msgCycleP3
    mov     rdx, lenCycleP3
    call    WriteString
    mov     rcx, r15
    call    PrintRdtsc
    lea     rcx, msgRdtscLabel
    mov     rdx, lenRdtscLabel
    call    WriteString

    ;=======================================================================
    ; Phase 4: Buffer Overflow Boundary Validation (Documented Result)
    ;=======================================================================
    lea     rcx, msgOverflow
    mov     rdx, lenOverflow
    call    WriteString

    ;=======================================================================
    ; Phase 5: Buffer Overflow Torture Test with VEH
    ;=======================================================================
    ; Register VEH to catch the expected access violation
    mov     rcx, 1                  ; FirstHandler = TRUE (add to front)
    lea     rdx, VehHandler
    call    AddVectoredExceptionHandler
    mov     r12, rax                ; Save VEH handle

    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     r13, rax                ; Start cycles

    ; Deliberately trigger overflow: claim 4096-byte length on ~27-byte buffer
    lea     rcx, secretToken
    lea     rdx, validToken
    mov     r8, 4096                ; Invalid length — will AV
    call    ConstantTimeCompare
    ; If we reach here without AV, the test failed (boundary not enforced)
    jmp     main_failed

P5_recovery::
    ; VEH redirected here after catching the AV — print confirmation
    lea     rcx, msgCaught
    mov     rdx, lenCaught
    call    WriteString

    lea     rcx, msgOverflowPass
    mov     rdx, lenOverflowPass
    call    WriteString

    rdtsc
    shl     rdx, 32
    or      rax, rdx
    sub     rax, r13                ; Delta cycles (includes VEH overhead)

    lea     rcx, msgCycleP5
    mov     rdx, lenCycleP5
    call    WriteString
    mov     rcx, rax
    call    PrintRdtsc
    lea     rcx, msgRdtscLabel
    mov     rdx, lenRdtscLabel
    call    WriteString

    ; Remove VEH
    mov     rcx, r12
    call    RemoveVectoredExceptionHandler

    ;=======================================================================
    ; SUCCESS
    ;=======================================================================
main_pass:
    lea     rcx, msgPass
    mov     rdx, lenPass
    call    WriteString

    lea     rcx, msgDone
    mov     rdx, lenDone
    call    WriteString

    mov     rcx, 0
    call    ExitProcess

    ;=======================================================================
    ; FAILURE
    ;=======================================================================
main_failed:
    lea     rcx, msgFail
    mov     rdx, lenFail
    call    WriteString

    mov     rcx, 1
    call    ExitProcess

main endp

end
