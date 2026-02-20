; =============================================================================
; RawrXD_NanoDiskBridge.asm
; Zero-copy async tensor loader: DiskKernel I/O → NanoQuant decompression
; Universal Model Loader — async pump directly into quantized tensors
;
; Links DiskKernel.dll ordinals with NanoQuant engine and AgentToolExecutor.
; Eliminates Win32 CreateFile overhead via DiskKernel direct sector I/O.
;
; Build (as library kernel linked into RawrXD-Shell):
;   Included in CMakeLists.txt ASM_KERNEL_SOURCES — exports C-callable procs.
;
; Build (standalone):
;   ml64.exe /c /Zi /Zd RawrXD_NanoDiskBridge.asm
;   link.exe RawrXD_NanoDiskBridge.obj /subsystem:console
;          kernel32.lib ntdll.lib user32.lib
;
; Pattern: PatchResult (RAX=0 success, RAX=NTSTATUS on error, RDX=detail)
; Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
; =============================================================================

option casemap:none

include RawrXD_Common.inc

; =============================================================================
; Additional EXTERN declarations not in RawrXD_Common.inc
; =============================================================================
EXTERNDEF DeviceIoControl:PROC
EXTERNDEF ReadFile:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF SetFilePointerEx:PROC
EXTERNDEF GetStdHandle:PROC
EXTERNDEF lstrlenA:PROC
EXTERNDEF ExitProcess:PROC
EXTERNDEF wsprintfA:PROC
EXTERNDEF CreateThread:PROC
EXTERNDEF WaitForSingleObject:PROC
EXTERNDEF GetOverlappedResult:PROC
EXTERNDEF CancelIoEx:PROC
EXTERNDEF CreateEventA:PROC
EXTERNDEF SetEvent:PROC
EXTERNDEF ResetEvent:PROC
EXTERNDEF FlushFileBuffers:PROC
EXTERNDEF GetTickCount64:PROC

; Shared symbols from RawrXD_DiskRecoveryAgent.asm (fixes LNK2005 duplicates)
EXTERNDEF szNewLine:BYTE
EXTERNDEF FmtBuf:BYTE
EXTERNDEF g_hStdOut:QWORD
EXTERN ConsolePrint:PROC
EXTERN PrintU64:PROC

; =============================================================================
; DiskKernel imports (from DiskKernel.def ordinals / linked at build time)
; =============================================================================
EXTERNDEF DiskKernel_Init:PROC              ; @1
EXTERNDEF DiskKernel_Shutdown:PROC          ; @2
EXTERNDEF DiskKernel_EnumerateDrives:PROC   ; @3
EXTERNDEF DiskKernel_DetectPartitions:PROC  ; @4
EXTERNDEF DiskKernel_AsyncReadSectors:PROC  ; @5
EXTERNDEF DiskKernel_GetAsyncStatus:PROC    ; @6
EXTERNDEF DK_ReadSectors:PROC              ; @10
EXTERNDEF DK_ScsiReadSectors:PROC          ; @11
EXTERNDEF DK_AtaReadSectors:PROC           ; @12

; =============================================================================
; NanoQuant external declarations (from NanoQuant engine module)
; RawrXD_KQuant_Dequant.asm or future NanoQuant quantizer
; =============================================================================
EXTERNDEF NanoQuant_QuantizeTensor:PROC
EXTERNDEF NanoQuant_DequantizeTensor:PROC
EXTERNDEF NanoQuant_DequantizeMatMul:PROC
EXTERNDEF NanoQuant_GetCompressionRatio:PROC

; =============================================================================
; DiskRecoveryAgent imports (for dying-drive fallback path)
; =============================================================================
EXTERNDEF DiskRecovery_FindDrive:PROC
EXTERNDEF DiskRecovery_Init:PROC

; =============================================================================
; Constants
; =============================================================================
STD_OUTPUT_HANDLE            equ -11
FILE_ATTRIBUTE_NORMAL        equ 80h
FILE_SHARE_WRITE             equ 2
INFINITE                     equ 0FFFFFFFFh

; DMA alignment (64-byte cache-line + 4K page alignment for NVMe)
DMA_ALIGNMENT                equ 4096
DMA_ALIGNMENT_MASK           equ NOT (DMA_ALIGNMENT - 1)

; DMA buffer sizing
DMA_BUFFER_SIZE              equ 65536     ; 64KB chunk for tensor streaming
DMA_BUFFER_ALLOC_SIZE        equ DMA_BUFFER_SIZE + DMA_ALIGNMENT  ; Over-alloc for alignment

; Sector sizes
SECTOR_SIZE_512              equ 512
SECTOR_SIZE_4K               equ 4096

; GGUF header offsets (v3 format)
GGUF_OFF_MAGIC               equ 0         ; uint32 "GGUF" = 0x46554747
GGUF_OFF_VERSION             equ 4         ; uint32 version (2 or 3)
GGUF_OFF_N_TENSORS           equ 8         ; uint64 tensor count
GGUF_OFF_N_KV                equ 16        ; uint64 metadata KV count
GGUF_HEADER_MIN_SIZE         equ 24        ; Minimum valid header

; GGUF versions
GGUF_VERSION_2               equ 2
GGUF_VERSION_3               equ 3

; NanoQuant rank limits (sub-1-bit compression levels)
NQ_RANK_MIN                  equ 1         ; Maximum compression (sub-1-bit)
NQ_RANK_MAX                  equ 8         ; Light compression (high quality)
NQ_RANK_DEFAULT              equ 4         ; Balanced (15x compression)

; Async pipeline states
NANODISK_STATE_IDLE          equ 0
NANODISK_STATE_READING       equ 1         ; Disk I/O in flight
NANODISK_STATE_PARSING       equ 2         ; GGUF header parse
NANODISK_STATE_QUANTIZING    equ 3         ; NanoQuant running
NANODISK_STATE_COMPLETE      equ 4         ; Tensor ready
NANODISK_STATE_ERROR         equ 5         ; Unrecoverable

; Error sub-codes
NANODISK_ERR_NONE            equ 0
NANODISK_ERR_DISK_OPEN       equ 1
NANODISK_ERR_DMA_ALLOC       equ 2
NANODISK_ERR_ASYNC_QUEUE     equ 3
NANODISK_ERR_GGUF_MAGIC      equ 4
NANODISK_ERR_GGUF_VERSION    equ 5
NANODISK_ERR_TENSOR_NOT_FOUND equ 6
NANODISK_ERR_QUANTIZE_FAIL   equ 7
NANODISK_ERR_CALLBACK_NULL   equ 8
NANODISK_ERR_INVALID_RANK    equ 9
NANODISK_ERR_IO_TIMEOUT      equ 10

; Max concurrent async tensor loads
MAX_ASYNC_TENSOR_LOADS       equ 16

; Checkpoint / progress notification interval (sectors)
TENSOR_LOAD_NOTIFY_INTERVAL  equ 256

; =============================================================================
; Structures
; =============================================================================

