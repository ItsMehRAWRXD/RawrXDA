; ============================================================================
; RawrXD Agentic IDE - Optimized File Enumeration (Pure MASM)
; Implements asynchronous file enumeration for performance optimization
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include ..\\include\\winapi_min.inc

extrn RtlZeroMemory:proc
extrn PerformanceMonitor_RecordMetric:proc
extrn PathMatchSpecA:proc

; ============================================================================
; CONSTANTS
; ============================================================================

MAX_PATH_LENGTH         EQU 260
MAX_PENDING_OPERATIONS  EQU 100
FILE_ENUM_BUFFER_SIZE   EQU 64 * 1024   ; 64KB buffer
METRIC_FILE_ENUM_TIME   EQU 4

; File operation types
OP_TYPE_ENUMERATE       EQU 1
OP_TYPE_SEARCH          EQU 2

; Operation status
OP_STATUS_PENDING       EQU 1
OP_STATUS_COMPLETED     EQU 2
OP_STATUS_CANCELLED     EQU 3
OP_STATUS_ERROR         EQU 4

; ============================================================================
; DATA STRUCTURES
; ============================================================================

FILE_INFO STRUCT
    fileName        BYTE MAX_PATH_LENGTH DUP(?)
    fileSize        DWORD ?
    fileAttributes  DWORD ?
    isDirectory     DWORD ?
FILE_INFO ENDS

PENDING_OPERATION STRUCT
    opType          DWORD ?
    status          DWORD ?
    hThread         DWORD ?
    hEvent          DWORD ?
    pPath           DWORD ?     ; Pointer to path string
    pResults        DWORD ?     ; Pointer to results buffer
    resultCount     DWORD ?
    maxResults      DWORD ?
    errorCode       DWORD ?
    hTreeControl    DWORD ?
    hParentItem     DWORD ?
    pPattern        DWORD ?
    startTick       DWORD ?
PENDING_OPERATION ENDS

FILE_ENUMERATION_CONTEXT STRUCT
    hTreeControl    DWORD ?
    hParentItem     DWORD ?
    currentPath     BYTE MAX_PATH_LENGTH DUP(?)
    isCancelled     DWORD ?
    fileCount       DWORD ?
    directoryCount  DWORD ?
    pResults        DWORD ?
    maxResults      DWORD ?
    resultCount     DWORD ?
    pPattern        DWORD ?
    isSearch        DWORD ?
    pOperation      DWORD ?
FILE_ENUMERATION_CONTEXT ENDS

; ============================================================================
; GLOBAL VARIABLES
; ============================================================================

.data
    g_pendingOps        PENDING_OPERATION MAX_PENDING_OPERATIONS DUP(<>)
    g_opCount           DWORD 0
    g_initialized       DWORD FALSE
    g_hWorkerThread     DWORD 0
    g_hWorkerEvent      DWORD 0
    g_workerRunning     DWORD FALSE
    szBackslash         db "\\",0
    szStarDotStar       db "*.*",0
    szDot               db ".",0
    szDotDot            db "..",0
    WM_FILE_ENUM_COMPLETE EQU WM_USER + 510

; ============================================================================
; FUNCTION PROTOTYPES
; ============================================================================

FileEnumeration_Init                proto
FileEnumeration_EnumerateAsync      proto :DWORD, :DWORD, :DWORD
FileEnumeration_SearchAsync         proto :DWORD, :DWORD, :DWORD
FileEnumeration_CancelOperation     proto :DWORD
FileEnumeration_GetOperationStatus  proto :DWORD
FileEnumeration_GetResults          proto :DWORD, :DWORD, :DWORD
FileEnumeration_Cleanup             proto
FileEnumeration_WorkerThread        proto :DWORD
FileEnumeration_EnumerateDirectory  proto :DWORD, :DWORD
FileEnumeration_ProcessFile         proto :DWORD, :DWORD, :DWORD

; ============================================================================
; EXTERNAL REFERENCES
; ============================================================================

