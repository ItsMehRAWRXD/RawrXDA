;=============================================================================
; Pure MASM Random Number Generator (RAND_bytes replacement)
; Zero dependencies - uses Windows CryptoAPI or RDRAND instruction
;=============================================================================

.686
.XMM
.MODEL flat, c
OPTION casemap:none

;=============================================================================
; External Windows API imports
;=============================================================================
extern __imp__CryptAcquireContextA@20:DWORD
extern __imp__CryptGenRandom@12:DWORD
extern __imp__CryptReleaseContext@8:DWORD

;=============================================================================
; RDRAND instruction support check
;=============================================================================
.CODE

;-----------------------------------------------------------------------------
; masm_crypto_rand_init - Initialize RNG (called once)
; Returns: 0=success, -1=error
;-----------------------------------------------------------------------------
masm_crypto_rand_init PROC
    ; Check for RDRAND support via CPUID
    mov eax, 1
    cpuid
    test ecx, 40000000h  ; Check bit 30 (RDRAND support)
    jz @F_use_windows_crypto
    
    ; RDRAND available - use hardware RNG
    mov eax, 1
    ret
    
@F_use_windows_crypto:
    ; Fall back to Windows CryptoAPI
    ; We'll acquire context on first use
    xor eax, eax
    ret
masm_crypto_rand_init ENDP

;-----------------------------------------------------------------------------
; masm_crypto_rand_bytes - Generate random bytes (RAND_bytes replacement)
; Input:  buf = pointer to buffer
;         num = number of bytes to generate
; Returns: 1=success, 0=failure
;-----------------------------------------------------------------------------
masm_crypto_rand_bytes PROC C, buf:DWORD, num:DWORD
    LOCAL hProv:DWORD
    LOCAL bytesLeft:DWORD
    LOCAL currentBuf:DWORD
    
    mov eax, num
    mov bytesLeft, eax
    mov eax, buf
    mov currentBuf, eax
    
    ; Try RDRAND first if available
    call masm_crypto_rand_init
    cmp eax, 1
    jne @F_use_cryptoapi
    
    ; Use RDRAND instruction
@R_use_rdrand:
    mov eax, bytesLeft
    cmp eax, 0
    je @R_success
    
    ; Generate 4 bytes at a time
    mov ecx, bytesLeft
    shr ecx, 2  ; Divide by 4
    jz @R_handle_remainder
    
@R_gen_loop:
    rdrand eax
    jc @R_rdrand_ok
    ; RDRAND failed, fall back to CryptoAPI
    jmp @F_use_cryptoapi
    
@R_rdrand_ok:
    mov ebx, currentBuf
    mov [ebx], eax
    mov eax, currentBuf
    add eax, 4
    mov currentBuf, eax
    mov eax, bytesLeft
    sub eax, 4
    mov bytesLeft, eax
    loop @R_gen_loop
    
@R_handle_remainder:
    mov ecx, bytesLeft
    and ecx, 3  ; Remainder bytes (0-3)
    jz @R_success
    
@R_gen_byte:
    rdrand eax
    jc @R_byte_ok
    jmp @F_use_cryptoapi
    
@R_byte_ok:
    mov ebx, currentBuf
    mov [ebx], al
    mov eax, currentBuf
    inc eax
    mov currentBuf, eax
    mov eax, bytesLeft
    dec eax
    mov bytesLeft, eax
    loop @R_gen_byte
    
@R_success:
    mov eax, 1
    ret
    
@F_use_cryptoapi:
    ; Use Windows CryptoAPI
    lea eax, hProv
    push eax
    push 0
    push 0
    push 0
    push 18h  ; PROV_RSA_FULL
    push 0
    call __imp__CryptAcquireContextA@20
    
    test eax, eax
    jz @F_error
    
    ; Generate random bytes
    push bytesLeft
    push currentBuf
    push hProv
    call __imp__CryptGenRandom@12
    
    test eax, eax
    jz @F_release_error
    
    ; Release context
    push hProv
    push 0
    call __imp__CryptReleaseContext@8
    
    mov eax, 1
    ret
    
@F_release_error:
    push hProv
    push 0
    call __imp__CryptReleaseContext@8
    
@F_error:
    xor eax, eax
    ret
masm_crypto_rand_bytes ENDP

;-----------------------------------------------------------------------------
; masm_crypto_rand_cleanup - Cleanup RNG resources
;-----------------------------------------------------------------------------
masm_crypto_rand_cleanup PROC
    ; Nothing to cleanup for RDRAND
    ; CryptoAPI contexts are released per-call
    ret
masm_crypto_rand_cleanup ENDP

END