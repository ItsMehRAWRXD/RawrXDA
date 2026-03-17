; Test file for drag-and-drop functionality
; This file can be dragged onto the RawrXD IDE to test the drag-and-drop feature

section .text
global main

main:
    mov rax, 60     ; sys_exit
    mov rdi, 0      ; exit status
    syscall

section .data
    test_message db "Drag and drop test successful!", 0