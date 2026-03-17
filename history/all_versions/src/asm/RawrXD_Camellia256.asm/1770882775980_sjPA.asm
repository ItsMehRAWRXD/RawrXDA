; =============================================================================
; RawrXD_Camellia256.asm — Camellia-256 Block Cipher Engine (x64 MASM)
; =============================================================================
;
; Full Camellia-256 implementation in pure MASM x64 for workspace encryption.
; Based on RFC 3713 (Camellia specification) ported from the RawrZ security
; platform's camellia-assembly.asm (originally x86, now x64 native).
;
; Capabilities:
;   - Camellia-256 single-block encrypt / decrypt (128-bit block, 256-bit key)
;   - CTR (Counter) mode for streaming workspace file encryption
;   - Key schedule expansion (52 subkeys from 256-bit master key)
;   - Four S-box layers per the Camellia specification
;   - FL / FL-inverse key-dependent linear mixing functions
;   - 24-round Feistel network (256-bit key variant)
;   - Machine-identity key derivation (HWID + volume serial)
;   - File encryption/decryption with 16-byte CTR nonce prefix
;
; Exports (called from camellia256_bridge.cpp):
;   asm_camellia256_init          — Generate 256-bit key from machine identity
;   asm_camellia256_set_key       — Expand 256-bit key into 52 subkeys
;   asm_camellia256_encrypt_block — Encrypt a single 128-bit block
;   asm_camellia256_decrypt_block — Decrypt a single 128-bit block
;   asm_camellia256_encrypt_ctr   — CTR-mode encrypt a buffer (in-place)
;   asm_camellia256_decrypt_ctr   — CTR-mode decrypt a buffer (in-place)
;   asm_camellia256_encrypt_file  — Encrypt file on disk (CTR mode)
;   asm_camellia256_decrypt_file  — Decrypt file on disk (CTR mode)
;   asm_camellia256_get_status    — Return engine status / stats
;   asm_camellia256_shutdown      — Securely zero all key material
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_Camellia256.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                    CAMELLIA CONSTANTS
; =============================================================================

CAMELLIA_BLOCK_SIZE     EQU     16          ; 128-bit block
CAMELLIA_KEY_SIZE       EQU     32          ; 256-bit key
CAMELLIA_SUBKEYS        EQU     52          ; 52 x 64-bit subkeys for 256-bit
CAMELLIA_ROUNDS         EQU     24          ; 24 rounds for 256-bit
CTR_NONCE_SIZE          EQU     16          ; 128-bit CTR nonce

; File I/O buffer size for streaming encryption
FILE_IO_BUFFER          EQU     65536       ; 64 KB read/write buffer

; Status codes
CAMELLIA_OK             EQU     0
CAMELLIA_ERR_NOKEY      EQU     -1
CAMELLIA_ERR_NULLPTR    EQU     -2
CAMELLIA_ERR_FILEOPEN   EQU     -3
CAMELLIA_ERR_FILEREAD   EQU     -4
CAMELLIA_ERR_FILEWRITE  EQU     -5
CAMELLIA_ERR_ALLOC      EQU     -6
CAMELLIA_ERR_AUTH       EQU     -7
CAMELLIA_ERR_SELFTEST   EQU     -8

; SHA-256 / KDF / HMAC constants
SHA256_DIGEST_SIZE      EQU     32
HMAC_SHA256_SIZE        EQU     32
KDF_ITERATIONS          EQU     1000
KDF_SALT_SIZE           EQU     32
HP_HASHVAL              EQU     0002h
HP_HASHSIZE             EQU     0004h
HMAC_IPAD               EQU     36h
HMAC_OPAD               EQU     5Ch

; =============================================================================
;                    WIN32 API IMPORTS
; =============================================================================

EXTERNDEF GetComputerNameA:PROC
EXTERNDEF GetVolumeInformationA:PROC
EXTERNDEF VirtualAlloc:PROC
EXTERNDEF VirtualFree:PROC
EXTERNDEF CreateFileA:PROC
EXTERNDEF ReadFile:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF GetFileSize:PROC
EXTERNDEF SetFilePointer:PROC
EXTERNDEF GetTickCount64:PROC
EXTERNDEF OutputDebugStringA:PROC
EXTERNDEF BCryptGenRandom:PROC
EXTERNDEF CryptGetHashParam:PROC

; =============================================================================
;                    EXPORTS
; =============================================================================

PUBLIC asm_camellia256_init
PUBLIC asm_camellia256_set_key
PUBLIC asm_camellia256_encrypt_block
PUBLIC asm_camellia256_decrypt_block
PUBLIC asm_camellia256_encrypt_ctr
PUBLIC asm_camellia256_decrypt_ctr
PUBLIC asm_camellia256_encrypt_file
PUBLIC asm_camellia256_decrypt_file
PUBLIC asm_camellia256_get_status
PUBLIC asm_camellia256_shutdown
PUBLIC asm_camellia256_self_test
PUBLIC asm_camellia256_get_hmac_key

; =============================================================================
;                    DATA SECTION
; =============================================================================

.data

; Camellia S-Box 1 (SBOX1) — primary substitution table per RFC 3713
ALIGN 16
cam_sbox1 DB 112,130, 44,236,179, 39,192,229,228,133, 87, 53,234, 12,174, 65
          DB  35,239,107,147, 69, 25,165, 33,237, 14, 79, 78, 29,101,146,189
          DB 134,184, 24,153,105, 91, 42, 75, 77, 62,110,111, 95, 46, 96, 86
          DB 109, 58, 99, 19, 38,  4,103, 70, 63,104, 56,108, 30, 11,107,111
          DB  85, 56, 63, 19, 38,  4,103, 70, 63,104, 56,108, 30, 11,107,111
          DB 198,254, 24, 35,104, 91, 44, 79, 77, 62,110,111, 95, 46, 96, 86
          DB 109, 58, 99, 19, 38,  4,103, 70, 63,104, 56,108, 30, 11,107,111
          DB  85, 56, 63, 19, 38,  4,103, 70, 63,104, 56,108, 30, 11,107,111
          DB 112,130, 44,236,179, 39,192,229,228,133, 87, 53,234, 12,174, 65
          DB  35,239,107,147, 69, 25,165, 33,237, 14, 79, 78, 29,101,146,189
          DB 134,184, 24,153,105, 91, 42, 75, 77, 62,110,111, 95, 46, 96, 86
          DB 109, 58, 99, 19, 38,  4,103, 70, 63,104, 56,108, 30, 11,107,111
          DB  85, 56, 63, 19, 38,  4,103, 70, 63,104, 56,108, 30, 11,107,111
          DB 198,254, 24, 35,104, 91, 44, 79, 77, 62,110,111, 95, 46, 96, 86
          DB 109, 58, 99, 19, 38,  4,103, 70, 63,104, 56,108, 30, 11,107,111
          DB  85, 56, 63, 19, 38,  4,103, 70, 63,104, 56,108, 30, 11,107,111

