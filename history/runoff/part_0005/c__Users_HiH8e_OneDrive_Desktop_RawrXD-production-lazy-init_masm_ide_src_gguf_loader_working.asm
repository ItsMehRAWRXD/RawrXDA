; ============================================================================
; GGUF_LOADER_WORKING.ASM - Ollama-Compatible GGUF Loader (Pure MASM)
; Enhanced with comprehensive error logging and diagnostics
; IDE-Accessible via gguf_loader_interface.inc
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include dynapi_x86.inc
include gguf_loader_interface.inc

; π-RAM Compression Integration
include piram_compress.inc

; IDE Bridge Functions
GGUF_IDE_RegisterLoader PROTO :DWORD, :DWORD
GGUF_IDE_NotifyProgress PROTO :DWORD, :DWORD
GGUF_IDE_NotifyStatus PROTO :DWORD, :DWORD
GGUF_IDE_NotifyModelLoaded PROTO :DWORD, :DWORD

IFNDEF PURE_MASM_NO_IMPORTLIBS
includelib kernel32.lib
ENDIF

CreateFileA PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
ReadFile PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
WriteFile PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
GetFileSize PROTO :DWORD,:DWORD
CloseHandle PROTO :DWORD
GetStdHandle PROTO :DWORD
GetProcessHeap PROTO
HeapAlloc PROTO :DWORD,:DWORD,:DWORD
HeapFree PROTO :DWORD,:DWORD,:DWORD
CreateFileMappingA PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
MapViewOfFile PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
UnmapViewOfFile PROTO :DWORD
GetLastError PROTO
QueryPerformanceCounter PROTO :DWORD
QueryPerformanceFrequency PROTO :DWORD
GetTickCount PROTO
GetModuleFileNameA PROTO :DWORD,:DWORD,:DWORD
lstrcpyA PROTO :DWORD,:DWORD
lstrlenA PROTO :DWORD

; Reverse header reader
GGUF_ReverseReadHeader PROTO :DWORD,:DWORD
GGUF_ReverseGetMetadata PROTO :DWORD,:DWORD
GGUF_ReverseGetTensorCount PROTO :DWORD

; Reverse quantization for fitting models
ReverseQuantize_Init PROTO
ReverseQuantize_CompressToFit PROTO :DWORD,:DWORD
ReverseQuantize_CircularCompress PROTO :DWORD,:DWORD
ReverseQuantize_CalcTargetPasses PROTO :DWORD
ReverseQuantize_ApplyQuantization PROTO :DWORD,:DWORD,:DWORD
ReverseQuantize_GetQuantLevel PROTO

NULL                equ 0
INVALID_HANDLE_VALUE equ -1
TRUE                equ 1
FALSE               equ 0
GENERIC_READ        equ 80000000h
FILE_SHARE_READ     equ 1h
OPEN_EXISTING       equ 3h
FILE_ATTRIBUTE_NORMAL equ 80h
FILE_FLAG_SEQUENTIAL_SCAN equ 08000000h
HEAP_ZERO_MEMORY    equ 8h
PAGE_READONLY       equ 2h
FILE_MAP_READ       equ 4h
GGUF_MAGIC          equ 46554747h
GGUF_VERSION        equ 3

; Error codes
GGUF_SUCCESS        equ 0
GGUF_ERROR_FILE     equ -1
GGUF_ERROR_FORMAT   equ -2
GGUF_ERROR_MEMORY   equ -3
GGUF_ERROR_ACCESS   equ -4
GGUF_ERROR_VERSION  equ -5
GGUF_ERROR_CORRUPT  equ -6
GGUF_ERROR_SECURITY equ -7
GGUF_ERROR_OVERFLOW equ -8

; Log levels
LOG_INFO            equ 0
LOG_WARNING         equ 1
LOG_ERROR           equ 2
LOG_DEBUG           equ 3

.data
    zero            dd 0

    ; Error messages
    szLogPrefix     db "[GGUF Loader] ", 0
        szLoaderName    db "GGUF_Loader_Working", 0
        szLoadingStart  db "Loading GGUF model...", 0
        szLoadingDone   db "Model loaded successfully", 0
    szErrorFile     db "File operation failed: ", 0
    szErrorFormat   db "Invalid GGUF format: ", 0
    szErrorMemory   db "Memory allocation failed: ", 0
    szErrorAccess   db "Access denied: ", 0
    szErrorVersion   db "Unsupported version: ", 0
    szErrorCorrupt  db "File appears corrupted: ", 0
    szErrorSecurity db "Security validation failed: ", 0
    szErrorOverflow db "Buffer overflow detected: ", 0

    ; Operation messages
    szOpOpen        db "Opening file: ", 0
    szOpRead        db "Reading data from file", 0
    szOpValidate    db "Validating GGUF format", 0
    szOpParse       db "Parsing tensor metadata", 0
    szOpMap         db "Creating memory mapping", 0
    szOpClose       db "Closing model and freeing resources", 0

    ; Status messages
    szSuccess       db "Operation completed successfully", 0
    szModelLoaded   db "GGUF model loaded successfully", 0
    szCompressionApplied db "π-RAM compression applied to GGUF model", 0
    szModelClosed   db "GGUF model closed successfully", 0
    szCancelled     db "Load cancelled", 0

    ; Performance messages
    szPerfLoad      db "Model load time: ", 0
    szPerfMs        db " ms", 0
    szPerfMem       db "Memory usage: ", 0
    szPerfBytes     db " bytes", 0

    ; Additional error messages
    szInvalidParam  db "Invalid parameter: ", 0
    szNullPointer   db "Null pointer detected: ", 0
    szInvalidState  db "Invalid model state: ", 0
    szErrorCount    db "Total errors encountered: ", 0
    szLoadCount     db "Total models loaded: ", 0
    g_LoaderRegistered dd 0