extrn hInstance:DWORD
extrn hMainWindow:DWORD

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; ============================================================================
; FileEnumeration_Init - Initialize file enumeration system
; ============================================================================
FileEnumeration_Init proc
    ; Check if already initialized
    .if g_initialized == TRUE
        jmp @Exit
    .endif
    
    ; Initialize pending operations array
    invoke RtlZeroMemory, addr g_pendingOps, sizeof g_pendingOps
    mov g_opCount, 0
    
    ; Create worker thread event
    invoke CreateEvent, NULL, FALSE, FALSE, NULL
    mov g_hWorkerEvent, eax
    
    ; Set initialized flag
    mov g_initialized, TRUE
    
@Exit:
    ret
FileEnumeration_Init endp

; ============================================================================
; FileEnumeration_EnumerateAsync - Asynchronously enumerate directory contents
; Parameters: hTreeControl - tree view control, hParentItem - parent item, pszPath - directory path
; Returns: Operation handle in eax
; ============================================================================
FileEnumeration_EnumerateAsync proc hTreeControl:DWORD, hParentItem:DWORD, pszPath:DWORD
    LOCAL opHandle:DWORD
    LOCAL pOp:DWORD
    LOCAL hThread:DWORD
    LOCAL pPathCopy:DWORD
    
    ; Check if initialized
    .if g_initialized == FALSE
        xor eax, eax
        jmp @Exit
    .endif
    
    ; Wrap and reuse oldest slot that is not pending
    mov ecx, 0
    mov eax, g_opCount
    .if eax >= MAX_PENDING_OPERATIONS
        mov eax, 0
        mov g_opCount, 0
    .endif
    
@FindSlot:
    cmp ecx, MAX_PENDING_OPERATIONS
    jge @Exit
    mov edx, g_opCount
    add edx, ecx
    cmp edx, MAX_PENDING_OPERATIONS
    jl @UseIndex
    sub edx, MAX_PENDING_OPERATIONS
@UseIndex:
    lea eax, g_pendingOps
    mov esi, edx
    imul esi, sizeof PENDING_OPERATION
    add eax, esi
    mov pOp, eax
    mov eax, [pOp].PENDING_OPERATION.status
    cmp eax, OP_STATUS_PENDING
    jne @SlotFound
    inc ecx
    jmp @FindSlot

@SlotFound:
    mov opHandle, pOp
    
    ; Initialize operation
    mov [pOp].PENDING_OPERATION.opType, OP_TYPE_ENUMERATE
    mov [pOp].PENDING_OPERATION.status, OP_STATUS_PENDING
    mov [pOp].PENDING_OPERATION.hEvent, 0
    mov [pOp].PENDING_OPERATION.resultCount, 0
    mov [pOp].PENDING_OPERATION.maxResults, 0
    mov [pOp].PENDING_OPERATION.errorCode, 0
    mov [pOp].PENDING_OPERATION.hTreeControl, hTreeControl
    mov [pOp].PENDING_OPERATION.hParentItem, hParentItem
    mov [pOp].PENDING_OPERATION.pPattern, 0
    
    ; Copy path
    invoke lstrlen, pszPath
    inc eax  ; Include null terminator
    invoke GlobalAlloc, GMEM_FIXED, eax
    mov pPathCopy, eax
    test eax, eax
    jz @Exit
    
    invoke lstrcpy, pPathCopy, pszPath
    mov [pOp].PENDING_OPERATION.pPath, pPathCopy
    
    ; Allocate results buffer
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, 1000 * sizeof FILE_INFO
    mov [pOp].PENDING_OPERATION.pResults, eax
    mov [pOp].PENDING_OPERATION.maxResults, 1000

    ; Capture start tick for duration metrics
    invoke GetTickCount
    mov [pOp].PENDING_OPERATION.startTick, eax
    
    ; Create thread for enumeration
    invoke CreateThread, NULL, 0, FileEnumeration_WorkerThread, pOp, 0, NULL
    mov hThread, eax
    mov [pOp].PENDING_OPERATION.hThread, eax
    
    ; Increment operation count cursor
    inc g_opCount
    
    ; Return operation handle
    mov eax, opHandle
    
