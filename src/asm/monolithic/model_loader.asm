; RawrXD Model Loader — GGUF mapping with SRWLOCK-protected hot-swap
; Exports: LoadModel, GetTensor, UnloadModel, ModelLoaderInit,
;          HotSwapModel, GetCurrentModelPath, GetModelLoadTimestamp,
;          ValidateModelCompat

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

<<<<<<< HEAD
; Phase 5 Tier 2: SlotRing integration for hot-swap
EXTERN SlotRing_EvictAll:PROC
EXTERN SlotRing_Init:PROC

=======
>>>>>>> origin/main
PUBLIC LoadModel
PUBLIC GetTensor
PUBLIC UnloadModel
PUBLIC ModelLoaderInit
PUBLIC HotSwapModel
PUBLIC GetCurrentModelPath
PUBLIC GetModelLoadTimestamp
PUBLIC ValidateModelCompat

; Beacon message type (match beacon.asm)
MODEL_HOTSWAP_COMPLETE  equ 1002h
MODEL_HOTSWAP_FAILED    equ 1003h

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

; ── Model metadata snapshot (for hot-swap compatibility checks) ──
align 8
g_curGgufVersion   dd ?         ; GGUF format version (offset +4)
g_curTensorCount   dq ?         ; tensor count from current model
g_curMetaKVCount   dq ?         ; metadata KV pair count
g_curAlignment     dq ?         ; data section alignment

<<<<<<< HEAD
; ── Tensor Registry Hash Table (1024 buckets × 16 bytes) ──
; Bucket: [name_hash:QWORD | offset:DWORD | size:WORD | type:WORD]
align 16
g_tensorRegistry  db 16384 dup(?)

; ── LRU Tensor Lookup Cache (8 entries × 16 bytes) ──
; Entry: [name_hash:QWORD | result_ptr:QWORD]
align 8
g_lruCache        db 128 dup(?)
g_lruCount        dd ?

; ── GGUF Parser State ──
align 8
g_ggufMagicBuf    dd ?
g_ggufParsedVer   dd ?
g_ggufTensorIdx   dq ?

; ── Model Metadata Cache (16 KV entries × 16 bytes) ──
; Entry: [key_hash:QWORD | value:QWORD]
align 8
g_metadataCache   db 256 dup(?)
g_metaCacheCount  dd ?

; ── Memory-Mapped File Handles (4 concurrent mapping slots) ──
; Slot: [hFile:QWORD | hMapping:QWORD | pBase:QWORD] = 24 bytes
align 8
g_mmapSlots       db 96 dup(?)
g_mmapActiveCount dd ?

; ── Compatibility Checker State ──
align 4
g_compatLastResult dd ?
g_supportMaxLayers dd ?
g_supportMaxHidden dd ?
g_supportMaxVocab  dd ?

; ── Model Loader Ready Flag ──
g_modelLoaderReady dd ?

=======
>>>>>>> origin/main
.const
GENERIC_READ     equ 80000000h
FILE_SHARE_READ  equ 1
OPEN_EXISTING    equ 3
PAGE_READONLY    equ 2
FILE_MAP_READ    equ 4
GGUF_MAGIC       equ 046554747h

<<<<<<< HEAD
; ── Tensor registry constants ──
TENSOR_BUCKETS     equ 1024
TENSOR_BUCKET_MASK equ 03FFh
TENSOR_ENTRY_SIZE  equ 16
LRU_CACHE_ENTRIES  equ 8
MAX_MMAP_SLOTS     equ 4

; ── FNV-1a 64-bit constants ──
FNV64_OFFSET       equ 0CBF29CE484222325h
FNV64_PRIME        equ 100000001B3h

; ── Architecture validation bounds ──
MIN_LAYERS         equ 1
MAX_LAYERS         equ 128
MIN_HIDDEN_DIM     equ 64
MAX_HIDDEN_DIM     equ 16384
MIN_VOCAB_SIZE     equ 100
MAX_VOCAB_SIZE     equ 200000
MAX_TENSOR_COUNT   equ 1000000
MAX_KV_COUNT       equ 100000

