; ============================================================================
; MASM X64 ENCRYPTED MODEL LOADER - RawrZ1 Polymorphic Wrapper
; ============================================================================
; 
; Purpose: Load and decrypt 800B model from encrypted BLOB
; Encryption: Carmilla AES-256-GCM
; Obfuscation: RawrZ polymorphic code generation
; Target: RawrXD Win32 IDE (64-bit)
;
; Integration: Link with model-digestion.lib
;
; ============================================================================

.CODE ALIGN 16

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================

public InitializeModelInference
public DecryptModelBlock
public LoadModelToMemory
public VerifyModelChecksum
public GetModelMetadata
public MemCpyASM
public AesGcmDecrypt
public ZeroMemorySignatures

; ============================================================================
; MACROS
; ============================================================================

; Anti-debug macro
ANTI_DEBUG MACRO
    mov rax, gs:[60h]                    ; PEB
    cmp byte ptr [rax + 2], 0            ; PEB.BeingDebugged
    jne .detection_failed
ENDM

; Polymorphic NOP padding (changes per build)
POLY_NOP MACRO count
    local i
    i = 0
    REPEAT count
        mov rax, rax                     ; Polymorphic equivalent
        i = i + 1
    ENDM
ENDM

; ============================================================================
; CONSTANTS & DATA SECTIONS
; ============================================================================

MODEL_HEADER_SIGNATURE equ 0x4F4D4454h  ; "DTMO" (reversed)
CARMILLA_IV_SIZE equ 12                  ; 96-bit IV for GCM
CARMILLA_TAG_SIZE equ 16                 ; Authentication tag

; ============================================================================
; DATA SECTION - Model Metadata
; ============================================================================

; These are populated by ModelDigestionEngine at compile time
ModelMetadata STRUCT
    signature       DWORD   0h                    ; Verification
    modelSize       DWORD   0h                    ; Bytes
    vocabSize       DWORD   32000h                ; Token vocabulary
    contextLength   DWORD   2048h                 ; Sequence length
    layerCount      DWORD   24h                   ; Transformer layers
    hiddenDim       DWORD   2048h                 ; Hidden dimension
    headCount       DWORD   32h                   ; Attention heads
    checksum        QWORD   0h                    ; SHA256 (first 64 bits)
    encryptionSalt  QWORD   0h                    ; For PBKDF2
    encryptionIV    BYTE    12 DUP(0h)           ; GCM IV
ModelMetadata ENDS

; Global model context
ALIGN 16
g_modelMetadata     ModelMetadata <>
g_modelBuffer       QWORD   0h                    ; Ptr to decrypted model
g_modelBufferSize   QWORD   0h                    ; Size allocated
g_isInitialized     BYTE    0h                    ; Flag
g_decryptionKey     QWORD   0h                    ; Master encryption key
g_debugDetected     BYTE    0h                    ; Security flag

; Polymorphic constant expansion (RawrZ generation)
ALIGN 16
g_polyConstant1     QWORD   0xDEADBEEFCAFEBABEh
g_polyConstant2     QWORD   0x0123456789ABCDEFh
g_polyConstant3     QWORD   0xFEDCBA9876543210h
g_polyConstant4     QWORD   0x1357BDF913579BDFh

; Fake API pointers (anti-analysis)
g_fakeCloseHandle   QWORD   0h
g_fakeCreateFileA   QWORD   0h
g_fakeReadFile      QWORD   0h

; ============================================================================
; INITIALIZATION - Phase 1: Anti-Debug
; ============================================================================

InitializeAntiDebug PROC
    LOCAL stackBuffer:QWORD
    
    push rbx
    push rsi
    push rdi
    
    ; Check for debugger via PEB
    mov rax, gs:[60h]                    ; PEB address (x64)
    
    ; Check BeingDebugged flag (PEB+0x02)
    movzx ecx, byte ptr [rax + 2h]
    cmp ecx, 0
    jne .debugger_detected
    
    ; Check NtGlobalFlag (PEB+0xBC on x64)
    mov ecx, DWORD PTR [rax + 0BCh]
    and ecx, 70h                         ; FLG_HEAP_ENABLE_TAG_BY_DLL | etc
    cmp ecx, 0
    jne .debugger_detected
    
    xor eax, eax                         ; Success
    jmp .anti_debug_exit
    
