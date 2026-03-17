; ============================================================================
; RawrXD Agentic IDE - GGUF Streaming Module
; Real-time streaming for GGUF model loading with progress tracking
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; External globals
extern g_hMainWindow:DWORD
extern g_hInstance:DWORD

; External performance metrics (with prototypes for stdcall)
PerfMetrics_AddBytes proto :DWORD
PerfMetrics_Update proto
BeaconCallback proto :DWORD, :DWORD, :DWORD, :DWORD, :DWORD

; ============================================================================
; Constants
; ============================================================================

; Streaming buffer sizes
STREAM_BUFFER_SIZE      equ 1048576     ; 1MB streaming buffer
STREAM_CHUNK_SIZE       equ 65536       ; 64KB read chunks
STREAM_PREFETCH_SIZE    equ 4194304     ; 4MB prefetch window

; Streaming states
STREAM_STATE_IDLE       equ 0
STREAM_STATE_OPENING    equ 1
STREAM_STATE_STREAMING  equ 2
STREAM_STATE_PAUSED     equ 3
STREAM_STATE_COMPLETE   equ 4
STREAM_STATE_ERROR      equ 5

; Memory mapping flags
MAP_SEQUENTIAL          equ 1
MAP_RANDOM              equ 2
MAP_STREAMING           equ 3

; ============================================================================
; Structures
; ============================================================================

STREAM_CONTEXT struct
    ; File info
    hFile               dd ?            ; File handle
    hFileMapping        dd ?            ; File mapping handle
    pMappedBase         dd ?            ; Base address of mapped view
    qwFileSize          dq ?            ; Total file size
    qwBytesRead         dq ?            ; Bytes read so far
    
    ; Streaming state
    dwState             dd ?            ; Current streaming state
    dwProgress          dd ?            ; Progress percentage (0-100)
    
    ; Buffer management
    pStreamBuffer       dd ?            ; Streaming buffer
    dwBufferSize        dd ?            ; Buffer size
    dwBufferOffset      dd ?            ; Current offset in buffer
    dwBufferValid       dd ?            ; Valid bytes in buffer
    
    ; Prefetch
    qwPrefetchStart     dq ?            ; Prefetch window start
    qwPrefetchEnd       dq ?            ; Prefetch window end
    
    ; Async I/O
    overlapped          OVERLAPPED <>   ; For async reads
    hEvent              dd ?            ; Completion event
    
    ; Timing
    qwStartTime         dq ?            ; Stream start time
    qwLastReadTime      dq ?            ; Last read completion time
    dwReadLatencyUs     dd ?            ; Average read latency
    
    ; Callbacks
    pfnProgress         dd ?            ; Progress callback
    pfnComplete         dd ?            ; Completion callback
    pfnError            dd ?            ; Error callback
    pUserData           dd ?            ; User data for callbacks
STREAM_CONTEXT ends

STREAM_STATS struct
    qwTotalBytes        dq ?            ; Total bytes streamed
    qwTotalTime         dq ?            ; Total streaming time
    dwAvgBitrateMbps    dd ?            ; Average bitrate
    dwPeakBitrateMbps   dd ?            ; Peak bitrate
    dwChunksRead        dd ?            ; Number of chunks read
    dwCacheHits         dd ?            ; Cache hits
    dwCacheMisses       dd ?            ; Cache misses
STREAM_STATS ends

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    ; Status messages
    szStreamOpening     db "Opening file for streaming...", 0
    szStreamStarted     db "Streaming started", 0
    szStreamProgress    db "Streaming: %d%% (%d MB / %d MB)", 0
    szStreamComplete    db "Streaming complete", 0
    szStreamError       db "Streaming error: %s", 0
    szStreamPaused      db "Streaming paused", 0
    szStreamResumed     db "Streaming resumed", 0
    
    ; Error messages
    szErrOpenFile       db "Failed to open file", 0
    szErrCreateMapping  db "Failed to create file mapping", 0
    szErrMapView        db "Failed to map view of file", 0
    szErrAllocBuffer    db "Failed to allocate streaming buffer", 0
    szErrRead           db "Read error", 0
    
    ; Global stream context
    g_StreamContext     STREAM_CONTEXT <>
    g_StreamStats       STREAM_STATS <>
    
    ; Initialized flag
    g_bStreamInitialized dd 0

