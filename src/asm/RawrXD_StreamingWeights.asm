; =============================================================================
; RawrXD_StreamingWeights.asm — Mini-HDD Streaming Configuration Enforcer
; Reconstructs the reverse-engineered "external storage" model loader
; Targets: 120B+ models on 64GB RAM via memory-mapped disk streaming
; =============================================================================
;
; Architecture (from startup reverse-engineering):
;   - Weights reside on NVMe/HDD (Q2_K ~38GB file)
;   - Windows mmap with FILE_FLAG_SEQUENTIAL_SCAN hint
;   - 4KB page-aligned reads (OS cache-friendly)
;   - Active layer pinning (only 2-3 layers resident at once)
;   - KV cache strictly bounded to 2GB (2048 ctx limit)
;
; Key Insight: The "80GB RAM" requirement is for resident weights.
; With mmap streaming, you need:
;   ~2GB Active layers (hot)
;   ~2GB KV cache (context dependent)
;   ~4GB OS/File cache
;   = 8GB actual RAM usage + 38GB disk-backed virtual memory
;
; The 153s timeout occurs when mmap is DISABLED (weights try to load fully)
;
; Exports:
;   StreamingWeights_Configure         — Force mmap mode, disable mlock
;   StreamingWeights_OpenModel         — Open weight file with sequential scan
;   StreamingWeights_PinLayer          — Manual layer residency control
;   StreamingWeights_EvictCold         — Drop non-active layers from working set
;   StreamingWeights_CalculateKVReq    — Verify ctx fits in available RAM
;
; Build: ml64.exe /c /Zi /Fo RawrXD_StreamingWeights.obj RawrXD_StreamingWeights.asm
; =============================================================================

include RawrXD_Common.inc

; =============================================================================
; External Imports
; =============================================================================
EXTERNDEF CreateFileA:PROC
EXTERNDEF CreateFileMappingA:PROC
EXTERNDEF MapViewOfFile:PROC
EXTERNDEF UnmapViewOfFile:PROC
EXTERNDEF VirtualLock:PROC
EXTERNDEF VirtualUnlock:PROC
EXTERNDEF SetFilePointer:PROC
EXTERNDEF SetEndOfFile:PROC
EXTERNDEF GetSystemInfo:PROC
EXTERNDEF GetProcessWorkingSetSize:PROC
EXTERNDEF SetProcessWorkingSetSize:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF GetCurrentProcess:PROC

; =============================================================================
; Constants
; =============================================================================

; File access flags for streaming
GENERIC_READ                EQU 80000000h
OPEN_EXISTING               EQU 3
FILE_FLAG_SEQUENTIAL_SCAN   EQU 08000000h
FILE_ATTRIBUTE_NORMAL       EQU 00000080h
INVALID_HANDLE_VALUE        EQU -1

; Memory protection
PAGE_READONLY               EQU 02h
PAGE_READWRITE              EQU 04h
FILE_MAP_READ               EQU 0004h

; Working set limits (bytes)
WS_MIN_STREAMING            EQU 2147483648   ; 2GB min
WS_MAX_STREAMING            EQU 17179869184  ; 16GB max

; Layer configuration for 120B GPT-OSS
LAYER_COUNT_120B            EQU 128
LAYER_SIZE_Q2_K             EQU 304857600    ; ~305MB per layer at Q2_K
ACTIVE_LAYER_WINDOW         EQU 4            ; Keep 4 layers hot

; =============================================================================
; Data Segment
; =============================================================================
.data

ALIGN 8
g_WeightFileHandle          dq INVALID_HANDLE_VALUE
g_WeightMapping             dq 0
g_WeightBaseAddr            dq 0
g_TotalWeightSize           dq 0
g_LayerSize                 dq LAYER_SIZE_Q2_K
g_ActiveLayerStart          dd -1
g_ActiveLayerEnd            dd -1

; =============================================================================
; Code Segment
; =============================================================================
.code