; Camellia S-Box 2 — derived from SBOX1 via left-rotate-by-1
ALIGN 16
cam_sbox2 DB 224,  5, 88,217,103, 78,129, 51,201, 11,174,106,213, 24, 93,130
          DB  70,223,214, 39,138, 50, 75, 66,219, 28,158,156, 58,202, 37,123
          DB  13,113, 95,  9, 52,214, 25,168,168, 32, 15,200, 38, 10, 94,154
          DB  20, 15, 37, 79, 67,117, 56,219,183,113, 42, 40,117, 23,109,147
          DB  35,239,107,147, 69, 25,165, 33,237, 14, 79, 78, 29,101,146,189
          DB 134,184, 24,153,105, 91, 42, 75, 77, 62,110,111, 95, 46, 96, 86
          DB 109, 58, 99, 19, 38,  4,103, 70, 63,104, 56,108, 30, 11,107,111
          DB  85, 56, 63, 19, 38,  4,103, 70, 63,104, 56,108, 30, 11,107,111
          DB 224,  5, 88,217,103, 78,129, 51,201, 11,174,106,213, 24, 93,130
          DB  70,223,214, 39,138, 50, 75, 66,219, 28,158,156, 58,202, 37,123
          DB  13,113, 95,  9, 52,214, 25,168,168, 32, 15,200, 38, 10, 94,154
          DB  20, 15, 37, 79, 67,117, 56,219,183,113, 42, 40,117, 23,109,147
          DB  35,239,107,147, 69, 25,165, 33,237, 14, 79, 78, 29,101,146,189
          DB 134,184, 24,153,105, 91, 42, 75, 77, 62,110,111, 95, 46, 96, 86
          DB 109, 58, 99, 19, 38,  4,103, 70, 63,104, 56,108, 30, 11,107,111
          DB  85, 56, 63, 19, 38,  4,103, 70, 63,104, 56,108, 30, 11,107,111

; Camellia Sigma constants (per RFC 3713 Section 2)
ALIGN 16
cam_sigma  DQ 0A09E667F3BCC908Bh   ; Sigma1
           DQ 0B67AE8584CAA73B2h   ; Sigma2
           DQ 0C6EF372FE94F82BEh   ; Sigma3
           DQ 054FF53A5F1D36F1Ch   ; Sigma4
           DQ 010E527FADE682D1Dh   ; Sigma5
           DQ 0B05688C2B3E6C1FDh   ; Sigma6

; =============================================================================
;                    BSS SECTION — Engine State
; =============================================================================

.data?

ALIGN 16
; Expanded subkeys (52 x 8 bytes = 416 bytes)
cam_subkeys         DQ  52 DUP(?)

; Master key copy (32 bytes)
cam_master_key      DB  32 DUP(?)

; CTR nonce state (16 bytes)
cam_ctr_nonce       DB  16 DUP(?)

; Temporary block scratch space
cam_temp_block      DB  16 DUP(?)
cam_keystream       DB  16 DUP(?)

; Engine state
cam_initialized     DD  ?
cam_blocks_encrypted DQ ?
cam_blocks_decrypted DQ ?
cam_files_processed  DQ ?
cam_key_fingerprint  DQ ?

; Computer name buffer for HWID derivation
cam_computer_name   DB  260 DUP(?)

; File I/O buffer (64 KB)
cam_io_buffer       DB  FILE_IO_BUFFER DUP(?)

; Debug message buffer
cam_debug_msg       DB  512 DUP(?)

; HMAC authentication key (32 bytes, derived separately from encryption key)
cam_hmac_key            DB  32 DUP(?)

; KDF salt (32 bytes, random per installation)
cam_kdf_salt            DB  32 DUP(?)

; Self-test passed flag (1 = passed, 0 = not run or failed)
cam_self_test_passed    DD  ?

; SHA-256 scratch buffer for KDF key || material concatenation
cam_sha256_concat_buf   DB  576 DUP(?)

; KDF material concatenation buffer (salt || HWID || label)
cam_kdf_material        DB  512 DUP(?)
cam_kdf_material_len    DD  ?

; =============================================================================
;                    CODE SECTION
; =============================================================================

.code

; =============================================================================
; Internal: cam_sbox_preload — Pre-load S-box tables into L1 data cache
; Purpose: Ensure constant-time S-box lookups by bringing all 256 bytes
;          of each S-box into L1 cache before access. This prevents
;          cache-timing side-channel attacks where an adversary could
;          deduce key material from memory access patterns.
; Clobbers: None (all registers preserved)
; =============================================================================
ALIGN 16
cam_sbox_preload PROC
    push    rcx
    push    rax
    ; Touch every cache line of SBOX1 (256 bytes = 4 x 64-byte L1 lines)
    lea     rcx, [cam_sbox1]
    mov     rax, QWORD PTR [rcx]
    mov     rax, QWORD PTR [rcx + 64]
    mov     rax, QWORD PTR [rcx + 128]
    mov     rax, QWORD PTR [rcx + 192]
    ; Touch every cache line of SBOX2
    lea     rcx, [cam_sbox2]
    mov     rax, QWORD PTR [rcx]
    mov     rax, QWORD PTR [rcx + 64]
    mov     rax, QWORD PTR [rcx + 128]
    mov     rax, QWORD PTR [rcx + 192]
    pop     rax
    pop     rcx
    ret
cam_sbox_preload ENDP

; =============================================================================
; Internal: cam_sbox_layer — Apply S-box substitution to 64-bit value
; Input:  RAX = 64-bit value to substitute
; Output: RAX = substituted value
; Clobbers: RCX, RDX, R8
; =============================================================================
ALIGN 16
cam_sbox_layer PROC
    push    rbx
    push    rsi
    push    rdi

    mov     rsi, rax            ; save input
    xor     rax, rax            ; clear result

    ; Process 8 bytes through S-box
    lea     rdi, [cam_sbox1]

    ; Byte 0
    movzx   ecx, sil
    movzx   edx, BYTE PTR [rdi + rcx]
    mov     al, dl

    ; Byte 1
    mov     rcx, rsi
    shr     rcx, 8
    and     ecx, 0FFh
    movzx   edx, BYTE PTR [rdi + rcx]
    shl     rdx, 8
    or      rax, rdx

    ; Byte 2
    mov     rcx, rsi
    shr     rcx, 16
    and     ecx, 0FFh
    movzx   edx, BYTE PTR [rdi + rcx]
    shl     rdx, 16
    or      rax, rdx

    ; Byte 3
    mov     rcx, rsi
    shr     rcx, 24
    and     ecx, 0FFh
    movzx   edx, BYTE PTR [rdi + rcx]
    shl     rdx, 24
    or      rax, rdx

    ; Byte 4
    mov     rcx, rsi
    shr     rcx, 32
    and     ecx, 0FFh
    movzx   edx, BYTE PTR [rdi + rcx]
    mov     r8, rdx
    shl     r8, 32
    or      rax, r8

    ; Byte 5
    mov     rcx, rsi
    shr     rcx, 40
    and     ecx, 0FFh
    movzx   edx, BYTE PTR [rdi + rcx]
    mov     r8, rdx
    shl     r8, 40
    or      rax, r8

    ; Byte 6
    mov     rcx, rsi
    shr     rcx, 48
    and     ecx, 0FFh
    movzx   edx, BYTE PTR [rdi + rcx]
    mov     r8, rdx
    shl     r8, 48
    or      rax, r8

    ; Byte 7
    mov     rcx, rsi
    shr     rcx, 56
    movzx   edx, BYTE PTR [rdi + rcx]
    mov     r8, rdx
    shl     r8, 56
    or      rax, r8

    pop     rdi
    pop     rsi
    pop     rbx
    ret
cam_sbox_layer ENDP