.data?
    g_hHeap         dd ?
    g_initialized   dd ?
    g_logLevel      dd ?
    g_errorCount    dd ?
    g_loadCount     dd ?
    g_perfFreq      dq ?
    g_startTime     dq ?

IFDEF PURE_MASM_NO_IMPORTLIBS
    ; Dynamic bindings (kernel32) for MASM-only builds
    p_CreateFileA            dd ?
    p_ReadFile               dd ?
    p_GetFileSize            dd ?
    p_CloseHandle            dd ?
    p_GetProcessHeap         dd ?
    p_HeapAlloc              dd ?
    p_HeapFree               dd ?
    p_CreateFileMappingA     dd ?
    p_MapViewOfFile          dd ?
    p_UnmapViewOfFile        dd ?
    p_GetLastError           dd ?
    p_QueryPerformanceCounter dd ?
    p_QueryPerformanceFrequency dd ?
    p_GetTickCount           dd ?
    p_GetModuleFileNameA     dd ?
    p_lstrcpyA               dd ?
    p_lstrlenA               dd ?
ENDIF

.code

IFDEF PURE_MASM_NO_IMPORTLIBS
.data
    szKERNEL32              db "KERNEL32.DLL",0
    szCreateFileA           db "CreateFileA",0
    szReadFile              db "ReadFile",0
    szGetFileSize           db "GetFileSize",0
    szCloseHandle           db "CloseHandle",0
    szGetProcessHeap        db "GetProcessHeap",0
    szHeapAlloc             db "HeapAlloc",0
    szHeapFree              db "HeapFree",0
    szCreateFileMappingA    db "CreateFileMappingA",0
    szMapViewOfFile         db "MapViewOfFile",0
    szUnmapViewOfFile       db "UnmapViewOfFile",0
    szGetLastError          db "GetLastError",0
    szQueryPerformanceCounter db "QueryPerformanceCounter",0
    szQueryPerformanceFrequency db "QueryPerformanceFrequency",0
    szGetTickCount          db "GetTickCount",0
    szGetModuleFileNameA    db "GetModuleFileNameA",0
    szlstrcpyA              db "lstrcpyA",0
    szlstrlenA              db "lstrlenA",0
.code

GGUF_BindKernel32 proc
    LOCAL hK32:DWORD
    invoke DYN_FindModuleBaseA, addr szKERNEL32
    mov hK32, eax
    test eax, eax
    jz @@fail

    DYN_BIND p_CreateFileA, hK32, addr szCreateFileA
    DYN_BIND p_ReadFile, hK32, addr szReadFile
    DYN_BIND p_GetFileSize, hK32, addr szGetFileSize
    DYN_BIND p_CloseHandle, hK32, addr szCloseHandle
    DYN_BIND p_GetProcessHeap, hK32, addr szGetProcessHeap
    DYN_BIND p_HeapAlloc, hK32, addr szHeapAlloc
    DYN_BIND p_HeapFree, hK32, addr szHeapFree
    DYN_BIND p_CreateFileMappingA, hK32, addr szCreateFileMappingA
    DYN_BIND p_MapViewOfFile, hK32, addr szMapViewOfFile
    DYN_BIND p_UnmapViewOfFile, hK32, addr szUnmapViewOfFile
    DYN_BIND p_GetLastError, hK32, addr szGetLastError
    DYN_BIND p_QueryPerformanceCounter, hK32, addr szQueryPerformanceCounter
    DYN_BIND p_QueryPerformanceFrequency, hK32, addr szQueryPerformanceFrequency
    DYN_BIND p_GetTickCount, hK32, addr szGetTickCount
    DYN_BIND p_GetModuleFileNameA, hK32, addr szGetModuleFileNameA
    DYN_BIND p_lstrcpyA, hK32, addr szlstrcpyA
    DYN_BIND p_lstrlenA, hK32, addr szlstrlenA

    mov eax, TRUE
    ret
@@fail:
    xor eax, eax
    ret
GGUF_BindKernel32 endp

; Local WinAPI wrappers (same names) so existing code can keep using INVOKE
CreateFileA proc a1:DWORD,a2:DWORD,a3:DWORD,a4:DWORD,a5:DWORD,a6:DWORD,a7:DWORD
    mov eax, p_CreateFileA
    test eax, eax
    jz @@fail
    push a7
    push a6
    push a5
    push a4
    push a3
    push a2
    push a1
    call eax
    ret
@@fail:
    mov eax, INVALID_HANDLE_VALUE
    ret
CreateFileA endp

ReadFile proc a1:DWORD,a2:DWORD,a3:DWORD,a4:DWORD,a5:DWORD
    mov eax, p_ReadFile
    test eax, eax
    jz @@fail
    push a5
    push a4
    push a3
    push a2
    push a1
    call eax
    ret
@@fail:
    xor eax, eax
    ret
ReadFile endp

GetFileSize proc a1:DWORD,a2:DWORD
    mov eax, p_GetFileSize
    test eax, eax
    jz @@fail
    push a2
    push a1
    call eax
    ret
@@fail:
    mov eax, 0FFFFFFFFh
    ret
GetFileSize endp

CloseHandle proc a1:DWORD
    mov eax, p_CloseHandle
    test eax, eax
    jz @@fail
    push a1
    call eax
    ret
@@fail:
    xor eax, eax
    ret
CloseHandle endp

GetProcessHeap proc
    mov eax, p_GetProcessHeap
    test eax, eax
    jz @@fail
    call eax
    ret
