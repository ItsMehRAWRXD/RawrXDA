; ═══════════════════════════════════════════════════════════════════
; exthost.asm — Extension Host: Sandboxed DLL Loader & Dispatcher
;
; Loads extension DLLs dynamically via LoadLibraryW, resolves their
; exported entry points, and dispatches messages between the host
; and loaded extensions. Each extension exports:
;   - RawrXD_ExtInit(version:DWORD) → DWORD (0=success)
;   - RawrXD_ExtShutdown() → void
;   - RawrXD_ExtOnMessage(msgId:DWORD, pData:PTR, dataLen:DWORD) → DWORD
;
; Extension table: up to MAX_EXTENSIONS entries, each 80 bytes:
;   +0:  hModule     (QWORD)  — LoadLibrary handle
;   +8:  pfnInit     (QWORD)  — RawrXD_ExtInit function pointer
;   +16: pfnShutdown (QWORD)  — RawrXD_ExtShutdown function pointer
;   +24: pfnMessage  (QWORD)  — RawrXD_ExtOnMessage function pointer
;   +32: flags       (DWORD)  — 1=loaded, 2=initialized, 4=error
;   +36: reserved    (DWORD)
;   +40: szName      (40 bytes WCHAR) — DLL filename for diagnostics
; ═══════════════════════════════════════════════════════════════════

PUBLIC ExtHostInit
PUBLIC ExtHostLoad
PUBLIC ExtHostUnload
PUBLIC ExtHostSendMessage
PUBLIC ExtHostGetCount
PUBLIC ExtHostShutdown

; ── Win32 imports ────────────────────────────────────────────────
EXTERN LoadLibraryW:PROC
EXTERN FreeLibrary:PROC
EXTERN GetProcAddress:PROC
EXTERN g_hHeap:QWORD
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC

; ── Beacon ───────────────────────────────────────────────────────
EXTERN BeaconSend:PROC

; ── Constants ────────────────────────────────────────────────────
MAX_EXTENSIONS      equ 16
EXT_ENTRY_SIZE      equ 80
EXT_HOST_VERSION    equ 1           ; Protocol version passed to ExtInit

; Entry offsets
EXT_OFF_HMODULE     equ 0
EXT_OFF_INIT        equ 8
EXT_OFF_SHUTDOWN    equ 16
EXT_OFF_MESSAGE     equ 24
EXT_OFF_FLAGS       equ 32
EXT_OFF_NAME        equ 40

; Flag bits
EXT_FLAG_LOADED     equ 1
EXT_FLAG_INITED     equ 2
EXT_FLAG_ERROR      equ 4

; Beacon
EXT_BEACON_SLOT     equ 16
EXT_EVT_LOAD        equ 0E1h
EXT_EVT_UNLOAD      equ 0E2h
EXT_EVT_MSG         equ 0E3h
EXT_EVT_ERROR       equ 0E4h

; ── Export name strings (ASCII for GetProcAddress) ───────────────
.const
szExtInit       db "RawrXD_ExtInit",0
szExtShutdown   db "RawrXD_ExtShutdown",0
szExtOnMessage  db "RawrXD_ExtOnMessage",0

; ── Data ─────────────────────────────────────────────────────────
.data
align 8
g_extCount      dd 0
g_extReady      dd 0

.data?
align 16
g_extTable      db (MAX_EXTENSIONS * EXT_ENTRY_SIZE) dup(?)

.code

; ════════════════════════════════════════════════════════════════════
; ExtHostInit — Initialize extension host subsystem
;   No args. Returns EAX = 0 success.
; ════════════════════════════════════════════════════════════════════
ExtHostInit PROC FRAME
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; Zero the extension table
    lea     rdi, g_extTable
    xor     eax, eax
    mov     ecx, (MAX_EXTENSIONS * EXT_ENTRY_SIZE) / 8
    rep     stosq

    mov     g_extCount, 0
    mov     g_extReady, 1

    xor     eax, eax
    add     rsp, 20h
    pop     rdi
    ret
ExtHostInit ENDP