@Exit:
    ret
FileEnumeration_EnumerateAsync endp

; ============================================================================
; FileEnumeration_SearchAsync - Asynchronously search for files
; Parameters: pszPath - search path, pszPattern - search pattern, pContext - search context
; Returns: Operation handle in eax
; ============================================================================
FileEnumeration_SearchAsync proc pszPath:DWORD, pszPattern:DWORD, pContext:DWORD
    LOCAL opHandle:DWORD
    LOCAL pOp:DWORD
    LOCAL hThread:DWORD
    
    ; Check if initialized
    .if g_initialized == FALSE
        xor eax, eax
        jmp @Exit
    .endif
    
    ; Wrap and reuse slot as needed
    mov ecx, 0
    mov eax, g_opCount
    .if eax >= MAX_PENDING_OPERATIONS
        mov eax, 0
        mov g_opCount, 0
    .endif

@FindSlotSearch:
    cmp ecx, MAX_PENDING_OPERATIONS
    jge @Exit
    mov edx, g_opCount
    add edx, ecx
    cmp edx, MAX_PENDING_OPERATIONS
    jl @UseIndexSearch
    sub edx, MAX_PENDING_OPERATIONS
@UseIndexSearch:
    lea eax, g_pendingOps
    mov esi, edx
    imul esi, sizeof PENDING_OPERATION
    add eax, esi
    mov pOp, eax
    mov eax, [pOp].PENDING_OPERATION.status
    cmp eax, OP_STATUS_PENDING
    jne @SlotFoundSearch
    inc ecx
    jmp @FindSlotSearch

@SlotFoundSearch:
    mov opHandle, pOp
    
    ; Initialize operation
    mov [pOp].PENDING_OPERATION.opType, OP_TYPE_SEARCH
    mov [pOp].PENDING_OPERATION.status, OP_STATUS_PENDING
    mov [pOp].PENDING_OPERATION.hEvent, 0
    mov [pOp].PENDING_OPERATION.resultCount, 0
    mov [pOp].PENDING_OPERATION.maxResults, 0
    mov [pOp].PENDING_OPERATION.errorCode, 0
    mov [pOp].PENDING_OPERATION.hTreeControl, 0
    mov [pOp].PENDING_OPERATION.hParentItem, 0

    ; Copy path
    invoke lstrlen, pszPath
    inc eax
    invoke GlobalAlloc, GMEM_FIXED, eax
    mov [pOp].PENDING_OPERATION.pPath, eax
    invoke lstrcpy, eax, pszPath

    ; Copy pattern
    invoke lstrlen, pszPattern
    inc eax
    invoke GlobalAlloc, GMEM_FIXED, eax
    mov [pOp].PENDING_OPERATION.pPattern, eax
    invoke lstrcpy, eax, pszPattern
    
    ; Allocate results buffer
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, 1000 * sizeof FILE_INFO
    mov [pOp].PENDING_OPERATION.pResults, eax
    mov [pOp].PENDING_OPERATION.maxResults, 1000

    ; Capture start tick
    invoke GetTickCount
    mov [pOp].PENDING_OPERATION.startTick, eax
    
    ; Create thread for search
    invoke CreateThread, NULL, 0, FileEnumeration_WorkerThread, pOp, 0, NULL
    mov hThread, eax
    mov [pOp].PENDING_OPERATION.hThread, eax
    
    ; Increment operation count cursor
    inc g_opCount
    
    ; Return operation handle
    mov eax, opHandle
    
@Exit:
    ret
FileEnumeration_SearchAsync endp

