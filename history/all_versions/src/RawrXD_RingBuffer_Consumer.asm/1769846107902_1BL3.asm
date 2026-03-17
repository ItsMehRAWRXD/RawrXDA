; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_RingBuffer_Consumer.asm
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
; OPTION WIN64:3

include windows.inc
include kernel32.inc

includelib kernel32.lib

.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; RingBufferConsumer_Initialize
; RCX = hWnd, RDX = VocabTable
; ═══════════════════════════════════════════════════════════════════════════════
RingBufferConsumer_Initialize PROC
    push rbx
    mov rax, 1
    pop rbx
    ret
RingBufferConsumer_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RingBufferConsumer_Shutdown
; ═══════════════════════════════════════════════════════════════════════════════
RingBufferConsumer_Shutdown PROC
    ret
RingBufferConsumer_Shutdown ENDP

PUBLIC RingBufferConsumer_Initialize
PUBLIC RingBufferConsumer_Shutdown

END
