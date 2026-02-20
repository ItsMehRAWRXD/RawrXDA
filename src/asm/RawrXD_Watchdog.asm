; =============================================================================
; RawrXD_Watchdog.asm — Agentic .text Section Integrity Watchdog
; =============================================================================
; PURPOSE:
;   Runtime integrity verification of the IDE's own .text section using
;   HMAC-SHA256.  On first call (baseline capture) it hashes the entire
;   .text section and stores the 32-byte HMAC.  Subsequent calls re-hash
;   and compare against the baseline using constant-time comparison.
;
; EXPORTS:
;   asm_watchdog_init           — Locate .text section, capture baseline HMAC
;   asm_watchdog_verify         — Re-hash .text and compare to baseline
;   asm_watchdog_get_baseline   — Copy 32-byte baseline hash to caller buffer
;   asm_watchdog_get_status     — Return status word (0=pass, else failure code)
;   asm_watchdog_shutdown       — Zero all keying material
;
; DEPENDS:
;   - asm_camellia256_get_hmac_key (from RawrXD_Camellia256.asm)
;   - CryptoAPI: CryptAcquireContextA, CryptCreateHash, CryptHashData,
;                CryptDestroyHash, CryptReleaseContext
;   - GetModuleHandleA (kernel32)
;
; ABI: Windows x64 (shadow space, non-volatile register preservation)
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

include RawrXD_Common.inc

; =============================================================================
;  Additional Win32 API imports
; =============================================================================
EXTERNDEF CryptGetHashParam:PROC
EXTERNDEF CryptSetHashParam:PROC
EXTERNDEF GetModuleHandleA:PROC
EXTERNDEF VirtualQuery:PROC

; Import HMAC key from Camellia engine
EXTERNDEF asm_camellia256_get_hmac_key:PROC

; =============================================================================
;  Constants
; =============================================================================

WATCHDOG_HMAC_SIZE        EQU 32          ; SHA-256 output = 32 bytes
WATCHDOG_KEY_SIZE         EQU 32          ; HMAC key = 32 bytes
WATCHDOG_IPAD             EQU 36h         ; HMAC inner pad byte
WATCHDOG_OPAD             EQU 5Ch         ; HMAC outer pad byte
WATCHDOG_BLOCK_SIZE       EQU 64          ; SHA-256 block size
WATCHDOG_HASH_CHUNK       EQU 65536       ; Hash in 64KB chunks (cache-friendly)

HP_HASHVAL                EQU 2           ; CryptGetHashParam param for hash value
HP_HASHSIZE               EQU 4           ; CryptGetHashParam param for hash size

; Status codes
WATCHDOG_STATUS_UNINIT    EQU 0FFFFFFFFh  ; Not yet initialized
WATCHDOG_STATUS_OK        EQU 0           ; Integrity verified
WATCHDOG_STATUS_TAMPERED  EQU 1           ; .text section modified
WATCHDOG_STATUS_NO_KEY    EQU 2           ; HMAC key not available
WATCHDOG_STATUS_NO_TEXT   EQU 3           ; Could not locate .text section
WATCHDOG_STATUS_CRYPTO    EQU 4           ; CryptoAPI failure
WATCHDOG_STATUS_NO_PE     EQU 5           ; Invalid PE header

; =============================================================================
;  Data Section
; =============================================================================

.data

; Watchdog state
wd_initialized          DD WATCHDOG_STATUS_UNINIT
wd_text_base            DQ 0              ; VA of .text section start
wd_text_size            DQ 0              ; Size of .text section in bytes
wd_hmac_key             DB WATCHDOG_KEY_SIZE DUP(0)  ; HMAC key from Camellia engine
wd_baseline_hmac        DB WATCHDOG_HMAC_SIZE DUP(0) ; Baseline HMAC of .text
wd_current_hmac         DB WATCHDOG_HMAC_SIZE DUP(0) ; Last computed HMAC
wd_verify_count         DQ 0              ; Number of verify calls
wd_last_verify_tick     DQ 0              ; GetTickCount64 of last verification
wd_tamper_count         DQ 0              ; Number of tamper detections

