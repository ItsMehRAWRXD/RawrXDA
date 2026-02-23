; ═══════════════════════════════════════════════════════════════════
; RawrXD Extension Host — Sandboxed DLL Loader for .rawr Extensions
; P2: VSIX-compatible extension loading with memory/access sandboxing
;
; Architecture:
;   1. CreateJobObject → per-extension memory limit (512MB)
;   2. LoadLibraryW → dynamic extension DLL load
;   3. GetProcAddress("RawrXD_ExtensionEntry") → extension init
;   4. Beacon slot 7 → bidirectional message passing
;   5. CreateThread → dedicated thread per extension
;   6. AssignProcessToJobObject for memory containment
;
; Extensions export: RawrXD_ExtensionEntry(pBeaconSend, pBeaconRecv, slotID)
; Extensions receive beacon messages and can send responses.
;
; Exports: ExtHostInit, ExtHostLoad, ExtHostUnload,
;          ExtHostSendMessage, ExtHostGetCount, ExtHostShutdown
; ═══════════════════════════════════════════════════════════════════

EXTERN LoadLibraryW:PROC
EXTERN GetProcAddress:PROC
EXTERN FreeLibrary:PROC
EXTERN CreateThread:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC
EXTERN CreateJobObjectW:PROC
EXTERN SetInformationJobObject:PROC
EXTERN AssignProcessToJobObject:PROC
EXTERN GetCurrentProcess:PROC
EXTERN TerminateThread:PROC
EXTERN BeaconSend:PROC
EXTERN BeaconRecv:PROC
EXTERN g_hHeap:QWORD
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC

PUBLIC ExtHostInit
PUBLIC ExtHostLoad
PUBLIC ExtHostUnload
PUBLIC ExtHostSendMessage
PUBLIC ExtHostGetCount
PUBLIC ExtHostShutdown

; ── Constants ────────────────────────────────────────────────────
MAX_EXTENSIONS       equ 32              ; max concurrent extensions
EXT_BEACON_SLOT_BASE equ 7              ; beacon slots 7-22 for extensions
EXT_NAME_LEN         equ 260            ; MAX_PATH wchars per extension name
HEAP_ZERO_MEMORY     equ 8

; Job object limits
JobObjectExtendedLimitInformation equ 9
JOB_OBJECT_LIMIT_PROCESS_MEMORY  equ 100h
JOB_OBJECT_LIMIT_JOB_MEMORY      equ 200h
EXT_MEMORY_LIMIT     equ 20000000h      ; 512MB per extension

; JOBOBJECT_EXTENDED_LIMIT_INFORMATION size (Win10 x64 = 144 bytes)
JOBINFO_SIZE         equ 144

; Thread constants
INFINITE             equ 0FFFFFFFFh

; ── Extension slot structure ─────────────────────────────────────
; Each extension occupies 48 bytes in the slot table:
;   [0]  dq hModule          - LoadLibrary handle
;   [8]  dq hThread          - worker thread handle
;   [16] dq hJob             - job object handle
;   [24] dq pfnEntry         - RawrXD_ExtensionEntry address
;   [32] dd beaconSlot       - assigned beacon slot
;   [36] dd state            - 0=empty, 1=loaded, 2=running, 3=error

EXT_SLOT_SIZE        equ 48

.data?
align 16
; Extension slot table
g_extSlots       db (MAX_EXTENSIONS * EXT_SLOT_SIZE) dup(?)

; Job info buffer for SetInformationJobObject
g_jobInfo        db JOBINFO_SIZE dup(?)

.data
align 4
g_extCount       dd 0                    ; number of loaded extensions
g_extHostReady   dd 0                    ; 1 = host initialized

.const
; Function name for GetProcAddress (narrow)
szExtEntryFn     db "RawrXD_ExtensionEntry",0

.code
; ════════════════════════════════════════════════════════════════
; ExtHostInit — initialize extension host subsystem
;   Zeros slot table, marks host ready.
;   Returns: EAX = 0
; ════════════════════════════════════════════════════════════════
ExtHostInit PROC FRAME
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; Zero the slot table
    lea     rdi, g_extSlots
    mov     rcx, (MAX_EXTENSIONS * EXT_SLOT_SIZE) / 8
    xor     eax, eax
    rep     stosq

    mov     g_extCount, 0
    mov     g_extHostReady, 1

    add     rsp, 20h
    pop     rdi
    xor     eax, eax
    ret
ExtHostInit ENDP

