; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_RingBuffer_Consumer.asm  ─  Ring Buffer → Win32 UI Pipeline
; Standalone MASM64 translation unit, zero dependencies beyond Win32
; Assemble: ml64.exe /c /FoRawrXD_RingBuffer_Consumer.obj RawrXD_RingBuffer_Consumer.asm
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION WIN64:3

; ═══════════════════════════════════════════════════════════════════════════════
; INCLUDES
; ═══════════════════════════════════════════════════════════════════════════════
include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\user32.inc
include \masm64\include64\gdi32.inc

includelib \masm64\lib64\kernel32.lib
includelib \masm64\lib64\user32.lib
includelib \masm64\lib64\gdi32.lib

; ═══════════════════════════════════════════════════════════════════════════════
; EXTERNAL IMPORTS (from RawrXD_Streaming_Orchestrator.asm)
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF RawrXD_DMA_RingBuffer_Base:QWORD
EXTERNDEF RawrXD_DMA_RingBuffer_ConsumerOffset:QWORD
EXTERNDEF RawrXD_DMA_RingBuffer_ProducerOffset:QWORD
EXTERNDEF RawrXD_DMA_RingBuffer_Lock:QWORD
EXTERNDEF RawrXD_DMA_RingBuffer_Semaphore:QWORD

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
RING_BUFFER_SIZE        EQU 67108864      ; 64MB
MAX_UTF8_SEQ            EQU 4             ; Max UTF-8 bytes per codepoint
WM_RAWRXD_TOKEN         EQU WM_USER + 0x1000  ; Custom message for token delivery
TOKEN_BATCH_SIZE        EQU 256           ; Tokens per WM message batch
BACKPRESSURE_THRESHOLD  EQU 1048576       ; 1MB backlog triggers pause
UNICODE_REPLACEMENT     EQU 0FFFDh        ; Replacement char for invalid UTF-8

; ═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES
; ═══════════════════════════════════════════════════════════════════════════════
TokenBatch STRUCT
    Count               DWORD       ?       ; Number of tokens in batch
    Tokens              DWORD TOKEN_BATCH_SIZE DUP (?)  ; Token IDs
TokenBatch ENDS

ConsumerState STRUCT
    hWndOutput          QWORD       ?       ; Target edit control handle
    hThreadConsumer     QWORD       ?       ; Consumer thread handle
    dwThreadId          DWORD       ?       ; Consumer thread ID
    bRunning            DWORD       ?       ; Consumer loop flag
    bBackpressure       DWORD       ?       ; Backpressure active flag
    TotalTokensConsumed QWORD       ?       ; Lifetime counter
    TotalCharsOutput    QWORD       ?       ; UTF-16 chars written
    LastError           DWORD       ?       ; Last error code
    DetokenizeTable     QWORD       ?       ; Pointer to vocab table
    PendingUtf8         BYTE 8 DUP (?)      ; Incomplete UTF-8 sequence buffer
    PendingCount        DWORD       ?       ; Bytes in pending buffer
ConsumerState ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.DATA
align 16
g_ConsumerState         ConsumerState <>