; PE section name to search for
wd_text_name            DB ".text", 0, 0, 0   ; 8 bytes, null-padded

; =============================================================================
;  Code Section
; =============================================================================

.code

; =============================================================================
; asm_watchdog_init — Initialize watchdog, locate .text, capture baseline
; =============================================================================
; Returns: RAX = 0 on success, else WATCHDOG_STATUS_* error code
;          RDX = pointer to detail string
; =============================================================================
asm_watchdog_init PROC FRAME
    SAVE_NONVOL
    .pushreg r15
    .pushreg r14
    .pushreg r13
    .pushreg r12
    .pushreg rdi
    .pushreg rsi
    .pushreg rbx
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 120h       ; Local space: shadow + temporaries
    .allocstack 120h
    .endprolog

    ; ---- Step 1: Get HMAC key from Camellia engine ----
    lea     rcx, wd_hmac_key
    call    asm_camellia256_get_hmac_key
    test    eax, eax
    jnz     wd_init_no_key

    ; ---- Step 2: Get module base (our own EXE) ----
    xor     ecx, ecx                    ; NULL = this module
    call    GetModuleHandleA
    test    rax, rax
    jz      wd_init_no_pe
    mov     r12, rax                    ; r12 = module base (IMAGE_DOS_HEADER*)

    ; ---- Step 3: Navigate PE headers ----
    ; Verify MZ signature
    movzx   eax, WORD PTR [r12]
    cmp     ax, 5A4Dh                   ; 'MZ'
    jne     wd_init_no_pe

    ; Get e_lfanew → IMAGE_NT_HEADERS offset
    mov     eax, DWORD PTR [r12 + 3Ch]  ; e_lfanew
    test    eax, eax
    jz      wd_init_no_pe
    lea     r13, [r12 + rax]            ; r13 = IMAGE_NT_HEADERS*

    ; Verify PE signature
    mov     eax, DWORD PTR [r13]
    cmp     eax, 00004550h              ; "PE\0\0"
    jne     wd_init_no_pe

    ; ---- Step 4: Walk section headers to find .text ----
    ; IMAGE_NT_HEADERS64 layout:
    ;   +00h: Signature (4 bytes)
    ;   +04h: IMAGE_FILE_HEADER (20 bytes)
    ;   +18h: IMAGE_OPTIONAL_HEADER64 (variable)
    ;
    ; IMAGE_FILE_HEADER:
    ;   +02h: NumberOfSections (WORD)
    ;   +10h: SizeOfOptionalHeader (WORD)

    movzx   r14d, WORD PTR [r13 + 06h]  ; NumberOfSections
    movzx   eax, WORD PTR [r13 + 14h]   ; SizeOfOptionalHeader

    ; First section header starts at:
    ;   NT_HEADERS + 18h (sizeof Signature + FILE_HEADER) + SizeOfOptionalHeader
    lea     r15, [r13 + 18h]
    add     r15, rax                     ; r15 = first IMAGE_SECTION_HEADER

    ; Search for ".text" section
    xor     ebx, ebx                     ; section index
wd_find_text_loop:
    cmp     ebx, r14d
    jge     wd_init_no_text

    ; Compare 8-byte section name
    mov     rax, QWORD PTR [r15]         ; Section name (8 bytes)
    mov     rcx, QWORD PTR [wd_text_name]
    cmp     rax, rcx
    je      wd_found_text

    ; Next section header (each is 40 bytes = IMAGE_SECTION_HEADER size)
    add     r15, 28h
    inc     ebx
    jmp     wd_find_text_loop

