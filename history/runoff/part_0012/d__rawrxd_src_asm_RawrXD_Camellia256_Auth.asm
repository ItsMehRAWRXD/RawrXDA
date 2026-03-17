; =============================================================================
; RawrXD_Camellia256_Auth.asm — Authenticated Encrypt/Decrypt Engine (x64 MASM)
; =============================================================================
;
; Pure MASM implementation of Encrypt-then-MAC authenticated encryption using
; the Camellia-256 CTR-mode cipher and HMAC-SHA256 authentication.
; Reverse-engineered from camellia256_bridge.cpp into bare-metal x64.
;
; File Format ("Project Lockdown" RCM2):
;   Offset  Size  Description
;   ------  ----  -----------
;        0     4  Magic: "RCM2" (0x52434D32 little-endian)
;        4     4  Version: 0x00020000
;        8    16  Nonce / IV (random, for CTR mode)
;       24    32  HMAC-SHA256 (covers: magic + version + nonce + ciphertext)
;       56     N  Ciphertext (CTR-mode encrypted plaintext)
;
; Security Properties:
;   - Encrypt-then-MAC: HMAC is computed over header + ciphertext (post-encrypt)
;   - Constant-time HMAC comparison prevents timing side-channels
;   - HMAC key is derived separately from encryption key (domain separation)
;   - Nonce is generated via BCryptGenRandom (CSPRNG)
;   - All key material is zeroed after use via volatile writes
;
; Exports (called from camellia256_bridge.cpp):
;   asm_camellia256_auth_encrypt_file  — Authenticated file encryption
;   asm_camellia256_auth_decrypt_file  — Authenticated file decryption
;   asm_camellia256_auth_encrypt_buf   — In-memory authenticated encryption
;   asm_camellia256_auth_decrypt_buf   — In-memory authenticated decryption
;
; Dependencies (from RawrXD_Camellia256.asm, linked in same binary):
;   asm_camellia256_encrypt_ctr   — CTR mode encrypt/decrypt
;   asm_camellia256_decrypt_ctr   — CTR mode decrypt (same as encrypt for CTR)
;   asm_camellia256_get_hmac_key  — Get 32-byte HMAC authentication key
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /I src/asm /Fo RawrXD_Camellia256_Auth.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                    CONSTANTS
; =============================================================================

AUTH_MAGIC               EQU     052434D32h  ; 'RCM2' little-endian
AUTH_VERSION             EQU     000020000h  ; v2.0
AUTH_HEADER_SIZE         EQU     56          ; 4 + 4 + 16 + 32
AUTH_PRE_HMAC_SIZE       EQU     24          ; magic(4) + version(4) + nonce(16)
AUTH_NONCE_SIZE          EQU     16
AUTH_HMAC_SIZE           EQU     32

; HMAC-SHA256 construction constants (RFC 2104)
HMAC_BLOCK_SIZE          EQU     64          ; SHA-256 input block = 64 bytes
HMAC_IPAD_BYTE           EQU     36h         ; Inner padding byte
HMAC_OPAD_BYTE           EQU     5Ch         ; Outer padding byte

; Error codes (must match RawrXD_Camellia256.asm)
CAMELLIA_OK              EQU     0
CAMELLIA_ERR_NOKEY       EQU     -1
CAMELLIA_ERR_NULLPTR     EQU     -2
CAMELLIA_ERR_FILEOPEN    EQU     -3
CAMELLIA_ERR_FILEREAD    EQU     -4
CAMELLIA_ERR_FILEWRITE   EQU     -5
CAMELLIA_ERR_ALLOC       EQU     -6
CAMELLIA_ERR_AUTH        EQU     -7

; SHA-256 CryptoAPI constants
CALG_SHA_256_LOCAL       EQU     0000800Ch
PROV_RSA_AES_LOCAL       EQU     24
HP_HASHVAL_LOCAL         EQU     0002h

; =============================================================================
;                    WIN32 API IMPORTS
; =============================================================================

EXTERNDEF CreateFileA:PROC
EXTERNDEF ReadFile:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF GetFileSize:PROC
EXTERNDEF VirtualAlloc:PROC
EXTERNDEF VirtualFree:PROC
EXTERNDEF BCryptGenRandom:PROC
EXTERNDEF OutputDebugStringA:PROC
EXTERNDEF CryptAcquireContextA:PROC
EXTERNDEF CryptCreateHash:PROC
EXTERNDEF CryptHashData:PROC
EXTERNDEF CryptGetHashParam:PROC
EXTERNDEF CryptDestroyHash:PROC
EXTERNDEF CryptReleaseContext:PROC

; Dependencies from RawrXD_Camellia256.asm (linked in same binary)
EXTERNDEF asm_camellia256_encrypt_ctr:PROC
EXTERNDEF asm_camellia256_decrypt_ctr:PROC
EXTERNDEF asm_camellia256_get_hmac_key:PROC

; =============================================================================
;                    EXPORTS
; =============================================================================

PUBLIC asm_camellia256_auth_encrypt_file
PUBLIC asm_camellia256_auth_decrypt_file
PUBLIC asm_camellia256_auth_encrypt_buf
PUBLIC asm_camellia256_auth_decrypt_buf

; =============================================================================
;                    DATA SECTION
; =============================================================================

.data

