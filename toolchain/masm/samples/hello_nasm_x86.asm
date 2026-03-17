extern _ExitProcess@4

section .text
global _start
_start:
    push dword 0
    call _ExitProcess@4