.data?
    ; Streaming buffer (allocated dynamically)
    g_pStreamBuffer     dd ?

    ; Optional beacon/wirecap hooks
    g_pBeaconCallback   dd ?
    g_pBeaconUserData   dd ?
    g_hWirecapFile      dd ?
    
    ; Temporary buffer for formatting
    szStreamBuffer      db 256 dup(?)

; ============================================================================
; CODE SECTION
; ============================================================================

.code

public GGUFStream_Init
public GGUFStream_Open
public GGUFStream_Read
public GGUFStream_Seek
public GGUFStream_Close
public GGUFStream_GetProgress
public GGUFStream_GetStats
public GGUFStream_Pause
public GGUFStream_Resume
public GGUFStream_SetBeaconCallback
public GGUFStream_DisableBeacon
public GGUFStream_EnableWirecap
public GGUFStream_DisableWirecap

; ============================================================================
; GGUFStream_Init - Initialize streaming system
; Returns: TRUE on success
; ============================================================================
GGUFStream_Init proc
    ; Allocate streaming buffer
    push STREAM_BUFFER_SIZE
    push 0
    push HEAP_ZERO_MEMORY
    call GetProcessHeap
    push eax
    call HeapAlloc
    mov g_pStreamBuffer, eax
    test eax, eax
    jz @fail
    
    ; Initialize context
    lea edi, g_StreamContext
    mov ecx, sizeof STREAM_CONTEXT
    xor eax, eax
    rep stosb
    
    ; Initialize stats
    lea edi, g_StreamStats
    mov ecx, sizeof STREAM_STATS
    xor eax, eax
    rep stosb
    
    ; Store buffer pointer
    mov eax, g_pStreamBuffer
    mov g_StreamContext.pStreamBuffer, eax
    mov g_StreamContext.dwBufferSize, STREAM_BUFFER_SIZE
    mov g_StreamContext.dwState, STREAM_STATE_IDLE

    ; Reset beacon/wirecap hooks
    mov g_pBeaconCallback, 0
    mov g_pBeaconUserData, 0
    mov g_hWirecapFile, 0
    
    ; Create completion event
    push 0
    push TRUE
    push FALSE
    push 0
    call CreateEventA
    mov g_StreamContext.hEvent, eax
    
    mov g_bStreamInitialized, 1
    mov eax, TRUE
    ret

@fail:
    xor eax, eax
    ret
GGUFStream_Init endp

; ============================================================================
; GGUFStream_Open - Open a file for streaming
; Input: pszFilePath - path to GGUF file
; Returns: TRUE on success
; ============================================================================
GGUFStream_Open proc pszFilePath:DWORD
    LOCAL dwFileSizeHigh:DWORD
    
    cmp g_bStreamInitialized, 0
    je @fail
    
    ; Close any existing stream
    cmp g_StreamContext.hFile, 0
    je @openNew
    push g_StreamContext.hFile
    call GGUFStream_Close
    
@openNew:
    mov g_StreamContext.dwState, STREAM_STATE_OPENING
    
    ; Open file with sequential scan optimization and overlapped I/O
    push 0
    push FILE_FLAG_SEQUENTIAL_SCAN or FILE_FLAG_OVERLAPPED
    push OPEN_EXISTING
    push 0
    push FILE_SHARE_READ
    push GENERIC_READ
    push pszFilePath
    call CreateFileA
    cmp eax, INVALID_HANDLE_VALUE
    je @try_nonoverlapped
    mov g_StreamContext.hFile, eax
    jmp @continue

@try_nonoverlapped:
    ; Fallback to normal read (no overlapped)
    push 0
    push FILE_FLAG_SEQUENTIAL_SCAN
    push OPEN_EXISTING
    push 0
    push FILE_SHARE_READ
    push GENERIC_READ
    push pszFilePath
    call CreateFileA
    cmp eax, INVALID_HANDLE_VALUE
    je @fail
    mov g_StreamContext.hFile, eax