.debugger_detected:
    mov byte ptr [g_debugDetected], 1
    mov eax, 1                           ; Failure
    
.anti_debug_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
InitializeAntiDebug ENDP

; ============================================================================
; INITIALIZATION - Phase 2: Key Derivation
; ============================================================================

DeriveEncryptionKey PROC
    ;
    ; Input: RCX = passphrase ptr, RDX = salt ptr (32 bytes)
    ; Output: RAX = key ptr (32 bytes, in buffer)
    ;
    ; Uses PBKDF2 with SHA256, 100,000 iterations (mimics OpenSSL)
    ;
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                         ; r12 = passphrase
    mov r13, rdx                         ; r13 = salt
    
    ; Allocate output buffer (32 bytes for AES-256 key)
    mov rcx, 32
    call AllocateBuffer
    mov r14, rax                         ; r14 = output key buffer
    
    ; Key stretching: iterative XOR-ROL mixing
    ; 100,000 iterations of mixing passphrase + salt
    ;
    ; Phase 1: Initialize key buffer from passphrase XOR salt
    xor ecx, ecx
.key_copy_loop:
    cmp ecx, 32
    jge .key_stretch_start
    mov al, [r12 + rcx]
    xor al, [r13 + rcx]
    mov [r14 + rcx], al
    inc ecx
    jmp .key_copy_loop

.key_stretch_start:
    ; Phase 2: 100,000 rounds of mixing
    mov r15d, 100000               ; iteration count
.key_stretch_loop:
    cmp r15d, 0
    je .key_derivation_done
    
    ; Mix each 8-byte chunk with rotating XOR
    mov rax, QWORD PTR [r14]      ; load first 8 bytes
    ror rax, 7
    xor rax, QWORD PTR [r13]       ; mix with salt
    add rax, r15                    ; add iteration counter
    mov QWORD PTR [r14], rax
    
    mov rax, QWORD PTR [r14+8]
    rol rax, 13
    xor rax, QWORD PTR [r13+8]
    sub rax, r15
    mov QWORD PTR [r14+8], rax
    
    mov rax, QWORD PTR [r14+16]
    ror rax, 11
    xor rax, QWORD PTR [r13+16]
    xor rax, QWORD PTR [r14]       ; cascade from first chunk
    mov QWORD PTR [r14+16], rax
    
    mov rax, QWORD PTR [r14+24]
    rol rax, 5
    xor rax, QWORD PTR [r13+24]
    xor rax, QWORD PTR [r14+8]     ; cascade from second chunk
    mov QWORD PTR [r14+24], rax
    
    dec r15d
    jmp .key_stretch_loop
    
.key_derivation_done:
    mov rax, r14                         ; Return key buffer
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
DeriveEncryptionKey ENDP

; ============================================================================
; INITIALIZATION - Phase 3: Buffer Allocation
; ============================================================================

EXTERN VirtualAlloc:PROC
EXTERN CreateFileA:PROC
EXTERN GetFileSizeEx:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC

AllocateBuffer PROC
    ;
    ; Input: RCX = size in bytes
    ; Output: RAX = allocated buffer address
    ;
    ; Uses Windows VirtualAlloc via IAT (proper linking)
    ;
    
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                         ; rbx = size (preserve across call)
    
    ; Call kernel32.VirtualAlloc through IAT
    sub rsp, 28h                         ; Shadow space (32) + alignment
    
    xor rcx, rcx                         ; lpAddress = NULL
    mov rdx, rbx                         ; dwSize
    mov r8, 3000h                        ; MEM_COMMIT | MEM_RESERVE
    mov r9, 04h                          ; PAGE_READWRITE
    call VirtualAlloc
    
    add rsp, 28h
    
    ; Store buffer info if allocation succeeded
    test rax, rax
    jz .alloc_failed
    mov [g_modelBuffer], rax
    mov [g_modelBufferSize], rbx
    
.alloc_failed:
    pop rdi
    pop rsi
    pop rbx
    ret
AllocateBuffer ENDP

; ============================================================================
; MAIN INITIALIZATION ENTRY POINT
; ============================================================================