; ── GGUF KV value type IDs ──
GGUF_TYPE_UINT8    equ 0
GGUF_TYPE_INT8     equ 1
GGUF_TYPE_UINT16   equ 2
GGUF_TYPE_INT16    equ 3
GGUF_TYPE_UINT32   equ 4
GGUF_TYPE_INT32    equ 5
GGUF_TYPE_FLOAT32  equ 6
GGUF_TYPE_BOOL     equ 7
GGUF_TYPE_STRING   equ 8
GGUF_TYPE_ARRAY    equ 9
GGUF_TYPE_UINT64   equ 10
GGUF_TYPE_INT64    equ 11
GGUF_TYPE_FLOAT64  equ 12

; ── GGUF metadata key suffix match constants (little-endian DWORDs) ──
; "block_count" (11 bytes): 62 6C 6F 63 6B 5F 63 6F 75 6E 74
SUFFIX_BC_DW0      equ 636F6C62h
SUFFIX_BC_DW1      equ 6F635F6Bh
SUFFIX_BC_W2       equ 6E75h
SUFFIX_BC_B3       equ 74h
SUFFIX_BC_LEN      equ 11
; "embedding_length" (16 bytes): 65 6D 62 65 64 64 69 6E 67 5F 6C 65 6E 67 74 68
SUFFIX_EL_DW0      equ 65626D65h
SUFFIX_EL_DW1      equ 6E696464h
SUFFIX_EL_DW2      equ 656C5F67h
SUFFIX_EL_DW3      equ 6874676Eh
SUFFIX_EL_LEN      equ 16
; "vocab_size" (10 bytes): 76 6F 63 61 62 5F 73 69 7A 65
SUFFIX_VS_DW0      equ 61636F76h
SUFFIX_VS_DW1      equ 69735F62h
SUFFIX_VS_W2       equ 657Ah
SUFFIX_VS_LEN      equ 10

.code
; ────────────────────────────────────────────────────────────────
; ModelLoaderInit — full model loader subsystem initialization
;   Creates GGUF parser state, tensor registry (1024-bucket hash
;   table), metadata cache, mmap slot array (4 concurrent), and
;   compatibility checker defaults.  Sets g_modelLoaderReady = 1.
;   Returns: EAX = 0 success, -1 failure
; ────────────────────────────────────────────────────────────────
ModelLoaderInit PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; ── 1. Initialize SRWLOCK for thread-safe hot-swap ──
    lea     rcx, g_modelLock
    call    InitializeSRWLock

    ; ── 2. Zero tensor registry hash table (1024 × 16 = 16384 bytes) ──
    lea     rdi, g_tensorRegistry
    xor     eax, eax
    mov     ecx, 2048                   ; 16384 / 8 qwords
    rep     stosq

    ; ── 3. Zero LRU tensor lookup cache (8 × 16 = 128 bytes) ──
    lea     rdi, g_lruCache
    xor     eax, eax
    mov     ecx, 16                     ; 128 / 8
    rep     stosq
    mov     dword ptr g_lruCount, 0

    ; ── 4. Initialize GGUF parser state ──
    mov     dword ptr g_ggufMagicBuf, 0
    mov     dword ptr g_ggufParsedVer, 0
    mov     qword ptr g_ggufTensorIdx, 0

    ; ── 5. Zero model metadata cache (16 × 16 = 256 bytes) ──
    lea     rdi, g_metadataCache
    xor     eax, eax
    mov     ecx, 32                     ; 256 / 8
    rep     stosq
    mov     dword ptr g_metaCacheCount, 0

    ; ── 6. Set up memory-mapped file handles (4 concurrent slots) ──
    lea     rdi, g_mmapSlots
    xor     eax, eax
    mov     ecx, 12                     ; 96 / 8
    rep     stosq
    ; Mark all hFile entries as INVALID_HANDLE_VALUE
    lea     rdi, g_mmapSlots
    mov     rax, -1
    mov     qword ptr [rdi],    rax     ; slot 0 hFile
    mov     qword ptr [rdi+24], rax     ; slot 1 hFile
    mov     qword ptr [rdi+48], rax     ; slot 2 hFile
    mov     qword ptr [rdi+72], rax     ; slot 3 hFile
    mov     dword ptr g_mmapActiveCount, 0

    ; ── 7. Initialize compatibility checker defaults ──
    mov     dword ptr g_compatLastResult, 0
    mov     dword ptr g_supportMaxLayers, MAX_LAYERS
    mov     dword ptr g_supportMaxHidden, MAX_HIDDEN_DIM
    mov     dword ptr g_supportMaxVocab, MAX_VOCAB_SIZE

    ; ── 8. Set model loader ready flag ──
    mov     dword ptr g_modelLoaderReady, 1

    add     rsp, 30h
    pop     rdi
    pop     rsi
    pop     rbx
    xor     eax, eax                    ; 0 = success