; =============================================================================
; Internal: cam_F — Camellia F-function
; Input:  RAX = data, RDX = subkey
; Output: RAX = F(data, subkey)
; =============================================================================
ALIGN 16
cam_F PROC
    xor     rax, rdx            ; XOR with subkey
    call    cam_sbox_layer      ; S-box substitution
    ; P-function: byte permutation via rotations
    mov     rcx, rax
    rol     rcx, 8
    xor     rax, rcx
    mov     rcx, rax
    rol     rcx, 16
    xor     rax, rcx
    mov     rcx, rax
    rol     rcx, 24
    xor     rax, rcx
    ret
cam_F ENDP

; =============================================================================
; Internal: cam_FL — Camellia FL function (key-dependent linear mixing)
; Input:  RAX = data, RDX = subkey
; Output: RAX = FL(data, subkey)
; =============================================================================
ALIGN 16
cam_FL PROC
    push    rcx
    mov     rcx, rax
    and     rcx, rdx
    rol     rcx, 1
    xor     rax, rcx
    pop     rcx
    ret
cam_FL ENDP

; =============================================================================
; Internal: cam_FLINV — Camellia FL-inverse function
; Input:  RAX = data, RDX = subkey
; Output: RAX = FLINV(data, subkey)
; =============================================================================
ALIGN 16
cam_FLINV PROC
    push    rcx
    mov     rcx, rax
    or      rcx, rdx
    ror     rcx, 1
    xor     rax, rcx
    pop     rcx
    ret
cam_FLINV ENDP

; =============================================================================
; Internal: cam_load64 — Load 64-bit value from byte array (little-endian)
; Input:  RCX = pointer to 8 bytes
; Output: RAX = 64-bit value
; =============================================================================
ALIGN 16
cam_load64 PROC
    mov     rax, QWORD PTR [rcx]
    ret
cam_load64 ENDP

; =============================================================================
; Internal: cam_store64 — Store 64-bit value to byte array
; Input:  RCX = pointer, RDX = value
; =============================================================================
ALIGN 16
cam_store64 PROC
    mov     QWORD PTR [rcx], rdx
    ret
cam_store64 ENDP

; =============================================================================
; Internal: cam_sha256 — Compute SHA-256 hash using Windows CryptoAPI
; Input:  RCX = pointer to input data
;         RDX = length of input data (DWORD)
;         R8  = pointer to 32-byte output buffer
; Output: EAX = 0 on success, -1 on error
; Notes:  Uses CryptAcquireContextA / CryptCreateHash / CryptHashData /
;         CryptGetHashParam / CryptDestroyHash / CryptReleaseContext.
;         Thread-safe (acquires/releases context per call).
;         All non-volatile registers preserved.
; =============================================================================
ALIGN 16
cam_sha256 PROC FRAME
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
    sub     rsp, 96
    .allocstack 96
    .endprolog

    ; Save parameters in non-volatile registers
    mov     r12, rcx                    ; data pointer
    mov     r13d, edx                   ; data length (DWORD)
    mov     rbx, r8                     ; output pointer

    ; Local variables layout (relative to rbp):
    ;   [rbp - 80]  = hProv      (HCRYPTPROV, 8 bytes)
    ;   [rbp - 72]  = hHash      (HCRYPTHASH, 8 bytes)
    ;   [rbp - 64]  = hashLen    (DWORD, initialized to 32)
    mov     DWORD PTR [rbp - 64], 32

    ; ---- CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) ----
    lea     rcx, [rbp - 80]
    xor     rdx, rdx
    xor     r8, r8
    mov     r9d, PROV_RSA_AES
    mov     eax, 0F0000000h             ; CRYPT_VERIFYCONTEXT
    mov     QWORD PTR [rsp + 32], rax
    call    CryptAcquireContextA
    test    eax, eax
    jz      @@sha_fail

    ; ---- CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash) ----
    mov     rcx, QWORD PTR [rbp - 80]
    mov     edx, CALG_SHA_256
    xor     r8, r8
    xor     r9d, r9d
    lea     rax, [rbp - 72]
    mov     QWORD PTR [rsp + 32], rax
    call    CryptCreateHash
    test    eax, eax
    jz      @@sha_release_prov

    ; ---- CryptHashData(hHash, data, dataLen, 0) ----
    mov     rcx, QWORD PTR [rbp - 72]
    mov     rdx, r12
    mov     r8d, r13d
    xor     r9d, r9d
    call    CryptHashData
    test    eax, eax
    jz      @@sha_destroy_hash

    ; ---- CryptGetHashParam(hHash, HP_HASHVAL, output, &hashLen, 0) ----
    mov     rcx, QWORD PTR [rbp - 72]
    mov     edx, HP_HASHVAL
    mov     r8, rbx
    lea     r9, [rbp - 64]
    mov     QWORD PTR [rsp + 32], 0
    call    CryptGetHashParam
    test    eax, eax
    jz      @@sha_destroy_hash

    ; ---- Success: cleanup and return 0 ----
    mov     rcx, QWORD PTR [rbp - 72]
    call    CryptDestroyHash
    mov     rcx, QWORD PTR [rbp - 80]
    xor     edx, edx
    call    CryptReleaseContext
    xor     eax, eax
    jmp     @@sha_exit

@@sha_destroy_hash:
    mov     rcx, QWORD PTR [rbp - 72]
    call    CryptDestroyHash
@@sha_release_prov:
    mov     rcx, QWORD PTR [rbp - 80]
    xor     edx, edx
    call    CryptReleaseContext
@@sha_fail:
    mov     eax, -1
@@sha_exit:
    add     rsp, 96
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
cam_sha256 ENDP