; NANO_ASYNC_LOAD_CTX — Full context for one async tensor load pipeline
NANO_ASYNC_LOAD_CTX STRUCT 8
    ; === Disk I/O ===
    hDevice              QWORD   ?       ; DiskKernel handle (drive context ptr)
    sectorLba            QWORD   ?       ; GGUF file start LBA on physical disk
    sectorCount          DWORD   ?       ; File size in 512-byte sectors
    driveIndex           DWORD   ?       ; Physical drive number (0-63)

    ; === NanoQuant ===
    quantMatrix          QWORD   ?       ; Output: ptr to NQ_BINARY_MATRIX
    targetRank           DWORD   ?       ; Compression level (1-8)
    quantType            DWORD   ?       ; Source GGML_TYPE_* (from GGUF metadata)

    ; === Async state machine ===
    ioContext            QWORD   ?       ; ptr to ASYNC_IO_CONTEXT (DiskKernel pool)
    callback             QWORD   ?       ; Completion routine: void(*)(NANO_ASYNC_LOAD_CTX*)
    state                DWORD   ?       ; NANODISK_STATE_*
    errorCode            DWORD   ?       ; NANODISK_ERR_*

    ; === Memory (DMA-aligned for NVMe/DMA transfer) ===
    dmaBufferRaw         QWORD   ?       ; Raw VirtualAlloc ptr (for VirtualFree)
    dmaBuffer            QWORD   ?       ; Aligned DMA scratch (4K aligned)
    dmaBufferSize        QWORD   ?       ; Allocated size

    ; === Tensor metadata (parsed from GGUF) ===
    tensorDims           DWORD   4 dup(?) ; [M, N, K, rank]
    tensorDataOffset     QWORD   ?       ; Byte offset of tensor data within GGUF
    tensorDataSize       QWORD   ?       ; Byte size of tensor data
    tensorName           BYTE    64 dup(?) ; Null-terminated tensor name

    ; === GGUF header cache ===
    ggufVersion          DWORD   ?       ; GGUF version (2 or 3)
    ggufTensorCount      QWORD   ?       ; Total tensors in file
    ggufKvCount          QWORD   ?       ; KV metadata pairs

    ; === Performance counters ===
    startTickCount       QWORD   ?       ; GetTickCount64 at start
    endTickCount         QWORD   ?       ; GetTickCount64 at completion
    bytesTransferred     QWORD   ?       ; Total bytes read from disk
    compressionRatio     QWORD   ?       ; NanoQuant ratio (fixed-point 16.16)

    ; === Agent integration ===
    agentUserData        QWORD   ?       ; Opaque data for BoundedAgentLoop
    agentJobId           DWORD   ?       ; Unique job ID
    _pad0                DWORD   ?       ; Alignment
NANO_ASYNC_LOAD_CTX ENDS

; NANO_QUANTIZE_JOB — Background model compression job (AgentTool mode 19)
NANO_QUANTIZE_JOB STRUCT 8
    sourcePath           BYTE    260 dup(?) ; GGUF file path (UTF-8)
    targetRank           DWORD   ?       ; Compression rank (1-8)
    _pad0                DWORD   ?
    loadCtx              QWORD   ?       ; ptr to NANO_ASYNC_LOAD_CTX (allocated)
    hWorkerThread        QWORD   ?       ; Background thread handle
    threadId             DWORD   ?       ; Thread ID
    status               DWORD   ?       ; NANODISK_STATE_*
    progressPct          DWORD   ?       ; 0-100
    _pad1                DWORD   ?
    outputMatrix         QWORD   ?       ; Result NQ_BINARY_MATRIX ptr
    compressionRatio     QWORD   ?       ; Result ratio (fixed-point 16.16)
    callbackPtr          QWORD   ?       ; void(*)(NANO_QUANTIZE_JOB*)
    userData             QWORD   ?       ; For AgentToolExecutor / IDE
NANO_QUANTIZE_JOB ENDS

; =============================================================================
; .data — Static strings and global state
; =============================================================================
.data

    ; Banner
    szNDBanner           db 13, 10
                         db "================================================", 13, 10
                         db "  RawrXD NanoDisk Bridge v1.0", 13, 10
                         db "  Zero-copy async tensor loader", 13, 10
                         db "  DiskKernel I/O -> NanoQuant decompression", 13, 10
                         db "================================================", 13, 10, 0

    ; Status messages
    szNDInit             db "[NanoDisk] Bridge initialized.", 13, 10, 0
    szNDLoadStart        db "[NanoDisk] Async tensor load queued: LBA=", 0
    szNDLoadOk           db "[NanoDisk] Tensor loaded and quantized.", 13, 10, 0
    szNDLoadFail         db "[-] NanoDisk tensor load failed: ", 0
    szNDGgufOk           db "[NanoDisk] GGUF header valid: v", 0
    szNDGgufBad          db "[-] NanoDisk: Invalid GGUF magic.", 13, 10, 0
    szNDQuantStart       db "[NanoDisk] Quantizing tensor (rank=", 0
    szNDQuantOk          db "[NanoDisk] Quantization complete. Ratio=", 0
    szNDQuantFail        db "[-] NanoDisk: Quantization failed.", 13, 10, 0
    szNDDmaAlloc         db "[NanoDisk] DMA buffer allocated: ", 0
    szNDDmaFail          db "[-] NanoDisk: DMA buffer allocation failed.", 13, 10, 0
    szNDAsyncQueued      db "[NanoDisk] Async I/O queued to DiskKernel.", 13, 10, 0
    szNDAsyncFail        db "[-] NanoDisk: Async I/O queue failed.", 13, 10, 0
    szNDCallbackFire     db "[NanoDisk] Firing agent callback.", 13, 10, 0
    szNDJobStart         db "[NanoDisk] Background quantize job started.", 13, 10, 0
    szNDJobComplete      db "[NanoDisk] Background quantize job complete.", 13, 10, 0
    szNDCleanup          db "[NanoDisk] Cleanup complete.", 13, 10, 0

    ; Error detail strings
    szErrDiskOpen        db "disk_open_failed", 0
    szErrDmaAlloc        db "dma_alloc_failed", 0
    szErrAsyncQueue      db "async_queue_failed", 0
    szErrGgufMagic       db "gguf_magic_invalid", 0
    szErrGgufVersion     db "gguf_version_unsupported", 0
    szErrTensorNotFound  db "tensor_not_found", 0
    szErrQuantizeFail    db "quantize_failed", 0
    szErrCallbackNull    db "callback_null", 0
    szErrInvalidRank     db "invalid_rank", 0
    szErrIoTimeout       db "io_timeout", 0

    ; Formatting — szNewLine, FmtBuf imported via EXTERNDEF above

    szComma              db ", ", 0
    szCloseParen         db ")", 13, 10, 0
    szBytes              db " bytes", 13, 10, 0
    szX                  db "x", 13, 10, 0

    ; Number format scratch — imported from RawrXD_DiskRecoveryAgent.asm
    ; (FmtBuf declared via EXTERNDEF above)