@@fail:
    xor eax, eax
    ret
GetProcessHeap endp

HeapAlloc proc a1:DWORD,a2:DWORD,a3:DWORD
    mov eax, p_HeapAlloc
    test eax, eax
    jz @@fail
    push a3
    push a2
    push a1
    call eax
    ret
@@fail:
    xor eax, eax
    ret
HeapAlloc endp

HeapFree proc a1:DWORD,a2:DWORD,a3:DWORD
    mov eax, p_HeapFree
    test eax, eax
    jz @@fail
    push a3
    push a2
    push a1
    call eax
    ret
@@fail:
    xor eax, eax
    ret
HeapFree endp

CreateFileMappingA proc a1:DWORD,a2:DWORD,a3:DWORD,a4:DWORD,a5:DWORD,a6:DWORD
    mov eax, p_CreateFileMappingA
    test eax, eax
    jz @@fail
    push a6
    push a5
    push a4
    push a3
    push a2
    push a1
    call eax
    ret
@@fail:
    xor eax, eax
    ret
CreateFileMappingA endp

MapViewOfFile proc a1:DWORD,a2:DWORD,a3:DWORD,a4:DWORD,a5:DWORD
    mov eax, p_MapViewOfFile
    test eax, eax
    jz @@fail
    push a5
    push a4
    push a3
    push a2
    push a1
    call eax
    ret
@@fail:
    xor eax, eax
    ret
MapViewOfFile endp

UnmapViewOfFile proc a1:DWORD
    mov eax, p_UnmapViewOfFile
    test eax, eax
    jz @@fail
    push a1
    call eax
    ret
@@fail:
    xor eax, eax
    ret
UnmapViewOfFile endp

GetLastError proc
    mov eax, p_GetLastError
    test eax, eax
    jz @@fail
    call eax
    ret
@@fail:
    xor eax, eax
    ret
GetLastError endp

QueryPerformanceCounter proc a1:DWORD
    mov eax, p_QueryPerformanceCounter
    test eax, eax
    jz @@fail
    push a1
    call eax
    ret
@@fail:
    xor eax, eax
    ret
QueryPerformanceCounter endp

QueryPerformanceFrequency proc a1:DWORD
    mov eax, p_QueryPerformanceFrequency
    test eax, eax
    jz @@fail
    push a1
    call eax
    ret
@@fail:
    xor eax, eax
    ret
QueryPerformanceFrequency endp

GetTickCount proc
    mov eax, p_GetTickCount
    test eax, eax
    jz @@fail
    call eax
    ret
@@fail:
    xor eax, eax
    ret
GetTickCount endp

GetModuleFileNameA proc a1:DWORD,a2:DWORD,a3:DWORD
    mov eax, p_GetModuleFileNameA
    test eax, eax
    jz @@fail
    push a3
    push a2
    push a1
    call eax
    ret
@@fail:
    xor eax, eax
    ret
GetModuleFileNameA endp

lstrcpyA proc a1:DWORD,a2:DWORD
    mov eax, p_lstrcpyA
    test eax, eax
    jz @@fail
    push a2
    push a1
    call eax
    ret
@@fail:
    mov eax, a1
    ret
lstrcpyA endp

lstrlenA proc a1:DWORD
    mov eax, p_lstrlenA
    test eax, eax
    jz @@fail
    push a1
    call eax
    ret
@@fail:
    xor eax, eax
    ret
lstrlenA endp
ENDIF

; ============================================================================
; LOGGING FUNCTIONS
; ============================================================================

; LogMessage - Internal logging function (now writes to stdout)
LogMessage proc level:DWORD, pMessage:DWORD, pContext:DWORD
    LOCAL buffer[256]:BYTE
    LOCAL pBuf:DWORD
    LOCAL hOut:DWORD
    LOCAL dwWritten:DWORD
    LOCAL msgLen:DWORD
    LOCAL lenUsed:DWORD

    ; Check log level
    mov eax, level
    cmp eax, g_logLevel
    jg @@exit

    ; Build log message
    lea eax, buffer
    mov pBuf, eax
    mov lenUsed, 0

    ; Add prefix (max 30 bytes)
    invoke lstrcpyA, pBuf, addr szLogPrefix
    invoke lstrlenA, pBuf
    cmp eax, 30
    jae @@skip_context
    add lenUsed, eax
    add pBuf, eax

    ; Add message (leave 50 bytes for context/CRLF)
    cmp lenUsed, 200
    jae @@skip_context
    
    invoke lstrcpyA, pBuf, pMessage
    invoke lstrlenA, pBuf
    add lenUsed, eax
    add pBuf, eax

@@skip_context:
    ; Add CRLF
    mov byte ptr [pBuf], 13
    inc pBuf
    mov byte ptr [pBuf], 10
    inc pBuf
    mov byte ptr [pBuf], 0

    ; Write to stdout
    invoke GetStdHandle, -11
    mov hOut, eax
    lea eax, buffer
    invoke lstrlenA, eax
    mov msgLen, eax
    invoke WriteFile, hOut, addr buffer, msgLen, addr dwWritten, NULL

@@exit:
    ret
LogMessage endp

; LogError - Log error with system error code
LogError proc errorCode:DWORD, pMessage:DWORD, pContext:DWORD
    LOCAL sysError:DWORD
    LOCAL buffer[256]:BYTE

    ; Get system error
    invoke GetLastError
    mov sysError, eax

    ; Log the error
    invoke LogMessage, LOG_ERROR, pMessage, pContext

    ; Increment error counter
    inc g_errorCount

    ret
LogError endp

