; RawrXD Model Loader — GGUF mapping with SRWLOCK-protected hot-swap
; Exports: LoadModel, GetTensor, UnloadModel, ModelLoaderInit,
;          HotSwapModel, GetCurrentModelPath, GetModelLoadTimestamp

EXTERN CreateFileW:PROC
EXTERN CreateFileMappingW:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN InitializeSRWLock:PROC
EXTERN AcquireSRWLockExclusive:PROC
EXTERN ReleaseSRWLockExclusive:PROC
EXTERN lstrcpyW:PROC
EXTERN GetTickCount64:PROC
EXTERN BeaconSend:PROC

EXTERN g_modelbase:QWORD
EXTERN ClearKVCache:PROC

PUBLIC LoadModel
PUBLIC GetTensor
PUBLIC UnloadModel
PUBLIC ModelLoaderInit
PUBLIC HotSwapModel
PUBLIC GetCurrentModelPath
PUBLIC GetModelLoadTimestamp

; Beacon message type (match beacon.asm)
MODEL_HOTSWAP_COMPLETE  equ 1002h

.data?
hFile         dq ?
hMapping      dq ?
pBase         dq ?
qTensorCount  dq ?

; SRWLOCK is a single pointer-sized value (8 bytes on x64)
align 8
g_modelLock   dq ?

; Current model path (260 WCHARs = 520 bytes)
g_currentPathW   db 520 dup(?)
; X+4 production: timestamp and KV-preserve flag for cache validation
g_loadTimestamp dq ?
g_preservedKV   db ?

.const
GENERIC_READ     equ 80000000h
FILE_SHARE_READ  equ 1
OPEN_EXISTING    equ 3
PAGE_READONLY    equ 2
FILE_MAP_READ    equ 4
GGUF_MAGIC       equ 046554747h

.code
; ────────────────────────────────────────────────────────────────
; ModelLoaderInit — initialize the SRWLOCK for thread-safe hot-swap
;   Called once during bootstrap (after InferenceEngineInit).
; ────────────────────────────────────────────────────────────────
ModelLoaderInit PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    lea     rcx, g_modelLock
    call    InitializeSRWLock

    add     rsp, 28h
    xor     eax, eax
    ret
ModelLoaderInit ENDP

; ────────────────────────────────────────────────────────────────
; LoadModel — memory-map a GGUF file
;   RCX = path (LPCWSTR)
;   Returns: EAX = 0 on success, -1 on failure
; ────────────────────────────────────────────────────────────────
LoadModel PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40h
    .allocstack 40h
    .endprolog
    mov     rbx, rcx
    mov     edx, GENERIC_READ
    mov     r8d, FILE_SHARE_READ
    xor     r9d, r9d
    mov     dword ptr [rsp+20h], OPEN_EXISTING
    xor     eax, eax
    mov     qword ptr [rsp+28h], rax
    mov     qword ptr [rsp+30h], rax
    call    CreateFileW
    mov     hFile, rax
    cmp     rax, -1
    je      @invalid
    ; CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL)
    mov     rcx, rax
    xor     edx, edx
    xor     r8d, r8d
    mov     r9d, PAGE_READONLY
    xor     eax, eax
    mov     qword ptr [rsp+20h], rax
    mov     qword ptr [rsp+28h], rax
    call    CreateFileMappingW
    mov     hMapping, rax
    test    rax, rax
    jz      @close_file
    mov     rcx, rax
    mov     edx, FILE_MAP_READ
    xor     r8d, r8d
    xor     r9d, r9d
    xor     eax, eax
    mov     qword ptr [rsp+20h], rax
    call    MapViewOfFile
    mov     pBase, rax
    test    rax, rax
    jz      @close_mapping
    mov     eax, dword ptr [rax]
    cmp     eax, GGUF_MAGIC
    jne     @unload
    mov     rax, pBase
    mov     rax, [rax+10h]
    mov     qTensorCount, rax
    add     rsp, 40h
    pop     rbx
    xor     eax, eax
    ret
@unload:
    call    UnloadModel
@close_mapping:
    mov     rcx, hMapping
    call    CloseHandle
@close_file:
    mov     rcx, hFile
    call    CloseHandle