ALIGN 16
cam_auth_str_enc_ok      DB "[RawrXD] Auth: File encrypted + HMAC-SHA256 authenticated (RCM2)", 13, 10, 0
cam_auth_str_dec_ok      DB "[RawrXD] Auth: File decrypted + HMAC verified (RCM2)", 13, 10, 0
cam_auth_str_hmac_fail   DB "[RawrXD] CRITICAL: HMAC-SHA256 mismatch — tamper/corruption detected!", 13, 10, 0
cam_auth_str_magic_fail  DB "[RawrXD] ERROR: Invalid RCM2 magic in encrypted file header", 13, 10, 0
cam_auth_str_ver_fail    DB "[RawrXD] ERROR: Unsupported RCM2 file format version", 13, 10, 0
cam_auth_str_size_fail   DB "[RawrXD] ERROR: File too small for authenticated format (need >= 57 bytes)", 13, 10, 0
cam_auth_str_alloc_fail  DB "[RawrXD] ERROR: VirtualAlloc failed for auth I/O buffer", 13, 10, 0
cam_auth_str_crypto_fail DB "[RawrXD] ERROR: CryptoAPI failure during HMAC computation", 13, 10, 0

; =============================================================================
;                    BSS SECTION — Auth Engine Workspace
; =============================================================================

.data?

ALIGN 16
; HMAC key cached from main Camellia engine (32 bytes)
cam_auth_hmac_key        DB  32 DUP(?)

; HMAC ipad/opad blocks (RFC 2104: key XOR'd with pad, zero-padded to 64 bytes)
cam_auth_ipad            DB  64 DUP(?)
cam_auth_opad            DB  64 DUP(?)

; SHA-256 intermediate digest (inner hash result)
cam_auth_inner_digest    DB  32 DUP(?)

; Final computed HMAC-SHA256 result
cam_auth_computed_hmac   DB  32 DUP(?)

; Nonce / CTR state for current operation
cam_auth_nonce           DB  16 DUP(?)
cam_auth_ctr_state       DB  16 DUP(?)

; Pre-HMAC header buffer (magic + version + nonce = 24 bytes)
cam_auth_header_buf      DB  24 DUP(?)

; CryptoAPI handles for HMAC computation
cam_auth_hProv           DQ  ?
cam_auth_hHash           DQ  ?
cam_auth_hashLen         DD  ?

; File I/O state
cam_auth_hInput          DQ  ?
cam_auth_hOutput         DQ  ?
cam_auth_fileSize        DQ  ?
cam_auth_pBuf            DQ  ?
cam_auth_bytesRW         DD  ?

; =============================================================================
;                    CODE SECTION
; =============================================================================

.code

; =============================================================================
; Internal: cam_auth_build_hmac_pads — Build ipad/opad blocks from HMAC key
;
; RFC 2104 HMAC construction:
;   ipad[0..31]  = hmac_key[0..31] XOR 0x36
;   ipad[32..63] = 0x36 (zero-padded key region)
;   opad[0..31]  = hmac_key[0..31] XOR 0x5C
;   opad[32..63] = 0x5C (zero-padded key region)
;
; Uses BSS: cam_auth_hmac_key, cam_auth_ipad, cam_auth_opad
; Clobbers: RAX, RCX, RDX, RSI, RDI
; =============================================================================
ALIGN 16
cam_auth_build_hmac_pads PROC
    lea     rsi, [cam_auth_hmac_key]
    lea     rdi, [cam_auth_ipad]
    lea     rdx, [cam_auth_opad]

    ; Bytes 0..31: key[i] XOR pad_byte
    xor     ecx, ecx
@@pad_key_loop:
    cmp     ecx, 32
    jge     @@pad_fill_loop
    movzx   eax, BYTE PTR [rsi + rcx]
    mov     r8d, eax
    xor     eax, HMAC_IPAD_BYTE
    mov     BYTE PTR [rdi + rcx], al
    xor     r8d, HMAC_OPAD_BYTE
    mov     BYTE PTR [rdx + rcx], r8b
    inc     ecx
    jmp     @@pad_key_loop

    ; Bytes 32..63: pad_byte (key is only 32 bytes, rest is zero-padded)
@@pad_fill_loop:
    cmp     ecx, HMAC_BLOCK_SIZE
    jge     @@pad_done
    mov     BYTE PTR [rdi + rcx], HMAC_IPAD_BYTE
    mov     BYTE PTR [rdx + rcx], HMAC_OPAD_BYTE
    inc     ecx
    jmp     @@pad_fill_loop

@@pad_done:
    ret
cam_auth_build_hmac_pads ENDP

; =============================================================================
; Internal: cam_auth_constant_time_cmp — Side-channel-safe 32-byte comparison
;
; Timing is constant regardless of where (or whether) bytes differ.
; Uses OR-accumulator across all QWORD comparisons.
;
; Input:  RCX = pointer A (32 bytes), RDX = pointer B (32 bytes)
; Output: EAX = 0 if equal, 1 if different
; Clobbers: R8, R9
; =============================================================================
ALIGN 16
cam_auth_constant_time_cmp PROC
    xor     r8, r8                      ; accumulator = 0

    ; Compare 4 x QWORD (32 bytes total), OR-ing any differences
    mov     r9, QWORD PTR [rcx]
    xor     r9, QWORD PTR [rdx]
    or      r8, r9

    mov     r9, QWORD PTR [rcx + 8]
    xor     r9, QWORD PTR [rdx + 8]
    or      r8, r9

    mov     r9, QWORD PTR [rcx + 16]
    xor     r9, QWORD PTR [rdx + 16]
    or      r8, r9

    mov     r9, QWORD PTR [rcx + 24]
    xor     r9, QWORD PTR [rdx + 24]
    or      r8, r9

    ; If r8 == 0, all 32 bytes matched
    test    r8, r8
    setnz   al
    movzx   eax, al
    ret
cam_auth_constant_time_cmp ENDP

; =============================================================================
; Internal: cam_auth_hmac_compute — Streaming HMAC-SHA256 over two data segments
;
; Computes HMAC-SHA256(key, seg1 || seg2) using CryptoAPI incremental hashing.
; This avoids allocating a contiguous buffer for (header + ciphertext).
;
; HMAC(K, m) = SHA256((K ^ opad) || SHA256((K ^ ipad) || m))
;
; Prerequisite: cam_auth_build_hmac_pads must be called first to populate
;               cam_auth_ipad and cam_auth_opad from cam_auth_hmac_key.
;
; Input:
;   RCX = segment1 pointer
;   RDX = segment1 length (DWORD)
;   R8  = segment2 pointer
;   R9  = segment2 length (DWORD)
;   [rsp+40] = output pointer (32 bytes for HMAC result)
;
; Output: EAX = 0 success, -1 error
; =============================================================================
ALIGN 16
cam_auth_hmac_compute PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 72
    .allocstack 72
    .endprolog

    ; Save parameters in nonvolatile registers
    mov     r12, rcx                    ; seg1 pointer
    mov     r13d, edx                   ; seg1 length
    mov     r14, r8                     ; seg2 pointer
    mov     r15d, r9d                   ; seg2 length
    mov     rbx, QWORD PTR [rbp + 48]  ; output pointer (6th param: rbp+16=retaddr, +48 = arg6 on stack)

    ; ---- CryptAcquireContextA ----
    lea     rcx, [cam_auth_hProv]
    xor     rdx, rdx
    xor     r8, r8
    mov     r9d, PROV_RSA_AES_LOCAL
    mov     rax, 0F0000000h
    mov     QWORD PTR [rsp + 32], rax
    call    CryptAcquireContextA
    test    eax, eax
    jz      @@hmac_fail

    ; ---- Inner hash: SHA256(ipad[64] || seg1 || seg2) ----

    ; CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)
    mov     rcx, QWORD PTR [cam_auth_hProv]
    mov     edx, CALG_SHA_256_LOCAL
    xor     r8, r8
    xor     r9d, r9d
    lea     rax, [cam_auth_hHash]
    mov     QWORD PTR [rsp + 32], rax
    call    CryptCreateHash
    test    eax, eax
    jz      @@hmac_release_prov

    ; CryptHashData(hHash, ipad, 64, 0)
    mov     rcx, QWORD PTR [cam_auth_hHash]
    lea     rdx, [cam_auth_ipad]
    mov     r8d, HMAC_BLOCK_SIZE
    xor     r9d, r9d
    call    CryptHashData
    test    eax, eax
    jz      @@hmac_destroy_inner

    ; CryptHashData(hHash, seg1, seg1Len, 0)
    mov     rcx, QWORD PTR [cam_auth_hHash]
    mov     rdx, r12
    mov     r8d, r13d
    xor     r9d, r9d
    call    CryptHashData
    test    eax, eax
    jz      @@hmac_destroy_inner

    ; CryptHashData(hHash, seg2, seg2Len, 0) — skip if seg2Len == 0
    test    r15d, r15d
    jz      @@hmac_inner_finalize
    mov     rcx, QWORD PTR [cam_auth_hHash]
    mov     rdx, r14
    mov     r8d, r15d
    xor     r9d, r9d
    call    CryptHashData
    test    eax, eax
    jz      @@hmac_destroy_inner