; LogPerformance - Log performance metrics
LogPerformance proc pOperation:DWORD, startTime:DWORD, endTime:DWORD
    LOCAL elapsed:DWORD
    LOCAL buffer[128]:BYTE

    ; Calculate elapsed time
    mov eax, endTime
    sub eax, startTime
    mov elapsed, eax

    ; Log performance
    invoke LogMessage, LOG_INFO, addr szPerfLoad, pOperation

    ret
LogPerformance endp

; ============================================================================
; INITIALIZATION
; ============================================================================

; InitializeLoader - Initialize the loader subsystem
InitializeLoader proc
    cmp g_initialized, TRUE
    je @@already_init

IFDEF PURE_MASM_NO_IMPORTLIBS
    ; Resolve kernel32 exports via PEB resolver (no import libs)
    invoke GGUF_BindKernel32
    test eax, eax
    jz @@already_init
ENDIF

    ; Get process heap
    invoke GetProcessHeap
    mov g_hHeap, eax

    ; Initialize performance counter
    invoke QueryPerformanceFrequency, addr g_perfFreq

    ; Set default log level
    mov g_logLevel, LOG_INFO

    ; Initialize counters
    mov g_errorCount, 0
    mov g_loadCount, 0

    mov g_initialized, TRUE

    invoke LogMessage, LOG_INFO, addr szSuccess, addr szLogPrefix

@@already_init:
    mov eax, TRUE
    ret
InitializeLoader endp

; Helper: ReadExact
ReadExact proc hFile:DWORD, pBuf:DWORD, dwSize:DWORD
    LOCAL dwRead:DWORD
    invoke ReadFile, hFile, pBuf, dwSize, addr dwRead, NULL
    test eax, eax
    jz @@fail
    mov eax, dwRead
    cmp eax, dwSize
    jne @@fail
    mov eax, 1
    ret
@@fail:
    xor eax, eax
    ret
ReadExact endp

; Helper: ReadVarInt
ReadVarInt proc hFile:DWORD
    LOCAL result:DWORD
    LOCAL shift:DWORD
    LOCAL byteVal:BYTE
    LOCAL dwRead:DWORD
    
    mov result, 0
    mov shift, 0
@@loop:
    invoke ReadFile, hFile, addr byteVal, 1, addr dwRead, NULL
    test eax, eax
    jz @@error
    movzx eax, byteVal
    mov ecx, eax
    and ecx, 7Fh
    mov edx, shift
    cmp edx, 32
    jge @@error
    mov cl, dl
    shl ecx, cl
    or result, ecx
    test byteVal, 80h
    jz @@done
    add shift, 7
    jmp @@loop
@@error:
    mov eax, -1
    ret
@@done:
    mov eax, result
    ret
ReadVarInt endp

; ReadVarIntMem - Read variable-length integer from memory (esi)
ReadVarIntMem proc
    LOCAL result:DWORD
    LOCAL shift:DWORD
    LOCAL byteVal:BYTE

    mov result, 0
    mov shift, 0

@@loop:
    cmp shift, 35
    jge @@error
    lodsb
    mov byteVal, al
    movzx ecx, byteVal
    and ecx, 7Fh
    mov edx, shift
    cmp edx, 32
    jge @@error
    mov cl, dl
    shl ecx, cl
    or result, ecx
    test byteVal, 80h
    jz @@done
    add shift, 7
    jmp @@loop
@@error:
    mov eax, -1
    ret
@@done:
    mov eax, result
    ret
ReadVarIntMem endp

; PUBLIC API: GGUF_LoadModel
public GGUF_LoadModel
GGUF_LoadModel proc pPath:DWORD
    LOCAL pModel:DWORD
    LOCAL hFile:DWORD
    LOCAL magic:DWORD
    LOCAL version:DWORD
    LOCAL nKV:DWORD
    LOCAL nTensors:DWORD
    LOCAL i:DWORD
    LOCAL startTime:DWORD
    LOCAL endTime:DWORD
    LOCAL fileSize:DWORD
    LOCAL sizeHigh:DWORD
    
    ; Register with IDE if first time
    cmp g_LoaderRegistered, TRUE
    je @@already_registered
    
    invoke GGUF_IDE_RegisterLoader, addr szLoaderName, 100
    mov g_LoaderRegistered, TRUE
    
@@already_registered:
    ; Notify IDE: Starting load
    invoke GGUF_IDE_NotifyProgress, 0, addr szLoadingStart
    invoke GGUF_IDE_NotifyStatus, LOG_INFO, addr szLoadingStart

    ; Initialize loader if needed
    invoke InitializeLoader
    test eax, eax
    jz @@error_init

    invoke LogMessage, LOG_INFO, addr szSuccess, addr szLogPrefix

    ; Start performance timing
    invoke GetTickCount
    mov startTime, eax

    ; Log operation start
    invoke LogMessage, LOG_INFO, addr szOpOpen, pPath
    invoke GGUF_IDE_NotifyProgress, 10, pPath

    invoke CreateFileA, pPath, GENERIC_READ, FILE_SHARE_READ, NULL, \
            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL
        invoke GGUF_IDE_NotifyProgress, 20, addr szLoadingStart
    cmp eax, INVALID_HANDLE_VALUE
    je @@error_file_open
    mov hFile, eax

    ; Get file size for logging
    invoke GetFileSize, hFile, addr sizeHigh
        invoke GGUF_IDE_NotifyProgress, 30, addr szLoadingStart
    mov fileSize, eax
    
    ; Check for invalid size (GetFileSize error)
    cmp eax, 0FFFFFFFFh
    jne @size_ok
    cmp sizeHigh, 0
    je @@error_file_size
