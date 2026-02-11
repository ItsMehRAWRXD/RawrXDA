; ╔══════════════════════════════════════════════════════════════════════════════╗
; ║ quantum_auth.asm - GHOST-C2 Entropy Binding for RawrXD Thermal Control      ║
; ║                                                                              ║
; ║ Purpose: Generate hardware-backed session keys using RDRAND/RDSEED for      ║
; ║          secure authentication between C++ thermal logic and MASM kernel    ║
; ║                                                                              ║
; ║ Author: RawrXD IDE Team                                                      ║
; ║ Version: 1.2.0                                                               ║
; ║ Target: x86-64 / AMD64 / Intel EM64T                                        ║
; ╚══════════════════════════════════════════════════════════════════════════════╝

; ═══════════════════════════════════════════════════════════════════════════════
; Build: ml64 /c /Fo quantum_auth.obj quantum_auth.asm
;        link quantum_auth.obj /DLL /OUT:quantum_auth.dll /DEF:quantum_auth.def
; ═══════════════════════════════════════════════════════════════════════════════

.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; RDRAND Feature Detection
; ═══════════════════════════════════════════════════════════════════════════════
; bool hasRdrand()
; Returns: 1 if RDRAND is supported, 0 otherwise
; ═══════════════════════════════════════════════════════════════════════════════

hasRdrand PROC
    push    rbx                     ; Save RBX (CPUID clobbers it)
    
    ; Check CPUID leaf 1, ECX bit 30 = RDRAND
    mov     eax, 1
    cpuid
    bt      ecx, 30                 ; Test RDRAND bit
    setc    al                      ; Set AL = 1 if carry (RDRAND supported)
    movzx   eax, al                 ; Zero-extend to EAX
    
    pop     rbx
    ret
hasRdrand ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RDSEED Feature Detection
; ═══════════════════════════════════════════════════════════════════════════════
; bool hasRdseed()
; Returns: 1 if RDSEED is supported, 0 otherwise
; ═══════════════════════════════════════════════════════════════════════════════

hasRdseed PROC
    push    rbx
    
    ; Check CPUID leaf 7, subleaf 0, EBX bit 18 = RDSEED
    mov     eax, 7
    xor     ecx, ecx
    cpuid
    bt      ebx, 18                 ; Test RDSEED bit
    setc    al
    movzx   eax, al
    
    pop     rbx
    ret
hasRdseed ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Generate 32-bit Random Value using RDRAND
; ═══════════════════════════════════════════════════════════════════════════════
; uint32_t generateRandom32()
; Returns: 32-bit random value, or 0 on failure
; ═══════════════════════════════════════════════════════════════════════════════

generateRandom32 PROC
    mov     ecx, 10                 ; Max retry count
    
@@retry32:
    rdrand  eax                     ; Try to get random value
    jc      @@success32             ; Jump if carry = success
    
    dec     ecx
    jnz     @@retry32               ; Retry if not exhausted
    
    ; All retries failed
    xor     eax, eax
    ret
    
@@success32:
    ret
generateRandom32 ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Generate 64-bit Random Value using RDRAND
; ═══════════════════════════════════════════════════════════════════════════════
; uint64_t generateRandom64()
; Returns: 64-bit random value, or 0 on failure
; ═══════════════════════════════════════════════════════════════════════════════

generateRandom64 PROC
    mov     ecx, 10                 ; Max retry count
    
@@retry64:
    rdrand  rax                     ; Try to get 64-bit random value
    jc      @@success64             ; Jump if carry = success
    
    dec     ecx
    jnz     @@retry64               ; Retry if not exhausted
    
    ; All retries failed
    xor     rax, rax
    ret
    
@@success64:
    ret
generateRandom64 ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Generate Session Key using RDRAND
; ═══════════════════════════════════════════════════════════════════════════════
; uint64_t generateSessionKey()
; Returns: 64-bit session key combining multiple random values
; ═══════════════════════════════════════════════════════════════════════════════

generateSessionKey PROC
    push    rbx
    push    r12
    
    ; Get first random value
    mov     ecx, 10