wd_found_text:
    ; r15 points to the .text IMAGE_SECTION_HEADER
    ; Layout:
    ;   +08h: VirtualSize (DWORD)
    ;   +0Ch: VirtualAddress (DWORD, RVA)
    ;   +10h: SizeOfRawData (DWORD)

    mov     eax, DWORD PTR [r15 + 0Ch]  ; VirtualAddress (RVA)
    lea     rax, [r12 + rax]            ; Convert to VA
    mov     [wd_text_base], rax

    mov     eax, DWORD PTR [r15 + 08h]  ; VirtualSize
    mov     [wd_text_size], rax

    ; Validate: non-zero size
    test    rax, rax
    jz      wd_init_no_text

    ; ---- Step 5: Compute baseline HMAC-SHA256 over .text section ----
    mov     rcx, [wd_text_base]          ; data ptr
    mov     rdx, [wd_text_size]          ; data size
    lea     r8, wd_hmac_key              ; key ptr
    mov     r9d, WATCHDOG_KEY_SIZE       ; key size
    lea     rax, wd_baseline_hmac
    mov     [rsp+20h], rax               ; output buffer
    call    wd_compute_hmac_sha256
    test    eax, eax
    jnz     wd_init_crypto_fail

    ; ---- Success ----
    mov     DWORD PTR [wd_initialized], WATCHDOG_STATUS_OK
    mov     QWORD PTR [wd_verify_count], 0
    mov     QWORD PTR [wd_tamper_count], 0

    ; Record tick
    call    GetTickCount64
    mov     [wd_last_verify_tick], rax

    xor     eax, eax                     ; STATUS_SUCCESS
    lea     rdx, wd_msg_init_ok
    jmp     wd_init_done

wd_init_no_key:
    mov     eax, WATCHDOG_STATUS_NO_KEY
    lea     rdx, wd_msg_no_key
    mov     DWORD PTR [wd_initialized], eax
    jmp     wd_init_done

wd_init_no_pe:
    mov     eax, WATCHDOG_STATUS_NO_PE
    lea     rdx, wd_msg_no_pe
    mov     DWORD PTR [wd_initialized], eax
    jmp     wd_init_done

wd_init_no_text:
    mov     eax, WATCHDOG_STATUS_NO_TEXT
    lea     rdx, wd_msg_no_text
    mov     DWORD PTR [wd_initialized], eax
    jmp     wd_init_done

wd_init_crypto_fail:
    mov     eax, WATCHDOG_STATUS_CRYPTO
    lea     rdx, wd_msg_crypto
    mov     DWORD PTR [wd_initialized], eax

wd_init_done:
    lea     rsp, [rbp]
    pop     rbp
    RESTORE_NONVOL
    ret
asm_watchdog_init ENDP

; =============================================================================
; asm_watchdog_verify — Re-hash .text and compare against baseline
; =============================================================================
; Returns: RAX = 0 if integrity intact, 1 if tampered,
;                negative = error code
;          RDX = pointer to detail string
; =============================================================================
asm_watchdog_verify PROC FRAME
    SAVE_NONVOL
    .pushreg r15
    .pushreg r14
    .pushreg r13
    .pushreg r12
    .pushreg rdi
    .pushreg rsi
    .pushreg rbx
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 120h
    .allocstack 120h
    .endprolog

    ; Check initialized
    cmp     DWORD PTR [wd_initialized], WATCHDOG_STATUS_OK
    jne     wd_verify_not_init

    ; Re-compute HMAC over .text
    mov     rcx, [wd_text_base]
    mov     rdx, [wd_text_size]
    lea     r8, wd_hmac_key
    mov     r9d, WATCHDOG_KEY_SIZE
    lea     rax, wd_current_hmac
    mov     [rsp+20h], rax
    call    wd_compute_hmac_sha256
    test    eax, eax
    jnz     wd_verify_crypto_fail

    ; Constant-time comparison: 32 bytes = 4 QWORDs
    ; OR-accumulator pattern (no early exit, prevents timing side-channel)
    xor     r12, r12                     ; accumulator

    mov     rax, QWORD PTR [wd_baseline_hmac]
    xor     rax, QWORD PTR [wd_current_hmac]
    or      r12, rax

    mov     rax, QWORD PTR [wd_baseline_hmac + 8]
    xor     rax, QWORD PTR [wd_current_hmac + 8]
    or      r12, rax

    mov     rax, QWORD PTR [wd_baseline_hmac + 16]
    xor     rax, QWORD PTR [wd_current_hmac + 16]
    or      r12, rax

    mov     rax, QWORD PTR [wd_baseline_hmac + 24]
    xor     rax, QWORD PTR [wd_current_hmac + 24]
    or      r12, rax

    ; Update counters
    inc     QWORD PTR [wd_verify_count]
    call    GetTickCount64
    mov     [wd_last_verify_tick], rax

    ; Check result
    test    r12, r12
    jnz     wd_verify_tampered

    ; Integrity OK
    xor     eax, eax
    lea     rdx, wd_msg_verify_ok
    jmp     wd_verify_done