; ============================================================================
; FileEnumeration_CancelOperation - Cancel a pending operation
; Parameters: opHandle - operation handle
; ============================================================================
FileEnumeration_CancelOperation proc opHandle:DWORD
    LOCAL pOp:DWORD
    
    mov eax, opHandle
    mov pOp, eax
    test eax, eax
    jz @Exit
    
    ; Validate operation handle
    lea ecx, g_pendingOps
    cmp eax, ecx
    jb @Exit
    
    lea ecx, g_pendingOps
    mov edx, MAX_PENDING_OPERATIONS
    imul edx, sizeof PENDING_OPERATION
    add ecx, edx
    cmp eax, ecx
    jae @Exit
    
    ; Mark as cancelled
    mov eax, OP_STATUS_CANCELLED
    mov [pOp].PENDING_OPERATION.status, eax

    ; If thread exists, wait briefly for clean exit
    mov eax, [pOp].PENDING_OPERATION.hThread
    test eax, eax
    jz @Exit
    invoke WaitForSingleObject, eax, 2000
    
@Exit:
    ret
FileEnumeration_CancelOperation endp

; ============================================================================
; FileEnumeration_GetOperationStatus - Get status of an operation
; Parameters: opHandle - operation handle
; Returns: Operation status in eax
; ============================================================================
FileEnumeration_GetOperationStatus proc opHandle:DWORD
    LOCAL pOp:DWORD
    
    mov eax, opHandle
    mov pOp, eax
    test eax, eax
    jz @Exit
    
    ; Validate operation handle
    lea ecx, g_pendingOps
    cmp eax, ecx
    jb @Exit
    
    lea ecx, g_pendingOps
    mov edx, MAX_PENDING_OPERATIONS
    imul edx, sizeof PENDING_OPERATION
    add ecx, edx
    cmp eax, ecx
    jae @Exit
    
    ; Return status
    mov eax, [pOp].PENDING_OPERATION.status
    jmp @Exit
    
@Exit:
    ret
FileEnumeration_GetOperationStatus endp

; ============================================================================
; FileEnumeration_GetResults - Get results from completed operation
; Parameters: opHandle - operation handle, ppResults - pointer to results buffer, pCount - pointer to result count
; ============================================================================
FileEnumeration_GetResults proc opHandle:DWORD, ppResults:DWORD, pCount:DWORD
    LOCAL pOp:DWORD
    
    mov eax, opHandle
    mov pOp, eax
    test eax, eax
    jz @Exit
    
    ; Validate operation handle
    lea ecx, g_pendingOps
    cmp eax, ecx
    jb @Exit
    
    lea ecx, g_pendingOps
    mov edx, MAX_PENDING_OPERATIONS
    imul edx, sizeof PENDING_OPERATION
    add ecx, edx
    cmp eax, ecx
    jae @Exit
    
    ; Check if operation is completed
    mov eax, [pOp].PENDING_OPERATION.status
    .if eax != OP_STATUS_COMPLETED
        xor eax, eax
        jmp @Exit
    .endif
    
    ; Return results
    mov eax, [pOp].PENDING_OPERATION.pResults
    mov ecx, ppResults
    test ecx, ecx
    jz @SkipResults
    mov [ecx], eax
    
@SkipResults:
    mov eax, [pOp].PENDING_OPERATION.resultCount
    mov ecx, pCount
    test ecx, ecx
    jz @Exit
    mov [ecx], eax
    
    mov eax, 1  ; Success
    
@Exit:
    ret
FileEnumeration_GetResults endp

; ============================================================================
; FileEnumeration_WorkerThread - Worker thread for file enumeration
; Parameters: pParam - pointer to operation structure
; ============================================================================
FileEnumeration_WorkerThread proc pParam:DWORD
    LOCAL pOp:DWORD
    LOCAL opType:DWORD
    LOCAL ctx:FILE_ENUMERATION_CONTEXT
    LOCAL startTick:DWORD
    LOCAL endTick:DWORD
    LOCAL duration:DWORD
    
    mov eax, pParam
    mov pOp, eax
    test eax, eax
    jz @Exit
    
    ; Get operation type
    mov eax, [pOp].PENDING_OPERATION.opType
    mov opType, eax

    ; Prime context
    invoke RtlZeroMemory, addr ctx, sizeof FILE_ENUMERATION_CONTEXT
    mov eax, [pOp].PENDING_OPERATION.hTreeControl
    mov ctx.hTreeControl, eax
    mov eax, [pOp].PENDING_OPERATION.hParentItem
    mov ctx.hParentItem, eax
    mov eax, [pOp].PENDING_OPERATION.pResults
    mov ctx.pResults, eax
    mov eax, [pOp].PENDING_OPERATION.maxResults
    mov ctx.maxResults, eax
    mov eax, [pOp].PENDING_OPERATION.pPattern
    mov ctx.pPattern, eax
    mov eax, [pOp].PENDING_OPERATION.opType
    cmp eax, OP_TYPE_SEARCH
    jne @NotSearch
    mov ctx.isSearch, TRUE