=======
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
>>>>>>> origin/main
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
    ; ── Cache GGUF header metadata for compatibility checks ──
    ; GGUF layout: [0]=magic(4), [4]=version(4), [8]=tensorCount(8), [10h]=metaKVCount(8)
    mov     rax, pBase
    mov     ecx, dword ptr [rax+4]       ; version
    mov     g_curGgufVersion, ecx
    mov     rcx, [rax+8]                 ; tensor_count (u64)
    mov     g_curTensorCount, rcx
    mov     qTensorCount, rcx
    mov     rcx, [rax+10h]               ; metadata_kv_count (u64)
    mov     g_curMetaKVCount, rcx
    ; Default alignment = 32 (GGUF spec); real value lives in metadata
    mov     qword ptr g_curAlignment, 32
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
<<<<<<< HEAD
; GetTensor — tensor lookup by name via FNV-1a hash + linear probe
;   RCX = pointer to tensor name string (ASCII, null-terminated)
;   Returns: RAX = pointer to tensor data (g_modelbase + offset)
;            RAX = 0 if not found or model not loaded
;   Maintains an 8-entry LRU cache for hot tensor access.
; ────────────────────────────────────────────────────────────────
GetTensor PROC FRAME
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
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rsi, rcx                    ; rsi = tensor name ptr
    test    rsi, rsi
    jz      @gt_fail

    ; Verify model is loaded
    mov     rax, qword ptr pBase
    test    rax, rax
    jz      @gt_fail

    ; ── Compute FNV-1a 64-bit hash of tensor name ──
    mov     rax, FNV64_OFFSET           ; offset basis
    mov     r12, FNV64_PRIME            ; prime
@gt_fnv:
    movzx   ecx, byte ptr [rsi]
    test    cl, cl
    jz      @gt_fnv_end
    xor     rax, rcx                    ; hash ^= byte
    imul    rax, r12                    ; hash *= prime
    inc     rsi
    jmp     @gt_fnv
@gt_fnv_end:
    mov     r12, rax                    ; r12 = final hash

    ; ── Check LRU cache (8-entry, most-recent-first) ──
    lea     rbx, g_lruCache
    mov     ecx, dword ptr g_lruCount
    xor     edx, edx
@gt_lru_scan:
    cmp     edx, ecx
    jge     @gt_lru_miss
    mov     eax, edx
    shl     eax, 4                      ; entry offset = index × 16
    cmp     r12, qword ptr [rbx + rax]  ; compare hash
    je      @gt_lru_hit
    inc     edx
    jmp     @gt_lru_scan

@gt_lru_hit:
    mov     rax, qword ptr [rbx + rax + 8]  ; cached result pointer
    jmp     @gt_done

@gt_lru_miss:
    ; ── Hash table lookup with linear probing ──
    mov     rax, r12
    and     eax, TENSOR_BUCKET_MASK     ; bucket index = hash & 0x3FF
    lea     rdi, g_tensorRegistry
    xor     r13d, r13d                  ; probe counter