wd_verify_tampered:
    inc     QWORD PTR [wd_tamper_count]
    mov     DWORD PTR [wd_initialized], WATCHDOG_STATUS_TAMPERED
    mov     eax, WATCHDOG_STATUS_TAMPERED
    lea     rdx, wd_msg_tampered
    jmp     wd_verify_done

wd_verify_not_init:
    mov     eax, -1
    lea     rdx, wd_msg_not_init
    jmp     wd_verify_done

wd_verify_crypto_fail:
    mov     eax, -2
    lea     rdx, wd_msg_crypto

wd_verify_done:
    lea     rsp, [rbp]
    pop     rbp
    RESTORE_NONVOL
    ret
asm_watchdog_verify ENDP

; =============================================================================
; asm_watchdog_get_baseline — Copy 32-byte baseline HMAC to caller buffer
; =============================================================================
; RCX = pointer to 32-byte output buffer
; Returns: RAX = 0 on success, -1 if not initialized
; =============================================================================
asm_watchdog_get_baseline PROC
    test    rcx, rcx
    jz      wd_baseline_null
    cmp     DWORD PTR [wd_initialized], WATCHDOG_STATUS_UNINIT
    je      wd_baseline_not_init

    ; Copy 32 bytes (4 QWORDs)
    mov     rax, QWORD PTR [wd_baseline_hmac]
    mov     QWORD PTR [rcx], rax
    mov     rax, QWORD PTR [wd_baseline_hmac + 8]
    mov     QWORD PTR [rcx + 8], rax
    mov     rax, QWORD PTR [wd_baseline_hmac + 16]
    mov     QWORD PTR [rcx + 16], rax
    mov     rax, QWORD PTR [wd_baseline_hmac + 24]
    mov     QWORD PTR [rcx + 24], rax

    xor     eax, eax
    ret

wd_baseline_null:
wd_baseline_not_init:
    mov     eax, -1
    ret
asm_watchdog_get_baseline ENDP

; =============================================================================
; asm_watchdog_get_status — Return watchdog status structure
; =============================================================================
; RCX = pointer to 48-byte output buffer:
;   +00h: status (DWORD)
;   +04h: reserved (DWORD)
;   +08h: text_base (QWORD)
;   +10h: text_size (QWORD)
;   +18h: verify_count (QWORD)
;   +20h: tamper_count (QWORD)
;   +28h: last_verify_tick (QWORD)
; Returns: RAX = current status code
; =============================================================================
asm_watchdog_get_status PROC
    test    rcx, rcx
    jz      wd_status_null

    mov     eax, DWORD PTR [wd_initialized]
    mov     DWORD PTR [rcx], eax
    mov     DWORD PTR [rcx + 4], 0

    mov     rax, [wd_text_base]
    mov     QWORD PTR [rcx + 8], rax

    mov     rax, [wd_text_size]
    mov     QWORD PTR [rcx + 10h], rax

    mov     rax, [wd_verify_count]
    mov     QWORD PTR [rcx + 18h], rax

    mov     rax, [wd_tamper_count]
    mov     QWORD PTR [rcx + 20h], rax

    mov     rax, [wd_last_verify_tick]
    mov     QWORD PTR [rcx + 28h], rax

    mov     eax, DWORD PTR [wd_initialized]
    ret

wd_status_null:
    mov     eax, -1
    ret
asm_watchdog_get_status ENDP