@NotSearch:
    mov ctx.pOperation, pOp

    mov eax, [pOp].PENDING_OPERATION.status
    cmp eax, OP_STATUS_PENDING
    jne @Exit

    ; Capture start tick
    mov eax, [pOp].PENDING_OPERATION.startTick
    mov startTick, eax
    
    ; Execute operation
    .if opType == OP_TYPE_ENUMERATE
        invoke FileEnumeration_EnumerateDirectory, [pOp].PENDING_OPERATION.pPath, addr ctx
    .elseif opType == OP_TYPE_SEARCH
        invoke FileEnumeration_EnumerateDirectory, [pOp].PENDING_OPERATION.pPath, addr ctx
    .endif

    ; Update counts
    mov eax, ctx.resultCount
    mov [pOp].PENDING_OPERATION.resultCount, eax
    
    ; Mark completion status
    mov eax, [pOp].PENDING_OPERATION.status
    cmp eax, OP_STATUS_CANCELLED
    je @SkipComplete
    mov [pOp].PENDING_OPERATION.status, OP_STATUS_COMPLETED
@SkipComplete:

    ; Record duration metric
    invoke GetTickCount
    mov endTick, eax
    mov eax, endTick
    sub eax, startTick
    mov duration, eax
    invoke PerformanceMonitor_RecordMetric, METRIC_FILE_ENUM_TIME, duration, 1  ; milliseconds

    ; Notify UI: post completion message to main window with op handle
    invoke PostMessage, hMainWindow, WM_FILE_ENUM_COMPLETE, pOp, 0

    ; Close thread handle to avoid leaks
    mov eax, [pOp].PENDING_OPERATION.hThread
    test eax, eax
    jz @SkipClose
    invoke CloseHandle, eax
    mov [pOp].PENDING_OPERATION.hThread, 0

@SkipClose:
    ; Free transient strings
    mov eax, [pOp].PENDING_OPERATION.pPath
    test eax, eax
    jz @SkipPathFree
    invoke GlobalFree, eax
    mov [pOp].PENDING_OPERATION.pPath, 0

@SkipPathFree:
    mov eax, [pOp].PENDING_OPERATION.pPattern
    test eax, eax
    jz @Exit
    invoke GlobalFree, eax
    mov [pOp].PENDING_OPERATION.pPattern, 0
    
@Exit:
    ; Exit thread
    invoke ExitThread, 0
    ret
FileEnumeration_WorkerThread endp

; ============================================================================
; FileEnumeration_EnumerateDirectory - Enumerate directory contents
; Parameters: pszPath - directory path, pContext - enumeration context
; ============================================================================
FileEnumeration_EnumerateDirectory proc pszPath:DWORD, pContext:DWORD
    LOCAL hFind:DWORD
    LOCAL findData:WIN32_FIND_DATA
    LOCAL szSearchPath[MAX_PATH]:BYTE
    LOCAL fileCount:DWORD
    LOCAL directoryCount:DWORD
    
    ; Build search path (path + \*.*)
    invoke lstrcpy, addr szSearchPath, pszPath
    invoke lstrcat, addr szSearchPath, addr szBackslash
    invoke lstrcat, addr szSearchPath, addr szStarDotStar
    
    ; Find first file
    invoke FindFirstFile, addr szSearchPath, addr findData
    mov hFind, eax
    cmp eax, INVALID_HANDLE_VALUE
    je @Done
    
    mov fileCount, 0
    mov directoryCount, 0
    
