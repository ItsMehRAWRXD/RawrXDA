; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_RingBuffer_Consumer.asm
; Stub implementation for ring buffer consumption
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

.CODE

RingBufferConsumer_Initialize PROC FRAME
    ; RCX = hWnd
    ; RDX = Vocab Table
    mov rax, 1 ; Success
    ret
RingBufferConsumer_Initialize ENDP

RingBufferConsumer_Shutdown PROC FRAME
    xor eax, eax
    ret
RingBufferConsumer_Shutdown ENDP

PUBLIC RingBufferConsumer_Initialize
PUBLIC RingBufferConsumer_Shutdown

END