@@hmac_inner_finalize:
    ; CryptGetHashParam(hHash, HP_HASHVAL, inner_digest, &hashLen, 0)
    mov     DWORD PTR [cam_auth_hashLen], 32
    mov     rcx, QWORD PTR [cam_auth_hHash]
    mov     edx, HP_HASHVAL_LOCAL
    lea     r8, [cam_auth_inner_digest]
    lea     r9, [cam_auth_hashLen]
    mov     QWORD PTR [rsp + 32], 0
    call    CryptGetHashParam
    test    eax, eax
    jz      @@hmac_destroy_inner

    ; Destroy inner hash handle
    mov     rcx, QWORD PTR [cam_auth_hHash]
    call    CryptDestroyHash

    ; ---- Outer hash: SHA256(opad[64] || inner_digest[32]) ----

    ; CryptCreateHash for outer
    mov     rcx, QWORD PTR [cam_auth_hProv]
    mov     edx, CALG_SHA_256_LOCAL
    xor     r8, r8
    xor     r9d, r9d
    lea     rax, [cam_auth_hHash]
    mov     QWORD PTR [rsp + 32], rax
    call    CryptCreateHash
    test    eax, eax
    jz      @@hmac_release_prov

    ; Hash opad block (64 bytes)
    mov     rcx, QWORD PTR [cam_auth_hHash]
    lea     rdx, [cam_auth_opad]
    mov     r8d, HMAC_BLOCK_SIZE
    xor     r9d, r9d
    call    CryptHashData
    test    eax, eax
    jz      @@hmac_destroy_outer

    ; Hash inner digest (32 bytes)
    mov     rcx, QWORD PTR [cam_auth_hHash]
    lea     rdx, [cam_auth_inner_digest]
    mov     r8d, 32
    xor     r9d, r9d
    call    CryptHashData
    test    eax, eax
    jz      @@hmac_destroy_outer

    ; Get final HMAC result
    mov     DWORD PTR [cam_auth_hashLen], 32
    mov     rcx, QWORD PTR [cam_auth_hHash]
    mov     edx, HP_HASHVAL_LOCAL
    mov     r8, rbx                     ; output pointer
    lea     r9, [cam_auth_hashLen]
    mov     QWORD PTR [rsp + 32], 0
    call    CryptGetHashParam
    test    eax, eax
    jz      @@hmac_destroy_outer

    ; ---- Success cleanup ----
    mov     rcx, QWORD PTR [cam_auth_hHash]
    call    CryptDestroyHash
    mov     rcx, QWORD PTR [cam_auth_hProv]
    xor     edx, edx
    call    CryptReleaseContext

    ; Secure-zero ipad/opad/inner_digest (contain key-derived material)
    push    rax
    lea     rdi, [cam_auth_ipad]
    xor     eax, eax
    mov     ecx, 64
    rep     stosb
    lea     rdi, [cam_auth_opad]
    xor     eax, eax
    mov     ecx, 64
    rep     stosb
    lea     rdi, [cam_auth_inner_digest]
    xor     eax, eax
    mov     ecx, 32
    rep     stosb
    pop     rax

    xor     eax, eax                    ; return 0 = success
    jmp     @@hmac_exit

