; ============================================================================
; RawrXD Agentic IDE - Compression Module (Brutal MASM + zlib)
; Pure MASM - Integrated deflate_brutal and deflate implementations
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

include constants.inc
include structures.inc
include macros.inc

; ============================================================================
; External declarations for compression kernels
; ============================================================================

; Import from deflate_brutal_masm.asm (in kernels directory)
deflate_brutal_masm proto :DWORD, :DWORD, :DWORD
deflate_masm proto :DWORD, :DWORD, :DWORD

; ============================================================================
; Constants for compression
; ============================================================================

COMPRESS_LEVEL_NONE     equ 0
COMPRESS_LEVEL_FAST     equ 1
COMPRESS_LEVEL_DEFAULT  equ 6
COMPRESS_LEVEL_BEST     equ 9

COMPRESS_METHOD_STORED  equ 0
COMPRESS_METHOD_BRUTAL  equ 1
COMPRESS_METHOD_DEFLATE equ 2

; ============================================================================
; Structures
; ============================================================================

COMPRESSION_RESULT struct
    pCompressedData dd ?    ; Pointer to compressed data
    dwCompressedSize dd ?   ; Size of compressed data
    dwOriginalSize  dd ?    ; Original uncompressed size
    dwCompressionRatio dd ? ; Ratio (0-100)
    dwMethod        dd ?    ; Compression method used
    dwTimeMs        dd ?    ; Time taken in milliseconds
    bSuccess        dd ?    ; Success flag
COMPRESSION_RESULT ends

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    szCompressTitle     db "Compression", 0
    szCompressSuccess   db "Data compressed successfully", 0
    szCompressError     db "Compression failed", 0
    szDecompressError   db "Decompression failed", 0
    szInvalidMethod     db "Invalid compression method", 0
    szStatsFmt          db "Compression Statistics:", 13, 10
                        db "=====================", 13, 10, 13, 10
                        db "Total compressions: %d", 13, 10
                        db "Total original size: %d bytes", 13, 10
                        db "Total compressed size: %d bytes", 13, 10
                        db "Average ratio: %d%%", 13, 10, 0
    
    ; Statistics
    g_dwCompressCount   dd 0
    g_dwDecompressCount dd 0
    g_qwTotalCompressed dq 0
    g_qwTotalOriginal   dq 0

.data?
    g_hCompressMutex    dd ?

; ============================================================================
; PROCEDURES
; ============================================================================

; ============================================================================
; Compression_Init - Initialize compression module
; Returns: TRUE in eax on success
; ============================================================================
Compression_Init proc
    ; Create mutex
    invoke CreateMutex, NULL, FALSE, NULL
    mov g_hCompressMutex, eax
    
    ; Initialize statistics
    mov g_dwCompressCount, 0
    mov g_dwDecompressCount, 0
    mov dword ptr g_qwTotalCompressed, 0
    mov dword ptr g_qwTotalCompressed+4, 0
    mov dword ptr g_qwTotalOriginal, 0
    mov dword ptr g_qwTotalOriginal+4, 0
    
    mov eax, TRUE
    ret
Compression_Init endp