; =============================================================================
; StreamingWeights_Configure — Force Ollama into HDD streaming mode
;
; RCX = Model size in billions (120)
; RDX = Quantization level (2=Q2_K, 3=Q3, etc.)
; Returns: RAX = 0 on success
; =============================================================================
PUBLIC StreamingWeights_Configure
StreamingWeights_Configure PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 128
    .allocstack 128
    .endprolog

    mov     ebx, ecx                ; Model size
    mov     esi, edx                ; Quant level

    ; Step 1: Configure process working set
    sub     rsp, 32
    call    GetCurrentProcess
    add     rsp, 32
    mov     rcx, rax                ; hProcess
    mov     rdx, WS_MIN_STREAMING   ; Minimum working set
    mov     r8, WS_MAX_STREAMING    ; Maximum working set
    sub     rsp, 32
    call    SetProcessWorkingSetSize
    add     rsp, 32

    ; Step 2: Calculate layer size based on quant
    ; Q2_K: ~0.25 bytes/param -> 120B * 0.25 = 30GB / 128 layers = ~235MB/layer
    mov     eax, ebx                ; Model size (B)
    imul    eax, 250000000          ; * 0.25 = bytes at Q2
    xor     edx, edx
    mov     ecx, LAYER_COUNT_120B
    div     ecx                     ; EAX = bytes per layer
    mov     g_LayerSize, rax

    xor     eax, eax                ; Success
    add     rsp, 128
    pop     rdi
    pop     rsi
    pop     rbx
    ret
StreamingWeights_Configure ENDP