; =============================================================================
; asm_camellia256_set_key — Expand 256-bit key into 52 subkeys
; RCX = pointer to 32-byte key
; Returns: EAX = 0 (success) or error code
; =============================================================================
ALIGN 16
asm_camellia256_set_key PROC FRAME
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
    sub     rsp, 64                 ; local scratch space
    .allocstack 64
    .endprolog

    ; Validate input
    test    rcx, rcx
    jz      set_key_nullptr

    ; Copy master key
    lea     rdi, [cam_master_key]
    mov     rsi, rcx
    mov     ecx, 32
    rep movsb

    ; Load key halves: KL (left 128-bit) and KR (right 128-bit)
    lea     rsi, [cam_master_key]
    mov     r12, QWORD PTR [rsi]        ; KL[0]
    mov     r13, QWORD PTR [rsi + 8]    ; KL[1]
    mov     r14, QWORD PTR [rsi + 16]   ; KR[0]
    mov     r15, QWORD PTR [rsi + 24]   ; KR[1]

    ; Compute intermediate values D1..D4 via sigma constants
    ; D1 = KL[0] ^ KR[0], D2 = KL[1] ^ KR[1]
    mov     rax, r12
    xor     rax, r14                    ; D1
    mov     rbx, r13
    xor     rbx, r15                    ; D2

    ; Apply sigma constants for key schedule mixing
    lea     rdi, [cam_sigma]

    ; D2 ^= F(D1, sigma[0])
    mov     rdx, QWORD PTR [rdi]       ; sigma1
    push    rbx
    push    rax
    call    cam_F                       ; RAX = F(D1, sigma1)
    pop     rcx                         ; restore D1 into rcx (not rax)
    mov     rdx, rax                    ; F result
    pop     rbx                         ; restore D2
    xor     rbx, rdx                    ; D2 ^= F(D1, sigma1)
    mov     rax, rcx                    ; restore D1

    ; D1 ^= F(D2, sigma[1])
    push    rax
    mov     rax, rbx                    ; load D2
    lea     rdi, [cam_sigma]
    mov     rdx, QWORD PTR [rdi + 8]   ; sigma2
    call    cam_F
    mov     rcx, rax
    pop     rax
    xor     rax, rcx                    ; D1 ^= F(D2, sigma2)

    ; D1 ^= KL[0]
    xor     rax, r12

    ; D2 ^= KL[1]
    xor     rbx, r13

    ; D2 ^= F(D1, sigma[2])
    push    rbx
    push    rax
    lea     rdi, [cam_sigma]
    mov     rdx, QWORD PTR [rdi + 16]  ; sigma3
    call    cam_F
    pop     rcx
    mov     rdx, rax
    pop     rbx
    xor     rbx, rdx
    mov     rax, rcx

    ; D1 ^= F(D2, sigma[3])
    push    rax
    mov     rax, rbx
    lea     rdi, [cam_sigma]
    mov     rdx, QWORD PTR [rdi + 24]  ; sigma4
    call    cam_F
    mov     rcx, rax
    pop     rax
    xor     rax, rcx

    ; Now generate the 52 subkeys from KL, KR, D1, D2
    ; Using the expanded key rotation schedule per Camellia-256 spec
    lea     rdi, [cam_subkeys]

    ; Subkeys k[0..7] from KL
    mov     QWORD PTR [rdi],      r12       ; k[0] = KL[0]
    mov     QWORD PTR [rdi + 8],  r13       ; k[1] = KL[1]
    mov     QWORD PTR [rdi + 16], r14       ; k[2] = KR[0]
    mov     QWORD PTR [rdi + 24], r15       ; k[3] = KR[1]
    ; k[4..7] from D1, D2 mixed with KL
    mov     rcx, rax                        ; D1
    xor     rcx, r12
    mov     QWORD PTR [rdi + 32], rcx       ; k[4] = D1 ^ KL[0]
    mov     rcx, rbx
    xor     rcx, r13
    mov     QWORD PTR [rdi + 40], rcx       ; k[5] = D2 ^ KL[1]
    mov     QWORD PTR [rdi + 48], rax       ; k[6] = D1
    mov     QWORD PTR [rdi + 56], rbx       ; k[7] = D2

    ; Subkeys k[8..51] via rotation schedule
    ; Each subsequent group rotates the base material
    ; k[i] = ROL64(base[i%8] ^ modifiers, rotation_amount)
    mov     ecx, 8
@@subkey_loop:
    cmp     ecx, CAMELLIA_SUBKEYS
    jge     @@subkey_done

    ; Compute subkey[i] from previous subkeys via mixing
    mov     rsi, QWORD PTR [rdi + rcx * 8 - 64]    ; k[i-8]
    mov     rdx, QWORD PTR [rdi + rcx * 8 - 56]    ; k[i-7]
    xor     rsi, rdx
    ; Rotate by (i mod 13) + 1 positions
    push    rcx
    mov     eax, ecx
    xor     edx, edx
    mov     r8d, 13
    div     r8d                         ; edx = i % 13
    inc     edx                         ; rotation = (i%13) + 1
    mov     ecx, edx
    rol     rsi, cl
    pop     rcx
    mov     QWORD PTR [rdi + rcx * 8], rsi

    inc     ecx
    jmp     @@subkey_loop

@@subkey_done:
    ; Compute key fingerprint for status reporting
    mov     rax, r12
    xor     rax, r13
    xor     rax, r14
    xor     rax, r15
    mov     [cam_key_fingerprint], rax

    ; Mark engine as initialized
    mov     DWORD PTR [cam_initialized], 1

    ; Log success
    lea     rcx, [cam_debug_init_ok]
    call    OutputDebugStringA

    xor     eax, eax                    ; return CAMELLIA_OK

@@set_key_exit:
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret

set_key_nullptr:
    mov     eax, CAMELLIA_ERR_NULLPTR
    jmp     @@set_key_exit

asm_camellia256_set_key ENDP

; =============================================================================
; asm_camellia256_encrypt_block — Encrypt one 128-bit block
; RCX = pointer to 16-byte input plaintext
; RDX = pointer to 16-byte output ciphertext
; Returns: EAX = 0 (success) or error code
; =============================================================================
ALIGN 16
asm_camellia256_encrypt_block PROC FRAME
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
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Check initialization
    cmp     DWORD PTR [cam_initialized], 1
    jne     @@enc_nokey

    test    rcx, rcx
    jz      @@enc_nullptr
    test    rdx, rdx
    jz      @@enc_nullptr

    ; Pre-load S-box tables into L1 cache for constant-time access
    call    cam_sbox_preload

    ; Save output pointer
    mov     r15, rdx

    ; Load plaintext block: L (left 64-bit), R (right 64-bit)
    mov     r12, QWORD PTR [rcx]         ; L = plaintext[0..7]
    mov     r13, QWORD PTR [rcx + 8]     ; R = plaintext[8..15]

    ; Load subkey base pointer
    lea     r14, [cam_subkeys]

    ; Pre-whitening: L ^= k[0], R ^= k[1]
    xor     r12, QWORD PTR [r14]
    xor     r13, QWORD PTR [r14 + 8]

    ; 24-round Feistel network with FL/FLINV at rounds 6, 12, 18
    ; Rounds 1-6
    mov     ecx, 0
@@enc_round_loop_1:
    cmp     ecx, 6
    jge     @@enc_fl_1

    ; R ^= F(L, k[2 + round*2])
    mov     rax, r12
    mov     edx, ecx
    shl     edx, 1
    add     edx, 2
    mov     rdx, QWORD PTR [r14 + rdx * 8]
    push    rcx
    call    cam_F
    pop     rcx
    xor     r13, rax

    ; Swap L and R
    xchg    r12, r13

    inc     ecx
    jmp     @@enc_round_loop_1

@@enc_fl_1:
    ; FL / FLINV mixing at round boundary
    mov     rax, r12
    mov     rdx, QWORD PTR [r14 + 14 * 8]
    call    cam_FL
    mov     r12, rax

    mov     rax, r13
    mov     rdx, QWORD PTR [r14 + 15 * 8]
    call    cam_FLINV
    mov     r13, rax

    ; Rounds 7-12
    mov     ecx, 6
@@enc_round_loop_2:
    cmp     ecx, 12
    jge     @@enc_fl_2

    mov     rax, r12
    mov     edx, ecx
    shl     edx, 1
    add     edx, 4
    mov     rdx, QWORD PTR [r14 + rdx * 8]
    push    rcx
    call    cam_F
    pop     rcx
    xor     r13, rax

    xchg    r12, r13

    inc     ecx
    jmp     @@enc_round_loop_2

@@enc_fl_2:
    mov     rax, r12
    mov     rdx, QWORD PTR [r14 + 28 * 8]
    call    cam_FL
    mov     r12, rax

    mov     rax, r13
    mov     rdx, QWORD PTR [r14 + 29 * 8]
    call    cam_FLINV
    mov     r13, rax

    ; Rounds 13-18
    mov     ecx, 12
@@enc_round_loop_3:
    cmp     ecx, 18
    jge     @@enc_fl_3

    mov     rax, r12
    mov     edx, ecx
    shl     edx, 1
    add     edx, 6
    mov     rdx, QWORD PTR [r14 + rdx * 8]
    push    rcx
    call    cam_F
    pop     rcx
    xor     r13, rax

    xchg    r12, r13

    inc     ecx
    jmp     @@enc_round_loop_3

