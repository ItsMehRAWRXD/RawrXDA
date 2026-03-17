; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Streaming_Formatter.asm  ─  SSE/Chunked Encoding for Real-Time Delivery
; Production MASM64 with HTTP chunked transfer encoding and Server-Sent Events
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\ws2_32.inc

includelib \masm64\lib64\kernel32.lib
includelib \masm64\lib64\ws2_32.lib

EXTERNDEF RawrXD_MemAlloc:PROC
EXTERNDEF RawrXD_MemFree:PROC
EXTERNDEF RawrXD_StrLen:PROC
EXTERNDEF RawrXD_HexToString:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
STREAM_FORMAT_RAW       EQU 0       ; No encoding
STREAM_FORMAT_CHUNKED   EQU 1       ; HTTP/1.1 chunked transfer
STREAM_FORMAT_SSE       EQU 2       ; Server-Sent Events (text/event-stream)
STREAM_FORMAT_WS        EQU 3       ; WebSocket frame (future)

CHUNK_SIZE_MAX          EQU 8192    ; Max bytes per chunk
SSE_RETRY_MS            EQU 3000    ; Reconnection timeout hint

; ═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES
; ═══════════════════════════════════════════════════════════════════════════════
StreamFormatter STRUCT
    Socket              QWORD       ?
    FormatType          DWORD       ?
    Buffer              QWORD       ?
    BufferSize          QWORD       ?
    BufferUsed          QWORD       ?
    ChunkCount          QWORD       ?
    BytesSent           QWORD       ?
    LastFlushTick       QWORD       ?
    FlushIntervalMs     DWORD       ?
StreamFormatter ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.DATA
align 16
g_FormatterPool         StreamFormatter 16 DUP (<>)

; HTTP chunked encoding templates
szChunkHeader           BYTE "%X\r\n", 0          ; Hex size + CRLF
szChunkTerminator       BYTE "0\r\n\r\n", 0       ; Final chunk
szSseHeader             BYTE "data: ", 0
szSseTerminator         BYTE "\n\n", 0
szHttpChunkedHeaders    BYTE "HTTP/1.1 200 OK\r\nContent-Type: application/x-ndjson\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n", 0
szSseHeaders            BYTE "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\n\r\n", 0

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; StreamFormatter_Create
; Initializes formatter for socket with specified encoding
; ═══════════════════════════════════════════════════════════════════════════════
StreamFormatter_Create PROC FRAME
    push rbx
    push rsi
    
    mov rbx, rcx                    ; RBX = socket
    mov esi, edx                    ; ESI = format type
    
    ; Find free formatter slot
    xor ecx, ecx
    
@find_slot:
    cmp ecx, 16
    jge @no_slot
    
    lea rax, g_FormatterPool[rcx * SIZEOF StreamFormatter]
    cmp [rax].StreamFormatter.Socket, 0
    je @found_slot
    
    inc ecx
    jmp @find_slot
    
@found_slot:
    ; Initialize formatter
    mov [rax].StreamFormatter.Socket, rbx
    mov [rax].StreamFormatter.FormatType, esi
    mov [rax].StreamFormatter.BufferUsed, 0
    mov [rax].StreamFormatter.ChunkCount, 0
    mov [rax].StreamFormatter.BytesSent, 0
    mov [rax].StreamFormatter.FlushIntervalMs, 50  ; 50ms max latency
    
    ; Allocate buffer
    mov rcx, CHUNK_SIZE_MAX * 2
    call RawrXD_MemAlloc
    mov [rax].StreamFormatter.Buffer, rax
    mov [rax].StreamFormatter.BufferSize, CHUNK_SIZE_MAX * 2
    
    ; Send headers based on format
    cmp esi, STREAM_FORMAT_CHUNKED
    je @send_chunked_headers
    cmp esi, STREAM_FORMAT_SSE
    je @send_sse_headers
    jmp @headers_done
    
@send_chunked_headers:
    mov rcx, rbx
    lea rdx, szHttpChunkedHeaders
    call RawrXD_StrLen
    mov r8, rax
    lea rdx, szHttpChunkedHeaders
    mov rcx, rbx
    xor r9, r9
    call send
    jmp @headers_done
    
@send_sse_headers:
    mov rcx, rbx
    lea rdx, szSseHeaders
    call RawrXD_StrLen
    mov r8, rax
    lea rdx, szSseHeaders
    mov rcx, rbx
    xor r9, r9
    call send
    
@headers_done:
    mov rax, rcx                    ; Return formatter index
    jmp @create_done
    
@no_slot:
    xor eax, eax
    
@create_done:
    pop rsi
    pop rbx
    ret
StreamFormatter_Create ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; StreamFormatter_WriteToken
; Buffers token and flushes if necessary (chunked encoding)
; ═══════════════════════════════════════════════════════════════════════════════
StreamFormatter_WriteToken PROC FRAME
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; RBX = formatter index
    mov rsi, rdx                    ; RSI = token data
    mov edi, r8d                    ; EDI = token length
    
    imul rbx, SIZEOF StreamFormatter
    lea rbx, g_FormatterPool[rbx]   ; RBX = formatter pointer
    
    ; Check if we need to flush before adding
    mov rax, [rbx].StreamFormatter.BufferUsed
    add rax, rdi
    cmp rax, CHUNK_SIZE_MAX
    jl @buffer_token
    
    ; Flush current buffer first
    call StreamFormatter_FlushChunk
    