@gt_probe:
    cmp     r13d, TENSOR_BUCKETS
    jge     @gt_fail                    ; full table scan — not found
    mov     ecx, eax
    shl     ecx, 4                      ; bucket byte offset
    mov     rdx, qword ptr [rdi + rcx]  ; stored hash
    test    rdx, rdx
    jz      @gt_fail                    ; empty bucket = not in table
    cmp     rdx, r12
    je      @gt_hit
    inc     eax                         ; linear probe: next bucket
    and     eax, TENSOR_BUCKET_MASK
    inc     r13d
    jmp     @gt_probe

@gt_hit:
    ; ecx = bucket byte offset (from last probe)
    ; Bucket layout: [hash:8][offset:4][size:2][type:2]
    mov     r13d, dword ptr [rdi + rcx + 8]  ; tensor offset (DWORD)
    mov     rax, qword ptr g_modelbase
    test    rax, rax
    jz      @gt_fail
    add     rax, r13                    ; rax = g_modelbase + tensor_offset
    mov     r13, rax                    ; save result in r13

    ; ── LRU insert: shift entries down, place new at front ──
    ; rbx still = &g_lruCache from scan phase
    mov     ecx, dword ptr g_lruCount
    cmp     ecx, LRU_CACHE_ENTRIES
    jge     @gt_lru_clamp
    mov     edx, ecx                    ; shift all 'count' entries
    jmp     @gt_lru_doshift
@gt_lru_clamp:
    mov     edx, LRU_CACHE_ENTRIES - 1  ; shift 7 (oldest drops off)
@gt_lru_doshift:
    test    edx, edx
    jz      @gt_lru_store
@gt_lru_shift:
    dec     edx
    mov     eax, edx
    shl     eax, 4                      ; source entry offset
    mov     rcx, qword ptr [rbx + rax]
    mov     qword ptr [rbx + rax + 16], rcx   ; shift hash
    mov     rcx, qword ptr [rbx + rax + 8]
    mov     qword ptr [rbx + rax + 24], rcx   ; shift ptr
    test    edx, edx
    jnz     @gt_lru_shift

@gt_lru_store:
    mov     qword ptr [rbx], r12        ; hash  at slot 0
    mov     qword ptr [rbx + 8], r13    ; ptr   at slot 0
    mov     ecx, dword ptr g_lruCount
    cmp     ecx, LRU_CACHE_ENTRIES
    jge     @gt_lru_capped
    inc     ecx
    mov     dword ptr g_lruCount, ecx
@gt_lru_capped:
    mov     rax, r13                    ; return result pointer

@gt_done:
    add     rsp, 20h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@gt_fail:
    xor     eax, eax
    add     rsp, 20h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
=======
; GetTensor — return base + header skip (GGUF tensors start past metadata)
;   RCX = tensor name (currently unused; sequential access)
;   Returns: RAX = pointer into mapped region, or 0 if unmapped
; ────────────────────────────────────────────────────────────────
GetTensor PROC
    mov     rax, pBase
    test    rax, rax
    jz      @tensor_null
    add     rax, 100h
    ret
@tensor_null:
    xor     eax, eax
>>>>>>> origin/main
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
;   When preserveKV=1, runs ValidateModelCompat first.
; ────────────────────────────────────────────────────────────────
HotSwapModel PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    mov     rbx, rcx                    ; rbx = newPath
    movzx   esi, dl                     ; esi = preserveKV

    ; ── Snapshot current model metadata before swap ──
    mov     eax, g_curGgufVersion
    mov     [rsp+20h], eax              ; save old version
    mov     rax, g_curTensorCount
    mov     [rsp+28h], rax              ; save old tensor count
    mov     rax, g_curMetaKVCount
    mov     [rsp+30h], rax              ; save old meta KV count

    ; Acquire exclusive lock
    lea     rcx, g_modelLock
    call    AcquireSRWLockExclusive

<<<<<<< HEAD
    ; Phase 5: Flush all paged tensor slots before unmapping
    call    SlotRing_EvictAll

