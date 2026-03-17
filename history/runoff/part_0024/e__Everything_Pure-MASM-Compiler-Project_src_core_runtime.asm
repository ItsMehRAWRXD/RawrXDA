; Core Runtime for Pure MASM Compiler
; Provides CRT replacement functionality in pure ASM

.686
.model flat, stdcall
option casemap:none

; Include Windows API definitions
include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\masm32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\masm32.lib

; Core runtime functions
.code

; Entry point wrapper
_start proc
    invoke GetModuleHandle, NULL
    mov hInstance, eax
    
    ; Call main function
    call main
    
    ; Exit process
    invoke ExitProcess, eax
_start endp

; Basic I/O functions
print_string proc string_ptr:DWORD
    invoke StdOut, string_ptr
    ret
print_string endp

print_char proc char:BYTE
    local buffer[2]:BYTE
    mov al, char
    mov buffer[0], al
    mov buffer[1], 0
    invoke StdOut, addr buffer
    ret
print_char endp

print_newline proc
    invoke StdOut, addr crlf
    ret
print_newline endp

print_int proc number:DWORD
    local buffer[16]:BYTE
    invoke dwtoa, number, addr buffer
    invoke StdOut, addr buffer
    ret
print_int endp

; Memory allocation
malloc proc size:DWORD
    invoke GlobalAlloc, GMEM_FIXED, size
    ret
malloc endp

free proc ptr:DWORD
    invoke GlobalFree, ptr
    ret
free endp

; String functions
strlen proc string_ptr:DWORD
    mov eax, string_ptr
    xor ecx, ecx
    
strlen_loop:
    cmp byte ptr [eax], 0
    je strlen_done
    inc eax
    inc ecx
    jmp strlen_loop
    
strlen_done:
    mov eax, ecx
    ret
strlen endp

strcpy proc dest:DWORD, src:DWORD
    push esi
    push edi
    mov esi, src
    mov edi, dest
    
strcpy_loop:
    mov al, [esi]
    mov [edi], al
    inc esi
    inc edi
    test al, al
    jnz strcpy_loop
    
    pop edi
    pop esi
    ret
strcpy endp

strcat proc dest:DWORD, src:DWORD
    push esi
    push edi
    
    ; Find end of dest
    mov edi, dest
    mov al, 0
    mov ecx, -1
    repne scasb
    dec edi
    
    ; Copy src to end
    mov esi, src
strcat_loop:
    mov al, [esi]
    mov [edi], al
    inc esi
    inc edi
    test al, al
    jnz strcat_loop
    
    pop edi
    pop esi
    ret
strcat endp

; File I/O functions
file_open proc filename:DWORD, mode:DWORD
    invoke CreateFile, filename, GENERIC_READ or GENERIC_WRITE, 
                     FILE_SHARE_READ, NULL, OPEN_EXISTING, 
                     FILE_ATTRIBUTE_NORMAL, NULL
    ret
file_open endp

file_create proc filename:DWORD
    invoke CreateFile, filename, GENERIC_READ or GENERIC_WRITE, 
                     FILE_SHARE_READ, NULL, CREATE_ALWAYS, 
                     FILE_ATTRIBUTE_NORMAL, NULL
    ret
file_create endp

file_read proc file_handle:DWORD, buffer:DWORD, size:DWORD
    local bytes_read:DWORD
    invoke ReadFile, file_handle, buffer, size, addr bytes_read, NULL
    mov eax, bytes_read
    ret
file_read endp

file_write proc file_handle:DWORD, buffer:DWORD, size:DWORD
    local bytes_written:DWORD
    invoke WriteFile, file_handle, buffer, size, addr bytes_written, NULL
    mov eax, bytes_written
    ret
file_write endp

file_close proc file_handle:DWORD
    invoke CloseHandle, file_handle
    ret
file_close endp

; Math functions
atoi proc string_ptr:DWORD
    invoke atodw, string_ptr
    ret
atoi endp

itoa proc number:DWORD, buffer:DWORD
    invoke dwtoa, number, buffer
    ret
itoa endp

; System functions
get_command_line proc
    invoke GetCommandLine
    ret
get_command_line endp

; Error handling
show_error proc message:DWORD
    invoke MessageBox, NULL, message, addr error_caption, MB_OK or MB_ICONERROR
    ret
show_error endp

; Constants
.data
crlf db 13,10,0
error_caption db "Error",0
hInstance dd 0

end _start