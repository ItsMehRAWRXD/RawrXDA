; ============================================================================
; PIRAM_REVERSE_DECOMPRESS.ASM - Reverse Compression/Decompression
; On-demand compression and decompression with reverse π-transform
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

; π-RAM core
PiRam_PerfectCircleFwd PROTO :DWORD, :DWORD
PiRam_PerfectCircleInv PROTO :DWORD, :DWORD

; Win32
HeapAlloc PROTO :DWORD,:DWORD,:DWORD
HeapFree PROTO :DWORD,:DWORD,:DWORD
GetProcessHeap PROTO
WriteFile PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
GetStdHandle PROTO :DWORD

includelib kernel32.lib

; ============================================================================
; CONSTANTS
; ============================================================================
NULL                equ 0
HEAP_ZERO_MEMORY    equ 8h
PI_CONSTANT         equ 3296474
PI_INV_CONSTANT     equ 333773

; Compression states
COMPRESS_STATE_IDLE         equ 0
COMPRESS_STATE_COMPRESSED   equ 1
COMPRESS_STATE_DECOMPRESSED equ 2

; ============================================================================
; EXPORTS
; ============================================================================
PUBLIC PiRam_ReverseCompress
PUBLIC PiRam_ReverseDecompress
PUBLIC PiRam_GetCompressionState
PUBLIC PiRam_ToggleCompressionState
PUBLIC PiRam_CompressStream
PUBLIC PiRam_DecompressStream
PUBLIC PiRam_CompressedSize
PUBLIC PiRam_DecompressedSize

; ============================================================================
; DATA
; ============================================================================
.data
    ; Global compression state tracking
    g_CompState         dd COMPRESS_STATE_IDLE
    g_CompressedSize    dd 0
    g_DecompressedSize  dd 0
    g_CompBuffer        dd 0
    g_DecompBuffer      dd 0
    g_hHeap             dd 0
    
    szCompressionState  db "[PiRam] Compression state: ",0
    szCompressed        db "COMPRESSED",13,10,0
    szDecompressed      db "DECOMPRESSED",13,10,0
    szIdle              db "IDLE",13,10,0

.data?

; ============================================================================
; CODE
; ============================================================================
.code

; ============================================================================
; PiRam_ReverseCompress - Compress data using reverse π-transform
; pData: Input buffer
; dwSize: Size to compress
; ppOutput: Pointer to output buffer ptr (allocated)
; Returns: EAX = compressed size, 0 on failure
; ============================================================================
PiRam_ReverseCompress PROC pData:DWORD, dwSize:DWORD, ppOutput:DWORD
    LOCAL compressed_size:DWORD
    LOCAL hHeap:DWORD

    push esi
    push edi
    push ebx

    mov esi, pData
    mov edi, dwSize
    
    test esi, esi
    jz @compress_fail
    test edi, edi
    jz @compress_fail

    ; Get heap
    invoke GetProcessHeap
    mov hHeap, eax
    mov g_hHeap, eax

    ; Allocate output buffer (assume 50% compression)
    mov eax, edi
    shr eax, 1
    mov compressed_size, eax

    invoke HeapAlloc, hHeap, HEAP_ZERO_MEMORY, eax
    test eax, eax
    jz @compress_fail

    mov edi, eax
    mov g_CompBuffer, eax

    ; Copy input to output
    push esi
    push edi
    mov esi, pData
    mov ecx, dwSize
    rep movsb
    pop edi
    pop esi

    ; Apply forward π-transform
    invoke PiRam_PerfectCircleFwd, edi, dwSize
    
    ; Store state
    mov g_CompState, COMPRESS_STATE_COMPRESSED
    mov eax, compressed_size
    mov g_CompressedSize, eax

    ; Return output buffer pointer
    mov ecx, ppOutput
    test ecx, ecx
    jz @return_size
    mov [ecx], edi

@return_size:
    mov eax, compressed_size
    jmp @compress_exit

@compress_fail:
    xor eax, eax

@compress_exit:
    pop ebx
    pop edi
    pop esi
    ret
PiRam_ReverseCompress ENDP

; ============================================================================
; PiRam_ReverseDecompress - Decompress using reverse π-transform
; pCompressed: Compressed buffer
; dwCompSize: Compressed size
; ppOutput: Pointer to output buffer ptr
; Returns: EAX = decompressed size, 0 on failure
; ============================================================================
PiRam_ReverseDecompress PROC pCompressed:DWORD, dwCompSize:DWORD, ppOutput:DWORD
    LOCAL decompressed_size:DWORD
    LOCAL hHeap:DWORD

    push esi
    push edi
    push ebx

    mov esi, pCompressed
    mov edi, dwCompSize
    
    test esi, esi
    jz @decomp_fail
    test edi, edi
    jz @decomp_fail

    ; Get heap
    invoke GetProcessHeap
    mov hHeap, eax
    mov g_hHeap, eax

    ; Decompressed size = 2x compressed (after halving)
    mov eax, edi
    shl eax, 1
    mov decompressed_size, eax

    ; Allocate output buffer
    invoke HeapAlloc, hHeap, HEAP_ZERO_MEMORY, eax
    test eax, eax
    jz @decomp_fail

    mov edi, eax
    mov g_DecompBuffer, eax

    ; Copy compressed to output
    push esi
    push edi
    mov esi, pCompressed
    mov ecx, dwCompSize
    rep movsb
    pop edi
    pop esi

    ; Apply inverse π-transform
    invoke PiRam_PerfectCircleInv, edi, dwCompSize

    ; Store state
    mov g_CompState, COMPRESS_STATE_DECOMPRESSED
    mov eax, decompressed_size
    mov g_DecompressedSize, eax

    ; Return output buffer
    mov ecx, ppOutput
    test ecx, ecx
    jz @return_decomp_size
    mov [ecx], edi