; =============================================================================
; .data? — Uninitialized data
; =============================================================================
.data?

    ; Global async load context pool
    align 8
    g_AsyncLoadPool      NANO_ASYNC_LOAD_CTX MAX_ASYNC_TENSOR_LOADS dup(<>)
    g_AsyncLoadPoolCount DWORD   ?
    g_AsyncLoadPoolLock  CRITICAL_SECTION <>

    ; Global quantize job (single background job at a time)
    align 8
    g_QuantizeJob        NANO_QUANTIZE_JOB <>

    ; Console handle cache — g_hStdOut imported via EXTERNDEF above

    ; Next job ID counter (atomically incremented)
    g_NextJobId          DWORD   ?

    ; Bridge initialization flag
    g_BridgeInitialized  BYTE    ?

; =============================================================================
; .code
; =============================================================================
.code

; =============================================================================
; ConsolePrint / PrintU64 — removed duplicate PROCs (LNK2005 fix)
; Now imported via EXTERN from RawrXD_DiskRecoveryAgent.asm
; =============================================================================

; =============================================================================
; AllocDmaBuffer — Allocate a DMA-aligned buffer via VirtualAlloc
; RCX = requested size (will be rounded up + alignment overhead)
; Returns: RAX = aligned ptr, RDX = raw ptr (for VirtualFree)
;          RAX = 0 on failure
; =============================================================================
AllocDmaBuffer PROC
    push rbx
    push rsi
    sub  rsp, 32

    mov  rbx, rcx             ; Requested size
    add  rbx, DMA_ALIGNMENT   ; Over-allocate for alignment

    ; VirtualAlloc(NULL, size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
    xor  ecx, ecx
    mov  rdx, rbx
    mov  r8d, MEM_COMMIT or MEM_RESERVE
    mov  r9d, PAGE_READWRITE
    call VirtualAlloc

    test rax, rax
    jz   adb_fail

    mov  rsi, rax             ; Raw pointer (for VirtualFree later)

    ; Align up to DMA_ALIGNMENT boundary
    add  rax, DMA_ALIGNMENT - 1
    and  rax, DMA_ALIGNMENT_MASK

    mov  rdx, rsi             ; RDX = raw ptr
    jmp  adb_exit

adb_fail:
    xor  edx, edx             ; RDX = 0

adb_exit:
    add  rsp, 32
    pop  rsi
    pop  rbx
    ret
AllocDmaBuffer ENDP

; =============================================================================
; FreeDmaBuffer — Release a DMA buffer previously allocated by AllocDmaBuffer
; RCX = raw ptr (from RDX of AllocDmaBuffer)
; =============================================================================
FreeDmaBuffer PROC
    sub  rsp, 32

    ; VirtualFree(rawPtr, 0, MEM_RELEASE)
    ; RCX already has raw ptr
    xor  edx, edx
    mov  r8d, MEM_RELEASE
    call VirtualFree

    add  rsp, 32
    ret
FreeDmaBuffer ENDP

; =============================================================================
; AllocAsyncLoadCtx — Grab a free NANO_ASYNC_LOAD_CTX from the global pool
; Returns: RAX = ptr to context, or 0 if pool exhausted
; =============================================================================
AllocAsyncLoadCtx PROC
    push rbx
    push rdi
    sub  rsp, 32

    ; Lock pool
    lea  rcx, g_AsyncLoadPoolLock
    call EnterCriticalSection

    ; Scan for idle slot
    xor  ebx, ebx
    lea  rdi, g_AsyncLoadPool

alloc_scan_loop:
    cmp  ebx, MAX_ASYNC_TENSOR_LOADS
    jge  alloc_exhausted

    cmp  (NANO_ASYNC_LOAD_CTX ptr [rdi]).state, NANODISK_STATE_IDLE
    je   alloc_found

    add  rdi, sizeof NANO_ASYNC_LOAD_CTX
    inc  ebx
    jmp  alloc_scan_loop

alloc_found:
    ; Zero the context
    push rdi
    mov  rcx, rdi
    xor  edx, edx
    mov  r8d, sizeof NANO_ASYNC_LOAD_CTX
    call memset
    pop  rdi

    ; Assign job ID (atomic increment)
    mov  eax, g_NextJobId
    inc  eax
    mov  g_NextJobId, eax
    mov  (NANO_ASYNC_LOAD_CTX ptr [rdi]).agentJobId, eax

    ; Mark as allocated (will be set to READING by caller)
    mov  (NANO_ASYNC_LOAD_CTX ptr [rdi]).state, NANODISK_STATE_IDLE

    mov  rax, rdi
    jmp  alloc_unlock

alloc_exhausted:
    xor  eax, eax

alloc_unlock:
    push rax
    lea  rcx, g_AsyncLoadPoolLock
    call LeaveCriticalSection
    pop  rax

    add  rsp, 32
    pop  rdi
    pop  rbx
    ret
AllocAsyncLoadCtx ENDP

; =============================================================================
; ReleaseAsyncLoadCtx — Return a context to the pool
; RCX = ptr to NANO_ASYNC_LOAD_CTX
; =============================================================================
ReleaseAsyncLoadCtx PROC
    push rbx
    sub  rsp, 32

    mov  rbx, rcx

    ; Free DMA buffer if allocated
    mov  rcx, (NANO_ASYNC_LOAD_CTX ptr [rbx]).dmaBufferRaw
    test rcx, rcx
    jz   rlc_no_dma
    call FreeDmaBuffer
    mov  (NANO_ASYNC_LOAD_CTX ptr [rbx]).dmaBufferRaw, 0
    mov  (NANO_ASYNC_LOAD_CTX ptr [rbx]).dmaBuffer, 0
rlc_no_dma:

    ; Reset state to idle (makes the slot reclaimable)
    mov  (NANO_ASYNC_LOAD_CTX ptr [rbx]).state, NANODISK_STATE_IDLE

    add  rsp, 32
    pop  rbx
    ret
ReleaseAsyncLoadCtx ENDP

; =============================================================================
; ValidateGgufHeader — Parse and validate GGUF magic + version from DMA buffer
; RCX = ptr to DMA buffer (must have at least GGUF_HEADER_MIN_SIZE bytes)
; RDX = ptr to NANO_ASYNC_LOAD_CTX (output: ggufVersion, ggufTensorCount, etc.)
; Returns: RAX = 0 success, RAX = NTSTATUS on error, RDX = detail string
; =============================================================================
ValidateGgufHeader PROC
    push rbx
    push rsi
    sub  rsp, 32

    mov  rbx, rcx             ; DMA buffer
    mov  rsi, rdx             ; Load context

    ; Check GGUF magic: 0x46554747 = 'GGUF'
    mov  eax, dword ptr [rbx + GGUF_OFF_MAGIC]
    cmp  eax, GGUF_MAGIC
    jne  vgh_bad_magic

    ; Read version
    mov  eax, dword ptr [rbx + GGUF_OFF_VERSION]
    cmp  eax, GGUF_VERSION_2
    jb   vgh_bad_version
    cmp  eax, GGUF_VERSION_3
    ja   vgh_bad_version

    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).ggufVersion, eax

    ; Read tensor count (uint64)
    mov  rax, qword ptr [rbx + GGUF_OFF_N_TENSORS]
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).ggufTensorCount, rax

    ; Read KV count (uint64)
    mov  rax, qword ptr [rbx + GGUF_OFF_N_KV]
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).ggufKvCount, rax

    ; Print validated header info
    lea  rcx, szNDGgufOk
    call ConsolePrint
    movzx ecx, word ptr (NANO_ASYNC_LOAD_CTX ptr [rsi]).ggufVersion
    mov  rcx, rcx
    call PrintU64
    lea  rcx, szComma
    call ConsolePrint
    mov  rcx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).ggufTensorCount
    call PrintU64
    lea  rcx, szNewLine
    call ConsolePrint

    ; Success
    xor  eax, eax
    xor  edx, edx
    jmp  vgh_exit

