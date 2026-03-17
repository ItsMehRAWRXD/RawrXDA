; piram_streaming_to_disc.asm - Streaming Engine for Disc I/O
; NEXT WEEK Task #3: High-performance streaming with async I/O
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC StreamDisc_Init
PUBLIC StreamDisc_OpenStream
PUBLIC StreamDisc_WriteChunk
PUBLIC StreamDisc_ReadChunk
PUBLIC StreamDisc_CloseStream
PUBLIC StreamDisc_SetBufferSize
PUBLIC StreamDisc_EnableAsync
PUBLIC StreamDisc_GetProgress

; Stream modes
STREAM_MODE_SEQUENTIAL  EQU 1
STREAM_MODE_RANDOM      EQU 2
STREAM_MODE_BUFFERED    EQU 3
STREAM_MODE_ASYNC       EQU 4

; Stream structure
StreamContext STRUCT
    hFile           dd ?
    mode            dd ?
    cbBuffer        dd ?
    pBuffer         dd ?
    offset          dq ?
    totalSize       dq ?
    bytesWritten    dq ?
    bytesRead       dq ?
    bAsync          dd ?
    hAsyncEvent     dd ?
    overlapped      OVERLAPPED <>
StreamContext ENDS

.data
MAX_STREAMS EQU 16

g_Streams StreamContext MAX_STREAMS DUP(<0,0,0,0,0,0,0,0,0,0>)
g_StreamCount dd 0

; Default buffer size: 4MB for optimal performance
DEFAULT_BUFFER_SIZE EQU 4194304

; Stream statistics
g_Stats_TotalWritten    dq 0
g_Stats_TotalRead       dq 0
g_Stats_ActiveStreams   dd 0

szStreamError db "Stream I/O error",0

.code

; ============================================================
; StreamDisc_Init - Initialize streaming system
; Output: EAX = 1 success
; ============================================================
StreamDisc_Init PROC
    push ebx
    
    ; Clear all streams
    mov ecx, MAX_STREAMS
    lea ebx, g_Streams
    
@clear_loop:
    mov dword ptr [ebx], 0
    add ebx, SIZEOF StreamContext
    loop @clear_loop
    
    mov [g_StreamCount], 0
    mov dword ptr [g_Stats_TotalWritten], 0
    mov dword ptr [g_Stats_TotalWritten + 4], 0
    mov dword ptr [g_Stats_TotalRead], 0
    mov dword ptr [g_Stats_TotalRead + 4], 0
    mov [g_Stats_ActiveStreams], 0
    
    mov eax, 1
    pop ebx
    ret
StreamDisc_Init ENDP

; ============================================================
; StreamDisc_OpenStream - Open file for streaming
; Input:  ECX = file path
;         EDX = mode (read/write)
;         ESI = stream mode flags
; Output: EAX = stream handle, 0 on failure
; ============================================================
StreamDisc_OpenStream PROC lpPath:DWORD, dwAccess:DWORD, dwMode:DWORD
    LOCAL hFile:DWORD
    push ebx
    push esi
    push edi
    
    ; Determine access flags
    mov ebx, dwAccess
    mov esi, GENERIC_READ
    test ebx, 1
    jnz @open_read
    mov esi, GENERIC_WRITE
    test ebx, 2
    jnz @open_write
    or esi, GENERIC_READ or GENERIC_WRITE
    
@open_read:
@open_write:
    ; Open file with overlapped I/O support
    invoke CreateFileA, lpPath, esi, 0, 0, OPEN_ALWAYS, \
        FILE_ATTRIBUTE_NORMAL or FILE_FLAG_OVERLAPPED or FILE_FLAG_SEQUENTIAL_SCAN, 0
    
    cmp eax, INVALID_HANDLE_VALUE
    je @fail
    
    mov hFile, eax
    
    ; Find free stream slot
    mov ecx, MAX_STREAMS
    lea ebx, g_Streams
    xor edi, edi
    
@find_slot:
    cmp dword ptr [ebx], 0
    je @found_slot
    add ebx, SIZEOF StreamContext
    inc edi
    loop @find_slot
    
    ; No free slots
    invoke CloseHandle, hFile
    jmp @fail
    