@@hmac_destroy_inner:
    mov     rcx, QWORD PTR [cam_auth_hHash]
    call    CryptDestroyHash
    jmp     @@hmac_release_prov

@@hmac_destroy_outer:
    mov     rcx, QWORD PTR [cam_auth_hHash]
    call    CryptDestroyHash

@@hmac_release_prov:
    mov     rcx, QWORD PTR [cam_auth_hProv]
    xor     edx, edx
    call    CryptReleaseContext

@@hmac_fail:
    mov     eax, -1

@@hmac_exit:
    add     rsp, 72
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
cam_auth_hmac_compute ENDP

; =============================================================================
; asm_camellia256_auth_encrypt_file — Authenticated file encryption
;
; Reads input file, encrypts via CTR mode, computes HMAC-SHA256 over
; (header || ciphertext), writes RCM2 authenticated container.
;
; RCX = input file path (null-terminated ANSI)
; RDX = output file path (null-terminated ANSI)
; Returns: EAX = 0 (CAMELLIA_OK) or error code
; =============================================================================
ALIGN 16
asm_camellia256_auth_encrypt_file PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 72
    .allocstack 72
    .endprolog

    ; Validate parameters
    test    rcx, rcx
    jz      @@ae_nullptr
    test    rdx, rdx
    jz      @@ae_nullptr

    mov     r12, rcx                    ; r12 = input path
    mov     r13, rdx                    ; r13 = output path

    ; ---- Step 1: Get HMAC key from main Camellia engine ----
    lea     rcx, [cam_auth_hmac_key]
    call    asm_camellia256_get_hmac_key
    test    eax, eax
    jnz     @@ae_nokey

    ; ---- Step 2: Open input file ----
    mov     rcx, r12
    mov     edx, GENERIC_READ
    mov     r8d, FILE_SHARE_READ
    xor     r9d, r9d
    mov     DWORD PTR [rsp + 32], OPEN_EXISTING
    mov     DWORD PTR [rsp + 40], 0
    mov     QWORD PTR [rsp + 48], 0
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@ae_open_fail
    mov     QWORD PTR [cam_auth_hInput], rax

    ; ---- Step 3: Get file size ----
    mov     rcx, rax
    xor     edx, edx
    call    GetFileSize
    cmp     eax, 0FFFFFFFFh
    je      @@ae_close_input
    mov     QWORD PTR [cam_auth_fileSize], rax
    mov     r14, rax                    ; r14 = fileSize

    ; ---- Step 4: VirtualAlloc buffer for file data ----
    xor     ecx, ecx
    mov     rdx, r14
    add     rdx, 16                     ; Extra space for safety
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@ae_alloc_fail
    mov     QWORD PTR [cam_auth_pBuf], rax

    ; ---- Step 5: Read entire input file ----
    mov     rcx, QWORD PTR [cam_auth_hInput]
    mov     rdx, QWORD PTR [cam_auth_pBuf]
    mov     r8d, r14d
    lea     r9, [cam_auth_bytesRW]
    mov     QWORD PTR [rsp + 32], 0
    call    ReadFile
    test    eax, eax
    jz      @@ae_read_fail

    ; ---- Step 6: Close input file ----
    mov     rcx, QWORD PTR [cam_auth_hInput]
    call    CloseHandle

    ; ---- Step 7: Generate random 16-byte nonce ----
    xor     ecx, ecx
    lea     rdx, [cam_auth_nonce]
    mov     r8d, AUTH_NONCE_SIZE
    mov     r9d, 2                      ; BCRYPT_USE_SYSTEM_PREFERRED_RNG
    call    BCryptGenRandom

    ; ---- Step 8: Copy nonce to CTR state for encryption ----
    lea     rsi, [cam_auth_nonce]
    lea     rdi, [cam_auth_ctr_state]
    mov     rax, QWORD PTR [rsi]
    mov     QWORD PTR [rdi], rax
    mov     rax, QWORD PTR [rsi + 8]
    mov     QWORD PTR [rdi + 8], rax

    ; ---- Step 9: CTR-encrypt buffer in-place ----
    mov     rcx, QWORD PTR [cam_auth_pBuf]
    mov     rdx, r14                    ; fileSize (buffer length)
    lea     r8, [cam_auth_ctr_state]
    call    asm_camellia256_encrypt_ctr
    test    eax, eax
    jnz     @@ae_free_buf

    ; ---- Step 10: Build pre-HMAC header (24 bytes) ----
    lea     rdi, [cam_auth_header_buf]
    mov     DWORD PTR [rdi], AUTH_MAGIC          ; 'RCM2'
    mov     DWORD PTR [rdi + 4], AUTH_VERSION    ; 0x00020000
    lea     rsi, [cam_auth_nonce]
    mov     rax, QWORD PTR [rsi]
    mov     QWORD PTR [rdi + 8], rax
    mov     rax, QWORD PTR [rsi + 8]
    mov     QWORD PTR [rdi + 16], rax

    ; ---- Step 11: Build ipad/opad for HMAC ----
    call    cam_auth_build_hmac_pads

    ; ---- Step 12: Compute HMAC-SHA256(header[24] || ciphertext[fileSize]) ----
    ; cam_auth_hmac_compute(seg1, seg1Len, seg2, seg2Len, output)
    lea     rcx, [cam_auth_header_buf]  ; seg1 = header
    mov     edx, AUTH_PRE_HMAC_SIZE     ; seg1Len = 24
    mov     r8, QWORD PTR [cam_auth_pBuf] ; seg2 = ciphertext
    mov     r9d, r14d                   ; seg2Len = fileSize
    lea     rax, [cam_auth_computed_hmac]
    mov     QWORD PTR [rsp + 32], rax   ; output pointer
    call    cam_auth_hmac_compute
    test    eax, eax
    jnz     @@ae_crypto_fail

    ; ---- Step 13: Open output file ----
    mov     rcx, r13
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d
    xor     r9d, r9d
    mov     DWORD PTR [rsp + 32], CREATE_ALWAYS
    mov     DWORD PTR [rsp + 40], 0
    mov     QWORD PTR [rsp + 48], 0
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@ae_open_out_fail
    mov     QWORD PTR [cam_auth_hOutput], rax

    ; ---- Step 14: Write header (24 bytes: magic + version + nonce) ----
    mov     rcx, QWORD PTR [cam_auth_hOutput]
    lea     rdx, [cam_auth_header_buf]
    mov     r8d, AUTH_PRE_HMAC_SIZE
    lea     r9, [cam_auth_bytesRW]
    mov     QWORD PTR [rsp + 32], 0
    call    WriteFile

    ; ---- Step 15: Write HMAC (32 bytes) ----
    mov     rcx, QWORD PTR [cam_auth_hOutput]
    lea     rdx, [cam_auth_computed_hmac]
    mov     r8d, AUTH_HMAC_SIZE
    lea     r9, [cam_auth_bytesRW]
    mov     QWORD PTR [rsp + 32], 0
    call    WriteFile

    ; ---- Step 16: Write ciphertext (fileSize bytes) ----
    mov     rcx, QWORD PTR [cam_auth_hOutput]
    mov     rdx, QWORD PTR [cam_auth_pBuf]
    mov     r8d, r14d
    lea     r9, [cam_auth_bytesRW]
    mov     QWORD PTR [rsp + 32], 0
    call    WriteFile

    ; ---- Step 17: Close output file ----
    mov     rcx, QWORD PTR [cam_auth_hOutput]
    call    CloseHandle

    ; ---- Step 18: Free buffer ----
    mov     rcx, QWORD PTR [cam_auth_pBuf]
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree

    ; ---- Step 19: Secure-zero HMAC key and computed HMAC ----
    lea     rdi, [cam_auth_hmac_key]
    xor     eax, eax
    mov     ecx, 32
    rep     stosb
    lea     rdi, [cam_auth_computed_hmac]
    xor     eax, eax
    mov     ecx, 32
    rep     stosb

    ; Success
    lea     rcx, [cam_auth_str_enc_ok]
    call    OutputDebugStringA
    xor     eax, eax
    jmp     @@ae_exit

    ; ---- Error paths ----