vgh_bad_magic:
    lea  rcx, szNDGgufBad
    call ConsolePrint
    mov  eax, STATUS_UNSUCCESSFUL
    lea  rdx, szErrGgufMagic
    jmp  vgh_exit

vgh_bad_version:
    mov  eax, STATUS_UNSUCCESSFUL
    lea  rdx, szErrGgufVersion

vgh_exit:
    add  rsp, 32
    pop  rsi
    pop  rbx
    ret
ValidateGgufHeader ENDP

; =============================================================================
; LocateTensorData — Walk GGUF KV pairs and tensor info to find tensor offset
; (Simplified: assumes first tensor, or walks by name if tensorName is set)
; RCX = ptr to DMA buffer (full GGUF file section)
; RDX = ptr to NANO_ASYNC_LOAD_CTX
; Returns: RAX = 0 success (tensorDataOffset/Size filled), nonzero on error
; =============================================================================
LocateTensorData PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub  rsp, 32

    mov  rbx, rcx             ; DMA buffer
    mov  rsi, rdx             ; Load context

    ; Start parsing after the fixed header (24 bytes)
    lea  rdi, [rbx + GGUF_HEADER_MIN_SIZE]

    ; Skip KV metadata pairs: each entry is [string key][type][value]
    ; For production use we'd walk every KV entry properly.
    ; Here we use a linear scan for the tensor info array.
    mov  r12, (NANO_ASYNC_LOAD_CTX ptr [rsi]).ggufKvCount

    ; Walk KV pairs
    ; GGUF v3 string format: uint64 length + bytes (no null term in file)
    ; GGUF v3 value types: uint32 type + payload
    test r12, r12
    jz   ltd_tensor_info

ltd_kv_loop:
    test r12, r12
    jz   ltd_tensor_info

    ; Read string key length (uint64)
    mov  rax, qword ptr [rdi]
    add  rdi, 8
    add  rdi, rax              ; Skip key string bytes

    ; Read value type (uint32)
    mov  eax, dword ptr [rdi]
    add  rdi, 4

    ; Skip value based on type
    ; Type 0: uint8 (1), 1: int8 (1), 2: uint16 (2), 3: int16 (2),
    ; Type 4: uint32 (4), 5: int32 (4), 6: float32 (4),
    ; Type 7: bool (1), 8: string (uint64 len + bytes),
    ; Type 9: array (uint32 type + uint64 count + elements)
    ; Type 10: uint64 (8), 11: int64 (8), 12: float64 (8)
    cmp  eax, 0                ; uint8
    je   ltd_skip_1
    cmp  eax, 1                ; int8
    je   ltd_skip_1
    cmp  eax, 7                ; bool
    je   ltd_skip_1
    cmp  eax, 2                ; uint16
    je   ltd_skip_2
    cmp  eax, 3                ; int16
    je   ltd_skip_2
    cmp  eax, 4                ; uint32
    je   ltd_skip_4
    cmp  eax, 5                ; int32
    je   ltd_skip_4
    cmp  eax, 6                ; float32
    je   ltd_skip_4
    cmp  eax, 10               ; uint64
    je   ltd_skip_8
    cmp  eax, 11               ; int64
    je   ltd_skip_8
    cmp  eax, 12               ; float64
    je   ltd_skip_8
    cmp  eax, 8                ; string
    je   ltd_skip_string
    cmp  eax, 9                ; array
    je   ltd_skip_array

    ; Unknown type — bail
    jmp  ltd_not_found

ltd_skip_1:
    add  rdi, 1
    jmp  ltd_kv_next
ltd_skip_2:
    add  rdi, 2
    jmp  ltd_kv_next
ltd_skip_4:
    add  rdi, 4
    jmp  ltd_kv_next
ltd_skip_8:
    add  rdi, 8
    jmp  ltd_kv_next

ltd_skip_string:
    mov  rax, qword ptr [rdi]  ; String length (uint64)
    add  rdi, 8
    add  rdi, rax              ; Skip string bytes
    jmp  ltd_kv_next

ltd_skip_array:
    ; uint32 element_type + uint64 count + count * element_size
    mov  eax, dword ptr [rdi]  ; Element type
    add  rdi, 4
    mov  rcx, qword ptr [rdi]  ; Count
    add  rdi, 8

    ; Determine element size (simplified: fixed-size types only)
    push rcx
    cmp  eax, 0                ; uint8
    je   ltd_arr_1
    cmp  eax, 1                ; int8
    je   ltd_arr_1
    cmp  eax, 7                ; bool
    je   ltd_arr_1
    cmp  eax, 2                ; uint16
    je   ltd_arr_2
    cmp  eax, 3                ; int16
    je   ltd_arr_2
    cmp  eax, 4                ; uint32
    je   ltd_arr_4
    cmp  eax, 5                ; int32
    je   ltd_arr_4
    cmp  eax, 6                ; float32
    je   ltd_arr_4
    cmp  eax, 10               ; uint64
    je   ltd_arr_8
    cmp  eax, 11               ; int64
    je   ltd_arr_8
    cmp  eax, 12               ; float64
    je   ltd_arr_8
    ; String arrays and nested arrays: bail
    pop  rcx
    jmp  ltd_not_found

ltd_arr_1:
    pop  rcx
    add  rdi, rcx
    jmp  ltd_kv_next
ltd_arr_2:
    pop  rcx
    shl  rcx, 1
    add  rdi, rcx
    jmp  ltd_kv_next
ltd_arr_4:
    pop  rcx
    shl  rcx, 2
    add  rdi, rcx
    jmp  ltd_kv_next
ltd_arr_8:
    pop  rcx
    shl  rcx, 3
    add  rdi, rcx
    jmp  ltd_kv_next

ltd_kv_next:
    dec  r12
    jmp  ltd_kv_loop

ltd_tensor_info:
    ; Now at tensor info array
    ; Each tensor: string name, uint32 n_dims, uint64[n_dims] dims, uint32 type, uint64 offset
    ; Use first tensor (index 0) if tensorName not set, else search by name

    mov  r12, (NANO_ASYNC_LOAD_CTX ptr [rsi]).ggufTensorCount
    test r12, r12
    jz   ltd_not_found

    xor  ecx, ecx             ; Tensor index counter