@found_slot:
    ; Initialize stream context
    mov eax, hFile
    mov [ebx].StreamContext.hFile, eax
    mov eax, dwMode
    mov [ebx].StreamContext.mode, eax
    mov eax, DEFAULT_BUFFER_SIZE
    mov [ebx].StreamContext.cbBuffer, eax
    
    ; Allocate buffer
    invoke VirtualAlloc, 0, DEFAULT_BUFFER_SIZE, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    test eax, eax
    jz @fail_cleanup
    
    mov [ebx].StreamContext.pBuffer, eax
    
    ; Reset counters
    xor eax, eax
    mov dword ptr [ebx].StreamContext.offset, eax
    mov dword ptr [ebx].StreamContext.offset + 4, eax
    mov dword ptr [ebx].StreamContext.bytesWritten, eax
    mov dword ptr [ebx].StreamContext.bytesWritten + 4, eax
    mov dword ptr [ebx].StreamContext.bytesRead, eax
    mov dword ptr [ebx].StreamContext.bytesRead + 4, eax
    
    ; Create async event
    invoke CreateEventA, 0, TRUE, FALSE, 0
    mov [ebx].StreamContext.hAsyncEvent, eax
    
    ; Return stream handle (index)
    inc [g_StreamCount]
    inc [g_Stats_ActiveStreams]
    
    mov eax, edi
    pop edi
    pop esi
    pop ebx
    ret
    
@fail_cleanup:
    invoke CloseHandle, hFile
    
@fail:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
StreamDisc_OpenStream ENDP

; ============================================================
; StreamDisc_WriteChunk - Write data chunk to stream
; Input:  ECX = stream handle
;         EDX = data pointer
;         ESI = data size
; Output: EAX = bytes written, 0 on failure
; ============================================================
StreamDisc_WriteChunk PROC hStream:DWORD, pData:DWORD, cbData:DWORD
    LOCAL bytesWritten:DWORD
    push ebx
    push esi
    push edi
    
    ; Validate stream handle
    mov eax, hStream
    cmp eax, MAX_STREAMS
    jae @fail
    
    ; Get stream context
    imul eax, SIZEOF StreamContext
    lea ebx, g_Streams
    add ebx, eax
    
    ; Check if stream is open
    cmp [ebx].StreamContext.hFile, 0
    je @fail
    
    ; Check for async mode
    cmp [ebx].StreamContext.bAsync, 1
    je @async_write
    
    ; Synchronous write
    invoke WriteFile, [ebx].StreamContext.hFile, pData, cbData, ADDR bytesWritten, 0
    test eax, eax
    jz @fail
    
    mov eax, bytesWritten
    jmp @update_stats
    
@async_write:
    ; Setup overlapped structure
    lea edi, [ebx].StreamContext.overlapped
    
    ; Set offset
    mov eax, dword ptr [ebx].StreamContext.offset
    mov [edi].OVERLAPPED.Offset, eax
    mov eax, dword ptr [ebx].StreamContext.offset + 4
    mov [edi].OVERLAPPED.OffsetHigh, eax
    
    ; Set event
    mov eax, [ebx].StreamContext.hAsyncEvent
    mov [edi].OVERLAPPED.hEvent, eax
    
    ; Initiate async write
    invoke WriteFile, [ebx].StreamContext.hFile, pData, cbData, ADDR bytesWritten, edi
    
    mov eax, bytesWritten
    
@update_stats:
    ; Update stream offset
    add dword ptr [ebx].StreamContext.offset, eax
    adc dword ptr [ebx].StreamContext.offset + 4, 0
    
    ; Update bytes written
    add dword ptr [ebx].StreamContext.bytesWritten, eax
    adc dword ptr [ebx].StreamContext.bytesWritten + 4, 0
    
    ; Update global stats
    add dword ptr [g_Stats_TotalWritten], eax
    adc dword ptr [g_Stats_TotalWritten + 4], 0
    
    pop edi
    pop esi
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
StreamDisc_WriteChunk ENDP

; ============================================================
; StreamDisc_ReadChunk - Read data chunk from stream
; Input:  ECX = stream handle
;         EDX = output buffer
;         ESI = bytes to read
; Output: EAX = bytes read, 0 on failure
; ============================================================
StreamDisc_ReadChunk PROC hStream:DWORD, pBuffer:DWORD, cbToRead:DWORD
    LOCAL bytesRead:DWORD
    push ebx
    push esi
    push edi
    
    ; Validate stream handle
    mov eax, hStream
    cmp eax, MAX_STREAMS
    jae @fail
    
    ; Get stream context
    imul eax, SIZEOF StreamContext
    lea ebx, g_Streams
    add ebx, eax
    
    cmp [ebx].StreamContext.hFile, 0
    je @fail
    
    ; Check for async mode
    cmp [ebx].StreamContext.bAsync, 1
    je @async_read
    
    ; Synchronous read
    invoke ReadFile, [ebx].StreamContext.hFile, pBuffer, cbToRead, ADDR bytesRead, 0
    test eax, eax
    jz @fail
    
    mov eax, bytesRead
    jmp @update_stats
    