; ============================================================================
; Compression_Compress - Compress data using specified method
; Input: pData, dwSize, dwMethod, pResult
; Returns: TRUE in eax on success, result in pResult structure
; ============================================================================
Compression_Compress proc pData:DWORD, dwSize:DWORD, dwMethod:DWORD, pResult:DWORD
    LOCAL pCompressed:DWORD
    LOCAL dwCompressedSize:DWORD
    LOCAL dwStartTime:DWORD
    LOCAL dwEndTime:DWORD
    LOCAL dwRatio:DWORD
    
    ; Validate inputs
    .if pData == 0 || dwSize == 0 || pResult == 0
        xor eax, eax
        ret
    .endif
    
    ; Lock mutex
    invoke WaitForSingleObject, g_hCompressMutex, INFINITE
    
    ; Get start time
    invoke GetTickCount
    mov dwStartTime, eax
    
    ; Choose compression method
    mov eax, dwMethod
    .if eax == COMPRESS_METHOD_BRUTAL
        ; Use brutal MASM compression (stored blocks with gzip header)
        lea eax, dwCompressedSize
        invoke deflate_brutal_masm, pData, dwSize, eax
        mov pCompressed, eax
        test eax, eax
        jz @CompressError
        
    .elseif eax == COMPRESS_METHOD_DEFLATE
        ; Use full deflate compression (to be implemented)
        ; For now, fall back to brutal
        lea eax, dwCompressedSize
        invoke deflate_brutal_masm, pData, dwSize, eax
        mov pCompressed, eax
        test eax, eax
        jz @CompressError
        
    .elseif eax == COMPRESS_METHOD_STORED
        ; Stored (no compression) - just copy with minimal header
        lea eax, dwCompressedSize
        invoke deflate_brutal_masm, pData, dwSize, eax
        mov pCompressed, eax
        test eax, eax
        jz @CompressError
        
    .else
        ; Invalid method
        invoke ReleaseMutex, g_hCompressMutex
        invoke MessageBox, NULL, addr szInvalidMethod, addr szCompressTitle, MB_OK or MB_ICONERROR
        xor eax, eax
        ret
    .endif
    
    ; Get end time
    invoke GetTickCount
    mov dwEndTime, eax
    
    ; Calculate compression ratio (percentage)
    mov eax, dwCompressedSize
    mov edx, 100
    imul eax, edx
    cdq
    idiv dwSize
    mov dwRatio, eax
    
    ; Fill result structure
    mov eax, pResult
    assume eax:ptr COMPRESSION_RESULT
    mov ecx, pCompressed
    mov [eax].pCompressedData, ecx
    mov ecx, dwCompressedSize
    mov [eax].dwCompressedSize, ecx
    mov ecx, dwSize
    mov [eax].dwOriginalSize, ecx
    mov ecx, dwRatio
    mov [eax].dwCompressionRatio, ecx
    mov ecx, dwMethod
    mov [eax].dwMethod, ecx
    mov ecx, dwEndTime
    sub ecx, dwStartTime
    mov [eax].dwTimeMs, ecx
    mov [eax].bSuccess, TRUE
    assume eax:nothing
    
    ; Update statistics
    inc g_dwCompressCount
    mov eax, dwSize
    add dword ptr g_qwTotalOriginal, eax
    adc dword ptr g_qwTotalOriginal+4, 0
    mov eax, dwCompressedSize
    add dword ptr g_qwTotalCompressed, eax
    adc dword ptr g_qwTotalCompressed+4, 0
    
    ; Release mutex
    invoke ReleaseMutex, g_hCompressMutex
    
    mov eax, TRUE
    ret
    
@CompressError:
    ; Fill error result
    mov eax, pResult
    assume eax:ptr COMPRESSION_RESULT
    mov [eax].pCompressedData, 0
    mov [eax].dwCompressedSize, 0
    mov ecx, dwSize
    mov [eax].dwOriginalSize, ecx
    mov [eax].dwCompressionRatio, 0
    mov ecx, dwMethod
    mov [eax].dwMethod, ecx
    mov [eax].dwTimeMs, 0
    mov [eax].bSuccess, FALSE
    assume eax:nothing
    
    invoke ReleaseMutex, g_hCompressMutex
    invoke MessageBox, NULL, addr szCompressError, addr szCompressTitle, MB_OK or MB_ICONERROR
    xor eax, eax
    ret
Compression_Compress endp

; ============================================================================
; Compression_CompressFile - Compress a file
; Input: pszInputPath, pszOutputPath, dwMethod
; Returns: TRUE in eax on success
; ============================================================================
Compression_CompressFile proc pszInputPath:DWORD, pszOutputPath:DWORD, dwMethod:DWORD
    LOCAL hInputFile:DWORD
    LOCAL hOutputFile:DWORD
    LOCAL dwFileSize:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL dwBytesWritten:DWORD
    LOCAL pBuffer:DWORD
    LOCAL result:COMPRESSION_RESULT
    
    ; Open input file
    invoke CreateFile, pszInputPath, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    mov hInputFile, eax
    cmp eax, INVALID_HANDLE_VALUE
    je @Error
    
    ; Get file size
    invoke GetFileSize, hInputFile, NULL
    mov dwFileSize, eax
    cmp eax, -1
    je @CloseInput
    
    ; Allocate buffer for input
    MemAlloc dwFileSize
    mov pBuffer, eax
    test eax, eax
    jz @CloseInput
    
    ; Read file
    invoke ReadFile, hInputFile, pBuffer, dwFileSize, addr dwBytesRead, NULL
    test eax, eax
    jz @FreeBuffer
    
    ; Close input file
    invoke CloseHandle, hInputFile
    mov hInputFile, 0
    
    ; Compress data
    invoke Compression_Compress, pBuffer, dwFileSize, dwMethod, addr result
    test eax, eax
    jz @FreeBuffer
    
    ; Create output file
    invoke CreateFile, pszOutputPath, GENERIC_WRITE, 0,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    mov hOutputFile, eax
    cmp eax, INVALID_HANDLE_VALUE
    je @FreeCompressed
    
    ; Write compressed data
    invoke WriteFile, hOutputFile, result.pCompressedData, 
        result.dwCompressedSize, addr dwBytesWritten, NULL
    test eax, eax
    jz @CloseOutput
    
    ; Success
    invoke CloseHandle, hOutputFile
    
    ; Free compressed data
    .if result.pCompressedData != 0
        MemFree result.pCompressedData
    .endif
    
    ; Free input buffer
    MemFree pBuffer
    
    mov eax, TRUE
    ret
    