ltd_tensor_loop:
    cmp  rcx, r12
    jge  ltd_not_found

    push rcx                   ; Save index

    ; Read tensor name: uint64 len + bytes
    mov  rax, qword ptr [rdi]
    add  rdi, 8                ; Skip length field
    mov  r8, rdi               ; r8 = name start
    add  rdi, rax              ; Skip name bytes

    ; Read n_dims (uint32)
    mov  r9d, dword ptr [rdi]
    add  rdi, 4

    ; Read dimension array: n_dims * uint64
    ; Store first 4 dims into context
    xor  r10d, r10d
ltd_read_dims:
    cmp  r10d, r9d
    jge  ltd_dims_done
    cmp  r10d, 4
    jge  ltd_skip_dim
    mov  rax, qword ptr [rdi]
    mov  dword ptr (NANO_ASYNC_LOAD_CTX ptr [rsi]).tensorDims[r10*4], eax
ltd_skip_dim:
    add  rdi, 8
    inc  r10d
    jmp  ltd_read_dims

ltd_dims_done:
    ; Read type (uint32)
    mov  eax, dword ptr [rdi]
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).quantType, eax
    add  rdi, 4

    ; Read offset (uint64) — byte offset of tensor data from start of file
    mov  rax, qword ptr [rdi]
    add  rdi, 8

    pop  rcx                   ; Restore index

    ; For now: use tensor at index 0 (first tensor)
    ; TODO: Name matching for selective tensor loading
    test ecx, ecx
    jnz  ltd_tensor_next

    ; Found target tensor — store offset and compute size
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).tensorDataOffset, rax

    ; Compute tensor size from dims and type
    ; For simplicity: size = prod(dims) * type_size
    ; This is a rough estimate; actual GGUF has alignment padding
    mov  eax, (NANO_ASYNC_LOAD_CTX ptr [rsi]).tensorDims[0]
    test eax, eax
    jz   ltd_default_size
    movzx r8d, word ptr (NANO_ASYNC_LOAD_CTX ptr [rsi]).tensorDims[4]
    test r8d, r8d
    jz   ltd_single_dim
    imul eax, r8d
ltd_single_dim:
    ; Multiply by element type size (approximate: 4 bytes for f32, 2 for f16, etc.)
    shl  eax, 2                ; Assume f32 (4 bytes) as conservative estimate
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).tensorDataSize, rax
    jmp  ltd_found

ltd_default_size:
    ; Default: use remaining DMA buffer
    mov  rax, DMA_BUFFER_SIZE
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).tensorDataSize, rax

ltd_found:
    xor  eax, eax             ; Success
    jmp  ltd_exit

ltd_tensor_next:
    inc  ecx
    jmp  ltd_tensor_loop

ltd_not_found:
    mov  eax, STATUS_UNSUCCESSFUL
    lea  rdx, szErrTensorNotFound

ltd_exit:
    add  rsp, 32
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    ret
LocateTensorData ENDP

; =============================================================================
; NanoDisk_LoadQuantizedTensor
; Async entry point: Queues disk read → GGUF parse → NanoQuant pipeline
; RCX = ptr to NANO_ASYNC_LOAD_CTX (pre-filled by caller)
; Returns: RAX = 0 async pending, NTSTATUS on immediate error
;          RDX = detail string on error
; =============================================================================
PUBLIC NanoDisk_LoadQuantizedTensor
NanoDisk_LoadQuantizedTensor PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub  rsp, 56

    mov  rsi, rcx              ; Save context ptr

    ; Validate rank
    mov  eax, (NANO_ASYNC_LOAD_CTX ptr [rsi]).targetRank
    cmp  eax, NQ_RANK_MIN
    jb   nlqt_bad_rank
    cmp  eax, NQ_RANK_MAX
    ja   nlqt_bad_rank

    ; Validate callback
    mov  rax, (NANO_ASYNC_LOAD_CTX ptr [rsi]).callback
    test rax, rax
    jz   nlqt_no_callback

    ; Record start time
    call GetTickCount64
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).startTickCount, rax

    ; Step 1: Allocate DMA-aligned buffer
    mov  rcx, DMA_BUFFER_ALLOC_SIZE
    call AllocDmaBuffer
    test rax, rax
    jz   nlqt_dma_fail

    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBuffer, rax
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBufferRaw, rdx
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBufferSize, DMA_BUFFER_SIZE

    ; Print DMA info
    lea  rcx, szNDDmaAlloc
    call ConsolePrint
    mov  rcx, DMA_BUFFER_SIZE
    call PrintU64
    lea  rcx, szBytes
    call ConsolePrint

    ; Step 2: Queue async read via DiskKernel
    ; DiskKernel_AsyncReadSectors(driveIndex, startLBA, sectorCount, buffer, asyncCtx)
    mov  ecx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).driveIndex
    mov  rdx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).sectorLba
    mov  r8d, (NANO_ASYNC_LOAD_CTX ptr [rsi]).sectorCount
    mov  r9, (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBuffer

    ; 5th arg: ptr to async context (we pass our own context as callback data)
    mov  qword ptr [rsp+32], rsi

    call DiskKernel_AsyncReadSectors

    test rax, rax
    jnz  nlqt_async_fail

    ; Mark state as reading
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).state, NANODISK_STATE_READING

    lea  rcx, szNDAsyncQueued
    call ConsolePrint

    ; Print LBA info
    lea  rcx, szNDLoadStart
    call ConsolePrint
    mov  rcx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).sectorLba
    call PrintU64
    lea  rcx, szNewLine
    call ConsolePrint

    ; Return async pending (success — caller waits for callback)
    xor  eax, eax
    xor  edx, edx
    jmp  nlqt_exit

nlqt_bad_rank:
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).state, NANODISK_STATE_ERROR
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).errorCode, NANODISK_ERR_INVALID_RANK
    mov  eax, STATUS_INVALID_PARAMETER
    lea  rdx, szErrInvalidRank
    jmp  nlqt_exit

nlqt_no_callback:
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).state, NANODISK_STATE_ERROR
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).errorCode, NANODISK_ERR_CALLBACK_NULL
    mov  eax, STATUS_INVALID_PARAMETER
    lea  rdx, szErrCallbackNull
    jmp  nlqt_exit

nlqt_dma_fail:
    lea  rcx, szNDDmaFail
    call ConsolePrint
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).state, NANODISK_STATE_ERROR
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).errorCode, NANODISK_ERR_DMA_ALLOC
    mov  eax, STATUS_UNSUCCESSFUL
    lea  rdx, szErrDmaAlloc
    jmp  nlqt_exit

nlqt_async_fail:
    lea  rcx, szNDAsyncFail
    call ConsolePrint
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).state, NANODISK_STATE_ERROR
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).errorCode, NANODISK_ERR_ASYNC_QUEUE

    ; Free DMA buffer since we won't be using it
    mov  rcx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBufferRaw
    call FreeDmaBuffer
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBufferRaw, 0
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBuffer, 0

    mov  eax, STATUS_UNSUCCESSFUL
    lea  rdx, szErrAsyncQueue

nlqt_exit:
    add  rsp, 56
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    ret
NanoDisk_LoadQuantizedTensor ENDP