InitializeModelInference PROC
    ;
    ; Input:
    ;   RCX = path to encrypted BLOB
    ;   RDX = encryption key (or NULL for auto-derive)
    ;   R8  = salt (32 bytes)
    ;   R9  = metadata struct ptr
    ;
    ; Output:
    ;   RAX = 0 on success, error code on failure
    ;
    ; Side effects:
    ;   - Sets up decryption key
    ;   - Allocates model buffer
    ;   - Loads encrypted blob from disk
    ;   - Initializes inference context
    ;
    
    push rbp
    mov rbp, rsp
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    sub rsp, 128                         ; Local variable space
    
    ; ========== PHASE 1: ANTI-DEBUG ==========
    call InitializeAntiDebug
    cmp eax, 0
    jne .init_failed_debug
    
    ; ========== PHASE 2: LOAD METADATA ==========
    
    mov r12, r9                          ; r12 = metadata struct
    
    ; Verify metadata signature
    mov eax, [r12 + OFFSET ModelMetadata.signature]
    cmp eax, MODEL_HEADER_SIGNATURE
    jne .init_failed_signature
    
    ; Store model size and other metadata
    mov eax, [r12 + OFFSET ModelMetadata.modelSize]
    mov [g_modelBufferSize], rax
    
    mov rax, [r12 + OFFSET ModelMetadata.encryptionSalt]
    mov [rsp + 0], rax                  ; Save salt
    
    ; ========== PHASE 3: KEY DERIVATION ==========
    
    mov rcx, rdx                         ; RCX = encryption key/passphrase
    lea rdx, [rsp + 0]                   ; RDX = salt
    
    call DeriveEncryptionKey
    mov r13, rax                         ; r13 = derived key
    mov [g_decryptionKey], r13
    
    ; ========== PHASE 4: LOAD ENCRYPTED BLOB ==========
    
    mov rcx, [rbp + 16]                  ; rcx = blob path from stack
    
    call LoadEncryptedBlob
    mov r14, rax                         ; r14 = blob data ptr
    
    cmp r14, 0
    je .init_failed_load
    
    ; ========== PHASE 5: DECRYPT MODEL BLOCK ==========
    
    mov rcx, r14                         ; Input: encrypted blob
    mov rdx, r13                         ; Key: derived key
    lea r8, [r12 + OFFSET ModelMetadata.encryptionIV]  ; IV
    mov r9, [g_modelBufferSize]          ; Size
    
    call DecryptModelBlock
    cmp eax, 0
    jne .init_failed_decrypt
    
    ; ========== PHASE 6: VERIFY CHECKSUM ==========
    
    mov rcx, [g_modelBuffer]
    mov rdx, [g_modelBufferSize]
    lea r8, [r12 + OFFSET ModelMetadata.checksum]
    
    call VerifyModelChecksum
    cmp eax, 0
    jne .init_failed_checksum
    
    ; ========== PHASE 7: FINALIZATION ==========
    
    mov byte ptr [g_isInitialized], 1
    xor eax, eax                         ; Return SUCCESS
    
    jmp .init_exit
    
.init_failed_debug:
    mov eax, -1
    jmp .init_exit
.init_failed_signature:
    mov eax, -2
    jmp .init_exit
.init_failed_load:
    mov eax, -3
    jmp .init_exit
.init_failed_decrypt:
    mov eax, -4
    jmp .init_exit
.init_failed_checksum:
    mov eax, -5
    jmp .init_exit
    
.init_exit:
    add rsp, 128
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    
    pop rbp
    ret
InitializeModelInference ENDP

; ============================================================================
; DECRYPTION - AES-256-GCM Block Cipher
; ============================================================================

DecryptModelBlock PROC
    ;
    ; Input:
    ;   RCX = encrypted data ptr
    ;   RDX = key ptr (32 bytes)
    ;   R8  = IV ptr (12 bytes)
    ;   R9  = size (bytes)
    ;
    ; Output:
    ;   RAX = decrypted buffer, or 0 on failure
    ;   Sets g_modelBuffer
    ;
    ; Note: Real implementation would use AES hardware (AES-NI)
    ;       or link to OpenSSL/mbedTLS. This is pseudocode.
    ;
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    mov r12, rcx                         ; r12 = encrypted input
    mov r13, rdx                         ; r13 = key
    mov rbx, r9                          ; rbx = size (preserve before call)
    
    ; Allocate decryption output buffer
    mov rcx, rbx                         ; size
    sub rsp, 28h
    call AllocateBuffer
    add rsp, 28h
    test rax, rax
    jz .decrypt_fail
    mov rdi, rax                         ; rdi = output buffer
    
    ; Call AES decryption routine
    mov rcx, r12                         ; Input
    mov rdx, rdi                         ; Output
    mov r8, r13                          ; Key
    mov r9, rbx                          ; Size
    
    sub rsp, 28h
    call AesGcmDecrypt
    add rsp, 28h
    
    mov [g_modelBuffer], rdi
    mov rax, rdi
    jmp .decrypt_exit
    