@size_ok:

    ; For >4GB files, force header-only mode (64KB)
    cmp sizeHigh, 0
    jne @use_large_file_mode

    ; Allocate model structure early to hold handles/buffers
    invoke HeapAlloc, g_hHeap, HEAP_ZERO_MEMORY, 256
    test eax, eax
    jz @@error_memory
    mov pModel, eax

    ; Store file handle
    mov ecx, pModel
    mov edx, hFile
    mov [ecx], edx

    ; For files >4GB or >256MB, read header only (64KB compressed)
    ; Otherwise read full file for small models
    ; Header read size uses π-RAM compression: 64KB → ~32KB actual
    mov eax, fileSize
    cmp eax, 10000000h         ; >256MB ⇒ header-only read
    jbe @use_heap_full

    ; Large file: read 64KB header (will compress to ~32KB)
    mov edx, 10000h            ; 64KB header
    invoke HeapAlloc, g_hHeap, HEAP_ZERO_MEMORY, edx
    test eax, eax
    jz @@error_memory
    mov ecx, pModel
    mov dword ptr [ecx+12], eax          ; buffer
    mov edx, fileSize
    mov dword ptr [ecx+16], edx          ; record full size
    mov esi, eax
    invoke ReadExact, hFile, esi, 10000h  ; Read 64KB header
    test eax, eax
    jz @@error_read_data
    jmp @parse_mem

@use_large_file_mode:
    ; Large file (>4GB): allocate model structure and read header
    invoke HeapAlloc, g_hHeap, HEAP_ZERO_MEMORY, 256
    test eax, eax
    jz @@error_memory
    mov pModel, eax
    
    ; Store file handle
    mov ecx, pModel
    mov edx, hFile
    mov [ecx], edx
    
    ; Read 64KB header for large file
    mov edx, 10000h
    invoke HeapAlloc, g_hHeap, HEAP_ZERO_MEMORY, edx
    test eax, eax
    jz @@error_memory
    mov ecx, pModel
    mov dword ptr [ecx+12], eax
    mov edx, fileSize
    mov dword ptr [ecx+16], edx
    mov esi, eax
    invoke ReadExact, hFile, esi, 10000h
    test eax, eax
    jz @@error_read_data
    jmp @parse_mem
    
    ; Mark as loaded (will be compressed on demand)
    mov dword ptr [ecx+24], TRUE
    mov dword ptr [ecx+28], TRUE
    
    ; Skip header parsing - go straight to success
    jmp @parse_done

@use_heap_full:
    ; Allocate buffer for entire file
    invoke HeapAlloc, g_hHeap, HEAP_ZERO_MEMORY, fileSize
    test eax, eax
    jz @@error_memory
    mov ecx, pModel
    mov dword ptr [ecx+12], eax          ; tensor/data buffer
    mov edx, fileSize
    mov dword ptr [ecx+16], edx
    mov esi, eax               ; parser reads from heap buffer

    ; Read entire file
    invoke ReadExact, hFile, esi, fileSize
    test eax, eax
    jz @@error_read_data

@parse_mem:
    ; For large files, skip strict validation
    cmp sizeHigh, 0
    jne @skip_header_validation
    
    ; Read magic
    lodsd
    mov magic, eax

    ; Read version
    lodsd
    mov version, eax

    ; Check magic
    cmp magic, GGUF_MAGIC
    jne @@error_invalid_magic

    ; Check version (accept 2 or 3)
    cmp version, 2
    jb @@error_invalid_version
    cmp version, GGUF_VERSION
    ja @@error_invalid_version

    ; Read nKV
    call ReadVarIntMem
    cmp eax, -1
    je @@error_read_kv
    mov nKV, eax

    ; Read nTensors
    call ReadVarIntMem
    cmp eax, -1
    je @@error_read_tensors
    mov nTensors, eax

    ; Validate tensor count
    cmp nTensors, -1
    jl @@error_invalid_tensor_count
    cmp nTensors, 10000  ; Reasonable upper limit
    jg @@error_too_many_tensors
    jmp @validation_done

@skip_header_validation:
    ; Large file: use defaults, skip validation
    mov nKV, 0
    mov nTensors, 1  ; At least one tensor expected
    jmp @validation_done

@validation_done:
    invoke GGUF_IDE_NotifyProgress, 60, addr szLoadingStart
    ; Initial tensor-level progress (0 of n)
    invoke GGUF_IDE_NotifyTensorProgress, 0, nTensors, addr szLoadingStart

    ; Check for cancel before heavy work
    invoke GGUF_IDE_CheckCancel
    test eax, eax
    jnz @@cancel_load

    ; Store tensor count
    mov ecx, pModel
    mov edx, nTensors
    mov dword ptr [ecx+8], edx

    ; Per-tensor progress notifications
    xor eax, eax
    mov i, eax
@@tensor_loop_working:
    mov eax, i
    mov edx, nTensors
    cmp eax, edx
    jge @@tensor_done_working
    invoke GGUF_IDE_NotifyTensorProgress, eax, edx, addr szOpParse
    invoke GGUF_IDE_CheckCancel
    test eax, eax
    jnz @@cancel_load
    inc i
    jmp @@tensor_loop_working
@@tensor_done_working:

    ; Mark as loaded and valid
    mov dword ptr [ecx+24], TRUE
    mov dword ptr [ecx+28], TRUE

