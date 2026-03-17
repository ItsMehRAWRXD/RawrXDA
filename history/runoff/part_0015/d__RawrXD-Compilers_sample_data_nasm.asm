; sample_data.asm - NASM syntax test file (65 bytes source)
; Target: x64 Windows PE
; Expected: Generates minimal PE with NOP + RET

section .text
global _start

_start:
    nop
    ret
