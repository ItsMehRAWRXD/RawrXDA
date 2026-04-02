; RawrXD_RDNA3_MMIO_Recon.asm
; MMIO Register Reconnaissance
; Maps and accesses GPU registers for sovereign control

.DATA
MMIO_BASE equ 0FE000000h ; GPU MMIO base

.CODE

; RDNA3_MMIO_Read PROC
; Reads GPU register via MMIO
RDNA3_MMIO_Read PROC
    push rbp
    mov rbp, rsp

    ; Read from MMIO register
    mov rdx, MMIO_BASE
    add rdx, rcx ; offset
    mov rax, [rdx]

    leave
    ret
RDNA3_MMIO_Read ENDP

; RDNA3_MMIO_Write PROC
; Writes GPU register via MMIO
RDNA3_MMIO_Write PROC
    push rbp
    mov rbp, rsp

    ; Write to MMIO register
    mov rdx, MMIO_BASE
    add rdx, rcx ; offset
    mov [rdx], rdx ; value in rdx

    leave
    ret
RDNA3_MMIO_Write ENDP

END