@@ae_crypto_fail:
    lea     rcx, [cam_auth_str_crypto_fail]
    call    OutputDebugStringA
    jmp     @@ae_free_buf

@@ae_read_fail:
    mov     rcx, QWORD PTR [cam_auth_hInput]
    call    CloseHandle
@@ae_free_buf:
    mov     rcx, QWORD PTR [cam_auth_pBuf]
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
    mov     eax, CAMELLIA_ERR_FILEREAD
    jmp     @@ae_exit

@@ae_open_out_fail:
    mov     rcx, QWORD PTR [cam_auth_pBuf]
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
    mov     eax, CAMELLIA_ERR_FILEOPEN
    jmp     @@ae_exit

@@ae_alloc_fail:
    lea     rcx, [cam_auth_str_alloc_fail]
    call    OutputDebugStringA
    mov     rcx, QWORD PTR [cam_auth_hInput]
    call    CloseHandle
    mov     eax, CAMELLIA_ERR_ALLOC
    jmp     @@ae_exit

@@ae_close_input:
    mov     rcx, QWORD PTR [cam_auth_hInput]
    call    CloseHandle
    mov     eax, CAMELLIA_ERR_FILEREAD
    jmp     @@ae_exit

@@ae_open_fail:
    mov     eax, CAMELLIA_ERR_FILEOPEN
    jmp     @@ae_exit

@@ae_nokey:
    mov     eax, CAMELLIA_ERR_NOKEY
    jmp     @@ae_exit

@@ae_nullptr:
    mov     eax, CAMELLIA_ERR_NULLPTR

@@ae_exit:
    add     rsp, 72
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
asm_camellia256_auth_encrypt_file ENDP