=======
>>>>>>> origin/main
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

    ; ── Compatibility guard when preserving KV ──
    test    esi, esi
    jz      @skip_compat                ; preserveKV=0 → no check needed

    ; Compare tensor count: old vs new
    mov     rax, [rsp+28h]              ; old tensor count
    cmp     rax, g_curTensorCount
    jne     @compat_fail                ; tensor count mismatch

    ; Compare GGUF version: old vs new
    mov     eax, [rsp+20h]              ; old version
    cmp     eax, g_curGgufVersion
    jne     @compat_fail                ; version mismatch

@skip_compat:
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
<<<<<<< HEAD
    ; Phase 5: Re-initialize SlotRing with default 4GB budget for new model
    mov     rcx, 100000000h             ; 4GB
    call    SlotRing_Init
=======
>>>>>>> origin/main
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

    add     rsp, 40h
    pop     rdi
    pop     rsi
    pop     rbx
    mov     eax, 1                      ; success
    ret

@compat_fail:
    ; Model is incompatible — unload it, restore null state
    call    UnloadModel
    xor     eax, eax
    mov     g_modelbase, rax

    ; Signal failure via Beacon
    mov     ecx, 0
    mov     rdx, rbx
    mov     r8d, MODEL_HOTSWAP_FAILED
    call    BeaconSend

@swap_fail:
    ; Release lock even on failure
    lea     rcx, g_modelLock
    call    ReleaseSRWLockExclusive

    add     rsp, 40h
    pop     rdi
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

; ────────────────────────────────────────────────────────────────
<<<<<<< HEAD
; ValidateModelCompat — full GGUF model compatibility validation
;   RCX = pointer to model base (mapped GGUF file)
;   Returns: EAX = compatibility flags bitfield
;     bit 0 = GGUF magic OK      bit 1 = version OK (1/2/3)
;     bit 2 = architecture OK    bit 3 = size OK
;     0Fh = fully compatible
;   Walks GGUF metadata KV section to find and validate:
;     block_count (n_layers 1-128), embedding_length (hidden_dim
;     64-16384 power-of-2), vocab_size (100-200000).
; ────────────────────────────────────────────────────────────────
ValidateModelCompat PROC FRAME
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
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Locals: [rsp+0]=n_layers [rsp+4]=hidden_dim
    ;         [rsp+8]=vocab_size [rsp+0Ch]=found_flags
    xor     eax, eax
    mov     dword ptr [rsp], eax
    mov     dword ptr [rsp+4], eax
    mov     dword ptr [rsp+8], eax
    mov     dword ptr [rsp+0Ch], eax

    xor     ebx, ebx                    ; ebx = result flags
    mov     r14, rcx                    ; r14 = model base
    test    r14, r14
    jz      @vmc_done

    ; ── Bit 0: GGUF magic (0x46554747) ──
    mov     eax, dword ptr [r14]
    cmp     eax, GGUF_MAGIC
    jne     @vmc_b1
    or      ebx, 1

@vmc_b1:
    ; ── Bit 1: Version must be 1, 2, or 3 ──
    mov     eax, dword ptr [r14+4]
    cmp     eax, 1
    je      @vmc_ver_ok
    cmp     eax, 2
    je      @vmc_ver_ok
    cmp     eax, 3
    je      @vmc_ver_ok
    jmp     @vmc_b2
@vmc_ver_ok:
    or      ebx, 2

@vmc_b2:
    ; ── Bit 2: Walk metadata KV to find arch params ──
    mov     r13, qword ptr [r14+10h]    ; r13 = metadata_kv_count
    lea     r12, [r14 + 18h]            ; r12 = past 24-byte GGUF header
    test    r13, r13
    jz      @vmc_kv_done