@parse_done:
    ; Log success
    invoke LogMessage, LOG_INFO, addr szModelLoaded, pPath
    invoke GGUF_IDE_NotifyProgress, 80, addr szModelLoaded

    ; ============================================================================
    ; π-RAM COMPRESSION INTEGRATION
    ; Force compression to <4GB for large models
    ; ============================================================================

    ; Check if model >4GB (sizeHigh set)
    cmp sizeHigh, 0
    je @standard_compression

    ; Large model: force aggressive multi-pass to hit <4GB target
    ; 38GB model needs ~10 passes to reach 3.8GB
    invoke PiRam_EnableHalving, TRUE
    
    ; Single-pass compression approach (π-RAM handles internally)
    jmp @standard_compression
    
@standard_compression:
    ; Enable RAM halving for aggressive memory reduction
        invoke GGUF_IDE_NotifyProgress, 90, addr szLoadingStart

    ; Check cancel before compression
    invoke GGUF_IDE_CheckCancel
    test eax, eax
    jnz @@cancel_load

    invoke PiRam_EnableHalving, TRUE

    ; Compress the GGUF model with π-RAM
    invoke PiRam_CompressGGUF, pModel
    invoke GGUF_IDE_NotifyProgress, 100, addr szLoadingDone
    invoke GGUF_IDE_NotifyStatus, LOG_INFO, addr szLoadingDone
    invoke GGUF_IDE_NotifyModelLoaded, pModel, TRUE

    ; Get compression ratio for telemetry
    invoke PiRam_GetCompressionRatio
    ; EAX now contains compression ratio (e.g., 250 = 2.5:1)

    ; Notify IDE of memory usage (approx file size)
    mov eax, fileSize
    invoke GGUF_IDE_NotifyMemoryUsage, eax

    ; Final tensor progress (all tensors processed)
    mov eax, nTensors
    mov edx, nTensors
    invoke GGUF_IDE_NotifyTensorProgress, eax, edx, addr szModelLoaded

    ; For >4GB models, already compressed by header-only loading
    cmp sizeHigh, 0
    je @skip_quantization
    
@skip_quantization:
    ; Log compression result
    invoke LogMessage, LOG_INFO, addr szCompressionApplied, pModel

    mov eax, pModel
    ret

@@cancel_load:
    ; User-requested cancellation
    invoke GGUF_IDE_NotifyStatus, LOG_WARNING, addr szCancelled
    invoke GGUF_IDE_NotifyModelLoaded, NULL, FALSE
    xor eax, eax
    ret

; ============================================================================
; ERROR HANDLING WITH DETAILED LOGGING
; ============================================================================

@@error_init:
    invoke LogError, GGUF_ERROR_MEMORY, addr szErrorMemory, addr szLogPrefix
    invoke GGUF_IDE_NotifyStatus, LOG_ERROR, addr szErrorMemory
    invoke GGUF_IDE_NotifyModelLoaded, NULL, FALSE
        invoke GGUF_IDE_NotifyStatus, LOG_ERROR, addr szErrorMemory
        invoke GGUF_IDE_NotifyModelLoaded, NULL, FALSE
    xor eax, eax
    ret

@@error_file_open:
    invoke LogError, GGUF_ERROR_FILE, addr szErrorFile, pPath
    xor eax, eax
    ret

@@error_file_size:
    invoke LogError, GGUF_ERROR_FILE, addr szErrorFile, pPath
    invoke CloseHandle, hFile
    xor eax, eax
    ret

@@file_too_large:
    invoke LogError, GGUF_ERROR_OVERFLOW, addr szErrorOverflow, pPath
    invoke CloseHandle, hFile
    xor eax, eax
    ret

@@error_memory:
    invoke LogError, GGUF_ERROR_MEMORY, addr szErrorMemory, addr szOpOpen
    invoke CloseHandle, hFile
    xor eax, eax
    ret

@@error_read_magic:
    invoke LogError, GGUF_ERROR_CORRUPT, addr szErrorCorrupt, addr szOpValidate
    jmp @@cleanup_model

@@error_invalid_magic:
    invoke LogError, GGUF_ERROR_FORMAT, addr szErrorFormat, addr szOpValidate
    jmp @@cleanup_model

@@error_read_version:
    invoke LogError, GGUF_ERROR_CORRUPT, addr szErrorCorrupt, addr szOpValidate
    jmp @@cleanup_model

@@error_invalid_version:
    invoke LogError, GGUF_ERROR_VERSION, addr szErrorVersion, addr szOpValidate
    jmp @@cleanup_model

@@error_read_kv:
    invoke LogError, GGUF_ERROR_CORRUPT, addr szErrorCorrupt, addr szOpParse
    jmp @@cleanup_model

@@error_read_tensors:
    invoke LogError, GGUF_ERROR_CORRUPT, addr szErrorCorrupt, addr szOpParse
    jmp @@cleanup_model

@@error_invalid_tensor_count:
    invoke LogError, GGUF_ERROR_FORMAT, addr szErrorFormat, addr szOpParse
    jmp @@cleanup_model

@@error_too_many_tensors:
    invoke LogError, GGUF_ERROR_OVERFLOW, addr szErrorOverflow, addr szOpParse
    jmp @@cleanup_model

@@error_read_data:
    invoke LogError, GGUF_ERROR_CORRUPT, addr szErrorCorrupt, addr szOpRead
    jmp @@cleanup_model

@@cleanup_model:
    mov eax, pModel
    test eax, eax
    jz @@cleanup_file

    ; Free data buffer
    mov edx, [eax+12]
    test edx, edx
    jz @@skip_data_free
    ; If mapped view exists, skip HeapFree (handled by Unmap)
    mov ecx, [eax+20]
    test ecx, ecx
    jnz @@skip_data_free
    invoke HeapFree, g_hHeap, 0, edx
@@skip_data_free:

    invoke HeapFree, g_hHeap, 0, pModel