; =============================================================================
; asm_camellia256_auth_decrypt_file — Authenticated file decryption
;
; Reads RCM2 container, validates magic/version, verifies HMAC-SHA256,
; then CTR-decrypts the ciphertext and writes plaintext to output.
;
; RCX = input file path (null-terminated ANSI, RCM2 format)
; RDX = output file path (null-terminated ANSI, plaintext output)
; Returns: EAX = 0 (CAMELLIA_OK) or error code
;          CAMELLIA_ERR_AUTH (-7) if HMAC verification fails
; =============================================================================
ALIGN 16
asm_camellia256_auth_decrypt_file PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 72
    .allocstack 72
    .endprolog

    test    rcx, rcx
    jz      @@ad_nullptr
    test    rdx, rdx
    jz      @@ad_nullptr

    mov     r12, rcx                    ; r12 = input path
    mov     r13, rdx                    ; r13 = output path

    ; ---- Step 1: Get HMAC key ----
    lea     rcx, [cam_auth_hmac_key]
    call    asm_camellia256_get_hmac_key
    test    eax, eax
    jnz     @@ad_nokey

    ; ---- Step 2: Open input file ----
    mov     rcx, r12
    mov     edx, GENERIC_READ
    mov     r8d, FILE_SHARE_READ
    xor     r9d, r9d
    mov     DWORD PTR [rsp + 32], OPEN_EXISTING
    mov     DWORD PTR [rsp + 40], 0
    mov     QWORD PTR [rsp + 48], 0
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@ad_open_fail
    mov     QWORD PTR [cam_auth_hInput], rax

    ; ---- Step 3: Get file size ----
    mov     rcx, rax
    xor     edx, edx
    call    GetFileSize
    cmp     eax, 0FFFFFFFFh
    je      @@ad_close_input
    mov     QWORD PTR [cam_auth_fileSize], rax
    mov     r14, rax                    ; r14 = fileSize

    ; Verify file is large enough for header (56 bytes min + at least 1 byte data)
    cmp     r14, AUTH_HEADER_SIZE
    jbe     @@ad_size_fail

    ; ---- Step 4: VirtualAlloc buffer for entire file ----
    xor     ecx, ecx
    mov     rdx, r14
    add     rdx, 16
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@ad_alloc_fail
    mov     QWORD PTR [cam_auth_pBuf], rax
    mov     r15, rax                    ; r15 = pBuf

    ; ---- Step 5: Read entire file ----
    mov     rcx, QWORD PTR [cam_auth_hInput]
    mov     rdx, r15
    mov     r8d, r14d
    lea     r9, [cam_auth_bytesRW]
    mov     QWORD PTR [rsp + 32], 0
    call    ReadFile
    test    eax, eax
    jz      @@ad_read_fail

    ; Close input file (no longer needed)
    mov     rcx, QWORD PTR [cam_auth_hInput]
    call    CloseHandle

    ; ---- Step 6: Verify magic ----
    mov     eax, DWORD PTR [r15]
    cmp     eax, AUTH_MAGIC
    jne     @@ad_magic_fail

    ; ---- Step 7: Verify version ----
    mov     eax, DWORD PTR [r15 + 4]
    cmp     eax, AUTH_VERSION
    jne     @@ad_version_fail

    ; ---- Step 8: Extract nonce (offset 8, 16 bytes) ----
    lea     rsi, [r15 + 8]
    lea     rdi, [cam_auth_nonce]
    mov     rax, QWORD PTR [rsi]
    mov     QWORD PTR [rdi], rax
    mov     rax, QWORD PTR [rsi + 8]
    mov     QWORD PTR [rdi + 8], rax

    ; ---- Step 9: Build ipad/opad for HMAC verification ----
    call    cam_auth_build_hmac_pads

    ; ---- Step 10: Compute HMAC over header[0..23] + ciphertext[56..end] ----
    ; Ciphertext length = fileSize - 56
    mov     ebx, r14d
    sub     ebx, AUTH_HEADER_SIZE       ; ebx = ciphertext length

    ; seg1 = file[0..23] (header: magic+version+nonce)
    ; seg2 = file[56..end] (ciphertext)
    mov     rcx, r15                    ; seg1 = pBuf[0]
    mov     edx, AUTH_PRE_HMAC_SIZE     ; seg1Len = 24
    lea     r8, [r15 + AUTH_HEADER_SIZE] ; seg2 = pBuf[56]
    mov     r9d, ebx                    ; seg2Len = ciphertext length
    lea     rax, [cam_auth_computed_hmac]
    mov     QWORD PTR [rsp + 32], rax
    call    cam_auth_hmac_compute
    test    eax, eax
    jnz     @@ad_crypto_fail

    ; ---- Step 11: Constant-time HMAC comparison ----
    ; Compare computed HMAC with stored HMAC at pBuf[24..55]
    lea     rcx, [cam_auth_computed_hmac]
    lea     rdx, [r15 + AUTH_PRE_HMAC_SIZE]  ; stored HMAC at offset 24
    call    cam_auth_constant_time_cmp
    test    eax, eax
    jnz     @@ad_hmac_fail

    ; ---- Step 12: Copy nonce to CTR state for decryption ----
    lea     rsi, [cam_auth_nonce]
    lea     rdi, [cam_auth_ctr_state]
    mov     rax, QWORD PTR [rsi]
    mov     QWORD PTR [rdi], rax
    mov     rax, QWORD PTR [rsi + 8]
    mov     QWORD PTR [rdi + 8], rax

    ; ---- Step 13: CTR-decrypt ciphertext in-place ----
    ; CTR mode: encrypt and decrypt are identical
    lea     rcx, [r15 + AUTH_HEADER_SIZE]   ; ciphertext at offset 56
    mov     edx, ebx                        ; ciphertext length
    lea     r8, [cam_auth_ctr_state]
    call    asm_camellia256_decrypt_ctr
    test    eax, eax
    jnz     @@ad_free_buf

    ; ---- Step 14: Open output file ----
    mov     rcx, r13
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d
    xor     r9d, r9d
    mov     DWORD PTR [rsp + 32], CREATE_ALWAYS
    mov     DWORD PTR [rsp + 40], 0
    mov     QWORD PTR [rsp + 48], 0
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@ad_open_out_fail
    mov     QWORD PTR [cam_auth_hOutput], rax

    ; ---- Step 15: Write plaintext (decrypted ciphertext, skip header) ----
    mov     rcx, QWORD PTR [cam_auth_hOutput]
    lea     rdx, [r15 + AUTH_HEADER_SIZE]
    mov     r8d, ebx                    ; plaintext length = ciphertext length
    lea     r9, [cam_auth_bytesRW]
    mov     QWORD PTR [rsp + 32], 0
    call    WriteFile

    ; ---- Step 16: Close output file ----
    mov     rcx, QWORD PTR [cam_auth_hOutput]
    call    CloseHandle

    ; ---- Step 17: Free buffer ----
    mov     rcx, QWORD PTR [cam_auth_pBuf]
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree

    ; ---- Step 18: Secure-zero sensitive state ----
    lea     rdi, [cam_auth_hmac_key]
    xor     eax, eax
    mov     ecx, 32
    rep     stosb
    lea     rdi, [cam_auth_computed_hmac]
    xor     eax, eax
    mov     ecx, 32
    rep     stosb

    ; Success
    lea     rcx, [cam_auth_str_dec_ok]
    call    OutputDebugStringA
    xor     eax, eax
    jmp     @@ad_exit

    ; ---- Error paths ----
