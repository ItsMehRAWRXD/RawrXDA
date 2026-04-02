; RawrXD_DDR5_Saturation_Streamer.asm
; DDR5 Bandwidth Saturation Streamer
; Maximizes memory bandwidth for data streaming

.DATA
STREAM_SIZE equ 1000000h ; 16MB

.CODE

; DDR5_Saturate_Bandwidth PROC
; Streams data to saturate DDR5
DDR5_Saturate_Bandwidth PROC
    push rbp
    mov rbp, rsp

    ; Stream data (simplified prefetch)
    mov rcx, STREAM_SIZE
    mov rdx, rcx ; address
loop_stream:
    prefetchnta [rdx]
    add rdx, 64
    loop loop_stream

    leave
    ret
DDR5_Saturate_Bandwidth ENDP

END