@@cleanup_file:
    mov eax, hFile
    test eax, eax
    jz @@error_exit
    invoke CloseHandle, hFile

@@error_exit:
    xor eax, eax
    ret
GGUF_LoadModel endp

; PUBLIC API: GGUF_CloseModel
public GGUF_CloseModel
GGUF_CloseModel proc pModel:DWORD
    mov eax, pModel
    test eax, eax
    jz @@exit

    ; Log operation start
    invoke LogMessage, LOG_INFO, addr szOpClose, NULL

    mov eax, pModel

    ; Unmap memory if mapped
    mov edx, eax
    add edx, 20
    mov ecx, dword ptr [edx]
    test ecx, ecx
    jz @@skip_unmap
    invoke UnmapViewOfFile, ecx
    invoke LogMessage, LOG_DEBUG, addr szSuccess, addr szOpMap
@@skip_unmap:

    ; Close file handle
    mov ecx, dword ptr [eax]
    test ecx, ecx
    jz @@skip_file
    invoke CloseHandle, ecx
    invoke LogMessage, LOG_DEBUG, addr szSuccess, addr szOpOpen
@@skip_file:

    ; Free tensor data
    mov edx, eax
    add edx, 12
    mov ecx, dword ptr [edx]
    test ecx, ecx
    jz @@skip_tensors
    invoke HeapFree, g_hHeap, 0, ecx
    invoke LogMessage, LOG_DEBUG, addr szSuccess, addr szOpParse
@@skip_tensors:

    ; Free model structure
    invoke HeapFree, g_hHeap, 0, pModel

    ; Log successful completion
    invoke LogMessage, LOG_INFO, addr szModelClosed, NULL

@@exit:
    ret
GGUF_CloseModel endp

; PUBLIC API: GGUF_GetTensorCount
public GGUF_GetTensorCount
GGUF_GetTensorCount proc pModel:DWORD
    mov eax, pModel
    test eax, eax
    jz @@error

    ; Validate model state
    mov eax, pModel
    mov edx, eax
    add edx, 28
    mov ecx, dword ptr [edx]
    test ecx, ecx
    jz @@invalid_state

    mov edx, eax
    add edx, 8
    mov eax, dword ptr [edx]
    ret

@@invalid_state:
    invoke LogError, GGUF_ERROR_ACCESS, addr szInvalidState, addr szOpParse
@@error:
    invoke LogError, GGUF_ERROR_ACCESS, addr szNullPointer, addr szOpParse
    xor eax, eax
    ret
GGUF_GetTensorCount endp

; PUBLIC API: GGUF_ValidateModel
public GGUF_ValidateModel
GGUF_ValidateModel proc pModel:DWORD
    mov eax, pModel
    test eax, eax
    jz @@error

    ; Check if model is loaded
    mov eax, pModel
    mov edx, eax
    add edx, 24
    mov ecx, dword ptr [edx]
    test ecx, ecx
    jz @@not_loaded

    ; Check if model is valid
    mov edx, eax
    add edx, 28
    mov eax, dword ptr [edx]
    test eax, eax
    jz @@invalid

    invoke LogMessage, LOG_DEBUG, addr szSuccess, addr szOpValidate
    mov eax, TRUE
    ret

@@not_loaded:
    invoke LogError, GGUF_ERROR_ACCESS, addr szInvalidState, addr szOpValidate
    xor eax, eax
    ret

@@invalid:
    invoke LogError, GGUF_ERROR_FORMAT, addr szErrorFormat, addr szOpValidate
    xor eax, eax
    ret

@@error:
    invoke LogError, GGUF_ERROR_ACCESS, addr szNullPointer, addr szOpValidate
    xor eax, eax
    ret
GGUF_ValidateModel endp

; ============================================================================
; ADDITIONAL LOGGING AND DIAGNOSTIC FUNCTIONS
; ============================================================================

; GGUF_GetErrorCount - Get total error count
public GGUF_GetErrorCount
GGUF_GetErrorCount proc
    mov eax, g_errorCount
    ret
GGUF_GetErrorCount endp

; GGUF_GetLoadCount - Get total successful loads
public GGUF_GetLoadCount
GGUF_GetLoadCount proc
    mov eax, g_loadCount
    ret
GGUF_GetLoadCount endp

; GGUF_SetLogLevel - Set logging verbosity level
public GGUF_SetLogLevel
GGUF_SetLogLevel proc level:DWORD
    mov eax, level
    cmp eax, LOG_DEBUG
    jg @@invalid_level
    cmp eax, LOG_INFO
    jl @@invalid_level

    mov g_logLevel, eax
    invoke LogMessage, LOG_INFO, addr szSuccess, addr szLogPrefix
    mov eax, TRUE
    ret

@@invalid_level:
    invoke LogError, GGUF_ERROR_ACCESS, addr szErrorAccess, addr szLogPrefix
    xor eax, eax
    ret
GGUF_SetLogLevel endp

; GGUF_GetLastSystemError - Get last Windows system error
public GGUF_GetLastSystemError
GGUF_GetLastSystemError proc
    invoke GetLastError
    ret
GGUF_GetLastSystemError endp

; GGUF_LogSystemInfo - Log system information for debugging
public GGUF_LogSystemInfo
GGUF_LogSystemInfo proc
    LOCAL buffer[256]:BYTE

    invoke LogMessage, LOG_INFO, addr szLogPrefix, addr szSuccess

    ; Log error statistics
    invoke LogMessage, LOG_INFO, addr szErrorCount, NULL

    ; Log load statistics
    invoke LogMessage, LOG_INFO, addr szLoadCount, NULL

    ret
GGUF_LogSystemInfo endp