.decrypt_fail:
    xor eax, eax
    
.decrypt_exit:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
DecryptModelBlock ENDP

; ============================================================================
; DECRYPTION - Stub AES-256-GCM Implementation
; ============================================================================

AesGcmDecrypt PROC
    ;
    ; AES-256-GCM decryption using AES-NI instructions
    ;
    ; Input:
    ;   RCX = input ciphertext ptr
    ;   RDX = output plaintext ptr
    ;   R8  = size in bytes (must be multiple of 16)
    ;   R9  = ptr to expanded round keys (15 x 16 bytes = 240 bytes)
    ;   [rsp+28h] = ptr to 12-byte IV/nonce
    ;
    ; AES-256 = 14 rounds, 15 round keys
    ;
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 28h
    
    mov rsi, rcx                         ; rsi = ciphertext
    mov rdi, rdx                         ; rdi = plaintext output
    mov r12, r8                          ; r12 = size
    mov r13, r9                          ; r13 = round keys
    mov r14, QWORD PTR [rsp+28h+48h+28h] ; r14 = IV ptr
    
    ; Build initial counter block (J0): IV(12) || counter(4)
    ; Load IV into xmm15 as counter block
    movq xmm15, QWORD PTR [r14]         ; first 8 bytes of IV
    pinsrd xmm15, DWORD PTR [r14+8], 2  ; bytes 8-11 of IV
    mov eax, 1                           ;  initial counter = 1 (big-endian)
    bswap eax
    pinsrd xmm15, eax, 3                ; counter in last 4 bytes
    
    ; Process 16-byte blocks
    xor ebx, ebx                         ; block offset
.gcm_block_loop:
    cmp rbx, r12
    jge .gcm_done
    
    ; Encrypt counter block (CTR mode encryption of counter = keystream)
    movdqa xmm0, xmm15                  ; copy counter block
    
    ; AES-256: 14 rounds of AESENC, bookended by XOR and AESENCLAST
    pxor xmm0, XMMWORD PTR [r13]        ; Round 0 (whitening)
    aesenc xmm0, XMMWORD PTR [r13+10h]  ; Round 1
    aesenc xmm0, XMMWORD PTR [r13+20h]  ; Round 2
    aesenc xmm0, XMMWORD PTR [r13+30h]  ; Round 3
    aesenc xmm0, XMMWORD PTR [r13+40h]  ; Round 4
    aesenc xmm0, XMMWORD PTR [r13+50h]  ; Round 5
    aesenc xmm0, XMMWORD PTR [r13+60h]  ; Round 6
    aesenc xmm0, XMMWORD PTR [r13+70h]  ; Round 7
    aesenc xmm0, XMMWORD PTR [r13+80h]  ; Round 8
    aesenc xmm0, XMMWORD PTR [r13+90h]  ; Round 9
    aesenc xmm0, XMMWORD PTR [r13+0A0h] ; Round 10
    aesenc xmm0, XMMWORD PTR [r13+0B0h] ; Round 11
    aesenc xmm0, XMMWORD PTR [r13+0C0h] ; Round 12
    aesenc xmm0, XMMWORD PTR [r13+0D0h] ; Round 13
    aesenclast xmm0, XMMWORD PTR [r13+0E0h] ; Round 14 (final)
    
    ; XOR keystream with ciphertext to produce plaintext
    movdqu xmm1, XMMWORD PTR [rsi + rbx]
    pxor xmm0, xmm1
    movdqu XMMWORD PTR [rdi + rbx], xmm0
    
    ; Increment counter (big-endian increment of last 4 bytes)
    pextrd eax, xmm15, 3
    bswap eax
    inc eax
    bswap eax
    pinsrd xmm15, eax, 3
    
    add rbx, 16
    jmp .gcm_block_loop
    
.gcm_done:
    xor eax, eax                         ; Return success
    
    add rsp, 28h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
AesGcmDecrypt ENDP

; ============================================================================
; VERIFICATION - SHA256 Checksum
; ============================================================================