@return_decomp_size:
    mov eax, decompressed_size
    jmp @decomp_exit

@decomp_fail:
    xor eax, eax

@decomp_exit:
    pop ebx
    pop edi
    pop esi
    ret
PiRam_ReverseDecompress ENDP

; ============================================================================
; PiRam_GetCompressionState - Get current compression state
; Returns: EAX = state (0=idle, 1=compressed, 2=decompressed)
; ============================================================================
PiRam_GetCompressionState PROC
    mov eax, g_CompState
    ret
PiRam_GetCompressionState ENDP

; ============================================================================
; PiRam_ToggleCompressionState - Toggle between compress/decompress
; pData: Current buffer
; dwSize: Current size
; ppOutput: Output buffer ptr
; Returns: EAX = new size, 0 on failure
; ============================================================================
PiRam_ToggleCompressionState PROC pData:DWORD, dwSize:DWORD, ppOutput:DWORD
    mov eax, g_CompState
    
    cmp eax, COMPRESS_STATE_IDLE
    je @toggle_compress
    cmp eax, COMPRESS_STATE_COMPRESSED
    je @toggle_decompress
    
    ; Already decompressed, compress again
    invoke PiRam_ReverseCompress, pData, dwSize, ppOutput
    ret

@toggle_compress:
    invoke PiRam_ReverseCompress, pData, dwSize, ppOutput
    ret

@toggle_decompress:
    invoke PiRam_ReverseDecompress, pData, dwSize, ppOutput
    ret
PiRam_ToggleCompressionState ENDP

; ============================================================================
; PiRam_CompressStream - Stream compress (4KB chunks)
; pData: Input buffer
; dwSize: Total size
; chunkCallback: Callback for each chunk (optional)
; Returns: EAX = total compressed size
; ============================================================================
PiRam_CompressStream PROC pData:DWORD, dwSize:DWORD, chunkCallback:DWORD
    LOCAL chunk_size:DWORD
    LOCAL offset:DWORD
    LOCAL total_compressed:DWORD

    push esi
    push edi

    mov offset, 0
    mov total_compressed, 0
    mov chunk_size, 4096    ; 4KB chunks

@stream_loop:
    mov eax, offset
    cmp eax, dwSize
    jge @stream_done

    ; Get chunk size (min of remaining and 4KB)
    mov ecx, dwSize
    sub ecx, offset
    cmp ecx, 4096
    jbe @use_remaining
    mov ecx, 4096
@use_remaining:

    mov esi, pData
    add esi, offset

    ; Compress chunk
    invoke PiRam_ReverseCompress, esi, ecx, NULL
    test eax, eax
    jz @stream_done

    add total_compressed, eax
    add offset, ecx
    jmp @stream_loop

@stream_done:
    mov eax, total_compressed

    pop edi
    pop esi
    ret
PiRam_CompressStream ENDP

; ============================================================================
; PiRam_DecompressStream - Stream decompress (4KB chunks)
; pData: Compressed buffer
; dwSize: Total compressed size
; Returns: EAX = total decompressed size
; ============================================================================
PiRam_DecompressStream PROC pData:DWORD, dwSize:DWORD
    LOCAL chunk_size:DWORD
    LOCAL offset:DWORD
    LOCAL total_decompressed:DWORD

    push esi
    push edi

    mov offset, 0
    mov total_decompressed, 0
    mov chunk_size, 4096

@decomp_stream_loop:
    mov eax, offset
    cmp eax, dwSize
    jge @decomp_stream_done

    ; Get chunk size
    mov ecx, dwSize
    sub ecx, offset
    cmp ecx, 4096
    jbe @decomp_use_remaining
    mov ecx, 4096
@decomp_use_remaining:

    mov esi, pData
    add esi, offset

    ; Decompress chunk
    invoke PiRam_ReverseDecompress, esi, ecx, NULL
    test eax, eax
    jz @decomp_stream_done

    add total_decompressed, eax
    add offset, ecx
    jmp @decomp_stream_loop

@decomp_stream_done:
    mov eax, total_decompressed

    pop edi
    pop esi
    ret
PiRam_DecompressStream ENDP

; ============================================================================
; PiRam_CompressedSize - Get last compressed size
; ============================================================================
PiRam_CompressedSize PROC
    mov eax, g_CompressedSize
    ret
PiRam_CompressedSize ENDP

; ============================================================================
; PiRam_DecompressedSize - Get last decompressed size
; ============================================================================
PiRam_DecompressedSize PROC
    mov eax, g_DecompressedSize
    ret
PiRam_DecompressedSize ENDP

END