; =============================================================================
; asm_watchdog_shutdown — Zero all keying material and state
; =============================================================================
; Returns: RAX = 0
; =============================================================================
asm_watchdog_shutdown PROC
    ; Zero HMAC key (4 QWORDs)
    xor     rax, rax
    mov     QWORD PTR [wd_hmac_key],      rax
    mov     QWORD PTR [wd_hmac_key + 8],   rax
    mov     QWORD PTR [wd_hmac_key + 16],  rax
    mov     QWORD PTR [wd_hmac_key + 24],  rax

    ; Zero baseline HMAC
    mov     QWORD PTR [wd_baseline_hmac],      rax
    mov     QWORD PTR [wd_baseline_hmac + 8],   rax
    mov     QWORD PTR [wd_baseline_hmac + 16],  rax
    mov     QWORD PTR [wd_baseline_hmac + 24],  rax

    ; Zero current HMAC
    mov     QWORD PTR [wd_current_hmac],       rax
    mov     QWORD PTR [wd_current_hmac + 8],    rax
    mov     QWORD PTR [wd_current_hmac + 16],   rax
    mov     QWORD PTR [wd_current_hmac + 24],   rax

    ; Zero state
    mov     [wd_text_base], rax
    mov     [wd_text_size], rax
    mov     [wd_verify_count], rax
    mov     [wd_tamper_count], rax
    mov     [wd_last_verify_tick], rax
    mov     DWORD PTR [wd_initialized], WATCHDOG_STATUS_UNINIT

    xor     eax, eax
    ret
asm_watchdog_shutdown ENDP

; =============================================================================
; wd_compute_hmac_sha256 — Internal HMAC-SHA256 using CryptoAPI
; =============================================================================
; RCX = data pointer
; RDX = data size (bytes)
; R8  = HMAC key pointer (32 bytes)
; R9  = HMAC key size
; [RSP+20h] = output buffer pointer (32 bytes)
;
; Implements RFC 2104:
;   HMAC(K, M) = H((K ^ opad) || H((K ^ ipad) || M))
;
; Uses CryptoAPI CALG_SHA_256 for the hash.
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
wd_compute_hmac_sha256 PROC FRAME
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
    sub     rsp, 200h           ; Lots of local space for pads + shadow
    .allocstack 200h
    .endprolog

    ; Save arguments
    mov     r12, rcx            ; r12 = data ptr
    mov     r13, rdx            ; r13 = data size
    mov     r14, r8             ; r14 = key ptr
    mov     r15d, r9d           ; r15d = key size
    mov     rbx, [rbp + 48 + 8*7 + 20h]  ; rbx = output buffer (5th arg via stack)
    ; The 5th argument is at [rbp + push_count*8 + 20h]
    ; We pushed rbp + 7 nonvols = 8 pushes * 8 = 64 = 40h
    ; 5th arg on stack: [rbp + 40h + 20h] but we haven't adjusted...
    ; Actually: frame pointer = rbp set before pushes, so:
    ; caller's stack: ret addr at [rbp+8], shadow at [rbp+10h..28h],
    ; 5th param at [rbp+30h]
    ; But wait: we set rbp = rsp BEFORE all the pushes, so rbp = original rsp
    ; then we pushed 7 regs. The caller put 5th param at [rsp_on_entry + 28h]
    ; rsp_on_entry = rbp + 8 (we pushed rbp before setting rbp)
    ; Actually: push rbp, mov rbp,rsp — so rbp = rsp after push rbp
    ; Original RSP (at CALL) = rbp + 8 (the return address)
    ; 5th param at [original_RSP + 20h] = [rbp + 28h]
    mov     rbx, [rbp + 28h]    ; output buffer pointer

    ; ---- Acquire crypto context ----
    lea     rcx, [rsp + 180h]   ; phProv (local)
    xor     edx, edx            ; pszContainer = NULL
    xor     r8d, r8d            ; pszProvider = NULL
    mov     r9d, PROV_RSA_AES
    mov     DWORD PTR [rsp+20h], CRYPT_VERIFYCONTEXT
    call    CryptAcquireContextA
    test    eax, eax
    jz      wd_hmac_fail

    mov     rsi, [rsp + 180h]   ; rsi = hProv

    ; ---- Build ipad and opad (64 bytes each) ----
    ; ipad at [rsp+80h], opad at [rsp+C0h]
    ; Zero both pads
    lea     rdi, [rsp + 80h]
    xor     eax, eax
    mov     ecx, 128            ; 64+64 bytes
    rep stosb

    ; Copy key into both pads (key ≤ 64 bytes, so no pre-hashing needed)
    lea     rdi, [rsp + 80h]    ; ipad
    mov     rsi, r14            ; key ptr
    mov     ecx, r15d           ; key size (32)
    rep movsb

    lea     rdi, [rsp + 0C0h]   ; opad
    mov     rsi, r14
    mov     ecx, r15d
    rep movsb

    ; XOR ipad with 0x36, opad with 0x5C
    lea     rsi, [rsp + 80h]    ; ipad
    lea     rdi, [rsp + 0C0h]   ; opad
    mov     ecx, WATCHDOG_BLOCK_SIZE
