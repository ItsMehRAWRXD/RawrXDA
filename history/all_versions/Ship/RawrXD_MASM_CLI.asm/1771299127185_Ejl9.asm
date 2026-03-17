; RawrXD Pure x64 MASM CLI
; A command-line interface for MASM operations, running in the IDE's right terminal pane
; Pure x64 assembly - no C runtime

.code

; Entry point
main PROC
    ; Initialize
    sub rsp, 32                          ; Shadow space for Windows calls
    
    ; Print banner
    lea rbx, [rel banner_msg]
    call print_string
    
    ; Main command loop
    jmp command_loop
    
command_loop:
    ; Print prompt
    lea rbx, [rel prompt_msg]
    call print_string
    
    ; Read input
    lea rbx, [rel input_buffer]
    mov rcx, 256
    call read_input
    
    ; Parse command
    lea rbx, [rel input_buffer]
    movzx eax, byte [rbx]               ; Check first char
    cmp al, 0                            ; Empty input?
    je command_loop
    
    ; Check "exit" command
    lea rcx, [rel exit_cmd]
    call strcmp
    cmp eax, 0
    je exit_program
    
    ; Check "help" command
    lea rcx, [rel help_cmd]
    call strcmp
    cmp eax, 0
    je show_help
    
    ; Check "assemble" command
    lea rcx, [rel asm_cmd]
    call strcmp
    cmp eax, 0
    je assemble_cmd
    
    ; Check "version" command
    lea rcx, [rel ver_cmd]
    call strcmp
    cmp eax, 0
    je show_version
    
    ; Unknown command
    lea rbx, [rel unknown_msg]
    call print_string
    jmp command_loop
    
show_help:
    lea rbx, [rel help_text]
    call print_string
    jmp command_loop
    
show_version:
    lea rbx, [rel version_msg]
    call print_string
    jmp command_loop
    
assemble_cmd:
    lea rbx, [rel asm_stub]
    call print_string
    jmp command_loop
    
exit_program:
    xor eax, eax                        ; Return 0
    add rsp, 32
    ret
main ENDP

; Print string function (simple stdout write)
print_string PROC
    ; rbx = pointer to string
    ; Print until null terminator
    xor ecx, ecx
    
print_loop:
    movzx eax, byte [rbx + rcx]
    cmp al, 0
    je print_done
    
    ; For now, just count length
    inc ecx
    jmp print_loop
    
print_done:
    ; TODO: Call Windows WriteFile API
    ret
print_string ENDP

; Read input function (simple stdin read)
read_input PROC
    ; rbx = buffer, rcx = max length
    ; TODO: Call Windows ReadFile API
    mov qword [rbx], 0                  ; For now, clear buffer
    ret
read_input ENDP

; String comparison
strcmp PROC
    ; rbx = string1, rcx = string2
    ; Returns 0 if equal in eax
    xor eax, eax
    
strcmp_loop:
    movzx edx, byte [rbx]
    movzx eax, byte [rcx]
    cmp dl, al
    jne strcmp_ne
    
    cmp dl, 0                            ; Check for end
    je strcmp_eq
    
    inc rbx
    inc rcx
    jmp strcmp_loop
    
strcmp_eq:
    xor eax, eax                        ; Equal
    ret
    
strcmp_ne:
    xor eax, eax
    dec eax                              ; Not equal (-1)
    ret
strcmp ENDP

.data

banner_msg:
    db "RawrXD MASM x64 CLI v1.0", 0dh, 0ah
    db "Pure assembly command processor", 0dh, 0ah
    db "Type 'help' for commands", 0dh, 0ah, 0ah, 0

prompt_msg:
    db "> ", 0

help_text:
    db "Available commands:", 0dh, 0ah
    db "  help           - Show this help", 0dh, 0ah
    db "  version        - Show version info", 0dh, 0ah
    db "  assemble FILE  - Assemble MASM x64 file", 0dh, 0ah
    db "  exit           - Exit CLI", 0dh, 0ah, 0ah, 0

version_msg:
    db "RawrXD MASM CLI v1.0 (x64 pure assembly)", 0dh, 0ah, 0

exit_cmd:
    db "exit", 0

help_cmd:
    db "help", 0

asm_cmd:
    db "assemble", 0

ver_cmd:
    db "version", 0

unknown_msg:
    db "Unknown command. Type 'help' for available commands.", 0dh, 0ah, 0

asm_stub:
    db "[MASM] Assembler not yet integrated", 0dh, 0ah, 0

input_buffer:
    db 256 dup(0)

END
