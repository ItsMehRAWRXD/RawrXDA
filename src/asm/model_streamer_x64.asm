; =============================================================================
; model_streamer_x64.asm — Zero-copy memory-mapped streamer helpers (x64 MASM)
; =============================================================================
;
; Focus: enable SeLockMemoryPrivilege for SEC_LARGE_PAGES / MEM_LARGE_PAGES paths.
; ABI:   Windows x64 (RCX, RDX, R8, R9 + 32B shadow space)
;
; Exported:
;   RawrXD_EnableSeLockMemoryPrivilege
;
; Return convention (RawrXD ASM standard):
;   RAX = 0 on success, STATUS_UNSUCCESSFUL on failure
;   RDX = 0 on success, GetLastError() code (QWORD) on failure
; =============================================================================

INCLUDE RawrXD_Common.inc

PUBLIC RawrXD_EnableSeLockMemoryPrivilege
PUBLIC RawrXD_MapModelView2MB
PUBLIC RawrXD_StreamToGPU_AVX512

.const
STATUS_UNSUCCESSFUL  EQU 0C0000001h
ERROR_SUCCESS        EQU 0
FILE_MAP_READ        EQU 4
ALIGN_64K_MASK       EQU 0FFFFFFFFFFFF0000h
ALIGN_2MB_MASK       EQU 0FFFFFFFFFFE00000h
TWO_MB_MASK          EQU 01FFFFFh

.data
align 8
szSeLockMemoryPrivilegeW  dw 'S','e','L','o','c','k','M','e','m','o','r','y','P','r','i','v','i','l','e','g','e',0

.code

; -----------------------------------------------------------------------------
; RawrXD_EnableSeLockMemoryPrivilege
;   Enables SeLockMemoryPrivilege for the current process token.
;
;   Returns:
;     RAX=0, RDX=0 on success
;     RAX=STATUS_UNSUCCESSFUL, RDX=GetLastError() on failure
; -----------------------------------------------------------------------------
RawrXD_EnableSeLockMemoryPrivilege PROC FRAME
    ; 32B shadow space + locals, keep 16B alignment before calls
    sub     rsp, 88h
    .allocstack 88h
    .endprolog

    ; Locals (within allocated stack):
    ; [rsp+40h] QWORD hToken
    ; [rsp+48h] LUID  luid (8 bytes)
    ; [rsp+50h] TOKEN_PRIVILEGES tp (16 bytes)

    ; hToken = 0
    xor     rax, rax
    mov     qword ptr [rsp+40h], rax

    ; OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &hToken)
    call    GetCurrentProcess
    mov     rcx, rax
    mov     edx, (TOKEN_ADJUST_PRIVILEGES or TOKEN_QUERY)
    lea     r8,  qword ptr [rsp+40h]
    call    OpenProcessToken
    test    eax, eax
    jz      near ptr @fail_last_error

    ; LookupPrivilegeValueW(NULL, L"SeLockMemoryPrivilege", &luid)
    xor     ecx, ecx
    lea     rdx, szSeLockMemoryPrivilegeW
    lea     r8,  qword ptr [rsp+48h]
    call    LookupPrivilegeValueW
    test    eax, eax
    jz      near ptr @fail_last_error_close_token

    ; TOKEN_PRIVILEGES tp = {1, {luid, SE_PRIVILEGE_ENABLED}}
    mov     dword ptr [rsp+50h], 1              ; PrivilegeCount
    mov     rax, qword ptr [rsp+48h]            ; Luid (8 bytes)
    mov     qword ptr [rsp+54h], rax            ; tp.Privileges.Luid (offset +4)
    mov     dword ptr [rsp+5Ch], SE_PRIVILEGE_ENABLED ; tp.Privileges.Attributes (offset +12)

    ; AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL)
    mov     rcx, qword ptr [rsp+40h]
    xor     edx, edx
    lea     r8,  qword ptr [rsp+50h]
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], 0              ; 5th arg: PreviousState = NULL
    mov     qword ptr [rsp+28h], 0              ; 6th arg: ReturnLength = NULL
    call    AdjustTokenPrivileges
    test    eax, eax
    jz      near ptr @fail_last_error_close_token

    ; AdjustTokenPrivileges can succeed but not assign the privilege -> check GetLastError()==ERROR_SUCCESS
    call    GetLastError
    test    eax, eax
    jnz     near ptr @fail_error_code_close_token  ; EAX has Win32 error

    ; CloseHandle(hToken)
    mov     rcx, qword ptr [rsp+40h]
    call    CloseHandle

    xor     eax, eax
    xor     edx, edx
    add     rsp, 88h
    ret

@fail_last_error_close_token:
    call    GetLastError
    ; fallthrough
@fail_error_code_close_token:
    ; CloseHandle(hToken) best-effort
    mov     rcx, qword ptr [rsp+40h]
    test    rcx, rcx
    jz      near ptr @fail_error_code
    call    CloseHandle