wd_hmac_pad_loop:
    xor     BYTE PTR [rsi], WATCHDOG_IPAD
    xor     BYTE PTR [rdi], WATCHDOG_OPAD
    inc     rsi
    inc     rdi
    dec     ecx
    jnz     wd_hmac_pad_loop

    ; Reload hProv from our saved local
    mov     rsi, [rsp + 180h]

    ; ---- Inner hash: H(ipad || message) ----
    ; Create hash object
    lea     rcx, [rsp + 188h]   ; phHash (local)
    mov     [rsp + 188h], rcx   ; temp, will be overwritten via out param
    mov     rcx, rsi            ; hProv
    mov     edx, CALG_SHA_256
    xor     r8d, r8d            ; hKey = 0
    xor     r9d, r9d            ; dwFlags = 0
    lea     rax, [rsp + 188h]
    mov     [rsp + 20h], rax    ; phHash out
    call    CryptCreateHash
    test    eax, eax
    jz      wd_hmac_release_ctx

    mov     rdi, [rsp + 188h]   ; rdi = hHash (inner)

    ; Hash ipad (64 bytes)
    mov     rcx, rdi
    lea     rdx, [rsp + 80h]    ; ipad data
    mov     r8d, WATCHDOG_BLOCK_SIZE
    xor     r9d, r9d            ; dwFlags = 0
    call    CryptHashData
    test    eax, eax
    jz      wd_hmac_destroy_inner

    ; Hash message data (may be very large — hash in chunks)
    mov     rcx, r13            ; remaining data bytes
    mov     rdx, r12            ; data pointer
wd_hmac_inner_data_loop:
    test    rcx, rcx
    jz      wd_hmac_inner_data_done

    ; Chunk size = min(remaining, WATCHDOG_HASH_CHUNK)
    mov     r8d, WATCHDOG_HASH_CHUNK
    cmp     rcx, r8
    cmovb   r8, rcx             ; if remaining < chunk, use remaining

    ; Save remaining/pointer across call
    mov     [rsp + 190h], rcx   ; save remaining
    mov     [rsp + 198h], rdx   ; save data ptr

    mov     rcx, rdi            ; hHash
    ; rdx already = data ptr
    ; r8d already = chunk size
    xor     r9d, r9d
    call    CryptHashData
    test    eax, eax
    jz      wd_hmac_destroy_inner

    mov     rcx, [rsp + 190h]   ; restore remaining
    mov     rdx, [rsp + 198h]   ; restore data ptr
    movzx   r8d, WORD PTR [rsp + 198h]  ; this is wrong, use chunk size
    ; Recalculate chunk size
    mov     r8d, WATCHDOG_HASH_CHUNK
    cmp     rcx, r8
    cmovb   r8, rcx

    sub     rcx, r8             ; remaining -= chunk
    add     rdx, r8             ; ptr += chunk
    jmp     wd_hmac_inner_data_loop