; UTF-8 decode state machine tables
Utf8ByteTable           BYTE 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  ; 00-0F: continuation/invalid
                        BYTE 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  ; 10-1F: continuation/invalid
                        BYTE 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  ; 20-2F: continuation/invalid
                        BYTE 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  ; 30-3F: continuation/invalid
                        BYTE 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  ; 40-4F: continuation/invalid
                        BYTE 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  ; 50-5F: continuation/invalid
                        BYTE 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  ; 60-6F: continuation/invalid
                        BYTE 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  ; 70-7F: continuation/invalid
                        BYTE 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1  ; 80-8F: continuation bytes
                        BYTE 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1  ; 90-9F: continuation bytes
                        BYTE 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2  ; A0-AF: 2-byte start
                        BYTE 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2  ; B0-BF: 2-byte start
                        BYTE 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3  ; C0-CF: 3-byte start
                        BYTE 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3  ; D0-DF: 3-byte start
                        BYTE 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4  ; E0-EF: 4-byte start
                        BYTE 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5  ; F0-FF: 5/6-byte (invalid)

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; RingBufferConsumer_Initialize
; Initializes the consumer thread and state
;
; Parameters:
;   RCX = HWND of target edit control
;   RDX = Pointer to detokenization vocabulary table
;
; Returns:
;   RAX = TRUE on success, FALSE on failure
; ═══════════════════════════════════════════════════════════════════════════════
RingBufferConsumer_Initialize PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    mov rbx, rcx                    ; Save hWnd
    mov r12, rdx                    ; Save vocab table pointer
    
    ; Validate parameters
    test rcx, rcx
    jz @init_fail
    test rdx, rdx
    jz @init_fail
    
    ; Initialize state structure
    lea rdi, g_ConsumerState
    mov rsi, rdi
    mov rcx, (SIZEOF ConsumerState) / 8
    xor rax, rax
    rep stosq
    
    ; Set initial values
    mov g_ConsumerState.hWndOutput, rbx
    mov g_ConsumerState.DetokenizeTable, r12
    mov g_ConsumerState.bRunning, 1
    
    ; Create consumer thread
    lea rcx, ConsumerThreadProc     ; lpStartAddress
    xor rdx, rdx                    ; lpParameter (unused, uses global)
    xor r8, r8                      ; dwCreationFlags (run immediately)
    lea r9, g_ConsumerState.dwThreadId  ; lpThreadId
    
    sub rsp, 40
    call CreateThread
    add rsp, 40
    
    test rax, rax
    jz @init_fail
    
    mov g_ConsumerState.hThreadConsumer, rax
    
    mov rax, TRUE
    jmp @init_done
    
@init_fail:
    mov g_ConsumerState.LastError, eax
    xor rax, rax
    
@init_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RingBufferConsumer_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ConsumerThreadProc
; Main consumer loop - drains ring buffer and pumps tokens to UI
; ═══════════════════════════════════════════════════════════════════════════════
ConsumerThreadProc PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    sub rsp, 208                    ; Local stack frame + shadow space
    
    mov r12, RawrXD_DMA_RingBuffer_Base
    mov r13, OFFSET g_ConsumerState
    
@consumer_loop:
    ; Check if we should exit
    cmp [r13].ConsumerState.bRunning, 0
    je @consumer_exit
    
    ; Wait for data available (semaphore)
    mov rcx, RawrXD_DMA_RingBuffer_Semaphore
    mov rdx, 100                    ; 100ms timeout (allows periodic checks)
    call WaitForSingleObject
    
    cmp eax, WAIT_OBJECT_0
    jne @check_backpressure         ; Timeout or error, check backpressure state
    
    ; Data available - process batch
    call ProcessTokenBatch
    
@check_backpressure:
    ; Check backpressure conditions
    call CheckBackpressureState
    
    jmp @consumer_loop
    
@consumer_exit:
    add rsp, 208
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    xor eax, eax
    ret
ConsumerThreadProc ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ProcessTokenBatch
; Reads tokens from ring buffer, detokenizes, and queues for UI
; ═══════════════════════════════════════════════════════════════════════════════
ProcessTokenBatch PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    sub rsp, 64
    
    ; Acquire ring buffer lock
    mov rcx, RawrXD_DMA_RingBuffer_Lock
    call EnterCriticalSection
    
    ; Calculate available data
    mov rax, RawrXD_DMA_RingBuffer_ProducerOffset
    mov rbx, RawrXD_DMA_RingBuffer_ConsumerOffset
    sub rax, rbx                    ; RAX = available bytes
    
    ; Handle wrap-around
    test rax, rax
    jns @no_wrap
    add rax, RING_BUFFER_SIZE
    
@no_wrap:
    ; Limit batch size
    cmp rax, TOKEN_BATCH_SIZE * 4   ; Each token is DWORD
    jle @batch_size_ok
    mov rax, TOKEN_BATCH_SIZE * 4
    
