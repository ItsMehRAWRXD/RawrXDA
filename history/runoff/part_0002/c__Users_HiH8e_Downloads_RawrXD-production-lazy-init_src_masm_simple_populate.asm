;==========================================================================
; Simple ui_populate_explorer replacement
; Just lists files in current directory - no drives, no complex logic
;==========================================================================

option casemap:none

; External functions
EXTERN FindFirstFileA : PROC
EXTERN FindNextFileA : PROC
EXTERN FindClose : PROC
EXTERN GetCurrentDirectoryA : PROC
EXTERN SendMessageA : PROC

; External data
EXTERN hwndExplorer : QWORD
EXTERN szExplorerDir : BYTE
EXTERN szExplorerPattern : BYTE
EXTERN find_data : BYTE

; Constants
LB_ADDSTRING equ 0180h
LB_RESETCONTENT equ 0184h

.code

PUBLIC ui_populate_explorer_simple

ui_populate_explorer_simple PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64

    ; Clear listbox
    mov rcx, hwndExplorer
    test rcx, rcx
    jz done_simple
    mov rdx, LB_RESETCONTENT
    xor r8, r8
    xor r9, r9
    call SendMessageA

    ; Get current directory
    mov rcx, 260
    lea rdx, szExplorerDir
    call GetCurrentDirectoryA
    test eax, eax
    jz done_simple

    ; Build pattern
    lea rsi, szExplorerDir
    lea rdi, szExplorerPattern
copy_path:
    lodsb
    stosb
    test al, al
    jnz copy_path
    ; Add "\*.*"
    mov BYTE PTR [rdi - 1], 92  ; '\'
    mov BYTE PTR [rdi], 42      ; '*'
    mov BYTE PTR [rdi + 1], 46  ; '.'
    mov BYTE PTR [rdi + 2], 42  ; '*'
    mov BYTE PTR [rdi + 3], 0

    ; Find files
    lea rcx, szExplorerPattern
    lea rdx, find_data
    call FindFirstFileA
    cmp rax, -1
    je done_simple
    mov rbx, rax

loop_files:
    ; Get filename (offset 44)
    lea r9, [find_data + 44]
    ; Skip "." and ".."
    mov al, BYTE PTR [r9]
    cmp al, 46
    jne add_it
    mov al, BYTE PTR [r9 + 1]
    test al, al
    jz next_file
    cmp al, 46
    jne add_it
    mov al, BYTE PTR [r9 + 2]
    test al, al
    jz next_file

add_it:
    mov rcx, hwndExplorer
    mov rdx, LB_ADDSTRING
    xor r8, r8
    ; r9 already points to filename
    call SendMessageA

next_file:
    mov rcx, rbx
    lea rdx, find_data
    call FindNextFileA
    test eax, eax
    jnz loop_files

    mov rcx, rbx
    call FindClose

done_simple:
    leave
    ret
ui_populate_explorer_simple ENDP

END