; ════════════════════════════════════════════════════════════════════
; ExtHostLoad — Load an extension DLL
;   RCX = pDllPath (WCHAR* null-terminated path to extension DLL)
;   Returns: EAX = extension index (0..MAX-1), or -1 on failure
;
;   FRAME: 3 pushes (rbx,rsi,r12) + 30h alloc
; ════════════════════════════════════════════════════════════════════
ExtHostLoad PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     rsi, rcx                     ; save DLL path

    ; Find free slot
    lea     r12, g_extTable
    xor     ebx, ebx
@@ehl_scan:
    cmp     ebx, MAX_EXTENSIONS
    jge     @ehl_full
    mov     eax, dword ptr [r12 + EXT_OFF_FLAGS]
    test    eax, EXT_FLAG_LOADED
    jz      @@ehl_found
    add     r12, EXT_ENTRY_SIZE
    inc     ebx
    jmp     @@ehl_scan

@@ehl_found:
    ; LoadLibraryW(pDllPath)
    mov     rcx, rsi
    call    LoadLibraryW
    test    rax, rax
    jz      @ehl_fail
    mov     qword ptr [r12 + EXT_OFF_HMODULE], rax
    mov     rsi, rax                     ; rsi = hModule

    ; Resolve RawrXD_ExtInit
    mov     rcx, rsi
    lea     rdx, szExtInit
    call    GetProcAddress
    mov     qword ptr [r12 + EXT_OFF_INIT], rax
    ; Init is optional — extensions without it just load

    ; Resolve RawrXD_ExtShutdown
    mov     rcx, rsi
    lea     rdx, szExtShutdown
    call    GetProcAddress
    mov     qword ptr [r12 + EXT_OFF_SHUTDOWN], rax

    ; Resolve RawrXD_ExtOnMessage
    mov     rcx, rsi
    lea     rdx, szExtOnMessage
    call    GetProcAddress
    mov     qword ptr [r12 + EXT_OFF_MESSAGE], rax

    ; Mark loaded
    mov     dword ptr [r12 + EXT_OFF_FLAGS], EXT_FLAG_LOADED

    ; Call ExtInit if available
    mov     rax, qword ptr [r12 + EXT_OFF_INIT]
    test    rax, rax
    jz      @ehl_no_init

    mov     ecx, EXT_HOST_VERSION
    call    rax
    test    eax, eax
    jnz     @ehl_init_fail
    or      dword ptr [r12 + EXT_OFF_FLAGS], EXT_FLAG_INITED

@ehl_no_init:
    inc     g_extCount

    ; Beacon: extension loaded
    mov     ecx, EXT_BEACON_SLOT
    mov     edx, EXT_EVT_LOAD
    mov     r8d, ebx
    call    BeaconSend

    mov     eax, ebx                     ; return index
    jmp     @ehl_ret

@ehl_init_fail:
    ; Init failed — unload the DLL
    or      dword ptr [r12 + EXT_OFF_FLAGS], EXT_FLAG_ERROR
    mov     rcx, qword ptr [r12 + EXT_OFF_HMODULE]
    call    FreeLibrary
    mov     qword ptr [r12 + EXT_OFF_HMODULE], 0
    mov     dword ptr [r12 + EXT_OFF_FLAGS], 0

@ehl_fail:
@ehl_full:
    mov     eax, -1

@ehl_ret:
    add     rsp, 30h
    pop     r12
    pop     rsi
    pop     rbx
    ret
ExtHostLoad ENDP


; ════════════════════════════════════════════════════════════════════
; ExtHostUnload — Unload an extension by index
;   ECX = extension index
;   Returns: EAX = 0 success, -1 failure
; ════════════════════════════════════════════════════════════════════
ExtHostUnload PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Validate index
    cmp     ecx, MAX_EXTENSIONS
    jge     @ehu_fail
    mov     ebx, ecx

    ; Get entry
    imul    eax, ecx, EXT_ENTRY_SIZE
    cdqe
    lea     r12, g_extTable
    add     r12, rax

    ; Check if loaded
    mov     eax, dword ptr [r12 + EXT_OFF_FLAGS]
    test    eax, EXT_FLAG_LOADED
    jz      @ehu_fail

    ; Call ExtShutdown if available
    mov     rax, qword ptr [r12 + EXT_OFF_SHUTDOWN]
    test    rax, rax
    jz      @ehu_no_shutdown
    call    rax
@ehu_no_shutdown:

    ; FreeLibrary
    mov     rcx, qword ptr [r12 + EXT_OFF_HMODULE]
    test    rcx, rcx
    jz      @ehu_clear
    call    FreeLibrary