@@key_rand1:
    rdrand  rax
    jc      @@key_got1
    dec     ecx
    jnz     @@key_rand1
    xor     rax, rax                ; Failure
    jmp     @@key_done
    
@@key_got1:
    mov     r12, rax                ; Save first value
    
    ; Get second random value
    mov     ecx, 10
@@key_rand2:
    rdrand  rbx
    jc      @@key_got2
    dec     ecx
    jnz     @@key_rand2
    xor     rax, rax                ; Failure
    jmp     @@key_done
    
@@key_got2:
    ; Combine values: XOR + rotate for better distribution
    xor     r12, rbx
    ror     r12, 17                 ; Rotate right by prime number
    
    ; Mix in timestamp counter for additional entropy
    rdtsc                           ; EDX:EAX = timestamp
    shl     rdx, 32
    or      rax, rdx                ; RAX = full 64-bit TSC
    xor     r12, rax
    
    mov     rax, r12                ; Return combined key
    
@@key_done:
    pop     r12
    pop     rbx
    ret
generateSessionKey ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Generate Seed Value using RDSEED (Higher Quality Entropy)
; ═══════════════════════════════════════════════════════════════════════════════
; uint64_t generateSeed64()
; Returns: 64-bit true random seed, or 0 on failure
; ═══════════════════════════════════════════════════════════════════════════════

generateSeed64 PROC
    mov     ecx, 100                ; RDSEED may need more retries
    
@@retry_seed:
    rdseed  rax                     ; Try to get seed value
    jc      @@seed_success          ; Jump if carry = success
    
    ; RDSEED entropy pool may be exhausted, brief pause
    pause
    dec     ecx
    jnz     @@retry_seed
    
    ; Fall back to RDRAND if RDSEED fails
    jmp     generateRandom64
    
@@seed_success:
    ret
generateSeed64 ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Fill Buffer with Random Bytes
; ═══════════════════════════════════════════════════════════════════════════════
; int fillRandomBuffer(void* buffer, size_t size)
; RCX = buffer pointer
; RDX = size in bytes
; Returns: Number of bytes filled, or 0 on failure
; ═══════════════════════════════════════════════════════════════════════════════

fillRandomBuffer PROC
    push    rbx
    push    r12
    push    r13
    push    r14
    
    mov     r12, rcx                ; Buffer pointer
    mov     r13, rdx                ; Size
    xor     r14, r14                ; Bytes filled
    
    ; Fill in 8-byte chunks
@@fill_loop:
    cmp     r13, 8
    jb      @@fill_remaining
    
    ; Get random 64-bit value
    mov     ecx, 10
@@fill_rand:
    rdrand  rax
    jc      @@fill_store
    dec     ecx
    jnz     @@fill_rand
    jmp     @@fill_done             ; Failed, return what we have
    
@@fill_store:
    mov     [r12], rax
    add     r12, 8
    sub     r13, 8
    add     r14, 8
    jmp     @@fill_loop
    
@@fill_remaining:
    ; Handle remaining bytes (< 8)
    test    r13, r13
    jz      @@fill_done
    
    mov     ecx, 10
@@fill_rand_rem:
    rdrand  eax
    jc      @@fill_rem_store
    dec     ecx
    jnz     @@fill_rand_rem
    jmp     @@fill_done
    
@@fill_rem_store:
    ; Store remaining bytes
@@fill_rem_loop:
    test    r13, r13
    jz      @@fill_done
    mov     [r12], al
    shr     eax, 8
    inc     r12
    dec     r13
    inc     r14
    jmp     @@fill_rem_loop
    
@@fill_done:
    mov     rax, r14                ; Return bytes filled
    
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
fillRandomBuffer ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Validate Session Key Against Control Block
; ═══════════════════════════════════════════════════════════════════════════════
; bool validateSessionKey(void* controlBlock, uint64_t expectedKey)
; RCX = control block pointer
; RDX = expected session key
; Returns: 1 if valid, 0 if invalid
; ═══════════════════════════════════════════════════════════════════════════════

; Control block offsets (from SovereignControlBlock.h)
SCB_AUTH_OFFSET         EQU 320
SCB_SESSION_KEY_OFFSET  EQU 0       ; Relative to auth section