@continue:
    
    ; Get file size
    lea edx, dwFileSizeHigh
    push edx
    push eax
    call GetFileSize
    mov dword ptr g_StreamContext.qwFileSize, eax
    mov eax, dwFileSizeHigh
    mov dword ptr g_StreamContext.qwFileSize+4, eax
    
    ; Create file mapping for large files
    mov eax, dword ptr g_StreamContext.qwFileSize+4
    cmp eax, 0
    jne @useMapping
    mov eax, dword ptr g_StreamContext.qwFileSize
    cmp eax, 100000000       ; 100MB threshold
    jb @noMapping
    
@useMapping:
    ; Create file mapping
    push 0
    push 0
    push 0
    push PAGE_READONLY
    push 0
    push g_StreamContext.hFile
    call CreateFileMappingA
    test eax, eax
    jz @noMapping
    mov g_StreamContext.hFileMapping, eax
    
    ; Map view of file
    push 0
    push 0
    push 0
    push FILE_MAP_READ
    push g_StreamContext.hFileMapping
    call MapViewOfFile
    test eax, eax
    jz @noMapping
    mov g_StreamContext.pMappedBase, eax
    
@noMapping:
    ; Initialize streaming state
    mov g_StreamContext.dwState, STREAM_STATE_STREAMING
    mov g_StreamContext.dwProgress, 0
    mov dword ptr g_StreamContext.qwBytesRead, 0
    mov dword ptr g_StreamContext.qwBytesRead+4, 0
    mov g_StreamContext.dwBufferOffset, 0
    mov g_StreamContext.dwBufferValid, 0
    
    ; Record start time
    lea eax, g_StreamContext.qwStartTime
    push eax
    call QueryPerformanceCounter
    
    mov eax, TRUE
    ret

@fail:
    mov g_StreamContext.dwState, STREAM_STATE_ERROR
    xor eax, eax
    ret
GGUFStream_Open endp

; ============================================================================
; GGUFStream_Read - Read data from stream
; Input: pBuffer - destination buffer
;        dwSize - bytes to read
; Returns: Bytes actually read
; ============================================================================
GGUFStream_Read proc pBuffer:DWORD, dwSize:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL qwPos:QWORD
    LOCAL dwWireWritten:DWORD
    
    cmp g_StreamContext.dwState, STREAM_STATE_STREAMING
    jne @fail

    ; Capture current offset before the read so callbacks see the correct window
    mov eax, dword ptr g_StreamContext.qwBytesRead
    mov dword ptr qwPos, eax
    mov eax, dword ptr g_StreamContext.qwBytesRead+4
    mov dword ptr qwPos+4, eax
    
    ; Check if we have a memory-mapped view
    cmp g_StreamContext.pMappedBase, 0
    je @fileRead
    
    ; Memory-mapped read (fastest)
    mov esi, g_StreamContext.pMappedBase
    add esi, dword ptr g_StreamContext.qwBytesRead
    mov edi, pBuffer
    mov ecx, dwSize
    
    ; Check bounds
    mov eax, dword ptr g_StreamContext.qwFileSize
    sub eax, dword ptr g_StreamContext.qwBytesRead
    cmp ecx, eax
    jbe @copyOK
    mov ecx, eax
@copyOK:
    mov dwBytesRead, ecx
    
    ; Copy data
    rep movsb
    
    jmp @updateStats

@fileRead:
    ; Standard file read with overlapped I/O
    ; Set offset in OVERLAPPED structure manually
    lea ebx, g_StreamContext.overlapped
    mov eax, dword ptr g_StreamContext.qwBytesRead
    mov [ebx], eax          ; Internal field (offset low)
    mov eax, dword ptr g_StreamContext.qwBytesRead+4
    mov [ebx+4], eax        ; InternalHigh (offset high)
    mov eax, g_StreamContext.hEvent
    mov [ebx+16], eax       ; hEvent
    
    lea eax, g_StreamContext.overlapped
    push eax
    lea eax, dwBytesRead
    push eax
    push dwSize
    push pBuffer
    push g_StreamContext.hFile
    call ReadFile
    
    ; Wait for completion if pending
    cmp eax, 0
    jne @readDone
    call GetLastError
    cmp eax, ERROR_IO_PENDING
    jne @fail
    
    push INFINITE
    push g_StreamContext.hEvent
    call WaitForSingleObject
    
    ; Get result
    lea edx, dwBytesRead
    push FALSE
    push edx
    lea eax, g_StreamContext.overlapped
    push eax
    push g_StreamContext.hFile
    call GetOverlappedResult
    test eax, eax
    jz @fail