@ehu_clear:

    ; Zero the entry
    mov     qword ptr [r12 + EXT_OFF_HMODULE], 0
    mov     qword ptr [r12 + EXT_OFF_INIT], 0
    mov     qword ptr [r12 + EXT_OFF_SHUTDOWN], 0
    mov     qword ptr [r12 + EXT_OFF_MESSAGE], 0
    mov     dword ptr [r12 + EXT_OFF_FLAGS], 0

    dec     g_extCount

    ; Beacon: extension unloaded
    mov     ecx, EXT_BEACON_SLOT
    mov     edx, EXT_EVT_UNLOAD
    mov     r8d, ebx
    call    BeaconSend

    xor     eax, eax
    jmp     @ehu_ret

@ehu_fail:
    mov     eax, -1

@ehu_ret:
    add     rsp, 28h
    pop     r12
    pop     rbx
    ret
ExtHostUnload ENDP


; ════════════════════════════════════════════════════════════════════
; ExtHostSendMessage — Dispatch message to an extension
;   ECX = extension index
;   EDX = messageId
;   R8  = pData (pointer to message payload)
;   R9D = dataLen (byte length of payload)
;   Returns: EAX = extension's return value, or -1 on failure
; ════════════════════════════════════════════════════════════════════
ExtHostSendMessage PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; Validate index
    cmp     ecx, MAX_EXTENSIONS
    jge     @ehm_fail
    mov     ebx, ecx                     ; save index

    ; Get entry
    imul    eax, ecx, EXT_ENTRY_SIZE
    cdqe
    lea     rcx, g_extTable
    add     rcx, rax

    ; Check loaded + initialized
    mov     eax, dword ptr [rcx + EXT_OFF_FLAGS]
    test    eax, EXT_FLAG_LOADED
    jz      @ehm_fail

    ; Get message handler
    mov     rax, qword ptr [rcx + EXT_OFF_MESSAGE]
    test    rax, rax
    jz      @ehm_fail

    ; Call: RawrXD_ExtOnMessage(msgId, pData, dataLen)
    mov     ecx, edx                     ; msgId
    mov     rdx, r8                      ; pData
    mov     r8d, r9d                     ; dataLen
    call    rax
    jmp     @ehm_ret

@ehm_fail:
    mov     eax, -1

@ehm_ret:
    add     rsp, 20h
    pop     rbx
    ret
ExtHostSendMessage ENDP


; ════════════════════════════════════════════════════════════════════
; ExtHostGetCount — Return number of loaded extensions
;   Returns: EAX = count
; ════════════════════════════════════════════════════════════════════
ExtHostGetCount PROC
    mov     eax, g_extCount
    ret
ExtHostGetCount ENDP


; ════════════════════════════════════════════════════════════════════
; ExtHostShutdown — Unload all extensions and clean up
;   Returns: EAX = 0
; ════════════════════════════════════════════════════════════════════
ExtHostShutdown PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    lea     r12, g_extTable
    xor     ebx, ebx

@@ehs_loop:
    cmp     ebx, MAX_EXTENSIONS
    jge     @@ehs_done

    mov     eax, dword ptr [r12 + EXT_OFF_FLAGS]
    test    eax, EXT_FLAG_LOADED
    jz      @@ehs_next

    ; Call shutdown if available
    mov     rax, qword ptr [r12 + EXT_OFF_SHUTDOWN]
    test    rax, rax
    jz      @@ehs_free
    call    rax

@@ehs_free:
    ; FreeLibrary
    mov     rcx, qword ptr [r12 + EXT_OFF_HMODULE]
    test    rcx, rcx
    jz      @@ehs_clear
    call    FreeLibrary

@@ehs_clear:
    mov     qword ptr [r12 + EXT_OFF_HMODULE], 0
    mov     dword ptr [r12 + EXT_OFF_FLAGS], 0

@@ehs_next:
    add     r12, EXT_ENTRY_SIZE
    inc     ebx
    jmp     @@ehs_loop

@@ehs_done:
    mov     g_extCount, 0
    mov     g_extReady, 0

    xor     eax, eax
    add     rsp, 28h
    pop     r12
    pop     rbx
    ret
ExtHostShutdown ENDP

END