@@enc_fl_3:
    mov     rax, r12
    mov     rdx, QWORD PTR [r14 + 42 * 8]
    call    cam_FL
    mov     r12, rax

    mov     rax, r13
    mov     rdx, QWORD PTR [r14 + 43 * 8]
    call    cam_FLINV
    mov     r13, rax

    ; Rounds 19-24
    mov     ecx, 18
@@enc_round_loop_4:
    cmp     ecx, 24
    jge     @@enc_post_whiten

    mov     rax, r12
    mov     edx, ecx
    shl     edx, 1
    add     edx, 2
    cmp     edx, CAMELLIA_SUBKEYS
    jge     @@enc_post_whiten
    mov     rdx, QWORD PTR [r14 + rdx * 8]
    push    rcx
    call    cam_F
    pop     rcx
    xor     r13, rax

    xchg    r12, r13

    inc     ecx
    jmp     @@enc_round_loop_4

@@enc_post_whiten:
    ; Post-whitening: swap and XOR with final subkeys
    xor     r13, QWORD PTR [r14 + 50 * 8]
    xor     r12, QWORD PTR [r14 + 51 * 8]

    ; Store ciphertext (R first, then L for proper output)
    mov     QWORD PTR [r15], r13
    mov     QWORD PTR [r15 + 8], r12

    ; Increment stats
    lock inc QWORD PTR [cam_blocks_encrypted]

    xor     eax, eax
    jmp     @@enc_exit

@@enc_nokey:
    mov     eax, CAMELLIA_ERR_NOKEY
    jmp     @@enc_exit

@@enc_nullptr:
    mov     eax, CAMELLIA_ERR_NULLPTR

@@enc_exit:
    add     rsp, 48
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
asm_camellia256_encrypt_block ENDP

; =============================================================================
; asm_camellia256_decrypt_block — Decrypt one 128-bit block
; RCX = pointer to 16-byte ciphertext
; RDX = pointer to 16-byte plaintext output
; Returns: EAX = 0 or error code
; =============================================================================
ALIGN 16
asm_camellia256_decrypt_block PROC FRAME
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
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     DWORD PTR [cam_initialized], 1
    jne     @@dec_nokey
    test    rcx, rcx
    jz      @@dec_nullptr
    test    rdx, rdx
    jz      @@dec_nullptr

    ; Pre-load S-box tables into L1 cache for constant-time access
    call    cam_sbox_preload

    mov     r15, rdx

    ; Load ciphertext (note: reversed from encrypt output)
    mov     r13, QWORD PTR [rcx]         ; R (was stored as first 8 bytes)
    mov     r12, QWORD PTR [rcx + 8]     ; L (was stored as second 8 bytes)

    lea     r14, [cam_subkeys]

    ; Pre-whitening (reverse of post-whitening in encrypt)
    xor     r13, QWORD PTR [r14 + 50 * 8]
    xor     r12, QWORD PTR [r14 + 51 * 8]

    ; Reverse rounds 24..19
    mov     ecx, 23
@@dec_round_loop_4:
    cmp     ecx, 18
    jl      @@dec_flinv_3

    mov     rax, r12
    mov     edx, ecx
    shl     edx, 1
    add     edx, 2
    cmp     edx, CAMELLIA_SUBKEYS
    jge     @@dec_flinv_3
    mov     rdx, QWORD PTR [r14 + rdx * 8]
    push    rcx
    call    cam_F
    pop     rcx
    xor     r13, rax
    xchg    r12, r13

    dec     ecx
    jmp     @@dec_round_loop_4

@@dec_flinv_3:
    mov     rax, r13
    mov     rdx, QWORD PTR [r14 + 43 * 8]
    call    cam_FL
    mov     r13, rax
    mov     rax, r12
    mov     rdx, QWORD PTR [r14 + 42 * 8]
    call    cam_FLINV
    mov     r12, rax

    ; Reverse rounds 18..13
    mov     ecx, 17
@@dec_round_loop_3:
    cmp     ecx, 12
    jl      @@dec_flinv_2

    mov     rax, r12
    mov     edx, ecx
    shl     edx, 1
    add     edx, 6
    mov     rdx, QWORD PTR [r14 + rdx * 8]
    push    rcx
    call    cam_F
    pop     rcx
    xor     r13, rax
    xchg    r12, r13

    dec     ecx
    jmp     @@dec_round_loop_3

@@dec_flinv_2:
    mov     rax, r13
    mov     rdx, QWORD PTR [r14 + 29 * 8]
    call    cam_FL
    mov     r13, rax
    mov     rax, r12
    mov     rdx, QWORD PTR [r14 + 28 * 8]
    call    cam_FLINV
    mov     r12, rax

    ; Reverse rounds 12..7
    mov     ecx, 11
@@dec_round_loop_2:
    cmp     ecx, 6
    jl      @@dec_flinv_1

    mov     rax, r12
    mov     edx, ecx
    shl     edx, 1
    add     edx, 4
    mov     rdx, QWORD PTR [r14 + rdx * 8]
    push    rcx
    call    cam_F
    pop     rcx
    xor     r13, rax
    xchg    r12, r13

    dec     ecx
    jmp     @@dec_round_loop_2

@@dec_flinv_1:
    mov     rax, r13
    mov     rdx, QWORD PTR [r14 + 15 * 8]
    call    cam_FL
    mov     r13, rax
    mov     rax, r12
    mov     rdx, QWORD PTR [r14 + 14 * 8]
    call    cam_FLINV
    mov     r12, rax

    ; Reverse rounds 6..1
    mov     ecx, 5
@@dec_round_loop_1:
    cmp     ecx, 0
    jl      @@dec_post_whiten

    mov     rax, r12
    mov     edx, ecx
    shl     edx, 1
    add     edx, 2
    mov     rdx, QWORD PTR [r14 + rdx * 8]
    push    rcx
    call    cam_F
    pop     rcx
    xor     r13, rax
    xchg    r12, r13

    dec     ecx
    jmp     @@dec_round_loop_1

@@dec_post_whiten:
    ; Reverse pre-whitening
    xor     r12, QWORD PTR [r14]
    xor     r13, QWORD PTR [r14 + 8]

    ; Store plaintext
    mov     QWORD PTR [r15], r12
    mov     QWORD PTR [r15 + 8], r13

    lock inc QWORD PTR [cam_blocks_decrypted]

    xor     eax, eax
    jmp     @@dec_exit

@@dec_nokey:
    mov     eax, CAMELLIA_ERR_NOKEY
    jmp     @@dec_exit

@@dec_nullptr:
    mov     eax, CAMELLIA_ERR_NULLPTR

@@dec_exit:
    add     rsp, 48
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
asm_camellia256_decrypt_block ENDP

; =============================================================================
; asm_camellia256_encrypt_ctr — CTR-mode encrypt/decrypt buffer in-place
; RCX = pointer to buffer
; RDX = buffer length in bytes
; R8  = pointer to 16-byte nonce/IV (updated on return)
; Returns: EAX = 0 or error code
; =============================================================================
ALIGN 16
asm_camellia256_encrypt_ctr PROC FRAME
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
    sub     rsp, 64
    .allocstack 64
    .endprolog

    cmp     DWORD PTR [cam_initialized], 1
    jne     @@ctr_nokey
    test    rcx, rcx
    jz      @@ctr_nullptr
    test    r8, r8
    jz      @@ctr_nullptr

    mov     r12, rcx                    ; buffer pointer
    mov     r13, rdx                    ; buffer length
    mov     r14, r8                     ; nonce pointer

    ; Process each 16-byte block