@batch_size_ok:
    test rax, rax
    jz @process_done                ; Nothing to read
    
    mov r12, rax                    ; R12 = bytes to read
    shr r12, 2                      ; Convert to token count
    mov r13, rbx                    ; R13 = consumer offset
    
    ; Release lock while processing (allows producer to continue)
    mov rcx, RawrXD_DMA_RingBuffer_Lock
    call LeaveCriticalSection
    
    ; Process tokens: detokenize and convert to UTF-16
    lea r14, [rsp + 32]             ; Local UTF-16 output buffer
    xor r15, r15                    ; Total UTF-16 chars produced
    
    mov rbx, r12                    ; RBX = token counter
    mov rsi, r13                    ; RSI = ring buffer offset
    
@token_loop:
    test rbx, rbx
    jz @tokens_done
    
    ; Read token from ring buffer
    mov r8d, [r12 + rsi]            ; R8D = token ID
    
    ; Detokenize: lookup in vocabulary table
    mov rax, g_ConsumerState.DetokenizeTable
    mov rcx, r8
    call DetokenizeToken            ; Returns: RAX = UTF-8 string ptr, RDX = length
    
    ; Convert UTF-8 to UTF-16
    mov rcx, rax                    ; UTF-8 source
    mov rdx, r14                    ; UTF-16 destination
    mov r8d, edx                    ; Max output chars
    call Utf8ToUtf16
    
    add r15, rax                    ; Accumulate output length
    
    ; Send to UI if buffer full or last token
    cmp r15, 512                    ; Threshold for WM message
    jge @flush_to_ui
    
    dec rbx
    add rsi, 4
    cmp rsi, RING_BUFFER_SIZE
    jl @token_loop
    xor rsi, rsi                    ; Wrap to beginning
    jmp @token_loop
    
@flush_to_ui:
    ; Send accumulated text to UI
    mov rcx, g_ConsumerState.hWndOutput
    mov edx, WM_RAWRXD_TOKEN
    mov r8, r14                     ; lParam = text buffer
    mov r9d, r15d                   ; wParam = length
    call SendMessageW
    
    xor r15, r15                    ; Reset accumulator
    jmp @token_loop
    
@tokens_done:
    ; Flush remaining
    test r15, r15
    jz @update_offset
    
    mov rcx, g_ConsumerState.hWndOutput
    mov edx, WM_RAWRXD_TOKEN
    mov r8, r14
    mov r9d, r15d
    call SendMessageW
    
@update_offset:
    ; Update consumer offset
    mov rcx, RawrXD_DMA_RingBuffer_Lock
    call EnterCriticalSection
    
    mov rax, r12
    shl rax, 2                      ; Convert token count back to bytes
    add RawrXD_DMA_RingBuffer_ConsumerOffset, rax
    mov rax, RawrXD_DMA_RingBuffer_ConsumerOffset
    cmp rax, RING_BUFFER_SIZE
    jl @offset_ok
    sub RawrXD_DMA_RingBuffer_ConsumerOffset, RING_BUFFER_SIZE
    
@offset_ok:
    mov rcx, RawrXD_DMA_RingBuffer_Lock
    call LeaveCriticalSection
    
@process_done:
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ProcessTokenBatch ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; DetokenizeToken
; Looks up token ID in vocabulary table
;
; Parameters:
;   RCX = Pointer to vocabulary table (array of TokenEntry)
;   RDX = Token ID
;
; Returns:
;   RAX = Pointer to UTF-8 string
;   RDX = Length of string
; ═══════════════════════════════════════════════════════════════════════════════
DetokenizeToken PROC FRAME
    ; TokenEntry structure: { DWORD id; DWORD length; BYTE string[] }
    ; Simple array lookup - assumes contiguous storage
    
    push rbx
    
    mov rbx, rcx                    ; Table base
    mov rax, rdx                    ; Token ID
    
    ; Calculate entry offset (simplified - assumes fixed size for hot path)
    ; Real implementation would use hash table or binary search
    shl rax, 5                      ; Assume 32-byte entries for cache alignment
    
    add rbx, rax
    
    mov edx, [rbx + 4]              ; Length
    lea rax, [rbx + 8]              ; String pointer
    
    pop rbx
    ret