@readDone:
@updateStats:
    ; Update bytes read
    mov eax, dwBytesRead
    add dword ptr g_StreamContext.qwBytesRead, eax
    adc dword ptr g_StreamContext.qwBytesRead+4, 0
    
    ; Update progress percentage
    mov eax, dword ptr g_StreamContext.qwBytesRead
    mov edx, 100
    mul edx
    mov ecx, dword ptr g_StreamContext.qwFileSize
    cmp ecx, 0
    je @skipProgress
    div ecx
    mov g_StreamContext.dwProgress, eax
@skipProgress:
    
    ; Update stats
    inc g_StreamStats.dwChunksRead
    mov eax, dwBytesRead
    add dword ptr g_StreamStats.qwTotalBytes, eax
    adc dword ptr g_StreamStats.qwTotalBytes+4, 0
    
    ; Report to performance metrics
    invoke PerfMetrics_AddBytes, dwBytesRead
    
    ; Check if complete
    mov eax, dword ptr g_StreamContext.qwBytesRead
    cmp eax, dword ptr g_StreamContext.qwFileSize
    jb @notComplete
    mov eax, dword ptr g_StreamContext.qwBytesRead+4
    cmp eax, dword ptr g_StreamContext.qwFileSize+4
    jb @notComplete
    mov g_StreamContext.dwState, STREAM_STATE_COMPLETE
    mov g_StreamContext.dwProgress, 100
@notComplete:

    ; Optional wirecap sink (best-effort mirror of the streamed bytes)
    cmp g_hWirecapFile, 0
    je @checkBeacon
    lea eax, dwWireWritten
    invoke WriteFile, g_hWirecapFile, pBuffer, dwBytesRead, eax, NULL

@checkBeacon:
    ; Optional beacon callback for live inspection
    cmp g_pBeaconCallback, 0
    je @doneBeacon
    mov eax, g_pBeaconCallback
    push g_pBeaconUserData
    push dword ptr qwPos+4
    push dword ptr qwPos
    push dwBytesRead
    push pBuffer
    call eax
@doneBeacon:
    
    mov eax, dwBytesRead
    ret

@fail:
    xor eax, eax
    ret
GGUFStream_Read endp

; ============================================================================
; GGUFStream_Seek - Seek to position in stream
; Input: qwPosition - 64-bit position
; Returns: TRUE on success
; ============================================================================
GGUFStream_Seek proc qwPosLow:DWORD, qwPosHigh:DWORD
    ; For memory-mapped, just update position
    cmp g_StreamContext.pMappedBase, 0
    je @fileSeek
    
    mov eax, qwPosLow
    mov dword ptr g_StreamContext.qwBytesRead, eax
    mov eax, qwPosHigh
    mov dword ptr g_StreamContext.qwBytesRead+4, eax
    
    mov eax, TRUE
    ret

@fileSeek:
    ; File pointer seek
    push FILE_BEGIN
    push qwPosHigh
    push qwPosLow
    push g_StreamContext.hFile
    call SetFilePointer
    cmp eax, INVALID_SET_FILE_POINTER
    je @fail
    
    mov eax, qwPosLow
    mov dword ptr g_StreamContext.qwBytesRead, eax
    mov eax, qwPosHigh
    mov dword ptr g_StreamContext.qwBytesRead+4, eax
    
    mov eax, TRUE
    ret

@fail:
    xor eax, eax
    ret
GGUFStream_Seek endp