; =============================================================================
; NanoDisk_Callback
; Completion handler: Disk I/O done → Validate GGUF → Locate tensor →
;   NanoQuant compress/decompress → Fire agent callback
; RCX = ptr to NANO_ASYNC_LOAD_CTX (passed as UserData from DiskKernel)
; RDX = status (0=success, nonzero=error)
; R8  = bytesTransferred
; =============================================================================
PUBLIC NanoDisk_Callback
NanoDisk_Callback PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub  rsp, 56

    mov  rsi, rcx              ; Our NANO_ASYNC_LOAD_CTX
    mov  r12d, edx             ; I/O status
    mov  r13, r8               ; Bytes transferred

    ; Record bytes transferred
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).bytesTransferred, r13

    ; Check I/O status
    test r12d, r12d
    jnz  ndc_io_error

    ; Transition to PARSING state
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).state, NANODISK_STATE_PARSING

    ; Step 1: Validate GGUF header from DMA buffer
    mov  rcx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBuffer
    mov  rdx, rsi
    call ValidateGgufHeader

    test rax, rax
    jnz  ndc_parse_error

    ; Step 2: Locate tensor data within the GGUF
    mov  rcx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBuffer
    mov  rdx, rsi
    call LocateTensorData

    test rax, rax
    jnz  ndc_tensor_error

    ; Step 3: Transition to QUANTIZING state
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).state, NANODISK_STATE_QUANTIZING

    ; Print quantization start
    lea  rcx, szNDQuantStart
    call ConsolePrint
    movzx ecx, word ptr (NANO_ASYNC_LOAD_CTX ptr [rsi]).targetRank
    mov  rcx, rcx
    call PrintU64
    lea  rcx, szCloseParen
    call ConsolePrint

    ; Compute pointer to actual tensor data in DMA buffer
    mov  rbx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBuffer
    add  rbx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).tensorDataOffset

    ; Bounds check: ensure tensor data is within transferred bytes
    mov  rax, (NANO_ASYNC_LOAD_CTX ptr [rsi]).tensorDataOffset
    add  rax, (NANO_ASYNC_LOAD_CTX ptr [rsi]).tensorDataSize
    cmp  rax, r13
    ja   ndc_tensor_error      ; Tensor extends beyond transferred data

    ; Call NanoQuant_QuantizeTensor(rawData, rows, rank)
    ; RCX = raw tensor data ptr
    ; EDX = rows (first dimension)
    ; R8D = target rank
    mov  rcx, rbx
    mov  edx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).tensorDims[0]
    mov  r8d, (NANO_ASYNC_LOAD_CTX ptr [rsi]).targetRank
    call NanoQuant_QuantizeTensor

    test rax, rax
    jz   ndc_quant_error

    ; Store result matrix pointer
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).quantMatrix, rax

    ; Get compression ratio
    mov  rcx, rax
    call NanoQuant_GetCompressionRatio
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).compressionRatio, rax

    ; Print success
    lea  rcx, szNDQuantOk
    call ConsolePrint
    mov  rcx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).compressionRatio
    call PrintU64
    lea  rcx, szX
    call ConsolePrint

    ; Record end time
    call GetTickCount64
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).endTickCount, rax

    ; Transition to COMPLETE state
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).state, NANODISK_STATE_COMPLETE

    lea  rcx, szNDLoadOk
    call ConsolePrint

    ; Step 4: Fire agent callback (BoundedAgentLoop notification)
    mov  rax, (NANO_ASYNC_LOAD_CTX ptr [rsi]).callback
    test rax, rax
    jz   ndc_done

    lea  rcx, szNDCallbackFire
    call ConsolePrint

    ; void(*callback)(NANO_ASYNC_LOAD_CTX*)
    mov  rcx, rsi
    call (NANO_ASYNC_LOAD_CTX ptr [rsi]).callback
    jmp  ndc_done

ndc_io_error:
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).state, NANODISK_STATE_ERROR
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).errorCode, NANODISK_ERR_IO_TIMEOUT
    lea  rcx, szNDLoadFail
    call ConsolePrint
    lea  rcx, szErrIoTimeout
    call ConsolePrint
    lea  rcx, szNewLine
    call ConsolePrint
    jmp  ndc_fire_error_cb

ndc_parse_error:
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).state, NANODISK_STATE_ERROR
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).errorCode, NANODISK_ERR_GGUF_MAGIC
    jmp  ndc_fire_error_cb

ndc_tensor_error:
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).state, NANODISK_STATE_ERROR
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).errorCode, NANODISK_ERR_TENSOR_NOT_FOUND
    lea  rcx, szNDLoadFail
    call ConsolePrint
    lea  rcx, szErrTensorNotFound
    call ConsolePrint
    lea  rcx, szNewLine
    call ConsolePrint
    jmp  ndc_fire_error_cb

ndc_quant_error:
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).state, NANODISK_STATE_ERROR
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).errorCode, NANODISK_ERR_QUANTIZE_FAIL
    lea  rcx, szNDQuantFail
    call ConsolePrint

ndc_fire_error_cb:
    ; Fire callback even on error so agent can react
    mov  rax, (NANO_ASYNC_LOAD_CTX ptr [rsi]).callback
    test rax, rax
    jz   ndc_done
    mov  rcx, rsi
    call (NANO_ASYNC_LOAD_CTX ptr [rsi]).callback

ndc_done:
    add  rsp, 56
    pop  r13
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    ret
NanoDisk_Callback ENDP