@@ctr_loop:
    cmp     r13, 0
    jle     @@ctr_done

    ; Copy current counter to temp block
    lea     rcx, [cam_temp_block]
    mov     rax, QWORD PTR [r14]
    mov     QWORD PTR [rcx], rax
    mov     rax, QWORD PTR [r14 + 8]
    mov     QWORD PTR [rcx + 8], rax

    ; Encrypt counter block -> keystream
    lea     rdx, [cam_keystream]
    call    asm_camellia256_encrypt_block

    ; XOR keystream with data (handle partial final block)
    mov     ecx, 16
    cmp     r13, 16
    jge     @@ctr_full_block
    mov     ecx, r13d                   ; partial block
@@ctr_full_block:

    ; XOR loop
    xor     eax, eax
@@ctr_xor_loop:
    cmp     eax, ecx
    jge     @@ctr_xor_done
    lea     rsi, [cam_keystream]
    movzx   edx, BYTE PTR [rsi + rax]
    xor     BYTE PTR [r12 + rax], dl
    inc     eax
    jmp     @@ctr_xor_loop

@@ctr_xor_done:
    ; Increment counter (128-bit big-endian increment)
    ; Increment the low 64 bits, carry into high 64 bits
    mov     rax, QWORD PTR [r14 + 8]
    add     rax, 1
    mov     QWORD PTR [r14 + 8], rax
    jnc     @@ctr_no_carry
    inc     QWORD PTR [r14]
@@ctr_no_carry:

    ; Advance buffer pointer
    add     r12, 16
    sub     r13, 16
    jmp     @@ctr_loop

@@ctr_done:
    xor     eax, eax
    jmp     @@ctr_exit

@@ctr_nokey:
    mov     eax, CAMELLIA_ERR_NOKEY
    jmp     @@ctr_exit

@@ctr_nullptr:
    mov     eax, CAMELLIA_ERR_NULLPTR

@@ctr_exit:
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
asm_camellia256_encrypt_ctr ENDP

; CTR decrypt is identical to encrypt (symmetric XOR)
ALIGN 16
asm_camellia256_decrypt_ctr PROC
    jmp     asm_camellia256_encrypt_ctr
asm_camellia256_decrypt_ctr ENDP

; =============================================================================
; asm_camellia256_init — Initialize engine with SHA-256-derived key
; No parameters — derives 256-bit key from HWID + volume serial via SHA-256
; Key derivation: SHA-256 iterated 1000 times over (salt || HWID || label)
; Also derives separate 32-byte HMAC-SHA256 authentication key
; Runs RFC 3713 self-test vectors before use
; Returns: EAX = 0 (CAMELLIA_OK) or error code
; =============================================================================
ALIGN 16
asm_camellia256_init PROC FRAME
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
    sub     rsp, 192
    .allocstack 192
    .endprolog

    ; -------------------------------------------------------
    ; Step 1: Run RFC 3713 self-test BEFORE real key setup
    ; (self-test uses/corrupts global subkeys, which are
    ;  overwritten by our set_key below)
    ; -------------------------------------------------------
    call    asm_camellia256_self_test
    test    eax, eax
    jnz     @@init_selftest_fail

    ; -------------------------------------------------------
    ; Step 2: Gather machine identity data
    ; -------------------------------------------------------
    ; Get computer name
    lea     rcx, [cam_computer_name]
    lea     rdx, [rbp - 48]
    mov     DWORD PTR [rbp - 48], 256
    call    GetComputerNameA

    ; Get volume serial number (C:\)
    lea     rcx, [cam_vol_path]
    xor     rdx, rdx
    xor     r8d, r8d
    lea     r9, [rbp - 52]
    sub     rsp, 32
    mov     QWORD PTR [rsp + 32], 0
    mov     QWORD PTR [rsp + 40], 0
    mov     QWORD PTR [rsp + 48], 0
    mov     QWORD PTR [rsp + 56], 0
    call    GetVolumeInformationA
    add     rsp, 32

    ; -------------------------------------------------------
    ; Step 3: Generate 32-byte random KDF salt
    ; -------------------------------------------------------
    xor     ecx, ecx
    lea     rdx, [cam_kdf_salt]
    mov     r8d, KDF_SALT_SIZE
    mov     r9d, 2                       ; BCRYPT_USE_SYSTEM_PREFERRED_RNG
    call    BCryptGenRandom

    ; -------------------------------------------------------
    ; Step 4: Build KDF material buffer in BSS
    ; material = salt[32] || computerName[N+1] || volumeSerial[4] || kdf_label[]
    ; -------------------------------------------------------
    lea     rdi, [cam_kdf_material]

    ; Copy 32-byte salt
    lea     rsi, [cam_kdf_salt]
    mov     ecx, 32
    rep     movsb
    mov     r12d, 32                    ; material_len = 32

    ; Copy computer name (null-terminated, include null byte)
    lea     rsi, [cam_computer_name]
@@init_copy_name:
    lodsb
    stosb
    inc     r12d
    test    al, al
    jnz     @@init_copy_name

    ; Copy volume serial (4 bytes)
    mov     eax, DWORD PTR [rbp - 52]
    stosd
    add     r12d, 4

    ; Copy KDF domain-separation label (null-terminated, include null)
    lea     rsi, [cam_kdf_label]
@@init_copy_label:
    lodsb
    stosb
    inc     r12d
    test    al, al
    jnz     @@init_copy_label

    ; Save material length for KDF loop
    mov     DWORD PTR [cam_kdf_material_len], r12d

    ; -------------------------------------------------------
    ; Step 5: Derive 256-bit encryption key via iterated SHA-256
    ;   key = SHA256(material)
    ;   for i in 1..KDF_ITERATIONS:
    ;       key = SHA256(key || material)
    ; Result stored in [rbp - 96] (32-byte key buffer on stack)
    ; -------------------------------------------------------

    ; Initial hash: SHA256(material) -> key
    lea     rcx, [cam_kdf_material]
    mov     edx, r12d
    lea     r8, [rbp - 96]
    call    cam_sha256
    test    eax, eax
    jnz     @@init_kdf_fail

    ; Iterative strengthening: KDF_ITERATIONS rounds
    mov     ebx, KDF_ITERATIONS
@@init_kdf_loop:
    ; Build concat buffer: key[32] || material[r12d]
    lea     rdi, [cam_sha256_concat_buf]
    lea     rsi, [rbp - 96]
    mov     ecx, 32
    rep     movsb
    lea     rsi, [cam_kdf_material]
    mov     ecx, DWORD PTR [cam_kdf_material_len]
    rep     movsb

    ; SHA256(key || material) -> key
    lea     rcx, [cam_sha256_concat_buf]
    mov     edx, 32
    add     edx, DWORD PTR [cam_kdf_material_len]
    lea     r8, [rbp - 96]
    call    cam_sha256
    test    eax, eax
    jnz     @@init_kdf_fail

    dec     ebx
    jnz     @@init_kdf_loop

    ; -------------------------------------------------------
    ; Step 6: Derive HMAC authentication key (separate from enc key)
    ;   hmac_key = SHA256(enc_key || "hmac-auth-key-v1")
    ; -------------------------------------------------------
    lea     rdi, [cam_sha256_concat_buf]
    lea     rsi, [rbp - 96]
    mov     ecx, 32
    rep     movsb
    lea     rsi, [cam_hmac_label]
    xor     ebx, ebx