; ============================================================================
; GGUFStream_Close - Close the stream
; Returns: TRUE
; ============================================================================
GGUFStream_Close proc
    ; Unmap view
    cmp g_StreamContext.pMappedBase, 0
    je @noView
    push g_StreamContext.pMappedBase
    call UnmapViewOfFile
    mov g_StreamContext.pMappedBase, 0
@noView:
    
    ; Close mapping
    cmp g_StreamContext.hFileMapping, 0
    je @noMapping
    push g_StreamContext.hFileMapping
    call CloseHandle
    mov g_StreamContext.hFileMapping, 0
@noMapping:
    
    ; Close file
    cmp g_StreamContext.hFile, 0
    je @noFile
    push g_StreamContext.hFile
    call CloseHandle
    mov g_StreamContext.hFile, 0
@noFile:

    ; Close wirecap sink if active
    cmp g_hWirecapFile, 0
    je @noWirecap
    push g_hWirecapFile
    call CloseHandle
    mov g_hWirecapFile, 0
@noWirecap:
    
    mov g_StreamContext.dwState, STREAM_STATE_IDLE
    mov g_StreamContext.dwProgress, 0
    
    mov eax, TRUE
    ret
GGUFStream_Close endp

; ============================================================================
; GGUFStream_GetProgress - Get streaming progress
; Returns: Progress percentage (0-100)
; ============================================================================
GGUFStream_GetProgress proc
    mov eax, g_StreamContext.dwProgress
    ret
GGUFStream_GetProgress endp

; ============================================================================
; GGUFStream_GetStats - Get streaming statistics
; Input: pStats - pointer to STREAM_STATS structure
; ============================================================================
GGUFStream_GetStats proc pStats:DWORD
    mov edi, pStats
    lea esi, g_StreamStats
    mov ecx, sizeof STREAM_STATS
    rep movsb
    ret
GGUFStream_GetStats endp

; ============================================================================
; GGUFStream_Pause - Pause streaming
; ============================================================================
GGUFStream_Pause proc
    cmp g_StreamContext.dwState, STREAM_STATE_STREAMING
    jne @done
    mov g_StreamContext.dwState, STREAM_STATE_PAUSED
@done:
    ret
GGUFStream_Pause endp

; ============================================================================
; GGUFStream_Resume - Resume streaming
; ============================================================================
GGUFStream_Resume proc
    cmp g_StreamContext.dwState, STREAM_STATE_PAUSED
    jne @done
    mov g_StreamContext.dwState, STREAM_STATE_STREAMING
@done:
    ret
GGUFStream_Resume endp

; ============================================================================
; GGUFStream_SetBeaconCallback - Register a capture callback
; Input: pCallback - signature (pData, size, offsetLow, offsetHigh, pUserData)
;        pUserData - user data forwarded to callback
; ============================================================================
GGUFStream_SetBeaconCallback proc pCallback:DWORD, pUserData:DWORD
    mov g_pBeaconCallback, pCallback
    mov g_pBeaconUserData, pUserData
    ret
GGUFStream_SetBeaconCallback endp

; ============================================================================
; GGUFStream_DisableBeacon - Clear callback hooks
; ============================================================================
GGUFStream_DisableBeacon proc
    mov g_pBeaconCallback, 0
    mov g_pBeaconUserData, 0
    ret
GGUFStream_DisableBeacon endp

; ============================================================================
; GGUFStream_EnableWirecap - Mirror streamed bytes to a file
; Input: pszPath - capture file path
; Returns: TRUE on success
; ============================================================================
GGUFStream_EnableWirecap proc pszPath:DWORD
    invoke CreateFileA, pszPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @fail
    mov g_hWirecapFile, eax
    mov eax, TRUE
    ret
@fail:
    mov g_hWirecapFile, 0
    xor eax, eax
    ret
GGUFStream_EnableWirecap endp

; ============================================================================
; GGUFStream_DisableWirecap - Stop mirroring and close capture file
; ============================================================================
GGUFStream_DisableWirecap proc
    cmp g_hWirecapFile, 0
    je @done
    push g_hWirecapFile
    call CloseHandle
    mov g_hWirecapFile, 0
@done:
    ret
GGUFStream_DisableWirecap endp

end