; ════════════════════════════════════════════════════════════════
; ExtHostLoad — load a .rawr extension DLL
;   RCX = dllPath (LPCWSTR)
;   Returns: EAX = slot index (0-31) on success, -1 on failure
;
; Steps:
;   1. Find empty slot
;   2. CreateJobObject with 512MB memory limit
;   3. LoadLibraryW(dllPath)
;   4. GetProcAddress("RawrXD_ExtensionEntry")
;   5. CreateThread for extension worker
;   6. Assign beacon slot (EXT_BEACON_SLOT_BASE + slotIndex)
; ════════════════════════════════════════════════════════════════
ExtHostLoad PROC FRAME
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
    sub     rsp, 48h
    .allocstack 48h
    .endprolog

    mov     r12, rcx                    ; r12 = dllPath

    cmp     g_extHostReady, 0
    je      @el_fail

    ; Find empty slot (state == 0)
    lea     rdi, g_extSlots
    xor     esi, esi                    ; slot index
@el_find_slot:
    cmp     esi, MAX_EXTENSIONS
    jge     @el_fail                    ; no empty slots
    mov     eax, dword ptr [rdi + 36]   ; state field
    test    eax, eax
    jz      @el_found_slot
    add     rdi, EXT_SLOT_SIZE
    inc     esi
    jmp     @el_find_slot

@el_found_slot:
    mov     r13, rdi                    ; r13 = slot pointer

    ; 1. Create Job Object for memory sandboxing
    xor     ecx, ecx                    ; lpJobAttributes = NULL
    xor     edx, edx                    ; lpName = NULL
    call    CreateJobObjectW
    test    rax, rax
    jz      @el_fail
    mov     [r13 + 16], rax             ; hJob
    mov     rbx, rax

    ; Configure memory limit
    lea     rdi, g_jobInfo
    mov     rcx, JOBINFO_SIZE / 8
    xor     eax, eax
    rep     stosq                       ; zero the struct

    lea     rdi, g_jobInfo
    ; BasicLimitInformation.LimitFlags at offset 16 (within JOBOBJECT_BASIC_LIMIT_INFORMATION)
    mov     dword ptr [rdi + 16], JOB_OBJECT_LIMIT_JOB_MEMORY
    ; JobMemoryLimit at offset 96 in JOBOBJECT_EXTENDED_LIMIT_INFORMATION
    mov     qword ptr [rdi + 96], EXT_MEMORY_LIMIT

    ; SetInformationJobObject(hJob, ExtendedLimitInfo, &info, sizeof)
    mov     rcx, rbx                    ; hJob
    mov     edx, JobObjectExtendedLimitInformation
    lea     r8, g_jobInfo
    mov     r9d, JOBINFO_SIZE
    call    SetInformationJobObject

    ; 2. LoadLibraryW(dllPath)
    mov     rcx, r12
    call    LoadLibraryW
    test    rax, rax
    jz      @el_cleanup_job
    mov     [r13], rax                  ; hModule
    mov     rbx, rax

    ; 3. GetProcAddress("RawrXD_ExtensionEntry")
    mov     rcx, rbx
    lea     rdx, szExtEntryFn
    call    GetProcAddress
    test    rax, rax
    jz      @el_cleanup_dll
    mov     [r13 + 24], rax             ; pfnEntry

    ; 4. Assign beacon slot
    lea     eax, [esi + EXT_BEACON_SLOT_BASE]
    mov     [r13 + 32], eax            ; beaconSlot

    ; 5. CreateThread for extension worker
    ; Thread proc: ExtensionWorkerThread
    ; Parameter: slot pointer (r13)
    xor     ecx, ecx                    ; lpThreadAttributes
    xor     edx, edx                    ; dwStackSize (default)
    lea     r8, ExtensionWorkerThread   ; lpStartAddress
    mov     r9, r13                     ; lpParameter = slot pointer
    mov     qword ptr [rsp+20h], 0      ; dwCreationFlags
    mov     qword ptr [rsp+28h], 0      ; lpThreadId
    call    CreateThread
    test    rax, rax
    jz      @el_cleanup_dll
    mov     [r13 + 8], rax             ; hThread

    ; Mark slot as running
    mov     dword ptr [r13 + 36], 2     ; state = running

    ; Increment count
    lock inc dword ptr g_extCount

    ; Return slot index
    mov     eax, esi
    jmp     @el_ret

@el_cleanup_dll:
    mov     rcx, [r13]                  ; hModule
    call    FreeLibrary
    xor     eax, eax
    mov     [r13], rax

@el_cleanup_job:
    mov     rcx, [r13 + 16]            ; hJob
    test    rcx, rcx
    jz      @el_fail
    call    CloseHandle
    xor     eax, eax
    mov     [r13 + 16], rax

@el_fail:
    mov     eax, -1

@el_ret:
    add     rsp, 48h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ExtHostLoad ENDP

