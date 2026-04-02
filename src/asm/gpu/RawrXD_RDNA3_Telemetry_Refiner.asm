; RawrXD_RDNA3_Telemetry_Refiner.asm
; Performance Telemetry Refiner
; Extracts and refines GPU telemetry data

.DATA
TELEMETRY_BASE equ 1A20h ; Page fault counter

.CODE

; RDNA3_Telemetry_Read PROC
; Reads refined telemetry
RDNA3_Telemetry_Read PROC
    push rbp
    mov rbp, rsp

    ; Read telemetry register
    mov rdx, TELEMETRY_BASE
    mov rax, [rdx]

    leave
    ret
RDNA3_Telemetry_Read ENDP

END