validateSessionKey PROC
    ; Get stored session key from control block
    mov     rax, [rcx + SCB_AUTH_OFFSET + SCB_SESSION_KEY_OFFSET]
    
    ; Compare with expected key
    cmp     rax, rdx
    sete    al                      ; AL = 1 if equal
    movzx   eax, al
    ret
validateSessionKey ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Write Session Key to Control Block
; ═══════════════════════════════════════════════════════════════════════════════
; void writeSessionKey(void* controlBlock, uint64_t key)
; RCX = control block pointer
; RDX = session key to write
; ═══════════════════════════════════════════════════════════════════════════════

SCB_KEY_VALID_OFFSET    EQU 16      ; Relative to auth section
SCB_AUTH_ATTEMPTS       EQU 20      ; Relative to auth section

writeSessionKey PROC
    ; Write session key
    mov     [rcx + SCB_AUTH_OFFSET + SCB_SESSION_KEY_OFFSET], rdx
    
    ; Set key valid flag
    mov     dword ptr [rcx + SCB_AUTH_OFFSET + SCB_KEY_VALID_OFFSET], 1
    
    ; Reset auth attempts
    mov     dword ptr [rcx + SCB_AUTH_OFFSET + SCB_AUTH_ATTEMPTS], 0
    
    ret
writeSessionKey ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Generate and Install Session Key (Combined Operation)
; ═══════════════════════════════════════════════════════════════════════════════
; uint64_t generateAndInstallKey(void* controlBlock)
; RCX = control block pointer
; Returns: Generated session key, or 0 on failure
; ═══════════════════════════════════════════════════════════════════════════════

generateAndInstallKey PROC
    push    rbx
    
    mov     rbx, rcx                ; Save control block pointer
    
    ; Generate session key
    call    generateSessionKey
    test    rax, rax
    jz      @@gen_fail              ; Failed to generate
    
    ; Install key in control block
    mov     rcx, rbx
    mov     rdx, rax
    push    rax                     ; Save key for return
    call    writeSessionKey
    pop     rax                     ; Restore key
    
    pop     rbx
    ret
    
@@gen_fail:
    xor     rax, rax
    pop     rbx
    ret
generateAndInstallKey ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Entropy Quality Test
; ═══════════════════════════════════════════════════════════════════════════════
; uint32_t testEntropyQuality(int samples)
; ECX = number of samples to test
; Returns: Quality score 0-100 (100 = perfect randomness)
; ═══════════════════════════════════════════════════════════════════════════════

testEntropyQuality PROC
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    
    mov     r12d, ecx               ; Sample count
    xor     r13, r13                ; Bit count (ones)
    xor     r14, r14                ; Total bits
    xor     r15, r15                ; Sample counter
    
@@entropy_loop:
    cmp     r15d, r12d
    jge     @@entropy_calc
    
    ; Get random value
    rdrand  rbx
    jnc     @@entropy_skip          ; Skip failed reads
    
    ; Count ones (popcount)
    popcnt  rax, rbx
    add     r13, rax
    add     r14, 64                 ; 64 bits per sample
    
@@entropy_skip:
    inc     r15d
    jmp     @@entropy_loop
    
@@entropy_calc:
    ; Calculate quality: ideal is 50% ones
    ; Score = 100 - |50 - actual_percentage| * 2
    
    test    r14, r14
    jz      @@entropy_zero
    
    ; Calculate percentage of ones (r13 * 100 / r14)
    mov     rax, r13
    mov     rcx, 100
    mul     rcx
    div     r14                     ; RAX = percentage
    
    ; Calculate deviation from 50%
    mov     rcx, rax
    sub     rcx, 50
    ; Absolute value
    test    rcx, rcx
    jns     @@entropy_pos
    neg     rcx
@@entropy_pos:
    ; Score = 100 - deviation * 2
    shl     rcx, 1
    mov     eax, 100
    sub     eax, ecx
    
    ; Clamp to 0-100
    test    eax, eax
    jns     @@entropy_done
    xor     eax, eax
    jmp     @@entropy_done
    
@@entropy_zero:
    xor     eax, eax
    
@@entropy_done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
testEntropyQuality ENDP

END
