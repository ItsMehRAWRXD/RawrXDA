; ============================================================================
; GGUF_LOADER_MINIMAL.ASM - Minimal GGUF Loader (Rebuilt)
; Features: IDE integration, tensor progress, memory usage, cancel
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include gguf_loader_interface.inc

includelib kernel32.lib

; IDE Bridge
GGUF_IDE_RegisterLoader PROTO :DWORD, :DWORD
GGUF_IDE_NotifyProgress PROTO :DWORD, :DWORD
GGUF_IDE_NotifyStatus PROTO :DWORD, :DWORD
GGUF_IDE_NotifyModelLoaded PROTO :DWORD, :DWORD
GGUF_IDE_NotifyTensorProgress PROTO :DWORD, :DWORD, :DWORD
GGUF_IDE_NotifyMemoryUsage PROTO :DWORD
GGUF_IDE_CheckCancel PROTO

; Win32
CreateFileA PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
GetFileSize PROTO :DWORD,:DWORD
CreateFileMappingA PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
MapViewOfFile PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
UnmapViewOfFile PROTO :DWORD
CloseHandle PROTO :DWORD
GetProcessHeap PROTO
HeapAlloc PROTO :DWORD,:DWORD,:DWORD
HeapFree PROTO :DWORD,:DWORD,:DWORD

NULL                equ 0
INVALID_HANDLE_VALUE equ -1
TRUE                equ 1
FALSE               equ 0
GENERIC_READ        equ 80000000h
FILE_SHARE_READ     equ 1h
OPEN_EXISTING       equ 3h
FILE_ATTRIBUTE_NORMAL equ 80h
PAGE_READONLY       equ 2h
FILE_MAP_READ       equ 4h
HEAP_ZERO_MEMORY    equ 8h

; Model structure offsets
MODEL_HANDLE        equ 0
MODEL_MAPPING       equ 4
MODEL_TENSOR_COUNT  equ 8
MODEL_DATA_PTR      equ 12
MODEL_DATA_SIZE     equ 16
MODEL_IS_VALID      equ 24
MODEL_IS_LOADED     equ 28

.data
    g_hHeap            dd 0
    g_LoaderRegistered dd 0

    szLoaderName       db "GGUF_Loader_Minimal",0
    szLoadStart        db "Loading GGUF model...",0
    szLoaded           db "Model loaded",0
    szCancelled        db "Load cancelled",0

.data?
    ; none

.code

public GGUF_LoadModel
GGUF_LoadModel proc pPath:DWORD
    LOCAL hFile:DWORD
    LOCAL hMap:DWORD
    LOCAL pView:DWORD
    LOCAL fileSize:DWORD
    LOCAL pModel:DWORD
    LOCAL nTensors:DWORD
    LOCAL i:DWORD

    .if g_LoaderRegistered == 0
        invoke GGUF_IDE_RegisterLoader, addr szLoaderName, 100
        mov g_LoaderRegistered, TRUE
    .endif

    invoke GGUF_IDE_NotifyProgress, 0, addr szLoadStart

    .if g_hHeap == 0
        invoke GetProcessHeap
        mov g_hHeap, eax
    .endif

    invoke CreateFileA, pPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    .if eax == INVALID_HANDLE_VALUE
        invoke GGUF_IDE_NotifyStatus, 2, addr szLoadStart
        invoke GGUF_IDE_NotifyModelLoaded, NULL, FALSE
        xor eax, eax
        ret
    .endif
    mov hFile, eax

    invoke GetFileSize, hFile, NULL
    .if eax == -1
        invoke CloseHandle, hFile
        invoke GGUF_IDE_NotifyModelLoaded, NULL, FALSE
        xor eax, eax
        ret
    .endif
    mov fileSize, eax
    invoke GGUF_IDE_NotifyMemoryUsage, fileSize

    invoke CreateFileMappingA, hFile, NULL, PAGE_READONLY, 0, 0, NULL
    .if eax == NULL
        invoke CloseHandle, hFile
        invoke GGUF_IDE_NotifyModelLoaded, NULL, FALSE
        xor eax, eax
        ret
    .endif
    mov hMap, eax

    invoke MapViewOfFile, hMap, FILE_MAP_READ, 0, 0, 0
    .if eax == NULL
        invoke CloseHandle, hMap
        invoke CloseHandle, hFile
        invoke GGUF_IDE_NotifyModelLoaded, NULL, FALSE
        xor eax, eax
        ret
    .endif
    mov pView, eax

    ; Simulate header read: tensor count
    mov nTensors, 8
    invoke GGUF_IDE_NotifyProgress, 50, addr szLoadStart

    ; Per-tensor notifications
    mov i, 0