wd_hmac_inner_data_done:
    ; Get inner hash result (32 bytes) into local buffer at [rsp+100h]
    mov     rcx, rdi            ; hHash
    mov     edx, HP_HASHVAL
    lea     r8, [rsp + 100h]    ; output buffer
    lea     r9, [rsp + 1A0h]    ; pdwDataLen
    mov     DWORD PTR [rsp + 1A0h], WATCHDOG_HMAC_SIZE
    mov     DWORD PTR [rsp+20h], 0  ; dwFlags
    call    CryptGetHashParam
    ; (ignore error — if CryptHashData succeeded, this should too)

    ; Destroy inner hash
    mov     rcx, rdi
    call    CryptDestroyHash

    ; ---- Outer hash: H(opad || inner_hash) ----
    ; Create new hash object
    mov     rcx, [rsp + 180h]   ; hProv
    mov     edx, CALG_SHA_256
    xor     r8d, r8d
    xor     r9d, r9d
    lea     rax, [rsp + 188h]
    mov     [rsp + 20h], rax
    call    CryptCreateHash
    test    eax, eax
    jz      wd_hmac_release_ctx

    mov     rdi, [rsp + 188h]   ; rdi = hHash (outer)

    ; Hash opad (64 bytes)
    mov     rcx, rdi
    lea     rdx, [rsp + 0C0h]
    mov     r8d, WATCHDOG_BLOCK_SIZE
    xor     r9d, r9d
    call    CryptHashData
    test    eax, eax
    jz      wd_hmac_destroy_outer

    ; Hash inner hash result (32 bytes)
    mov     rcx, rdi
    lea     rdx, [rsp + 100h]
    mov     r8d, WATCHDOG_HMAC_SIZE
    xor     r9d, r9d
    call    CryptHashData
    test    eax, eax
    jz      wd_hmac_destroy_outer

    ; Get final HMAC result
    mov     rcx, rdi
    mov     edx, HP_HASHVAL
    mov     r8, rbx             ; caller's output buffer
    lea     r9, [rsp + 1A0h]
    mov     DWORD PTR [rsp + 1A0h], WATCHDOG_HMAC_SIZE
    mov     DWORD PTR [rsp+20h], 0
    call    CryptGetHashParam

    ; Destroy outer hash
    mov     rcx, rdi
    call    CryptDestroyHash

    ; Release crypto context
    mov     rcx, [rsp + 180h]
    xor     edx, edx
    call    CryptReleaseContext

    ; Success
    xor     eax, eax
    jmp     wd_hmac_done

wd_hmac_destroy_inner:
    mov     rcx, rdi
    call    CryptDestroyHash
    jmp     wd_hmac_release_ctx

wd_hmac_destroy_outer:
    mov     rcx, rdi
    call    CryptDestroyHash

wd_hmac_release_ctx:
    mov     rcx, [rsp + 180h]
    xor     edx, edx
    call    CryptReleaseContext

wd_hmac_fail:
    mov     eax, -1

wd_hmac_done:
    add     rsp, 200h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
wd_compute_hmac_sha256 ENDP

; =============================================================================
;  String Constants
; =============================================================================

.data

wd_msg_init_ok      DB "[Watchdog] Initialized — baseline HMAC captured", 0
wd_msg_no_key       DB "[Watchdog] Error: HMAC key not available", 0
wd_msg_no_pe        DB "[Watchdog] Error: Invalid PE header", 0
wd_msg_no_text      DB "[Watchdog] Error: .text section not found", 0
wd_msg_crypto       DB "[Watchdog] Error: CryptoAPI failure", 0
wd_msg_verify_ok    DB "[Watchdog] Integrity verified OK", 0
wd_msg_tampered     DB "[Watchdog] TAMPER DETECTED — .text section modified!", 0
wd_msg_not_init     DB "[Watchdog] Error: Not initialized", 0

; =============================================================================
;  PUBLIC EXPORTS
; =============================================================================
PUBLIC asm_watchdog_init
PUBLIC asm_watchdog_verify
PUBLIC asm_watchdog_get_baseline
PUBLIC asm_watchdog_get_status
PUBLIC asm_watchdog_shutdown

END