@async_read:
    ; Setup overlapped structure
    lea edi, [ebx].StreamContext.overlapped
    
    mov eax, dword ptr [ebx].StreamContext.offset
    mov [edi].OVERLAPPED.Offset, eax
    mov eax, dword ptr [ebx].StreamContext.offset + 4
    mov [edi].OVERLAPPED.OffsetHigh, eax
    
    mov eax, [ebx].StreamContext.hAsyncEvent
    mov [edi].OVERLAPPED.hEvent, eax
    
    invoke ReadFile, [ebx].StreamContext.hFile, pBuffer, cbToRead, ADDR bytesRead, edi
    
    mov eax, bytesRead
    
@update_stats:
    ; Update stream offset
    add dword ptr [ebx].StreamContext.offset, eax
    adc dword ptr [ebx].StreamContext.offset + 4, 0
    
    ; Update bytes read
    add dword ptr [ebx].StreamContext.bytesRead, eax
    adc dword ptr [ebx].StreamContext.bytesRead + 4, 0
    
    ; Update global stats
    add dword ptr [g_Stats_TotalRead], eax
    adc dword ptr [g_Stats_TotalRead + 4], 0
    
    pop edi
    pop esi
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
StreamDisc_ReadChunk ENDP

; ============================================================
; StreamDisc_CloseStream - Close stream and free resources
; Input:  ECX = stream handle
; Output: EAX = 1 success
; ============================================================
StreamDisc_CloseStream PROC hStream:DWORD
    push ebx
    
    mov eax, hStream
    cmp eax, MAX_STREAMS
    jae @fail
    
    imul eax, SIZEOF StreamContext
    lea ebx, g_Streams
    add ebx, eax
    
    cmp [ebx].StreamContext.hFile, 0
    je @fail
    
    ; Close file handle
    invoke CloseHandle, [ebx].StreamContext.hFile
    
    ; Close async event
    cmp [ebx].StreamContext.hAsyncEvent, 0
    je @no_event
    invoke CloseHandle, [ebx].StreamContext.hAsyncEvent
    
@no_event:
    ; Free buffer
    cmp [ebx].StreamContext.pBuffer, 0
    je @no_buffer
    invoke VirtualFree, [ebx].StreamContext.pBuffer, 0, MEM_RELEASE
    
@no_buffer:
    ; Clear context
    mov dword ptr [ebx].StreamContext.hFile, 0
    
    dec [g_StreamCount]
    dec [g_Stats_ActiveStreams]
    
    mov eax, 1
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop ebx
    ret
StreamDisc_CloseStream ENDP

; ============================================================
; StreamDisc_SetBufferSize - Adjust buffer size
; Input:  ECX = stream handle
;         EDX = new buffer size
; ============================================================
StreamDisc_SetBufferSize PROC hStream:DWORD, cbNewSize:DWORD
    push ebx
    
    mov eax, hStream
    cmp eax, MAX_STREAMS
    jae @fail
    
    imul eax, SIZEOF StreamContext
    lea ebx, g_Streams
    add ebx, eax
    
    mov eax, cbNewSize
    mov [ebx].StreamContext.cbBuffer, eax
    
    mov eax, 1
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop ebx
    ret
StreamDisc_SetBufferSize ENDP

; ============================================================
; StreamDisc_EnableAsync - Toggle async mode
; Input:  ECX = stream handle
;         EDX = enable (1) or disable (0)
; ============================================================
StreamDisc_EnableAsync PROC hStream:DWORD, bEnable:DWORD
    push ebx
    
    mov eax, hStream
    cmp eax, MAX_STREAMS
    jae @fail
    
    imul eax, SIZEOF StreamContext
    lea ebx, g_Streams
    add ebx, eax
    
    mov eax, bEnable
    mov [ebx].StreamContext.bAsync, eax
    
    mov eax, 1
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop ebx
    ret
StreamDisc_EnableAsync ENDP

; ============================================================
; StreamDisc_GetProgress - Get stream progress percentage
; Input:  ECX = stream handle
; Output: EAX = progress (0-100), -1 on error
; ============================================================
StreamDisc_GetProgress PROC hStream:DWORD
    push ebx
    push edx
    
    mov eax, hStream
    cmp eax, MAX_STREAMS
    jae @fail
    
    imul eax, SIZEOF StreamContext
    lea ebx, g_Streams
    add ebx, eax
    
    ; Calculate progress: (bytesWritten * 100) / totalSize
    mov eax, dword ptr [ebx].StreamContext.bytesWritten
    mov edx, 100
    mul edx
    
    mov ecx, dword ptr [ebx].StreamContext.totalSize
    test ecx, ecx
    jz @no_total
    
    div ecx
    pop edx
    pop ebx
    ret
    
@no_total:
    xor eax, eax
    pop edx
    pop ebx
    ret
    
@fail:
    mov eax, -1
    pop edx
    pop ebx
    ret
StreamDisc_GetProgress ENDP

END