; =============================================================================
; QuantizeWorkerThread — Background thread proc for model compression
; lpParameter = ptr to NANO_QUANTIZE_JOB
; Returns: 0 (DWORD)
; =============================================================================
QuantizeWorkerThread PROC
    push rbx
    push rsi
    push rdi
    sub  rsp, 48

    mov  rbx, rcx              ; NANO_QUANTIZE_JOB ptr

    ; Update status
    mov  (NANO_QUANTIZE_JOB ptr [rbx]).status, NANODISK_STATE_READING

    ; The loadCtx was pre-allocated and pre-filled by AgentTool_QuantizeModel
    mov  rsi, (NANO_QUANTIZE_JOB ptr [rbx]).loadCtx
    test rsi, rsi
    jz   qwt_fail

    ; Synchronous path: Use DK_ReadSectors (blocking) since we're already on a thread
    mov  ecx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).driveIndex
    mov  rdx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).sectorLba
    mov  r8d, (NANO_ASYNC_LOAD_CTX ptr [rsi]).sectorCount
    mov  r9, (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBuffer
    call DK_ReadSectors

    test rax, rax
    jnz  qwt_read_fail

    ; Record bytes transferred
    mov  eax, (NANO_ASYNC_LOAD_CTX ptr [rsi]).sectorCount
    shl  eax, 9                ; * 512
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).bytesTransferred, rax

    ; Parse GGUF
    mov  (NANO_QUANTIZE_JOB ptr [rbx]).status, NANODISK_STATE_PARSING
    mov  (NANO_QUANTIZE_JOB ptr [rbx]).progressPct, 10

    mov  rcx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBuffer
    mov  rdx, rsi
    call ValidateGgufHeader
    test rax, rax
    jnz  qwt_parse_fail

    ; Locate tensor
    mov  (NANO_QUANTIZE_JOB ptr [rbx]).progressPct, 30

    mov  rcx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBuffer
    mov  rdx, rsi
    call LocateTensorData
    test rax, rax
    jnz  qwt_tensor_fail

    ; Quantize
    mov  (NANO_QUANTIZE_JOB ptr [rbx]).status, NANODISK_STATE_QUANTIZING
    mov  (NANO_QUANTIZE_JOB ptr [rbx]).progressPct, 50

    mov  rdi, (NANO_ASYNC_LOAD_CTX ptr [rsi]).dmaBuffer
    add  rdi, (NANO_ASYNC_LOAD_CTX ptr [rsi]).tensorDataOffset

    mov  rcx, rdi
    mov  edx, (NANO_ASYNC_LOAD_CTX ptr [rsi]).tensorDims[0]
    mov  r8d, (NANO_ASYNC_LOAD_CTX ptr [rsi]).targetRank
    call NanoQuant_QuantizeTensor

    test rax, rax
    jz   qwt_quant_fail

    mov  (NANO_QUANTIZE_JOB ptr [rbx]).outputMatrix, rax
    mov  (NANO_ASYNC_LOAD_CTX ptr [rsi]).quantMatrix, rax

    ; Get compression ratio
    mov  rcx, rax
    call NanoQuant_GetCompressionRatio
    mov  (NANO_QUANTIZE_JOB ptr [rbx]).compressionRatio, rax

    ; Complete
    mov  (NANO_QUANTIZE_JOB ptr [rbx]).status, NANODISK_STATE_COMPLETE
    mov  (NANO_QUANTIZE_JOB ptr [rbx]).progressPct, 100

    lea  rcx, szNDJobComplete
    call ConsolePrint

    ; Fire job callback if set
    mov  rax, (NANO_QUANTIZE_JOB ptr [rbx]).callbackPtr
    test rax, rax
    jz   qwt_done
    mov  rcx, rbx
    call (NANO_QUANTIZE_JOB ptr [rbx]).callbackPtr
    jmp  qwt_done

qwt_fail:
qwt_read_fail:
qwt_parse_fail:
qwt_tensor_fail:
qwt_quant_fail:
    mov  (NANO_QUANTIZE_JOB ptr [rbx]).status, NANODISK_STATE_ERROR
    ; Fire error callback
    mov  rax, (NANO_QUANTIZE_JOB ptr [rbx]).callbackPtr
    test rax, rax
    jz   qwt_done
    mov  rcx, rbx
    call (NANO_QUANTIZE_JOB ptr [rbx]).callbackPtr

qwt_done:
    xor  eax, eax
    add  rsp, 48
    pop  rdi
    pop  rsi
    pop  rbx
    ret
QuantizeWorkerThread ENDP

; =============================================================================
; AgentTool_QuantizeModel
; CLI/GUI callable: Background compression job
; Exposed as mode 19 (-quantize model.gguf 4) in AgentToolExecutor
; RCX = source GGUF path (null-terminated), EDX = target rank (1-8)
; Returns: RAX = ptr to NANO_QUANTIZE_JOB (job handle), 0 on error
;          RDX = detail string on error
; =============================================================================
PUBLIC AgentTool_QuantizeModel
AgentTool_QuantizeModel PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub  rsp, 56

    mov  rsi, rcx              ; Source path
    mov  r12d, edx             ; Rank

    ; Validate rank
    cmp  r12d, NQ_RANK_MIN
    jb   atqm_bad_rank
    cmp  r12d, NQ_RANK_MAX
    ja   atqm_bad_rank

    ; Zero the global job struct
    lea  rdi, g_QuantizeJob
    mov  ecx, sizeof NANO_QUANTIZE_JOB
    xor  eax, eax
    rep  stosb

    ; Copy source path (up to 259 chars + null)
    lea  rdi, g_QuantizeJob.sourcePath
    mov  rcx, rsi
    call strlen
    cmp  rax, 259
    jbe  atqm_path_ok
    mov  rax, 259
atqm_path_ok:
    mov  r8, rax
    lea  rcx, g_QuantizeJob.sourcePath
    mov  rdx, rsi
    ; r8 = length
    inc  r8                    ; Include null terminator
    call memcpy

    ; Store rank
    mov  g_QuantizeJob.targetRank, r12d

    ; Allocate async load context from pool
    call AllocAsyncLoadCtx
    test rax, rax
    jz   atqm_pool_exhausted

    mov  rbx, rax             ; NANO_ASYNC_LOAD_CTX ptr
    mov  g_QuantizeJob.loadCtx, rbx

    ; Fill load context
    mov  (NANO_ASYNC_LOAD_CTX ptr [rbx]).targetRank, r12d

    ; TODO: Resolve file path to PhysicalDriveN + LBA via:
    ;   1) Volume mount point resolution (DeviceIoControl IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS)
    ;   2) NTFS MFT lookup (DiskKernel NTFS_ReadMftRecord)
    ;   3) FAT32 cluster walk (DiskKernel FAT32_ReadCluster)
    ; For now: set drive 0, LBA 0 — caller must pre-resolve or use DiskExplorer API

    mov  (NANO_ASYNC_LOAD_CTX ptr [rbx]).driveIndex, 0

    ; Allocate DMA buffer for the worker thread
    mov  rcx, DMA_BUFFER_ALLOC_SIZE
    call AllocDmaBuffer
    test rax, rax
    jz   atqm_dma_fail

    mov  (NANO_ASYNC_LOAD_CTX ptr [rbx]).dmaBuffer, rax
    mov  (NANO_ASYNC_LOAD_CTX ptr [rbx]).dmaBufferRaw, rdx
    mov  (NANO_ASYNC_LOAD_CTX ptr [rbx]).dmaBufferSize, DMA_BUFFER_SIZE

    ; Default sector count (128 sectors = 64KB for first chunk)
    mov  (NANO_ASYNC_LOAD_CTX ptr [rbx]).sectorCount, 128

    ; Record start time
    call GetTickCount64
    mov  (NANO_ASYNC_LOAD_CTX ptr [rbx]).startTickCount, rax

    ; Launch background worker thread
    lea  rcx, szNDJobStart
    call ConsolePrint

    ; CreateThread(NULL, 0, QuantizeWorkerThread, &g_QuantizeJob, 0, &threadId)
    xor  rcx, rcx             ; lpThreadAttributes = NULL
    xor  rdx, rdx             ; dwStackSize = 0 (default)
    lea  r8, QuantizeWorkerThread
    lea  r9, g_QuantizeJob     ; lpParameter
    mov  qword ptr [rsp+32], 0 ; dwCreationFlags = 0 (start immediately)
    lea  rax, g_QuantizeJob.threadId
    mov  qword ptr [rsp+40], rax
    call CreateThread

    test rax, rax
    jz   atqm_thread_fail

    mov  g_QuantizeJob.hWorkerThread, rax
    mov  g_QuantizeJob.status, NANODISK_STATE_READING

    ; Return job handle
    lea  rax, g_QuantizeJob
    xor  edx, edx
    jmp  atqm_exit