@@ad_hmac_fail:
    lea     rcx, [cam_auth_str_hmac_fail]
    call    OutputDebugStringA
    mov     eax, CAMELLIA_ERR_AUTH
    jmp     @@ad_free_buf

@@ad_crypto_fail:
    lea     rcx, [cam_auth_str_crypto_fail]
    call    OutputDebugStringA
    mov     eax, CAMELLIA_ERR_AUTH
    jmp     @@ad_free_buf

@@ad_magic_fail:
    lea     rcx, [cam_auth_str_magic_fail]
    call    OutputDebugStringA
    mov     eax, CAMELLIA_ERR_AUTH
    jmp     @@ad_free_buf

@@ad_version_fail:
    lea     rcx, [cam_auth_str_ver_fail]
    call    OutputDebugStringA
    mov     eax, CAMELLIA_ERR_AUTH
    jmp     @@ad_free_buf

@@ad_read_fail:
    mov     rcx, QWORD PTR [cam_auth_hInput]
    call    CloseHandle
@@ad_free_buf:
    mov     rcx, QWORD PTR [cam_auth_pBuf]
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
    ; eax already set by caller or default
    cmp     eax, 0
    jne     @@ad_exit
    mov     eax, CAMELLIA_ERR_FILEREAD
    jmp     @@ad_exit

@@ad_open_out_fail:
    mov     rcx, QWORD PTR [cam_auth_pBuf]
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
    mov     eax, CAMELLIA_ERR_FILEOPEN
    jmp     @@ad_exit

@@ad_size_fail:
    lea     rcx, [cam_auth_str_size_fail]
    call    OutputDebugStringA
    mov     rcx, QWORD PTR [cam_auth_hInput]
    call    CloseHandle
    mov     eax, CAMELLIA_ERR_AUTH
    jmp     @@ad_exit

@@ad_alloc_fail:
    lea     rcx, [cam_auth_str_alloc_fail]
    call    OutputDebugStringA
    mov     rcx, QWORD PTR [cam_auth_hInput]
    call    CloseHandle
    mov     eax, CAMELLIA_ERR_ALLOC
    jmp     @@ad_exit

@@ad_close_input:
    mov     rcx, QWORD PTR [cam_auth_hInput]
    call    CloseHandle
    mov     eax, CAMELLIA_ERR_FILEREAD
    jmp     @@ad_exit

@@ad_open_fail:
    mov     eax, CAMELLIA_ERR_FILEOPEN
    jmp     @@ad_exit

@@ad_nokey:
    mov     eax, CAMELLIA_ERR_NOKEY
    jmp     @@ad_exit

@@ad_nullptr:
    mov     eax, CAMELLIA_ERR_NULLPTR

@@ad_exit:
    add     rsp, 72
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
asm_camellia256_auth_decrypt_file ENDP

; =============================================================================
; asm_camellia256_auth_encrypt_buf — In-memory authenticated encryption
;
; Encrypts a plaintext buffer and produces an authenticated container in RAM.
; Output format: [magic 4B][version 4B][nonce 16B][HMAC 32B][ciphertext NB]
;
; RCX = plaintext buffer (input, will NOT be modified)
; RDX = plaintext length (DWORD)
; R8  = output buffer (must be at least plaintextLen + 56 bytes)
; R9  = pointer to DWORD receiving output length
; Returns: EAX = 0 or error code
; =============================================================================
ALIGN 16
asm_camellia256_auth_encrypt_buf PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 72
    .allocstack 72
    .endprolog

    test    rcx, rcx
    jz      @@abe_nullptr
    test    r8, r8
    jz      @@abe_nullptr
    test    r9, r9
    jz      @@abe_nullptr

    mov     r12, rcx                    ; plaintext pointer
    mov     r13d, edx                   ; plaintext length
    mov     r14, r8                     ; output buffer
    mov     r15, r9                     ; output length pointer

    ; Get HMAC key
    lea     rcx, [cam_auth_hmac_key]
    call    asm_camellia256_get_hmac_key
    test    eax, eax
    jnz     @@abe_nokey

    ; Build header in output buffer
    mov     DWORD PTR [r14], AUTH_MAGIC
    mov     DWORD PTR [r14 + 4], AUTH_VERSION

    ; Generate random nonce at output[8..23]
    xor     ecx, ecx
    lea     rdx, [r14 + 8]
    mov     r8d, AUTH_NONCE_SIZE
    mov     r9d, 2
    call    BCryptGenRandom

    ; Copy nonce to CTR state
    mov     rax, QWORD PTR [r14 + 8]
    mov     QWORD PTR [cam_auth_ctr_state], rax
    mov     rax, QWORD PTR [r14 + 16]
    mov     QWORD PTR [cam_auth_ctr_state + 8], rax

    ; Copy plaintext to output[56..] (ciphertext area)
    lea     rdi, [r14 + AUTH_HEADER_SIZE]
    mov     rsi, r12
    mov     ecx, r13d
    rep     movsb

    ; CTR-encrypt the ciphertext area in-place
    lea     rcx, [r14 + AUTH_HEADER_SIZE]
    mov     edx, r13d
    lea     r8, [cam_auth_ctr_state]
    call    asm_camellia256_encrypt_ctr
    test    eax, eax
    jnz     @@abe_exit_err

    ; Build ipad/opad
    call    cam_auth_build_hmac_pads

    ; Compute HMAC over header[24] + ciphertext[plaintextLen]
    mov     rcx, r14                    ; seg1 = output header
    mov     edx, AUTH_PRE_HMAC_SIZE     ; 24 bytes
    lea     r8, [r14 + AUTH_HEADER_SIZE] ; seg2 = ciphertext
    mov     r9d, r13d                   ; plaintext length == ciphertext length
    lea     rax, [r14 + AUTH_PRE_HMAC_SIZE]  ; HMAC slot at output[24]
    mov     QWORD PTR [rsp + 32], rax
    call    cam_auth_hmac_compute
    test    eax, eax
    jnz     @@abe_exit_err

    ; Set output length = 56 + plaintextLen
    mov     eax, r13d
    add     eax, AUTH_HEADER_SIZE
    mov     DWORD PTR [r15], eax

    ; Secure-zero HMAC key
    lea     rdi, [cam_auth_hmac_key]
    xor     eax, eax
    mov     ecx, 32
    rep     stosb

    xor     eax, eax
    jmp     @@abe_exit