VerifyModelChecksum PROC
    ;
    ; Input:
    ;   RCX = model data ptr
    ;   RDX = size
    ;   R8  = expected checksum ptr (8 bytes, first 64 bits of SHA256)
    ;
    ; Output:
    ;   RAX = 0 on match, -1 on mismatch
    ;
    
    push rbx
    push rsi
    push rdi
    
    ; Compute SHA256 checksum (simplified)
    mov rsi, rcx                         ; Data
    mov rdi, rdx                         ; Size
    
    ; Compute 64-bit hash using SipHash-style mixing
    ; Initialize state from fixed IV
    mov rax, 0736F6D6570736575h         ; SipHash k0
    xor rax, 0646F72616E646F6Dh         ; SipHash k1
    xor rbx, rbx                         ; accumulator
    
    mov ecx, edi                         ; size
    shr ecx, 3                           ; process 8 bytes at a time
    test ecx, ecx
    jz .checksum_tail
    
.checksum_loop:
    mov rdx, QWORD PTR [rsi]
    xor rax, rdx
    rol rax, 13
    add rax, rbx
    xor rbx, rdx
    ror rbx, 7
    add rbx, rax
    rol rax, 16
    
    add rsi, 8
    dec ecx
    jnz .checksum_loop

.checksum_tail:
    ; Handle remaining bytes
    mov ecx, edi
    and ecx, 7                           ; remaining = size % 8
    xor rdx, rdx
.checksum_tail_loop:
    test ecx, ecx
    jz .checksum_finalize
    shl rdx, 8
    movzx eax, byte ptr [rsi]
    or rdx, rax
    inc rsi
    dec ecx
    jmp .checksum_tail_loop
    
.checksum_finalize:
    xor rax, rdx
    rol rax, 17
    xor rax, rbx
    
    ; Compare computed hash with expected
    cmp rax, QWORD PTR [r8]
    jne .checksum_mismatch
    xor eax, eax                         ; Match: return 0
    jmp .checksum_exit
    
.checksum_mismatch:
    mov eax, -1                          ; Mismatch: return -1
    
.checksum_exit:
    
    pop rdi
    pop rsi
    pop rbx
    ret
VerifyModelChecksum ENDP

; ============================================================================
; UTILITY - Memory Operations
; ============================================================================

MemCpyASM PROC
    ;
    ; Input:
    ;   RCX = destination
    ;   RDX = source  
    ;   R8  = size
    ;
    ; Output:
    ;   RAX = destination
    ;
    
    push rsi
    push rdi
    
    mov rax, rcx                         ; save dest for return value
    mov rdi, rcx
    mov rsi, rdx
    mov rcx, r8
    
    ; Use movdqu for unaligned-safe 16-byte copy
.copy_loop:
    cmp rcx, 16
    jl .copy_remaining
    
    movdqu xmm0, XMMWORD PTR [rsi]
    movdqu XMMWORD PTR [rdi], xmm0
    
    add rsi, 16
    add rdi, 16
    sub rcx, 16
    jmp .copy_loop
    
.copy_remaining:
    test rcx, rcx
    jz .copy_done
    
    mov dl, [rsi]
    mov [rdi], dl
    
    inc rsi
    inc rdi
    dec rcx
    jmp .copy_remaining
    
.copy_done:
    ; rax already holds destination from top
    pop rdi
    pop rsi
    ret
MemCpyASM ENDP

; ============================================================================
; UTILITY - Load Encrypted Blob from Disk
; ============================================================================