@vmc_kv_loop:
    test    r13, r13
    jz      @vmc_kv_done
    dec     r13

    ; Read key_len (uint64)
    mov     rsi, qword ptr [r12]
    add     r12, 8
    ; rdi = key start, rsi = key length
    mov     rdi, r12
    add     r12, rsi
    ; Read value_type (uint32)
    mov     r8d, dword ptr [r12]
    add     r12, 4

    ; ── Suffix: "block_count" (11 bytes) ──
    cmp     rsi, SUFFIX_BC_LEN
    jl      @vmc_chk_el
    lea     r9, [rdi + rsi - SUFFIX_BC_LEN]
    cmp     dword ptr [r9],    SUFFIX_BC_DW0
    jne     @vmc_chk_el
    cmp     dword ptr [r9+4],  SUFFIX_BC_DW1
    jne     @vmc_chk_el
    cmp     word  ptr [r9+8],  SUFFIX_BC_W2
    jne     @vmc_chk_el
    cmp     byte  ptr [r9+10], SUFFIX_BC_B3
    jne     @vmc_chk_el
    ; Matched → read integer value
    cmp     r8d, GGUF_TYPE_UINT32
    je      @vmc_bc32
    cmp     r8d, GGUF_TYPE_INT32
    je      @vmc_bc32
    cmp     r8d, GGUF_TYPE_UINT64
    je      @vmc_bc64
    cmp     r8d, GGUF_TYPE_INT64
    je      @vmc_bc64
    jmp     @vmc_skip_val
@vmc_bc32:
    mov     eax, dword ptr [r12]
    add     r12, 4
    mov     dword ptr [rsp], eax
    or      byte ptr [rsp+0Ch], 1
    jmp     @vmc_kv_loop
@vmc_bc64:
    mov     eax, dword ptr [r12]
    add     r12, 8
    mov     dword ptr [rsp], eax
    or      byte ptr [rsp+0Ch], 1
    jmp     @vmc_kv_loop

@vmc_chk_el:
    ; ── Suffix: "embedding_length" (16 bytes) ──
    cmp     rsi, SUFFIX_EL_LEN
    jl      @vmc_chk_vs
    lea     r9, [rdi + rsi - SUFFIX_EL_LEN]
    cmp     dword ptr [r9],    SUFFIX_EL_DW0
    jne     @vmc_chk_vs
    cmp     dword ptr [r9+4],  SUFFIX_EL_DW1
    jne     @vmc_chk_vs
    cmp     dword ptr [r9+8],  SUFFIX_EL_DW2
    jne     @vmc_chk_vs
    cmp     dword ptr [r9+12], SUFFIX_EL_DW3
    jne     @vmc_chk_vs
    cmp     r8d, GGUF_TYPE_UINT32
    je      @vmc_el32
    cmp     r8d, GGUF_TYPE_INT32
    je      @vmc_el32
    cmp     r8d, GGUF_TYPE_UINT64
    je      @vmc_el64
    cmp     r8d, GGUF_TYPE_INT64
    je      @vmc_el64
    jmp     @vmc_skip_val
@vmc_el32:
    mov     eax, dword ptr [r12]
    add     r12, 4
    mov     dword ptr [rsp+4], eax
    or      byte ptr [rsp+0Ch], 2
    jmp     @vmc_kv_loop
@vmc_el64:
    mov     eax, dword ptr [r12]
    add     r12, 8
    mov     dword ptr [rsp+4], eax
    or      byte ptr [rsp+0Ch], 2
    jmp     @vmc_kv_loop

@vmc_chk_vs:
    ; ── Suffix: "vocab_size" (10 bytes) ──
    cmp     rsi, SUFFIX_VS_LEN
    jl      @vmc_skip_val
    lea     r9, [rdi + rsi - SUFFIX_VS_LEN]
    cmp     dword ptr [r9],   SUFFIX_VS_DW0
    jne     @vmc_skip_val
    cmp     dword ptr [r9+4], SUFFIX_VS_DW1
    jne     @vmc_skip_val
    cmp     word  ptr [r9+8], SUFFIX_VS_W2
    jne     @vmc_skip_val
    cmp     r8d, GGUF_TYPE_UINT32
    je      @vmc_vs32
    cmp     r8d, GGUF_TYPE_INT32
    je      @vmc_vs32
    cmp     r8d, GGUF_TYPE_UINT64
    je      @vmc_vs64
    cmp     r8d, GGUF_TYPE_INT64
    je      @vmc_vs64
    jmp     @vmc_skip_val
