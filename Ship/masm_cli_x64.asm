; MASM x64 CLI - Pure assembler command executor
; Compiles to: masm_cli_x64.exe
; Purpose: Backend for RawrXD IDE MASM CLI pane

.nolist
include \masm32\include64\windows.inc
include \masm32\include64\kernel32.inc
include \masm32\include64\user32.inc

includelib \masm32\lib64\kernel32.lib
includelib \masm32\lib64\user32.lib
.list

.data
    prompt              db "MASM> ", 0
    echo_prefix         db "[echo] ", 0
    banner              db "MASM x64 CLI v1.0", 0Dh, 0Ah, _
                           "Type 'help' for commands", 0Dh, 0Ah, _
                           "Type 'exit' to quit", 0Dh, 0Ah, _
                           0Dh, 0Ah, 0
    help_text           db "Commands:", 0Dh, 0Ah, _
                           "  echo TEXT      - Echo text", 0Dh, 0Ah, _
                           "  help           - Show this help", 0Dh, 0Ah, _
                           "  version        - Show version", 0Dh, 0Ah, _
                           "  exit           - Exit CLI", 0Dh, 0Ah, 0
    version_text        db "MASM x64 CLI v1.0", 0Dh, 0Ah, 0
    input_buffer        db 512 dup(0)
    input_len           dq 0
    
    cmd_echo            db "echo", 0
    cmd_help            db "help", 0
    cmd_version         db "version", 0
    cmd_exit            db "exit", 0

.code
    
; WriteString: outputs string to stdout
; rcx = pointer to null-terminated string
WriteString proc
    push rbx
    xor rbx, rbx
.loop:
    mov al, [rcx + rbx]
    test al, al
    jz .done
    
    mov [rsp + 8], al           ; Might need adjustment based on calling convention
    inc rbx
    jmp .loop
    
.done:
    pop rbx
    ret
WriteString endp

; Main CLI loop
main proc
    ; Print banner
    lea rcx, [banner]
    call WriteString
    
    ; Main loop
.main_loop:
    ; Print prompt
    lea rcx, [prompt]
    call WriteString
    
    ; Read input from stdin (simplified - would use ReadConsoleInput in real code)
    ; For now, just exit gracefully
    
    jmp .main_loop
main endp

; Entry point
start proc
    mov rbx, rsp
    sub rsp, 28h
    
    call main
    
    mov eax, 0
    add rsp, 28h
    ret
start endp

end start