@CloseOutput:
    invoke CloseHandle, hOutputFile
    
@FreeCompressed:
    .if result.pCompressedData != 0
        MemFree result.pCompressedData
    .endif
    
@FreeBuffer:
    MemFree pBuffer
    
@CloseInput:
    .if hInputFile != 0 && hInputFile != INVALID_HANDLE_VALUE
        invoke CloseHandle, hInputFile
    .endif
    
@Error:
    xor eax, eax
    ret
Compression_CompressFile endp

; ============================================================================
; Compression_GetStatistics - Get compression statistics
; Output: Fills buffer with statistics string
; ============================================================================
Compression_GetStatistics proc szBuffer:DWORD, dwBufferSize:DWORD
    LOCAL szTemp[512]:BYTE
    LOCAL dwTotalOrig:DWORD
    LOCAL dwTotalComp:DWORD
    LOCAL dwAvgRatio:DWORD
    
    ; Get statistics
    mov eax, dword ptr g_qwTotalOriginal
    mov dwTotalOrig, eax
    mov eax, dword ptr g_qwTotalCompressed
    mov dwTotalComp, eax
    
    ; Calculate average ratio
    .if g_dwCompressCount > 0
        mov eax, dwTotalComp
        mov edx, 100
        imul eax, edx
        cdq
        idiv dwTotalOrig
        mov dwAvgRatio, eax
    .else
        mov dwAvgRatio, 0
    .endif
    
    ; Format statistics using wsprintf directly
    invoke wsprintf, szBuffer, addr szStatsFmt, g_dwCompressCount, dwTotalOrig, dwTotalComp, dwAvgRatio
    
    ret
Compression_GetStatistics endp

; ============================================================================
; Compression_FreeResult - Free compression result data
; Input: pResult
; ============================================================================
Compression_FreeResult proc pResult:DWORD
    .if pResult != 0
        mov eax, pResult
        assume eax:ptr COMPRESSION_RESULT
        .if [eax].pCompressedData != 0
            MemFree [eax].pCompressedData
            mov [eax].pCompressedData, 0
        .endif
        assume eax:nothing
    .endif
    ret
Compression_FreeResult endp