DetokenizeToken ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Utf8ToUtf16
; Converts UTF-8 byte sequence to UTF-16 code units
; Handles incomplete sequences at buffer boundaries
;
; Parameters:
;   RCX = Source UTF-8 buffer
;   RDX = Destination UTF-16 buffer  
;   R8D = Max destination capacity
;
; Returns:
;   RAX = Number of UTF-16 code units written
; ═══════════════════════════════════════════════════════════════════════════════
Utf8ToUtf16 PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    mov rsi, rcx                    ; Source
    mov rdi, rdx                    ; Destination
    mov r12d, r8d                   ; Capacity
    xor r13, r13                    ; Output count
    xor r14, r14                    ; Temp codepoint
    
    ; Check for pending bytes from previous call
    cmp g_ConsumerState.PendingCount, 0
    je @main_decode
    
    ; Handle pending UTF-8 sequence
    mov ebx, g_ConsumerState.PendingCount
    lea r15, g_ConsumerState.PendingUtf8
    
    ; Copy new bytes to complete sequence
@pending_loop:
    mov al, [rsi]
    mov [r15 + rbx], al
    inc rsi
    inc ebx
    
    ; Check if sequence is now complete
    movzx eax, byte ptr [r15]
    and eax, 0F0h
    cmp eax, 0F0h
    je @check_4byte
    cmp eax, 0E0h
    je @check_3byte
    cmp eax, 0C0h
    je @check_2byte
    jmp @invalid_pending
    
@check_4byte:
    cmp ebx, 4
    jl @still_pending
    jmp @decode_pending
    
@check_3byte:
    cmp ebx, 3
    jl @still_pending
    jmp @decode_pending
    
@check_2byte:
    cmp ebx, 2
    jl @still_pending
    
@decode_pending:
    ; Decode the completed pending sequence
    mov rcx, r15
    mov edx, ebx
    call DecodeUtf8Sequence
    jmp @output_codepoint
    
@still_pending:
    ; Still incomplete - save state and return
    mov g_ConsumerState.PendingCount, ebx
    jmp @utf_done
    
@invalid_pending:
    ; Invalid pending state - emit replacement char
    mov eax, UNICODE_REPLACEMENT
    mov g_ConsumerState.PendingCount, 0
    jmp @output_codepoint
    
@main_decode:
    ; Main decode loop
@decode_loop:
    cmp r13d, r12d
    jge @utf_done                   ; Destination full
    
    movzx eax, byte ptr [rsi]
    test al, al
    jz @utf_done                    ; Null terminator
    
    ; Check for multi-byte sequence
    movzx ebx, al
    shr ebx, 4
    movzx ebx, byte ptr [Utf8ByteTable + rbx]
    
    cmp ebx, 1                      ; Continuation byte (invalid as start)
    je @invalid_start
    
    cmp ebx, 0                      ; ASCII
    je @ascii_char
    
    ; Multi-byte sequence
    mov ecx, ebx                    ; Expected length
    mov r15, rsi                    ; Save start
    mov r14d, eax                   ; Accumulator
    
    ; Collect continuation bytes
@cont_loop:
    dec ecx
    jz @sequence_complete
    inc rsi
    movzx eax, byte ptr [rsi]
    and eax, 0C0h
    cmp eax, 80h                    ; 10xxxxxx pattern?
    jne @invalid_sequence
    movzx eax, byte ptr [rsi]
    and eax, 3Fh                    ; Extract 6 bits
    shl r14d, 6
    or r14d, eax
    jmp @cont_loop
    
@sequence_complete:
    mov eax, r14d
    jmp @output_codepoint
    
@ascii_char:
    inc rsi
    