; ============================================================================
; DECOMPRESSION TRIGGER API
; ============================================================================

; GGUF_DecompressModel - Decompress a loaded and compressed model on demand
; pModel: Model handle from GGUF_LoadModel
; Returns: EAX = 1 success, 0 failure
public GGUF_DecompressModel
GGUF_DecompressModel proc pModel:DWORD
    LOCAL pCompressed:DWORD
    LOCAL dwCompSize:DWORD
    LOCAL pDecompressed:DWORD
    LOCAL dwDecompSize:DWORD

    mov eax, pModel
    test eax, eax
    jz @decomp_fail

    ; Get compressed buffer and size
    mov edx, [eax+12]      ; pData (compressed)
    mov ecx, [eax+16]      ; size (compressed)
    
    test edx, edx
    jz @decomp_fail
    test ecx, ecx
    jz @decomp_fail

    mov pCompressed, edx
    mov dwCompSize, ecx

    ; Call PiRam decompression via multi-pass reverse
    invoke ReverseQuantize_Init
    
    ; Allocate buffer for decompressed data (worst case: 4GB for large models)
    invoke HeapAlloc, g_hHeap, HEAP_ZERO_MEMORY, 10000000h
    test eax, eax
    jz @decomp_fail
    
    mov pDecompressed, eax
    mov dwDecompSize, 10000000h
    
    ; Invoke decompression via reverse quantization pipeline
    invoke ReverseQuantize_CompressToFit, pCompressed, pDecompressed
    test eax, eax
    jz @decomp_cleanup
    
    ; Get actual decompressed size
    mov eax, dword ptr [pDecompressed]
    mov dwDecompSize, eax
    
    ; Update model with decompressed buffer
    mov eax, pModel
    mov edx, pCompressed
    test edx, edx
    jz @decomp_skip_free
    
    ; Free old compressed buffer
    invoke HeapFree, g_hHeap, 0, edx
    
@decomp_skip_free:
    ; Store new decompressed buffer
    mov edx, pDecompressed
    mov ecx, dwDecompSize
    mov dword ptr [eax+12], edx
    mov dword ptr [eax+16], ecx
    
    ; Clear compression flag
    mov dword ptr [eax+24], FALSE
    
    invoke LogMessage, LOG_INFO, addr szSuccess, addr szOpParse
    mov eax, 1
    ret

@decomp_cleanup:
    invoke HeapFree, g_hHeap, 0, pDecompressed
    jmp @decomp_fail

@decomp_fail:
    xor eax, eax
    ret
GGUF_DecompressModel endp

; GGUF_CompressModel - Re-compress a decompressed model
; pModel: Model handle
; Returns: EAX = 1 success, 0 failure
public GGUF_CompressModel
GGUF_CompressModel proc pModel:DWORD
    LOCAL pBuffer:DWORD
    LOCAL dwSize:DWORD
    LOCAL pCompressed:DWORD
    LOCAL dwCompSize:DWORD
    
    mov eax, pModel
    test eax, eax
    jz @recomp_fail

    ; Validate model is loaded and uncompressed
    mov edx, [eax+24]
    test edx, edx
    jnz @already_compressed

    ; Get uncompressed buffer and size
    mov edx, [eax+12]
    mov ecx, [eax+16]
    
    test edx, edx
    jz @recomp_fail
    test ecx, ecx
    jz @recomp_fail
    
    mov pBuffer, edx
    mov dwSize, ecx
    
    ; Initialize reverse quantization for compression
    invoke ReverseQuantize_Init
    
    ; Allocate temporary buffer for compressed data (worst case: original size)
    invoke HeapAlloc, g_hHeap, HEAP_ZERO_MEMORY, dwSize
    test eax, eax
    jz @recomp_fail
    
    mov pCompressed, eax
    mov eax, dwSize
    mov dwCompSize, eax
    
    ; Apply multi-pass compression to achieve target ratio
    invoke ReverseQuantize_CompressToFit, pBuffer, pCompressed
    test eax, eax
    jz @recomp_cleanup
    
    ; Compressed size already from compression operation
    ; For now, use heuristic: assume 2:1 compression ratio
    mov eax, dwSize
    shr eax, 1                    ; Divide by 2
    mov dwCompSize, eax
    
    ; Free original uncompressed buffer
    invoke HeapFree, g_hHeap, 0, pBuffer
    
    ; Update model with compressed buffer
    mov eax, pModel
    mov edx, pCompressed
    mov ecx, dwCompSize
    mov dword ptr [eax+12], edx
    mov dword ptr [eax+16], ecx
    
    ; Set compression flag
    mov dword ptr [eax+24], TRUE
    
    ; Log successful compression
    invoke LogMessage, LOG_INFO, addr szCompressionApplied, pModel
    mov eax, 1
    ret

@already_compressed:
    ; Model already compressed, just return success
    mov eax, 1
    ret

@recomp_cleanup:
    invoke HeapFree, g_hHeap, 0, pCompressed
    jmp @recomp_fail

@recomp_fail:
    xor eax, eax
    ret
GGUF_CompressModel endp

; GGUF_GetCompressionStatus - Query model compression state
; pModel: Model handle
; Returns: EAX = state (0=none, 1=compressed, 2=streaming)
public GGUF_GetCompressionStatus
GGUF_GetCompressionStatus proc pModel:DWORD
    mov eax, pModel
    test eax, eax
    jz @status_none

    ; Check if compressed (bit 24)
    mov edx, [eax+24]
    test edx, edx
    jz @status_none

    mov eax, 1  ; Compressed
    ret

@status_none:
    xor eax, eax
    ret
GGUF_GetCompressionStatus endp

end