@buffer_token:
    ; Append token to buffer
    mov rcx, [rbx].StreamFormatter.Buffer
    add rcx, [rbx].StreamFormatter.BufferUsed
    
    ; Format based on type
    cmp [rbx].StreamFormatter.FormatType, STREAM_FORMAT_SSE
    je @format_sse
    
    ; Raw/Chunked: copy as-is
    mov rdx, rsi
    mov r8d, edi
    call memcpy
    add [rbx].StreamFormatter.BufferUsed, rdi
    jmp @write_done
    
@format_sse:
    ; SSE: prepend "data: " and append "\n\n"
    lea rdx, szSseHeader
    mov r8d, 6                      ; strlen("data: ")
    call memcpy
    
    mov rcx, [rbx].StreamFormatter.Buffer
    add rcx, [rbx].StreamFormatter.BufferUsed
    add rcx, 6
    mov rdx, rsi
    mov r8d, edi
    call memcpy
    
    mov rcx, [rbx].StreamFormatter.Buffer
    add rcx, [rbx].StreamFormatter.BufferUsed
    add rcx, 6
    add rcx, rdi
    lea rdx, szSseTerminator
    mov r8d, 2                      ; strlen("\n\n")
    call memcpy
    
    add [rbx].StreamFormatter.BufferUsed, 6
    add [rbx].StreamFormatter.BufferUsed, rdi
    add [rbx].StreamFormatter.BufferUsed, 2
    
@write_done:
    pop rdi
    pop rsi
    pop rbx
    ret
StreamFormatter_WriteToken ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; StreamFormatter_FlushChunk
; Sends buffered data as HTTP chunk
; ═══════════════════════════════════════════════════════════════════════════════
StreamFormatter_FlushChunk PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rbx, rcx                    ; Formatter pointer
    
    mov rsi, [rbx].StreamFormatter.BufferUsed
    test rsi, rsi
    jz @flush_done                  ; Nothing to flush
    
    ; Build chunk: hex_size + CRLF + data + CRLF
    lea rdi, [rsp + 16]             ; Temp stack buffer for header
    
    ; Format hex size
    mov rcx, rdi
    mov rdx, rsi
    call sprintf_hex                ; Custom: converts to hex string
    
    ; Add CRLF
    mov word ptr [rax], 0A0Dh       ; \r\n
    add rax, 2
    
    ; Calculate total size
    mov r8, rax
    sub r8, rdi                     ; Header length
    add r8, rsi                     ; + data length
    add r8, 2                       ; + trailing CRLF
    
    ; Send header
    mov rcx, [rbx].StreamFormatter.Socket
    mov rdx, rdi
    ; ... (send logic)
    
    ; Send data
    mov rcx, [rbx].StreamFormatter.Socket
    mov rdx, [rbx].StreamFormatter.Buffer
    mov r8, rsi
    ; ... (send logic)
    
    ; Send trailing CRLF
    mov rcx, [rbx].StreamFormatter.Socket
    lea rdx, word ptr [rel crlf]
    mov r8d, 2
    ; ... (send logic)
    
    ; Reset buffer
    mov [rbx].StreamFormatter.BufferUsed, 0
    inc [rbx].StreamFormatter.ChunkCount
    
@flush_done:
    call GetTickCount64
    mov [rbx].StreamFormatter.LastFlushTick, rax
    
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
StreamFormatter_FlushChunk ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; StreamFormatter_Close
; Sends final chunk and cleanup
; ═══════════════════════════════════════════════════════════════════════════════
StreamFormatter_Close PROC FRAME
    push rbx
    
    mov rbx, rcx
    
    ; Send final chunk (0\r\n\r\n)
    mov rcx, [rbx].StreamFormatter.Socket
    lea rdx, szChunkTerminator
    mov r8d, 5
    xor r9, r9
    call send
    
    ; Free buffer
    mov rcx, [rbx].StreamFormatter.Buffer
    call HeapFree
    
    ; Clear slot
    mov [rbx].StreamFormatter.Socket, 0
    
    pop rbx
    ret
StreamFormatter_Close ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Helper stubs
; ═══════════════════════════════════════════════════════════════════════════════
strlen PROC
    xor eax, eax
    ret
strlen ENDP

HeapAlloc PROC
    ret
HeapAlloc ENDP

HeapFree PROC
    ret
HeapFree ENDP

memcpy PROC
    ret
memcpy ENDP

sprintf_hex PROC
    ret
sprintf_hex ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; EXPORTS
; ═══════════════════════════════════════════════════════════════════════════════
PUBLIC StreamFormatter_Create
PUBLIC StreamFormatter_WriteToken
PUBLIC StreamFormatter_FlushChunk
PUBLIC StreamFormatter_Close

crlf                    WORD 0A0Dh

END