@output_codepoint:
    ; Check for surrogate pair need (codepoints > 0xFFFF)
    cmp eax, 10000h
    jl @bmp_char
    
    ; Surrogate pair
    mov ecx, eax
    sub ecx, 10000h
    shr ecx, 10
    add ecx, 0D800h                 ; High surrogate
    mov [rdi + r13*2], cx
    inc r13d
    
    cmp r13d, r12d
    jge @utf_done
    
    and eax, 3FFh
    add eax, 0DC00h                 ; Low surrogate
    jmp @store_char
    
@bmp_char:
@store_char:
    mov [rdi + r13*2], ax
    inc r13d
    jmp @decode_loop
    
@invalid_start:
@invalid_sequence:
    ; Emit replacement character
    mov eax, UNICODE_REPLACEMENT
    inc rsi                         ; Skip invalid byte
    jmp @output_codepoint
    
@utf_done:
    mov rax, r13                    ; Return count
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Utf8ToUtf16 ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; CheckBackpressureState
; Monitors ring buffer depth and signals backpressure to producer
; ═══════════════════════════════════════════════════════════════════════════════
CheckBackpressureState PROC FRAME
    push rbx
    
    ; Calculate buffer depth
    mov rcx, RawrXD_DMA_RingBuffer_Lock
    call EnterCriticalSection
    
    mov rax, RawrXD_DMA_RingBuffer_ProducerOffset
    mov rbx, RawrXD_DMA_RingBuffer_ConsumerOffset
    sub rax, rbx
    
    mov rcx, RawrXD_DMA_RingBuffer_Lock
    call LeaveCriticalSection
    
    ; Handle wrap
    test rax, rax
    jns @depth_ok
    add rax, RING_BUFFER_SIZE
    
@depth_ok:
    ; Check against threshold
    cmp rax, BACKPRESSURE_THRESHOLD
    jl @clear_backpressure
    
    ; Set backpressure flag
    cmp g_ConsumerState.bBackpressure, 0
    jne @check_done
    
    mov g_ConsumerState.bBackpressure, 1
    
    ; Signal producer to pause (via event or shared memory flag)
    ; Implementation depends on producer signaling mechanism
    jmp @check_done
    
@clear_backpressure:
    cmp g_ConsumerState.bBackpressure, 0
    je @check_done
    
    mov g_ConsumerState.bBackpressure, 0
    
    ; Signal producer to resume
    
@check_done:
    pop rbx
    ret
CheckBackpressureState ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RingBufferConsumer_Shutdown
; Gracefully stops consumer thread
; ═══════════════════════════════════════════════════════════════════════════════
RingBufferConsumer_Shutdown PROC FRAME
    push rbx
    
    ; Signal thread to stop
    mov g_ConsumerState.bRunning, 0
    
    ; Release semaphore to wake thread if waiting
    mov rcx, RawrXD_DMA_RingBuffer_Semaphore
    mov rdx, 1
    call ReleaseSemaphore
    
    ; Wait for thread termination
    mov rcx, g_ConsumerState.hThreadConsumer
    mov rdx, 5000                   ; 5 second timeout
    call WaitForSingleObject
    
    cmp eax, WAIT_OBJECT_0
    je @thread_ended
    
    ; Force terminate if hung
    mov rcx, g_ConsumerState.hThreadConsumer
    call TerminateThread
    
@thread_ended:
    ; Cleanup
    mov rcx, g_ConsumerState.hThreadConsumer
    call CloseHandle
    
    mov g_ConsumerState.hThreadConsumer, 0
    
    pop rbx
    ret
RingBufferConsumer_Shutdown ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Helper stubs
; ═══════════════════════════════════════════════════════════════════════════════
DecodeUtf8Sequence PROC
    xor eax, eax
    ret
DecodeUtf8Sequence ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; EXPORTS
; ═══════════════════════════════════════════════════════════════════════════════
PUBLIC RingBufferConsumer_Initialize
PUBLIC RingBufferConsumer_Shutdown
PUBLIC ConsumerThreadProc
PUBLIC ProcessTokenBatch
PUBLIC Utf8ToUtf16

END