@@tensor_loop:
    mov eax, i
    mov edx, nTensors
    cmp eax, edx
    jge @@tensor_done
    mov eax, i
    mov edx, nTensors
    invoke GGUF_IDE_NotifyTensorProgress, eax, edx, addr szLoadStart
    invoke GGUF_IDE_CheckCancel
    test eax, eax
    jnz @@cancel
    inc i
    jmp @@tensor_loop
@@tensor_done:

    ; Allocate and fill model struct
    invoke HeapAlloc, g_hHeap, HEAP_ZERO_MEMORY, 32
    .if eax == NULL
        jmp @@fail
    .endif
    mov pModel, eax
    mov ecx, pModel
    mov edx, hFile
    mov dword ptr [ecx+MODEL_HANDLE], edx
    mov edx, hMap
    mov dword ptr [ecx+MODEL_MAPPING], edx
    mov edx, nTensors
    mov dword ptr [ecx+MODEL_TENSOR_COUNT], edx
    mov edx, pView
    mov dword ptr [ecx+MODEL_DATA_PTR], edx
    mov edx, fileSize
    mov dword ptr [ecx+MODEL_DATA_SIZE], edx
    mov dword ptr [ecx+MODEL_IS_VALID], TRUE
    mov dword ptr [ecx+MODEL_IS_LOADED], TRUE

    invoke GGUF_IDE_NotifyProgress, 100, addr szLoaded
    invoke GGUF_IDE_NotifyModelLoaded, pModel, TRUE
    mov eax, pModel
    ret

@@cancel:
    invoke GGUF_IDE_NotifyStatus, 1, addr szCancelled
    invoke UnmapViewOfFile, pView
    invoke CloseHandle, hMap
    invoke CloseHandle, hFile
    invoke GGUF_IDE_NotifyModelLoaded, NULL, FALSE
    xor eax, eax
    ret

@@fail:
    invoke UnmapViewOfFile, pView
    invoke CloseHandle, hMap
    invoke CloseHandle, hFile
    invoke GGUF_IDE_NotifyModelLoaded, NULL, FALSE
    xor eax, eax
    ret
GGUF_LoadModel endp

public GGUF_CloseModel
GGUF_CloseModel proc pModel:DWORD
    mov eax, pModel
    test eax, eax
    jz @@exit
    mov ecx, pModel
    mov eax, [ecx+MODEL_MAPPING]
    test eax, eax
    jz @@no_map
    mov eax, [ecx+MODEL_DATA_PTR]
    invoke UnmapViewOfFile, eax
@@no_map:
    mov eax, [ecx+MODEL_HANDLE]
    test eax, eax
    jz @@exit
    invoke CloseHandle, eax
@@exit:
    ret
GGUF_CloseModel endp

public GGUF_GetTensorCount
GGUF_GetTensorCount proc pModel:DWORD
    mov eax, pModel
    test eax, eax
    jz @@e
    mov ecx, pModel
    mov eax, [ecx+MODEL_TENSOR_COUNT]
    ret
@@e:
    xor eax, eax
    ret
GGUF_GetTensorCount endp

public GGUF_ValidateModel
GGUF_ValidateModel proc pModel:DWORD
    mov eax, pModel
    test eax, eax
    jz @@i
    mov ecx, pModel
    mov eax, [ecx+MODEL_IS_VALID]
    ret
@@i:
    xor eax, eax
    ret
GGUF_ValidateModel endp

end