@vmc_vs32:
    mov     eax, dword ptr [r12]
    add     r12, 4
    mov     dword ptr [rsp+8], eax
    or      byte ptr [rsp+0Ch], 4
    jmp     @vmc_kv_loop
@vmc_vs64:
    mov     eax, dword ptr [r12]
    add     r12, 8
    mov     dword ptr [rsp+8], eax
    or      byte ptr [rsp+0Ch], 4
    jmp     @vmc_kv_loop

    ; ── Skip value by type (advance r12 past value) ──
@vmc_skip_val:
    cmp     r8d, GGUF_TYPE_UINT8
    je      @vmc_sv1
    cmp     r8d, GGUF_TYPE_INT8
    je      @vmc_sv1
    cmp     r8d, GGUF_TYPE_BOOL
    je      @vmc_sv1
    cmp     r8d, GGUF_TYPE_UINT16
    je      @vmc_sv2
    cmp     r8d, GGUF_TYPE_INT16
    je      @vmc_sv2
    cmp     r8d, GGUF_TYPE_UINT32
    je      @vmc_sv4
    cmp     r8d, GGUF_TYPE_INT32
    je      @vmc_sv4
    cmp     r8d, GGUF_TYPE_FLOAT32
    je      @vmc_sv4
    cmp     r8d, GGUF_TYPE_UINT64
    je      @vmc_sv8
    cmp     r8d, GGUF_TYPE_INT64
    je      @vmc_sv8
    cmp     r8d, GGUF_TYPE_FLOAT64
    je      @vmc_sv8
    cmp     r8d, GGUF_TYPE_STRING
    je      @vmc_sv_str
    cmp     r8d, GGUF_TYPE_ARRAY
    je      @vmc_sv_arr
    jmp     @vmc_kv_done                ; unknown type → bail
@vmc_sv1:
    inc     r12
    jmp     @vmc_kv_loop
@vmc_sv2:
    add     r12, 2
    jmp     @vmc_kv_loop
@vmc_sv4:
    add     r12, 4
    jmp     @vmc_kv_loop
@vmc_sv8:
    add     r12, 8
    jmp     @vmc_kv_loop
@vmc_sv_str:
    mov     rax, qword ptr [r12]        ; string length
    add     r12, 8
    add     r12, rax
    jmp     @vmc_kv_loop
@vmc_sv_arr:
    mov     eax, dword ptr [r12]        ; element type
    add     r12, 4
    mov     rcx, qword ptr [r12]        ; element count
    add     r12, 8
    cmp     eax, GGUF_TYPE_UINT8
    je      @vmc_a1
    cmp     eax, GGUF_TYPE_INT8
    je      @vmc_a1
    cmp     eax, GGUF_TYPE_BOOL
    je      @vmc_a1
    cmp     eax, GGUF_TYPE_UINT16
    je      @vmc_a2
    cmp     eax, GGUF_TYPE_INT16
    je      @vmc_a2
    cmp     eax, GGUF_TYPE_UINT32
    je      @vmc_a4
    cmp     eax, GGUF_TYPE_INT32
    je      @vmc_a4
    cmp     eax, GGUF_TYPE_FLOAT32
    je      @vmc_a4
    cmp     eax, GGUF_TYPE_UINT64
    je      @vmc_a8
    cmp     eax, GGUF_TYPE_INT64
    je      @vmc_a8
    cmp     eax, GGUF_TYPE_FLOAT64
    je      @vmc_a8
    cmp     eax, GGUF_TYPE_STRING
    je      @vmc_as
    jmp     @vmc_kv_done                ; nested array / unknown → bail
@vmc_a1:
    add     r12, rcx
    jmp     @vmc_kv_loop