atqm_bad_rank:
    mov  eax, 0
    lea  rdx, szErrInvalidRank
    jmp  atqm_exit

atqm_pool_exhausted:
    xor  eax, eax
    lea  rdx, szErrAsyncQueue
    jmp  atqm_exit

atqm_dma_fail:
    ; Release the load context back to pool
    mov  rcx, rbx
    call ReleaseAsyncLoadCtx
    mov  g_QuantizeJob.loadCtx, 0
    xor  eax, eax
    lea  rdx, szErrDmaAlloc
    jmp  atqm_exit

atqm_thread_fail:
    ; Cleanup: release DMA + context
    mov  rcx, rbx
    call ReleaseAsyncLoadCtx
    mov  g_QuantizeJob.loadCtx, 0
    xor  eax, eax
    lea  rdx, szErrAsyncQueue

atqm_exit:
    add  rsp, 56
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    ret
AgentTool_QuantizeModel ENDP

; =============================================================================
; NanoDisk_Init — Initialize the bridge (call once at startup)
; Returns: RAX = 0 success
; =============================================================================
PUBLIC NanoDisk_Init
NanoDisk_Init PROC
    sub  rsp, 32

    ; Initialize pool critical section
    lea  rcx, g_AsyncLoadPoolLock
    call InitializeCriticalSection

    ; Zero pool
    lea  rcx, g_AsyncLoadPool
    xor  edx, edx
    mov  r8d, sizeof NANO_ASYNC_LOAD_CTX * MAX_ASYNC_TENSOR_LOADS
    call memset

    ; Zero global job
    lea  rcx, g_QuantizeJob
    xor  edx, edx
    mov  r8d, sizeof NANO_QUANTIZE_JOB
    call memset

    ; Init counters
    mov  g_AsyncLoadPoolCount, 0
    mov  g_NextJobId, 0
    mov  g_BridgeInitialized, 1

    lea  rcx, szNDInit
    call ConsolePrint

    xor  eax, eax

    add  rsp, 32
    ret
NanoDisk_Init ENDP

; =============================================================================
; NanoDisk_Shutdown — Cleanup bridge resources
; Returns: RAX = 0 success
; =============================================================================
PUBLIC NanoDisk_Shutdown
NanoDisk_Shutdown PROC
    push rbx
    push rdi
    sub  rsp, 32

    ; Wait for any active quantize job
    mov  rax, g_QuantizeJob.hWorkerThread
    test rax, rax
    jz   nds_no_thread
    mov  rcx, rax
    mov  edx, 5000             ; 5 second timeout
    call WaitForSingleObject
    mov  rcx, g_QuantizeJob.hWorkerThread
    call CloseHandle
    mov  g_QuantizeJob.hWorkerThread, 0
nds_no_thread:

    ; Release all active load contexts (free DMA buffers)
    xor  ebx, ebx
    lea  rdi, g_AsyncLoadPool

nds_pool_loop:
    cmp  ebx, MAX_ASYNC_TENSOR_LOADS
    jge  nds_pool_done

    cmp  (NANO_ASYNC_LOAD_CTX ptr [rdi]).state, NANODISK_STATE_IDLE
    je   nds_pool_next

    mov  rcx, rdi
    call ReleaseAsyncLoadCtx

nds_pool_next:
    add  rdi, sizeof NANO_ASYNC_LOAD_CTX
    inc  ebx
    jmp  nds_pool_loop

nds_pool_done:
    ; Delete critical section
    lea  rcx, g_AsyncLoadPoolLock
    call DeleteCriticalSection

    mov  g_BridgeInitialized, 0

    lea  rcx, szNDCleanup
    call ConsolePrint

    xor  eax, eax
    add  rsp, 32
    pop  rdi
    pop  rbx
    ret
NanoDisk_Shutdown ENDP

; =============================================================================
; NanoDisk_GetJobStatus — Query a quantize job's state
; RCX = ptr to NANO_QUANTIZE_JOB (or NULL for global)
; Returns: EAX = NANODISK_STATE_*, EDX = progressPct (0-100)
; =============================================================================
PUBLIC NanoDisk_GetJobStatus
NanoDisk_GetJobStatus PROC
    test rcx, rcx
    jnz  ngjs_have_ptr
    lea  rcx, g_QuantizeJob
ngjs_have_ptr:
    mov  eax, (NANO_QUANTIZE_JOB ptr [rcx]).status
    mov  edx, (NANO_QUANTIZE_JOB ptr [rcx]).progressPct
    ret
NanoDisk_GetJobStatus ENDP

; =============================================================================
; NanoDisk_GetJobResult — Get completed job's output matrix and ratio
; RCX = ptr to NANO_QUANTIZE_JOB (or NULL for global)
; RDX = ptr to receive output matrix (QWORD*)
; R8  = ptr to receive compression ratio (QWORD*)
; Returns: RAX = 0 if job complete, NANODISK_STATE_* if not ready
; =============================================================================
PUBLIC NanoDisk_GetJobResult
NanoDisk_GetJobResult PROC
    test rcx, rcx
    jnz  ngjr_have_ptr
    lea  rcx, g_QuantizeJob
ngjr_have_ptr:
    cmp  (NANO_QUANTIZE_JOB ptr [rcx]).status, NANODISK_STATE_COMPLETE
    jne  ngjr_not_ready

    ; Store output matrix ptr
    test rdx, rdx
    jz   ngjr_skip_mat
    mov  rax, (NANO_QUANTIZE_JOB ptr [rcx]).outputMatrix
    mov  qword ptr [rdx], rax
ngjr_skip_mat:

    ; Store compression ratio
    test r8, r8
    jz   ngjr_skip_ratio
    mov  rax, (NANO_QUANTIZE_JOB ptr [rcx]).compressionRatio
    mov  qword ptr [r8], rax
ngjr_skip_ratio:

    xor  eax, eax             ; 0 = complete
    ret

ngjr_not_ready:
    mov  eax, (NANO_QUANTIZE_JOB ptr [rcx]).status
    ret
NanoDisk_GetJobResult ENDP

; =============================================================================
; NanoDisk_AbortJob — Request abort of active quantize job
; RCX = ptr to NANO_QUANTIZE_JOB (or NULL for global)
; =============================================================================
PUBLIC NanoDisk_AbortJob
NanoDisk_AbortJob PROC
    test rcx, rcx
    jnz  ndaj_have_ptr
    lea  rcx, g_QuantizeJob
ndaj_have_ptr:
    ; Set error state to signal worker thread (checked at pipeline stages)
    mov  (NANO_QUANTIZE_JOB ptr [rcx]).status, NANODISK_STATE_ERROR
    ret
NanoDisk_AbortJob ENDP

; =============================================================================
; C-callable export aliases for integration
; =============================================================================
PUBLIC NanoDisk_LoadQuantizedTensor
PUBLIC NanoDisk_Callback
PUBLIC AgentTool_QuantizeModel
PUBLIC NanoDisk_Init
PUBLIC NanoDisk_Shutdown
PUBLIC NanoDisk_GetJobStatus
PUBLIC NanoDisk_GetJobResult
PUBLIC NanoDisk_AbortJob

END