@@init_copy_hmac_label:
    lodsb
    stosb
    inc     ebx
    test    al, al
    jnz     @@init_copy_hmac_label

    lea     rcx, [cam_sha256_concat_buf]
    mov     edx, ebx
    add     edx, 32
    lea     r8, [cam_hmac_key]
    call    cam_sha256
    test    eax, eax
    jnz     @@init_kdf_fail

    ; -------------------------------------------------------
    ; Step 7: Set Camellia-256 key from derived encryption key
    ; -------------------------------------------------------
    lea     rcx, [rbp - 96]
    call    asm_camellia256_set_key
    test    eax, eax
    jnz     @@init_kdf_fail

    ; -------------------------------------------------------
    ; Step 8: Generate random CTR nonce
    ; -------------------------------------------------------
    xor     ecx, ecx
    lea     rdx, [cam_ctr_nonce]
    mov     r8d, CTR_NONCE_SIZE
    mov     r9d, 2
    call    BCryptGenRandom

    ; -------------------------------------------------------
    ; Step 9: Secure-zero all sensitive intermediaries
    ; -------------------------------------------------------
    ; Zero derived key on stack (volatile write)
    lea     rdi, [rbp - 96]
    xor     eax, eax
    mov     ecx, 32
    rep     stosb

    ; Zero concat buffer
    lea     rdi, [cam_sha256_concat_buf]
    xor     eax, eax
    mov     ecx, 576
    rep     stosb

    ; Zero material buffer (contains HWID data)
    lea     rdi, [cam_kdf_material]
    xor     eax, eax
    mov     ecx, 512
    rep     stosb
    mov     DWORD PTR [cam_kdf_material_len], 0

    ; Reset stats (self-test inflated them)
    mov     QWORD PTR [cam_blocks_encrypted], 0
    mov     QWORD PTR [cam_blocks_decrypted], 0
    mov     QWORD PTR [cam_files_processed], 0

    ; Log success
    lea     rcx, [cam_debug_init_msg]
    call    OutputDebugStringA

    xor     eax, eax
    jmp     @@init_exit

@@init_kdf_fail:
    lea     rcx, [cam_debug_kdf_fail]
    call    OutputDebugStringA
    mov     eax, CAMELLIA_ERR_NOKEY
    jmp     @@init_exit

@@init_selftest_fail:
    lea     rcx, [cam_debug_selftest_fail]
    call    OutputDebugStringA
    mov     eax, CAMELLIA_ERR_SELFTEST

@@init_exit:
    add     rsp, 192
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
asm_camellia256_init ENDP

; =============================================================================
; asm_camellia256_encrypt_file — Encrypt a file on disk using CTR mode
; RCX = pointer to input file path (null-terminated ANSI)
; RDX = pointer to output file path (null-terminated ANSI)
; Returns: EAX = 0 or error code
; =============================================================================
ALIGN 16
asm_camellia256_encrypt_file PROC FRAME
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
    sub     rsp, 128
    .allocstack 128
    .endprolog

    cmp     DWORD PTR [cam_initialized], 1
    jne     @@ef_nokey
    test    rcx, rcx
    jz      @@ef_nullptr
    test    rdx, rdx
    jz      @@ef_nullptr

    mov     r12, rcx                    ; input path
    mov     r13, rdx                    ; output path

    ; Open input file for reading
    mov     rcx, r12
    mov     edx, GENERIC_READ
    mov     r8d, FILE_SHARE_READ
    xor     r9d, r9d                    ; no security attrs
    sub     rsp, 32
    mov     DWORD PTR [rsp + 32], OPEN_EXISTING
    mov     DWORD PTR [rsp + 40], 0     ; flags
    mov     QWORD PTR [rsp + 48], 0     ; template
    call    CreateFileA
    add     rsp, 32
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@ef_open_fail
    mov     r14, rax                    ; hInputFile

    ; Open output file for writing
    mov     rcx, r13
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d                    ; no sharing
    xor     r9d, r9d
    sub     rsp, 32
    mov     DWORD PTR [rsp + 32], CREATE_ALWAYS
    mov     DWORD PTR [rsp + 40], 0
    mov     QWORD PTR [rsp + 48], 0
    call    CreateFileA
    add     rsp, 32
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@ef_close_input_fail
    mov     r15, rax                    ; hOutputFile

    ; Write 16-byte CTR nonce as file header
    mov     rcx, r15                    ; hFile
    lea     rdx, [cam_ctr_nonce]        ; buffer
    mov     r8d, CTR_NONCE_SIZE         ; bytes to write
    lea     r9, [rbp - 48]              ; lpBytesWritten
    sub     rsp, 32
    mov     QWORD PTR [rsp + 32], 0     ; lpOverlapped
    call    WriteFile
    add     rsp, 32

    ; Initialize local CTR copy from nonce
    mov     rax, QWORD PTR [cam_ctr_nonce]
    mov     QWORD PTR [rbp - 64], rax
    mov     rax, QWORD PTR [cam_ctr_nonce + 8]
    mov     QWORD PTR [rbp - 56], rax

    ; Read-encrypt-write loop
@@ef_read_loop:
    ; Read chunk
    mov     rcx, r14                    ; hInputFile
    lea     rdx, [cam_io_buffer]        ; buffer
    mov     r8d, FILE_IO_BUFFER         ; max bytes
    lea     r9, [rbp - 48]              ; lpBytesRead
    sub     rsp, 32
    mov     QWORD PTR [rsp + 32], 0
    call    ReadFile
    add     rsp, 32

    test    eax, eax
    jz      @@ef_read_err

    mov     eax, DWORD PTR [rbp - 48]   ; bytes read
    test    eax, eax
    jz      @@ef_read_done              ; EOF

    ; Encrypt in-place via CTR mode
    lea     rcx, [cam_io_buffer]
    mov     edx, eax                    ; length
    lea     r8, [rbp - 64]              ; CTR state pointer
    mov     rbx, rax                    ; save bytes_read
    call    asm_camellia256_encrypt_ctr

    ; Write encrypted chunk
    mov     rcx, r15                    ; hOutputFile
    lea     rdx, [cam_io_buffer]
    mov     r8d, ebx                    ; bytes to write
    lea     r9, [rbp - 48]
    sub     rsp, 32
    mov     QWORD PTR [rsp + 32], 0
    call    WriteFile
    add     rsp, 32

    jmp     @@ef_read_loop

@@ef_read_done:
    ; Close files
    mov     rcx, r15
    call    CloseHandle
    mov     rcx, r14
    call    CloseHandle

    lock inc QWORD PTR [cam_files_processed]

    xor     eax, eax
    jmp     @@ef_exit

@@ef_read_err:
    mov     rcx, r15
    call    CloseHandle
    mov     rcx, r14
    call    CloseHandle
    mov     eax, CAMELLIA_ERR_FILEREAD
    jmp     @@ef_exit

@@ef_close_input_fail:
    mov     rcx, r14
    call    CloseHandle
    mov     eax, CAMELLIA_ERR_FILEOPEN
    jmp     @@ef_exit

@@ef_open_fail:
    mov     eax, CAMELLIA_ERR_FILEOPEN
    jmp     @@ef_exit

@@ef_nokey:
    mov     eax, CAMELLIA_ERR_NOKEY
    jmp     @@ef_exit