@vmc_a2:
    shl     rcx, 1
    add     r12, rcx
    jmp     @vmc_kv_loop
@vmc_a4:
    shl     rcx, 2
    add     r12, rcx
    jmp     @vmc_kv_loop
@vmc_a8:
    shl     rcx, 3
    add     r12, rcx
    jmp     @vmc_kv_loop
@vmc_as:
    ; Array of strings: iterate (uint64 len + bytes) per element
    test    rcx, rcx
    jz      @vmc_kv_loop
@vmc_as_lp:
    mov     rax, qword ptr [r12]
    add     r12, 8
    add     r12, rax
    dec     rcx
    jnz     @vmc_as_lp
    jmp     @vmc_kv_loop

@vmc_kv_done:
    ; ── Evaluate found architecture params → bit 2 ──
    test    byte ptr [rsp+0Ch], 1       ; n_layers found?
    jz      @vmc_b3                     ; not found → cannot verify arch
    mov     eax, dword ptr [rsp]        ; n_layers
    cmp     eax, MIN_LAYERS
    jl      @vmc_b3
    cmp     eax, MAX_LAYERS
    jg      @vmc_b3

    ; hidden_dim — validate only if found
    test    byte ptr [rsp+0Ch], 2
    jz      @vmc_arch_ok                ; not found → partial pass
    mov     eax, dword ptr [rsp+4]      ; hidden_dim
    cmp     eax, MIN_HIDDEN_DIM
    jl      @vmc_b3
    cmp     eax, MAX_HIDDEN_DIM
    jg      @vmc_b3
    ; Power-of-2 check: (n & (n-1)) == 0
    lea     ecx, [rax-1]
    test    ecx, eax
    jnz     @vmc_b3

    ; vocab_size — validate only if found
    test    byte ptr [rsp+0Ch], 4
    jz      @vmc_arch_ok                ; not found → partial pass
    mov     eax, dword ptr [rsp+8]      ; vocab_size
    cmp     eax, MIN_VOCAB_SIZE
    jl      @vmc_b3
    cmp     eax, MAX_VOCAB_SIZE
    jg      @vmc_b3

@vmc_arch_ok:
    or      ebx, 4                      ; bit 2 = arch OK

@vmc_b3:
    ; ── Bit 3: Size sanity (n_tensors < 1M, n_kv < 100K) ──
    mov     rax, qword ptr [r14+8]      ; n_tensors
    cmp     rax, MAX_TENSOR_COUNT
    ja      @vmc_done
    mov     rax, qword ptr [r14+10h]    ; n_kv
    cmp     rax, MAX_KV_COUNT
    ja      @vmc_done
    or      ebx, 8                      ; bit 3 = size OK

@vmc_done:
    mov     eax, ebx                    ; return flags
    mov     dword ptr g_compatLastResult, eax
    add     rsp, 28h
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
=======
; ValidateModelCompat — check if a candidate GGUF is compatible
;   with the currently loaded model (for KV cache preservation)
;   RCX = pCandidateBase (mapped GGUF region)
;   Returns: EAX = 1 (compatible), 0 (incompatible)
;
;   Checks:
;     1. GGUF magic matches
;     2. GGUF version matches current
;     3. Tensor count matches current
; ────────────────────────────────────────────────────────────────
ValidateModelCompat PROC
    test    rcx, rcx
    jz      @compat_no

    ; 1. Magic check
    mov     eax, dword ptr [rcx]
    cmp     eax, GGUF_MAGIC
    jne     @compat_no

    ; 2. Version match
    mov     eax, dword ptr [rcx+4]
    cmp     eax, g_curGgufVersion
    jne     @compat_no

    ; 3. Tensor count match
    mov     rax, [rcx+8]
    cmp     rax, g_curTensorCount
    jne     @compat_no

    mov     eax, 1
    ret

@compat_no:
    xor     eax, eax
>>>>>>> origin/main
    ret
ValidateModelCompat ENDP

END