@FindLoop:
    ; Check if operation was cancelled
    mov eax, pContext
    test eax, eax
    jz @NextFile
    mov ecx, [eax].FILE_ENUMERATION_CONTEXT.pOperation
    test ecx, ecx
    jz @CheckFlag
    mov edx, [ecx].PENDING_OPERATION.status
    cmp edx, OP_STATUS_CANCELLED
    je @CloseAndExit
@CheckFlag:
    mov edx, [eax].FILE_ENUMERATION_CONTEXT.isCancelled
    .if edx != 0
        jmp @CloseAndExit
    .endif
    
    ; Skip "." and ".."
    invoke lstrcmp, addr findData.cFileName, addr szDot
    .if eax != 0
        invoke lstrcmp, addr findData.cFileName, addr szDotDot
        .if eax != 0
            ; Process file
            invoke FileEnumeration_ProcessFile, addr findData, pszPath, pContext
            
            ; Update counters
            test findData.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY
            .if ZERO?
                inc fileCount
            .else
                inc directoryCount
            .endif
        .endif
    .endif
    
@NextFile:
    ; Find next file
    invoke FindNextFile, hFind, addr findData
    test eax, eax
    jnz @FindLoop
    
@CloseAndExit:
    ; Close search handle
    invoke FindClose, hFind
    
@Done:
    ; Update context with final counts
    mov eax, pContext
    test eax, eax
    jz @Exit
    
    mov ecx, fileCount
    mov [eax].FILE_ENUMERATION_CONTEXT.fileCount, ecx
    
    mov ecx, directoryCount
    mov [eax].FILE_ENUMERATION_CONTEXT.directoryCount, ecx
    
@Exit:
    ret
FileEnumeration_EnumerateDirectory endp

; ============================================================================
; FileEnumeration_ProcessFile - Process a single file during enumeration
; Parameters: pFindData - pointer to find data, pszBasePath - base path, pContext - enumeration context
; ============================================================================
FileEnumeration_ProcessFile proc pFindData:DWORD, pszBasePath:DWORD, pContext:DWORD
    LOCAL szFullPath[MAX_PATH]:BYTE
    LOCAL pFind:DWORD
    LOCAL isDirectory:DWORD
    LOCAL pResults:DWORD
    LOCAL idx:DWORD
    LOCAL pOp:DWORD

    mov eax, pFindData
    mov pFind, eax
    test eax, eax
    jz @Exit

    mov eax, pContext
    test eax, eax
    jz @Exit

    ; Respect cancellation
    mov pOp, [eax].FILE_ENUMERATION_CONTEXT.pOperation
    test pOp, pOp
    jz @BuildPath
    mov ecx, [pOp].PENDING_OPERATION.status
    cmp ecx, OP_STATUS_CANCELLED
    je @Exit

@BuildPath:
    ; Build full path
    invoke lstrcpy, addr szFullPath, pszBasePath
    invoke lstrcat, addr szFullPath, addr szBackslash
    invoke lstrcat, addr szFullPath, addr [pFind].WIN32_FIND_DATA.cFileName
    
    ; Determine if it's a directory
    test [pFind].WIN32_FIND_DATA.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY
    .if ZERO?
        mov isDirectory, FALSE
    .else
        mov isDirectory, TRUE
    .endif

    ; If search mode, filter with pattern
    mov ecx, [pContext].FILE_ENUMERATION_CONTEXT.isSearch
    .if ecx != 0
        mov ecx, [pContext].FILE_ENUMERATION_CONTEXT.pPattern
        test ecx, ecx
        jz @SkipMatch
        invoke PathMatchSpecA, addr szFullPath, ecx
        test eax, eax
        jz @Exit