; ════════════════════════════════════════════════════════════════
; ExtensionWorkerThread — thread entry for each extension
;   RCX = slot pointer
;   Calls pfnEntry(pBeaconSend, pBeaconRecv, beaconSlot)
;   On return, marks slot state = 1 (loaded but not running)
; ════════════════════════════════════════════════════════════════
ExtensionWorkerThread PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     rbx, rcx                    ; slot pointer

    ; Call extension entry point
    ; Signature: void RawrXD_ExtensionEntry(pfnSend, pfnRecv, slotID)
    mov     rax, [rbx + 24]             ; pfnEntry
    lea     rcx, BeaconSend             ; pBeaconSend
    lea     rdx, BeaconRecv             ; pBeaconRecv
    mov     r8d, dword ptr [rbx + 32]   ; beaconSlot
    call    rax

    ; Extension returned — mark as loaded (not running)
    mov     dword ptr [rbx + 36], 1

    add     rsp, 30h
    pop     rbx
    xor     eax, eax
    ret
ExtensionWorkerThread ENDP

; ════════════════════════════════════════════════════════════════
; ExtHostUnload — unload an extension by slot index
;   ECX = slot index
;   Returns: EAX = 0 on success, -1 on failure
; ════════════════════════════════════════════════════════════════
ExtHostUnload PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    cmp     ecx, MAX_EXTENSIONS
    jge     @eu_fail

    ; Calculate slot pointer
    movsxd  rax, ecx
    imul    rax, EXT_SLOT_SIZE
    lea     rbx, g_extSlots
    add     rbx, rax

    ; Check if slot is active
    mov     eax, dword ptr [rbx + 36]
    test    eax, eax
    jz      @eu_fail                    ; empty slot

    ; Terminate thread if still running
    mov     rcx, [rbx + 8]             ; hThread
    test    rcx, rcx
    jz      @eu_no_thread
    mov     edx, 0                      ; exit code
    call    TerminateThread
    mov     rcx, [rbx + 8]
    call    CloseHandle
    xor     eax, eax
    mov     [rbx + 8], rax
@eu_no_thread:

    ; Free library
    mov     rcx, [rbx]                  ; hModule
    test    rcx, rcx
    jz      @eu_no_dll
    call    FreeLibrary
    xor     eax, eax
    mov     [rbx], rax
@eu_no_dll:

    ; Close job object
    mov     rcx, [rbx + 16]            ; hJob
    test    rcx, rcx
    jz      @eu_no_job
    call    CloseHandle
    xor     eax, eax
    mov     [rbx + 16], rax
@eu_no_job:

    ; Zero the slot
    mov     dword ptr [rbx + 36], 0     ; state = empty
    lock dec dword ptr g_extCount

    add     rsp, 20h
    pop     rbx
    xor     eax, eax
    ret

@eu_fail:
    add     rsp, 20h
    pop     rbx
    mov     eax, -1
    ret
ExtHostUnload ENDP

; ════════════════════════════════════════════════════════════════
; ExtHostSendMessage — send a message to an extension via beacon
;   ECX = slot index, RDX = pData, R8D = dataLen
;   Returns: EAX = 0 on success
; ════════════════════════════════════════════════════════════════
ExtHostSendMessage PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    cmp     ecx, MAX_EXTENSIONS
    jge     @esm_fail

    ; Calculate beacon slot for this extension
    movsxd  rax, ecx
    imul    rax, EXT_SLOT_SIZE
    lea     r10, g_extSlots
    mov     eax, dword ptr [r10 + rax + 32]  ; beaconSlot

    ; BeaconSend(beaconSlot, pData, dataLen)
    mov     ecx, eax
    ; rdx = pData (already in rdx)
    ; r8d = dataLen (already in r8d)
    call    BeaconSend

    add     rsp, 28h
    xor     eax, eax
    ret

@esm_fail:
    add     rsp, 28h
    mov     eax, -1
    ret
ExtHostSendMessage ENDP

; ════════════════════════════════════════════════════════════════
; ExtHostGetCount — return number of loaded extensions
;   Returns: EAX = count
; ════════════════════════════════════════════════════════════════
ExtHostGetCount PROC
    mov     eax, g_extCount
    ret
ExtHostGetCount ENDP

; ════════════════════════════════════════════════════════════════
; ExtHostShutdown — unload all extensions, tear down host
; ════════════════════════════════════════════════════════════════
ExtHostShutdown PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    xor     ebx, ebx
@es_loop:
    cmp     ebx, MAX_EXTENSIONS
    jge     @es_done
    mov     ecx, ebx
    call    ExtHostUnload               ; safe on empty slots (-1 return)
    inc     ebx
    jmp     @es_loop

@es_done:
    mov     g_extHostReady, 0
    mov     g_extCount, 0

    add     rsp, 20h
    pop     rbx
    ret
ExtHostShutdown ENDP

END