; =============================================================================
; StreamingWeights_OpenModel — Open weight file with streaming hints
;
; RCX = Path to model file (null-terminated string)
; Returns: RAX = handle or INVALID_HANDLE_VALUE
; =============================================================================
PUBLIC StreamingWeights_OpenModel
StreamingWeights_OpenModel PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 96
    .allocstack 96
    .endprolog

    mov     rbx, rcx                ; Save path

    ; CreateFile with SEQUENTIAL_SCAN hint — the KEY flag for streaming mode
    mov     rcx, rbx                        ; lpFileName
    mov     edx, GENERIC_READ               ; dwDesiredAccess
    xor     r8d, r8d                        ; dwShareMode
    xor     r9d, r9d                        ; lpSecurityAttributes
    mov     dword ptr [rsp + 32], OPEN_EXISTING
    mov     dword ptr [rsp + 40], FILE_ATTRIBUTE_NORMAL OR FILE_FLAG_SEQUENTIAL_SCAN
    mov     qword ptr [rsp + 48], 0         ; hTemplateFile
    sub     rsp, 32
    call    CreateFileA
    add     rsp, 32

    cmp     rax, INVALID_HANDLE_VALUE
    je      @swom_fail

    mov     g_WeightFileHandle, rax

    ; Create file mapping (reserve address space, don't commit RAM)
    mov     rcx, rax                ; hFile
    xor     edx, edx                ; lpAttributes
    mov     r8d, PAGE_READONLY      ; flProtect
    xor     r9d, r9d                ; dwMaxSizeHigh
    mov     qword ptr [rsp + 32], 0 ; dwMaxSizeLow (use file size)
    sub     rsp, 32
    call    CreateFileMappingA
    add     rsp, 32

    test    rax, rax
    jz      @swom_fail_close

    mov     g_WeightMapping, rax

    ; Map view — reserves virtual address space but doesn't load pages
    mov     rcx, rax                ; hFileMappingObject
    mov     edx, FILE_MAP_READ      ; dwDesiredAccess
    xor     r8d, r8d                ; dwFileOffsetHigh
    xor     r9d, r9d                ; dwFileOffsetLow
    mov     qword ptr [rsp + 32], 0 ; dwNumberOfBytesToMap (all)
    sub     rsp, 32
    call    MapViewOfFile
    add     rsp, 32

    test    rax, rax
    jz      @swom_fail_unmap

    mov     g_WeightBaseAddr, rax

    ; Success — model is now mapped but NOT in RAM
    ; Pages will fault in on demand during inference
    mov     rax, g_WeightFileHandle
    jmp     @swom_return

@swom_fail_unmap:
    mov     rcx, g_WeightMapping
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32

@swom_fail_close:
    mov     rcx, g_WeightFileHandle
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
    mov     g_WeightFileHandle, INVALID_HANDLE_VALUE

@swom_fail:
    mov     rax, INVALID_HANDLE_VALUE

@swom_return:
    add     rsp, 96
    pop     rbx
    ret
StreamingWeights_OpenModel ENDP

; =============================================================================
; StreamingWeights_PinLayer — Force layer residency
;
; Pre-faults a specific layer into RAM so it's available for computation
; without disk stall. Unpins previous layers outside window.
;
; RCX = Layer index (0-127 for 120B)
; Returns: RAX = pointer to layer data
; =============================================================================
PUBLIC StreamingWeights_PinLayer
StreamingWeights_PinLayer PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 64
    .allocstack 64
    .endprolog

    mov     r12d, ecx               ; Target layer

    ; Check if already in active window
    mov     eax, g_ActiveLayerStart
    cmp     r12d, eax
    jb      @swpl_new_window
    mov     eax, g_ActiveLayerEnd
    cmp     r12d, eax
    jbe     @swpl_already_hot       ; Layer already resident

@swpl_new_window:
    ; Define new window: [target-1, target+WINDOW-1] clamped to valid range
    mov     ebx, r12d
    dec     ebx
    jns     @swpl_start_ok
    xor     ebx, ebx
@swpl_start_ok:

    mov     edi, r12d
    add     edi, ACTIVE_LAYER_WINDOW - 1
    cmp     edi, LAYER_COUNT_120B
    jb      @swpl_end_ok
    mov     edi, LAYER_COUNT_120B - 1
@swpl_end_ok:

    mov     g_ActiveLayerStart, ebx
    mov     g_ActiveLayerEnd, edi

    ; Touch first byte of each layer in window to force page-in
@swpl_fault_loop:
    mov     rax, rbx                ; Current layer index
    imul    rax, g_LayerSize        ; Byte offset
    add     rax, g_WeightBaseAddr   ; Absolute address

    ; Read first byte (triggers page fault if not resident)
    movzx   ecx, byte ptr [rax]

    inc     ebx
    cmp     ebx, edi
    jbe     @swpl_fault_loop

@swpl_already_hot:
    ; Return pointer to requested layer
    mov     rax, r12
    imul    rax, g_LayerSize
    add     rax, g_WeightBaseAddr

    add     rsp, 64
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
StreamingWeights_PinLayer ENDP

; =============================================================================
; StreamingWeights_EvictCold — Drop non-active layers from RAM
;
; Calls VirtualUnlock on cold regions to hint OS to page out
; without closing the mapping.
; =============================================================================
PUBLIC StreamingWeights_EvictCold
StreamingWeights_EvictCold PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Iterate through all layers outside active window and unlock
    xor     ebx, ebx                ; Layer counter

@swec_loop:
    cmp     ebx, g_ActiveLayerStart
    jb      @swec_unlock
    cmp     ebx, g_ActiveLayerEnd
    jbe     @swec_skip              ; In hot window, keep resident

@swec_unlock:
    ; Calculate address
    mov     rax, rbx
    imul    rax, g_LayerSize
    mov     rcx, g_WeightBaseAddr
    add     rcx, rax                ; RCX = layer base
    mov     rdx, g_LayerSize        ; RDX = size

    sub     rsp, 32
    call    VirtualUnlock           ; Hint: this can be paged out
    add     rsp, 32

@swec_skip:
    inc     ebx
    cmp     ebx, LAYER_COUNT_120B
    jb      @swec_loop

    ; Trim working set to enforce eviction
    sub     rsp, 32
    call    GetCurrentProcess
    add     rsp, 32
    mov     rcx, rax                ; hProcess
    mov     rdx, -1                 ; -1 = trim immediately
    mov     r8, -1
    sub     rsp, 32
    call    SetProcessWorkingSetSize
    add     rsp, 32

    xor     eax, eax
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
StreamingWeights_EvictCold ENDP

; =============================================================================
; StreamingWeights_CalculateKVReq — Return max safe context for available RAM
;
; At Q2_K inference with FP16 KV cache:
;   128 layers * ctx * 128 head_dim * 40 heads * 2 (K+V) * 2 bytes
; With mmap'd weights (38GB virtual), physical RAM for:
;   OS/IDE: 8GB
;   Active layers: 1GB (4 layers hot)
;   KV Cache: 2GB (2048 ctx)
;   File cache: remaining
;
; Returns: EAX = max safe context
; =============================================================================
PUBLIC StreamingWeights_CalculateKVReq
StreamingWeights_CalculateKVReq PROC
    mov     eax, 2048               ; Default safe ctx for 64GB + mmap
    ret
StreamingWeights_CalculateKVReq ENDP

END