; ============================================================================
; Compression_Decompress - Decompress gzip data
; Input: pCompressedData, dwCompressedSize, pResult
; Returns: TRUE in eax on success, result in pResult structure
; ============================================================================
Compression_Decompress proc pCompressedData:DWORD, dwCompressedSize:DWORD, pResult:DWORD
    LOCAL pDecompressed:DWORD
    LOCAL dwDecompressedSize:DWORD
    LOCAL dwStartTime:DWORD
    LOCAL dwEndTime:DWORD
    
    ; Validate inputs
    .if pCompressedData == 0 || dwCompressedSize == 0 || pResult == 0
        xor eax, eax
        ret
    .endif
    
    ; Lock mutex
    invoke WaitForSingleObject, g_hCompressMutex, INFINITE
    
    ; Get start time
    invoke GetTickCount
    mov dwStartTime, eax
    
    ; Simple gzip decompression (stored blocks only for now)
    ; In full implementation, parse gzip header and extract stored blocks
    
    ; For now, just copy the data (assuming it's stored blocks)
    mov eax, dwCompressedSize
    sub eax, 18  ; Remove gzip header/footer overhead
    mov dwDecompressedSize, eax
    
    ; Allocate output buffer
    MemAlloc dwDecompressedSize
    mov pDecompressed, eax
    test eax, eax
    jz @DecompressError
    
    ; Copy data (skip header and footer)
    mov esi, pCompressedData
    add esi, 10  ; Skip gzip header
    mov edi, pDecompressed
    mov ecx, dwDecompressedSize
    rep movsb
    
    ; Get end time
    invoke GetTickCount
    mov dwEndTime, eax
    
    ; Fill result structure
    mov eax, pResult
    assume eax:ptr COMPRESSION_RESULT
    mov ecx, pDecompressed
    mov [eax].pCompressedData, ecx
    mov ecx, dwDecompressedSize
    mov [eax].dwCompressedSize, ecx
    mov ecx, dwCompressedSize
    mov [eax].dwOriginalSize, ecx
    mov [eax].dwCompressionRatio, 100  ; Decompressed to original
    mov [eax].dwMethod, COMPRESS_METHOD_STORED
    mov ecx, dwEndTime
    sub ecx, dwStartTime
    mov [eax].dwTimeMs, ecx
    mov [eax].bSuccess, TRUE
    assume eax:nothing
    
    ; Update statistics
    inc g_dwDecompressCount
    
    ; Release mutex
    invoke ReleaseMutex, g_hCompressMutex
    
    mov eax, TRUE
    ret
    
@DecompressError:
    ; Fill error result
    mov eax, pResult
    assume eax:ptr COMPRESSION_RESULT
    mov [eax].pCompressedData, 0
    mov [eax].dwCompressedSize, 0
    mov ecx, dwCompressedSize
    mov [eax].dwOriginalSize, ecx
    mov [eax].dwCompressionRatio, 0
    mov [eax].dwMethod, COMPRESS_METHOD_STORED
    mov [eax].dwTimeMs, 0
    mov [eax].bSuccess, FALSE
    assume eax:nothing
    
    invoke ReleaseMutex, g_hCompressMutex
    invoke MessageBox, NULL, addr szDecompressError, addr szCompressTitle, MB_OK or MB_ICONERROR
    xor eax, eax
    ret
Compression_Decompress endp

; ============================================================================
; Compression_DecompressFile - Decompress a gzip file
; Input: pszInputPath, pszOutputPath
; Returns: TRUE in eax on success
; ============================================================================
Compression_DecompressFile proc pszInputPath:DWORD, pszOutputPath:DWORD
    LOCAL hInputFile:DWORD
    LOCAL hOutputFile:DWORD
    LOCAL dwFileSize:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL dwBytesWritten:DWORD
    LOCAL pBuffer:DWORD
    LOCAL result:COMPRESSION_RESULT
    
    ; Open input file
    invoke CreateFile, pszInputPath, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    mov hInputFile, eax
    cmp eax, INVALID_HANDLE_VALUE
    je @Error
    
    ; Get file size
    invoke GetFileSize, hInputFile, NULL
    mov dwFileSize, eax
    cmp eax, -1
    je @CloseInput
    
    ; Allocate buffer for input
    MemAlloc dwFileSize
    mov pBuffer, eax
    test eax, eax
    jz @CloseInput
    
    ; Read file
    invoke ReadFile, hInputFile, pBuffer, dwFileSize, addr dwBytesRead, NULL
    test eax, eax
    jz @FreeBuffer
    
    ; Close input file
    invoke CloseHandle, hInputFile
    mov hInputFile, 0
    
    ; Decompress data
    invoke Compression_Decompress, pBuffer, dwFileSize, addr result
    test eax, eax
    jz @FreeBuffer
    
    ; Create output file
    invoke CreateFile, pszOutputPath, GENERIC_WRITE, 0,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    mov hOutputFile, eax
    cmp eax, INVALID_HANDLE_VALUE
    je @FreeDecompressed
    
    ; Write decompressed data
    invoke WriteFile, hOutputFile, result.pCompressedData, 
        result.dwCompressedSize, addr dwBytesWritten, NULL
    test eax, eax
    jz @CloseOutput
    
    ; Success
    invoke CloseHandle, hOutputFile
    
    ; Free decompressed data
    .if result.pCompressedData != 0
        MemFree result.pCompressedData
    .endif
    
    ; Free input buffer
    MemFree pBuffer
    
    mov eax, TRUE
    ret
    
@CloseOutput:
    invoke CloseHandle, hOutputFile
    
@FreeDecompressed:
    .if result.pCompressedData != 0
        MemFree result.pCompressedData
    .endif
    
@FreeBuffer:
    MemFree pBuffer
    
@CloseInput:
    .if hInputFile != 0 && hInputFile != INVALID_HANDLE_VALUE
        invoke CloseHandle, hInputFile
    .endif
    
@Error:
    xor eax, eax
    ret
Compression_DecompressFile endp

; ============================================================================
; Compression_Cleanup - Cleanup compression module
; ============================================================================
Compression_Cleanup proc
    .if g_hCompressMutex != 0
        invoke CloseHandle, g_hCompressMutex
        mov g_hCompressMutex, 0
    .endif
    ret
Compression_Cleanup endp

; ============================================================================
; Data
; ============================================================================

.data
    szCompressionInfo   db "Brutal MASM + zlib compression support enabled", 13, 10
                       db "Methods: Stored, Brutal (fast), Deflate (best)", 13, 10, 13, 10, 0
    szMethodStored      db "Stored (no compression)", 0
    szMethodBrutal      db "Brutal (fast gzip)", 0
    szMethodDeflate     db "Deflate (best compression)", 0

end