@SkipMatch:
    .endif

    ; Guard capacity
    mov eax, [pContext].FILE_ENUMERATION_CONTEXT.resultCount
    mov idx, eax
    mov ecx, [pContext].FILE_ENUMERATION_CONTEXT.maxResults
    cmp eax, ecx
    jae @Exit

    ; Write into results buffer
    mov eax, [pContext].FILE_ENUMERATION_CONTEXT.pResults
    mov pResults, eax
    mov ecx, idx
    imul ecx, sizeof FILE_INFO
    add eax, ecx
    
    ; fileName
    lea ecx, [pFind].WIN32_FIND_DATA.cFileName
    invoke lstrcpyn, eax, ecx, MAX_PATH_LENGTH
    
    ; fileSize
    mov ecx, [pFind].WIN32_FIND_DATA.nFileSizeLow
    mov [eax].FILE_INFO.fileSize, ecx
    
    ; attributes and directory flag
    mov ecx, [pFind].WIN32_FIND_DATA.dwFileAttributes
    mov [eax].FILE_INFO.fileAttributes, ecx
    mov ecx, isDirectory
    mov [eax].FILE_INFO.isDirectory, ecx

    ; Increment count
    mov ecx, idx
    inc ecx
    mov [pContext].FILE_ENUMERATION_CONTEXT.resultCount, ecx

@Exit:
    ret
FileEnumeration_ProcessFile endp

; ============================================================================
; FileEnumeration_Cleanup - Clean up file enumeration system
; ============================================================================
FileEnumeration_Cleanup proc
    LOCAL i:DWORD
    LOCAL pOp:DWORD
    
    ; Check if initialized
    .if g_initialized == FALSE
        jmp @Exit
    .endif
    
    ; Cancel all pending operations
    mov i, 0
@CancelLoop:
    mov eax, i
    cmp eax, g_opCount
    jge @CleanupComplete
    
    ; Get operation pointer
    lea eax, g_pendingOps
    mov ecx, i
    imul ecx, sizeof PENDING_OPERATION
    add eax, ecx
    mov pOp, eax
    
    ; Cancel operation if pending
    mov eax, [pOp].PENDING_OPERATION.status
    .if eax == OP_STATUS_PENDING
        mov [pOp].PENDING_OPERATION.status, OP_STATUS_CANCELLED
    .endif
    
    ; Clean up resources
    mov eax, [pOp].PENDING_OPERATION.pPath
    test eax, eax
    jz @SkipPath
    invoke GlobalFree, eax
    mov [pOp].PENDING_OPERATION.pPath, 0
    
@SkipPath:
    mov eax, [pOp].PENDING_OPERATION.pResults
    test eax, eax
    jz @SkipResults
    invoke GlobalFree, eax
    mov [pOp].PENDING_OPERATION.pResults, 0

@SkipResults:
    mov eax, [pOp].PENDING_OPERATION.pPattern
    test eax, eax
    jz @SkipPattern
    invoke GlobalFree, eax
    mov [pOp].PENDING_OPERATION.pPattern, 0

@SkipPattern:
    mov eax, [pOp].PENDING_OPERATION.hThread
    test eax, eax
    jz @SkipThread
    invoke WaitForSingleObject, eax, 2000
    invoke CloseHandle, eax
    mov [pOp].PENDING_OPERATION.hThread, 0

@SkipThread:
    mov eax, [pOp].PENDING_OPERATION.hEvent
    test eax, eax
    jz @NextOp
    invoke CloseHandle, eax
    mov [pOp].PENDING_OPERATION.hEvent, 0
    
@NextOp:
    inc i
    jmp @CancelLoop
    
@CleanupComplete:
    ; Clean up worker thread resources
    mov eax, g_hWorkerEvent
    test eax, eax
    jz @SkipWorkerEvent
    invoke CloseHandle, eax
    mov g_hWorkerEvent, 0
    
@SkipWorkerEvent:
    ; Clear initialized flag
    mov g_initialized, FALSE
    mov g_opCount, 0
    
@Exit:
    ret
FileEnumeration_Cleanup endp

end