@invalid:
    add     rsp, 40h
    pop     rbx
    mov     eax, -1
    ret
LoadModel ENDP

; ────────────────────────────────────────────────────────────────
; GetTensor — stub: return base + 100h
; ────────────────────────────────────────────────────────────────
GetTensor PROC
    ; RCX = name
    mov     rax, pBase
    add     rax, 100h
    ret
GetTensor ENDP

; ────────────────────────────────────────────────────────────────
; UnloadModel — unmap and close current model handles
; ────────────────────────────────────────────────────────────────
UnloadModel PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     rcx, pBase
    test    rcx, rcx
    jz      @skip_unmap
    call    UnmapViewOfFile
    xor     eax, eax
    mov     pBase, rax
@skip_unmap:
    mov     rcx, hMapping
    test    rcx, rcx
    jz      @skip_mapping
    call    CloseHandle
    xor     eax, eax
    mov     hMapping, rax
@skip_mapping:
    mov     rcx, hFile
    cmp     rcx, -1
    je      @skip_file
    test    rcx, rcx
    jz      @skip_file
    call    CloseHandle
    mov     hFile, -1
@skip_file:
    xor     eax, eax
    add     rsp, 28h
    ret
UnloadModel ENDP

; ────────────────────────────────────────────────────────────────
; HotSwapModel — atomically swap the loaded GGUF model
;   RCX = newPath (LPCWSTR), DL = preserveKV (0 = clear, 1 = keep)
;   Returns: EAX = 1 on success, 0 on failure
;   Takes exclusive SRWLOCK for the entire swap.
; ────────────────────────────────────────────────────────────────
HotSwapModel PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     rbx, rcx                    ; rbx = newPath
    movzx   esi, dl                     ; esi = preserveKV

    ; Acquire exclusive lock
    lea     rcx, g_modelLock
    call    AcquireSRWLockExclusive

    ; Unmap old model (clears pBase, hMapping, hFile)
    call    UnloadModel

    ; Zero the global model base pointer
    xor     eax, eax
    mov     g_modelbase, rax

    ; Load new model
    mov     rcx, rbx
    call    LoadModel
    test    eax, eax
    jnz     @swap_fail                  ; LoadModel returns 0 on success

    ; Copy new path into g_currentPathW
    lea     rcx, g_currentPathW
    mov     rdx, rbx
    call    lstrcpyW

    ; Set global model base to new mapping
    mov     rax, pBase
    mov     g_modelbase, rax

    ; Optionally clear KV cache; record preservedKV for cache validation
    mov     g_preservedKV, sil
    test    esi, esi
    jnz     @swap_ok                    ; preserveKV=1 → skip clear
    call    ClearKVCache

@swap_ok:
    ; Timestamp for cache validation
    call    GetTickCount64
    mov     g_loadTimestamp, rax

    ; Release lock
    lea     rcx, g_modelLock
    call    ReleaseSRWLockExclusive

    ; Signal completion via Beacon (slot 0 = UI thread)
    mov     ecx, 0
    mov     rdx, rbx                    ; pNewPath
    mov     r8d, MODEL_HOTSWAP_COMPLETE
    call    BeaconSend

    add     rsp, 28h
    pop     rsi
    pop     rbx
    mov     eax, 1                      ; success
    ret

@swap_fail:
    ; Release lock even on failure
    lea     rcx, g_modelLock
    call    ReleaseSRWLockExclusive

    add     rsp, 28h
    pop     rsi
    pop     rbx
    xor     eax, eax                    ; failure
    ret
HotSwapModel ENDP

; ────────────────────────────────────────────────────────────────
; GetCurrentModelPath — returns pointer to the current model path
;   Returns: RAX = pointer to g_currentPathW (WSTR, 520 bytes max)
; ────────────────────────────────────────────────────────────────
GetCurrentModelPath PROC
    lea     rax, g_currentPathW
    ret
GetCurrentModelPath ENDP

; GetModelLoadTimestamp — GetTickCount64 at last successful load (for cache validation)
GetModelLoadTimestamp PROC
    mov     rax, g_loadTimestamp
    ret
GetModelLoadTimestamp ENDP

END