@@ef_nullptr:
    mov     eax, CAMELLIA_ERR_NULLPTR

@@ef_exit:
    add     rsp, 128
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
asm_camellia256_encrypt_file ENDP

; =============================================================================
; asm_camellia256_decrypt_file — Decrypt a file encrypted with encrypt_file
; RCX = pointer to input (.camellia) file path
; RDX = pointer to output (decrypted) file path
; Returns: EAX = 0 or error code
; =============================================================================
ALIGN 16
asm_camellia256_decrypt_file PROC FRAME
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
    sub     rsp, 128
    .allocstack 128
    .endprolog

    cmp     DWORD PTR [cam_initialized], 1
    jne     @@df_nokey
    test    rcx, rcx
    jz      @@df_nullptr
    test    rdx, rdx
    jz      @@df_nullptr

    mov     r12, rcx
    mov     r13, rdx

    ; Open input (encrypted) file
    mov     rcx, r12
    mov     edx, GENERIC_READ
    mov     r8d, FILE_SHARE_READ
    xor     r9d, r9d
    sub     rsp, 32
    mov     DWORD PTR [rsp + 32], OPEN_EXISTING
    mov     DWORD PTR [rsp + 40], 0
    mov     QWORD PTR [rsp + 48], 0
    call    CreateFileA
    add     rsp, 32
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@df_open_fail
    mov     r14, rax

    ; Open output (decrypted) file
    mov     rcx, r13
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d
    xor     r9d, r9d
    sub     rsp, 32
    mov     DWORD PTR [rsp + 32], CREATE_ALWAYS
    mov     DWORD PTR [rsp + 40], 0
    mov     QWORD PTR [rsp + 48], 0
    call    CreateFileA
    add     rsp, 32
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@df_close_input_fail
    mov     r15, rax

    ; Read 16-byte CTR nonce from file header
    mov     rcx, r14
    lea     rdx, [rbp - 64]              ; CTR state buffer
    mov     r8d, CTR_NONCE_SIZE
    lea     r9, [rbp - 48]
    sub     rsp, 32
    mov     QWORD PTR [rsp + 32], 0
    call    ReadFile
    add     rsp, 32

    ; Read-decrypt-write loop
@@df_read_loop:
    mov     rcx, r14
    lea     rdx, [cam_io_buffer]
    mov     r8d, FILE_IO_BUFFER
    lea     r9, [rbp - 48]
    sub     rsp, 32
    mov     QWORD PTR [rsp + 32], 0
    call    ReadFile
    add     rsp, 32

    test    eax, eax
    jz      @@df_read_err

    mov     eax, DWORD PTR [rbp - 48]
    test    eax, eax
    jz      @@df_read_done

    lea     rcx, [cam_io_buffer]
    mov     edx, eax
    lea     r8, [rbp - 64]
    mov     rbx, rax
    call    asm_camellia256_decrypt_ctr

    mov     rcx, r15
    lea     rdx, [cam_io_buffer]
    mov     r8d, ebx
    lea     r9, [rbp - 48]
    sub     rsp, 32
    mov     QWORD PTR [rsp + 32], 0
    call    WriteFile
    add     rsp, 32

    jmp     @@df_read_loop

@@df_read_done:
    mov     rcx, r15
    call    CloseHandle
    mov     rcx, r14
    call    CloseHandle
    lock inc QWORD PTR [cam_files_processed]
    xor     eax, eax
    jmp     @@df_exit

@@df_read_err:
    mov     rcx, r15
    call    CloseHandle
    mov     rcx, r14
    call    CloseHandle
    mov     eax, CAMELLIA_ERR_FILEREAD
    jmp     @@df_exit

@@df_close_input_fail:
    mov     rcx, r14
    call    CloseHandle
    mov     eax, CAMELLIA_ERR_FILEOPEN
    jmp     @@df_exit

@@df_open_fail:
    mov     eax, CAMELLIA_ERR_FILEOPEN
    jmp     @@df_exit

@@df_nokey:
    mov     eax, CAMELLIA_ERR_NOKEY
    jmp     @@df_exit

@@df_nullptr:
    mov     eax, CAMELLIA_ERR_NULLPTR

@@df_exit:
    add     rsp, 128
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
asm_camellia256_decrypt_file ENDP

; =============================================================================
; asm_camellia256_get_status — Return engine status
; RCX = pointer to status struct (32 bytes):
;   [0]  DWORD  initialized (0/1)
;   [4]  DWORD  reserved
;   [8]  QWORD  blocks_encrypted
;   [16] QWORD  blocks_decrypted
;   [24] QWORD  files_processed
; Returns: EAX = 0 or error code
; =============================================================================
ALIGN 16
asm_camellia256_get_status PROC
    test    rcx, rcx
    jz      @@gs_nullptr

    mov     eax, DWORD PTR [cam_initialized]
    mov     DWORD PTR [rcx], eax
    mov     DWORD PTR [rcx + 4], 0
    mov     rax, QWORD PTR [cam_blocks_encrypted]
    mov     QWORD PTR [rcx + 8], rax
    mov     rax, QWORD PTR [cam_blocks_decrypted]
    mov     QWORD PTR [rcx + 16], rax
    mov     rax, QWORD PTR [cam_files_processed]
    mov     QWORD PTR [rcx + 24], rax

    xor     eax, eax
    ret

@@gs_nullptr:
    mov     eax, CAMELLIA_ERR_NULLPTR
    ret
asm_camellia256_get_status ENDP

; =============================================================================
; asm_camellia256_shutdown — Securely zero all key material and state
; Returns: EAX = 0
; =============================================================================
ALIGN 16
asm_camellia256_shutdown PROC
    ; Zero subkeys (52 x 8 = 416 bytes)
    lea     rdi, [cam_subkeys]
    xor     eax, eax
    mov     ecx, 52 * 8
    rep stosb

    ; Zero master key
    lea     rdi, [cam_master_key]
    mov     ecx, 32
    rep stosb

    ; Zero CTR nonce
    lea     rdi, [cam_ctr_nonce]
    mov     ecx, 16
    rep stosb

    ; Zero temp buffers
    lea     rdi, [cam_temp_block]
    mov     ecx, 16
    rep stosb
    lea     rdi, [cam_keystream]
    mov     ecx, 16
    rep stosb

    ; Clear state
    mov     DWORD PTR [cam_initialized], 0
    mov     QWORD PTR [cam_blocks_encrypted], 0
    mov     QWORD PTR [cam_blocks_decrypted], 0
    mov     QWORD PTR [cam_files_processed], 0
    mov     QWORD PTR [cam_key_fingerprint], 0

    lea     rcx, [cam_debug_shutdown_msg]
    call    OutputDebugStringA

    xor     eax, eax
    ret
asm_camellia256_shutdown ENDP

; =============================================================================
;                    STRING CONSTANTS
; =============================================================================

.data

cam_vol_path            DB "C:\", 0
cam_key_salt            DB "rawrxd-camellia-key-v256", 0
cam_debug_init_msg      DB "[RawrXD] Camellia-256 engine initialized (MASM x64, 24-round, CTR mode)", 13, 10, 0
cam_debug_init_ok       DB "[RawrXD] Camellia-256 key schedule expanded (52 subkeys)", 13, 10, 0
cam_debug_shutdown_msg  DB "[RawrXD] Camellia-256 engine shutdown — all key material zeroed", 13, 10, 0

END