@@abe_exit_err:
    mov     eax, CAMELLIA_ERR_AUTH

@@abe_nokey:
    cmp     eax, 0
    jne     @@abe_exit
    mov     eax, CAMELLIA_ERR_NOKEY
    jmp     @@abe_exit

@@abe_nullptr:
    mov     eax, CAMELLIA_ERR_NULLPTR

@@abe_exit:
    add     rsp, 72
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
asm_camellia256_auth_encrypt_buf ENDP

; =============================================================================
; asm_camellia256_auth_decrypt_buf — In-memory authenticated decryption
;
; Validates RCM2 header, verifies HMAC-SHA256, and CTR-decrypts.
;
; RCX = authenticated data buffer (RCM2 format)
; RDX = authenticated data length (DWORD)
; R8  = output plaintext buffer (must be at least authDataLen - 56 bytes)
; R9  = pointer to DWORD receiving plaintext length
; Returns: EAX = 0 or error code (CAMELLIA_ERR_AUTH if tampered)
; =============================================================================
ALIGN 16
asm_camellia256_auth_decrypt_buf PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 72
    .allocstack 72
    .endprolog

    test    rcx, rcx
    jz      @@abd_nullptr
    test    r8, r8
    jz      @@abd_nullptr
    test    r9, r9
    jz      @@abd_nullptr

    mov     r12, rcx                    ; auth data
    mov     r13d, edx                   ; auth data length
    mov     r14, r8                     ; output buffer
    mov     r15, r9                     ; output length pointer

    ; Verify minimum size
    cmp     r13d, AUTH_HEADER_SIZE
    jbe     @@abd_auth_fail

    ; Verify magic
    mov     eax, DWORD PTR [r12]
    cmp     eax, AUTH_MAGIC
    jne     @@abd_auth_fail

    ; Verify version
    mov     eax, DWORD PTR [r12 + 4]
    cmp     eax, AUTH_VERSION
    jne     @@abd_auth_fail

    ; Get HMAC key
    lea     rcx, [cam_auth_hmac_key]
    call    asm_camellia256_get_hmac_key
    test    eax, eax
    jnz     @@abd_nokey

    ; Compute ciphertext length
    mov     ebx, r13d
    sub     ebx, AUTH_HEADER_SIZE       ; ebx = ciphertext length

    ; Build ipad/opad
    call    cam_auth_build_hmac_pads

    ; Compute HMAC over header[0..23] + ciphertext[56..end]
    mov     rcx, r12                    ; seg1 = header
    mov     edx, AUTH_PRE_HMAC_SIZE     ; 24 bytes
    lea     r8, [r12 + AUTH_HEADER_SIZE] ; seg2 = ciphertext
    mov     r9d, ebx                    ; seg2 length
    lea     rax, [cam_auth_computed_hmac]
    mov     QWORD PTR [rsp + 32], rax
    call    cam_auth_hmac_compute
    test    eax, eax
    jnz     @@abd_auth_fail

    ; Constant-time HMAC comparison
    lea     rcx, [cam_auth_computed_hmac]
    lea     rdx, [r12 + AUTH_PRE_HMAC_SIZE]  ; stored HMAC at offset 24
    call    cam_auth_constant_time_cmp
    test    eax, eax
    jnz     @@abd_hmac_fail

    ; Copy ciphertext to output and decrypt in-place
    lea     rsi, [r12 + AUTH_HEADER_SIZE]
    mov     rdi, r14
    mov     ecx, ebx
    rep     movsb

    ; Extract nonce for CTR state
    mov     rax, QWORD PTR [r12 + 8]
    mov     QWORD PTR [cam_auth_ctr_state], rax
    mov     rax, QWORD PTR [r12 + 16]
    mov     QWORD PTR [cam_auth_ctr_state + 8], rax

    ; CTR-decrypt
    mov     rcx, r14                    ; output buffer (ciphertext copy)
    mov     edx, ebx                    ; ciphertext length
    lea     r8, [cam_auth_ctr_state]
    call    asm_camellia256_decrypt_ctr
    test    eax, eax
    jnz     @@abd_auth_fail

    ; Set output length
    mov     DWORD PTR [r15], ebx

    ; Secure-zero HMAC key
    lea     rdi, [cam_auth_hmac_key]
    xor     eax, eax
    mov     ecx, 32
    rep     stosb
    lea     rdi, [cam_auth_computed_hmac]
    xor     eax, eax
    mov     ecx, 32
    rep     stosb

    xor     eax, eax
    jmp     @@abd_exit

@@abd_hmac_fail:
    lea     rcx, [cam_auth_str_hmac_fail]
    call    OutputDebugStringA
    mov     eax, CAMELLIA_ERR_AUTH
    jmp     @@abd_exit

@@abd_auth_fail:
    mov     eax, CAMELLIA_ERR_AUTH
    jmp     @@abd_exit

@@abd_nokey:
    mov     eax, CAMELLIA_ERR_NOKEY
    jmp     @@abd_exit

@@abd_nullptr:
    mov     eax, CAMELLIA_ERR_NULLPTR

@@abd_exit:
    add     rsp, 72
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
asm_camellia256_auth_decrypt_buf ENDP

; =============================================================================
; End
; =============================================================================
END