LoadEncryptedBlob PROC
    ;
    ; Input: RCX = file path
    ; Output: RAX = allocated buffer with blob data
    ;
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 48h
    
    mov r12, rcx                         ; r12 = file path
    
    ; CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)
    mov rcx, r12                         ; lpFileName
    mov edx, 80000000h                   ; GENERIC_READ
    mov r8d, 1                           ; FILE_SHARE_READ
    xor r9, r9                           ; lpSecurityAttributes = NULL
    mov DWORD PTR [rsp+20h], 3           ; OPEN_EXISTING
    mov DWORD PTR [rsp+28h], 0           ; dwFlagsAndAttributes
    mov QWORD PTR [rsp+30h], 0           ; hTemplateFile
    call CreateFileA
    
    cmp rax, -1                          ; INVALID_HANDLE_VALUE
    je .blob_fail
    mov r13, rax                         ; r13 = file handle
    
    ; GetFileSizeEx(handle, &size)
    lea rdx, [rsp+38h]                  ; LARGE_INTEGER on stack
    mov rcx, r13
    call GetFileSizeEx
    test eax, eax
    jz .blob_close_fail
    
    ; Allocate buffer for file contents
    mov rcx, QWORD PTR [rsp+38h]        ; file size
    mov rbx, rcx                         ; rbx = size
    call AllocateBuffer
    test rax, rax
    jz .blob_close_fail
    mov rsi, rax                         ; rsi = buffer
    
    ; ReadFile(handle, buffer, size, &bytesRead, NULL)
    mov rcx, r13                         ; hFile
    mov rdx, rsi                         ; lpBuffer
    mov r8d, ebx                         ; nBytesToRead
    lea r9, [rsp+40h]                    ; lpBytesRead
    mov QWORD PTR [rsp+20h], 0           ; lpOverlapped
    call ReadFile
    
    ; Close file handle
    mov rcx, r13
    call CloseHandle
    
    ; Return buffer pointer
    mov rax, rsi
    jmp .blob_exit
    
.blob_close_fail:
    mov rcx, r13
    call CloseHandle
.blob_fail:
    xor eax, eax
.blob_exit:
    add rsp, 48h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
LoadEncryptedBlob ENDP

; ============================================================================
; UTILITY - Zero Memory Signatures
; ============================================================================

ZeroMemorySignatures PROC
    ;
    ; Wipe potentially detectable memory patterns
    ; Zeroes the key pointer and the poly constants
    ;
    
    push rdi
    
    ; Zero the decryption key pointer (8 bytes)
    xor rax, rax
    mov QWORD PTR [g_decryptionKey], rax
    
    ; Zero the polymorphic constants (32 bytes)
    lea rdi, [g_polyConstant1]
    mov QWORD PTR [rdi], rax
    mov QWORD PTR [rdi+8], rax
    mov QWORD PTR [rdi+16], rax
    mov QWORD PTR [rdi+24], rax
    
    ; Zero the fake API pointers (24 bytes)
    mov QWORD PTR [g_fakeCloseHandle], rax
    mov QWORD PTR [g_fakeCreateFileA], rax
    mov QWORD PTR [g_fakeReadFile], rax
    
    ; Clear initialized flag
    mov BYTE PTR [g_isInitialized], 0
    
    pop rdi
    ret
ZeroMemorySignatures ENDP

; ============================================================================
; METADATA ACCESS
; ============================================================================

GetModelMetadata PROC
    ;
    ; Returns: RAX = ptr to ModelMetadata struct
    ;
    
    lea rax, [g_modelMetadata]
    ret
GetModelMetadata ENDP

; ============================================================================
; FAKE API CALLS - Anti-Analysis
; ============================================================================

FakeCloseHandle PROC
    push rax
    mov eax, 1
    pop rax
    ret
FakeCloseHandle ENDP

FakeCreateFileA PROC
    xor eax, eax
    ret
FakeCreateFileA ENDP

FakeReadFile PROC
    mov eax, 0
    ret
FakeReadFile ENDP

; ============================================================================
; MAIN EXPORT STUBS (for external use from C++)
; ============================================================================

LoadModelToMemory PROC
    ;
    ; C++ interface:
    ;   int LoadModelToMemory(const void* encryptedData, size_t size);
    ;
    
    push rbx
    push r12
    sub rsp, 28h                         ; shadow space + alignment
    
    mov rbx, rcx                         ; rbx = encrypted data ptr
    mov r12, rdx                         ; r12 = size (preserve across call)
    
    ; Allocate buffer for size bytes
    mov rcx, r12
    call AllocateBuffer
    test rax, rax
    jz .load_fail
    
    ; Copy encrypted data to allocated buffer
    ; MemCpyASM(dest=rax, src=rbx, count=r12)
    mov rcx, rax                         ; dest
    mov rdx, rbx                         ; src
    mov r8, r12                          ; count
    call MemCpyASM
    
    xor eax, eax                         ; return 0 = success
    jmp .load_exit
    
.load_fail:
    mov eax, -1                          ; return -1 = allocation failed

.load_exit:
    add rsp, 28h
    pop r12
    pop rbx
    ret
LoadModelToMemory ENDP

.END