@fail_error_code:
    mov     edx, eax                            ; RDX = error
    mov     eax, STATUS_UNSUCCESSFUL
    add     rsp, 88h
    ret

@fail_last_error:
    call    GetLastError
    mov     edx, eax
    mov     eax, STATUS_UNSUCCESSFUL
    add     rsp, 88h
    ret
RawrXD_EnableSeLockMemoryPrivilege ENDP

; -----------------------------------------------------------------------------
; RawrXD_MapModelView2MB
;   Map a view for streaming with a "2MB mode" fast-path:
;     - If Offset and ViewSize are 2MB-aligned, map directly (large-page compatible).
;     - Otherwise, fall back to 64KB alignment mapping and return an adjusted pointer.
;
; Parameters:
;   RCX = hFileMapping
;   RDX = FileOffset (uint64)
;   R8  = ViewSize   (size_t)  (recommended: multiple of 2MB for best throughput)
;   R9  = outBaseOrError (uint64*)  ; on success: base pointer, on failure: GetLastError()
;
; Returns:
;   RAX = pointer to requested bytes (base + delta) on success, 0 on failure
; -----------------------------------------------------------------------------
RawrXD_MapModelView2MB PROC FRAME
    ; Preserve non-volatile registers we use (Win64 ABI).
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15

    sub     rsp, 38h                        ; 32B shadow + 8B for 5th arg (keeps 16B alignment)
    .allocstack 38h
    .endprolog

    ; Save handle and args
    mov     r10, rcx                        ; hMapping
    mov     r11, rdx                        ; offset
    mov     r12, r8                         ; viewSize
    mov     r13, r9                         ; outBaseOrError*

    ; Fast path requires 2MB-aligned offset and viewSize.
    mov     rax, r11
    and     rax, TWO_MB_MASK
    jnz     short @fallback_64k
    mov     rax, r12
    and     rax, TWO_MB_MASK
    jnz     short @fallback_64k

    ; MapViewOfFile(hMapping, FILE_MAP_READ, off_hi, off_lo, viewSize)
    mov     rcx, r10
    mov     edx, FILE_MAP_READ
    mov     rax, r11
    mov     r8, rax
    shr     r8, 32                          ; off_hi
    mov     r9d, eax                        ; off_lo
    mov     qword ptr [rsp+20h], r12         ; 5th arg: viewSize
    call    MapViewOfFile
    test    rax, rax
    jz      @fail_last_error

    ; Success, delta=0
    add     rsp, 38h
    mov     qword ptr [r13], rax            ; *outBaseOrError = base
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    ret

@fallback_64k:
    ; Align offset down to 64KB and expand map size by delta
    mov     rax, r11
    and     rax, ALIGN_64K_MASK             ; alignedOffset
    mov     r13, rax
    mov     r14, r11
    sub     r14, r13                        ; delta = offset - alignedOffset

    mov     r15, r12
    add     r15, r14                        ; mapSize = viewSize + delta

    mov     rcx, r10
    mov     edx, FILE_MAP_READ
    mov     rax, r13
    mov     r8, rax
    shr     r8, 32                          ; off_hi
    mov     r9d, eax                        ; off_lo
    mov     qword ptr [rsp+20h], r15         ; 5th arg: mapSize
    call    MapViewOfFile
    test    rax, rax
    jz      @fail_last_error

    ; Return adjusted pointer and base for unmap
    mov     qword ptr [r13], rax            ; *outBaseOrError = base
    add     rax, r14                        ; base + delta
    add     rsp, 38h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    ret

@fail_last_error:
    call    GetLastError
    mov     dword ptr [r13], eax            ; *outBaseOrError = error
    xor     eax, eax
    add     rsp, 38h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    ret
RawrXD_MapModelView2MB ENDP

; -----------------------------------------------------------------------------
; RawrXD_StreamToGPU_AVX512
;   Non-temporal 64B streaming copy (AVX-512).
;   Caller MUST gate with rawr_cpu_has_avx512() before calling on unknown CPUs.
;
; Parameters:
;   RCX = Destination pointer (GPU upload heap / WC-mapped / staging mapped)
;   RDX = Source pointer (mapped model view)
;   R8  = Count of 64-byte blocks
; -----------------------------------------------------------------------------
RawrXD_StreamToGPU_AVX512 PROC
    test    r8, r8
    jz      short @done

    ALIGN 16
@loop:
    vmovdqu64   zmm0, zmmword ptr [rdx]
    vmovntdq    zmmword ptr [rcx], zmm0
    add     rdx, 64
    add     rcx, 64
    dec     r8
    jnz     @loop

    sfence
@done:
    vzeroupper
    ret
RawrXD_StreamToGPU_AVX512 ENDP

END

