; RawrXD_CommandCLI.asm - Pure x64 MASM CLI Command Interface
; Provides command parsing, execution, and result formatting
; Communicates with Win32 IDE via named pipe IPC

.code

; Command opcodes
CMD_NOOP        equ 0
CMD_FILE_OPEN   equ 1
CMD_FILE_SAVE   equ 2
CMD_BUILD_RUN   equ 3
CMD_AI_COMPLETE equ 4
CMD_SEARCH      equ 5
CMD_HELP        equ 6
CMD_EXIT        equ 7

; Command structure (passed via named pipe)
; struct CommandRequest {
;   uint32_t opcode;      [rsp+0]
;   char     argbuf[256]; [rsp+4]
; }

; ============================================================================
; Core Command Dispatcher
; ============================================================================

; CommandDispatch(opcode: rcx, argbuf: rdx) -> result_code: rax
public CommandDispatch
CommandDispatch PROC
    push rbx
    push r12
    
    mov r12, rdx            ; r12 = argbuf
    mov ebx, ecx            ; ebx = opcode
    xor eax, eax            ; eax = result (0 = success)
    
    ; Dispatch by opcode
    cmp ebx, CMD_FILE_OPEN
    je dispatch_file_open
    cmp ebx, CMD_FILE_SAVE
    je dispatch_file_save
    cmp ebx, CMD_BUILD_RUN
    je dispatch_build_run
    cmp ebx, CMD_AI_COMPLETE
    je dispatch_ai_complete
    cmp ebx, CMD_SEARCH
    je dispatch_search
    cmp ebx, CMD_HELP
    je dispatch_help
    cmp ebx, CMD_EXIT
    je dispatch_exit
    
    mov eax, -1             ; Unknown command
    jmp dispatch_done
    
dispatch_file_open:
    ; TODO: File open logic
    mov eax, 0
    jmp dispatch_done
    
dispatch_file_save:
    ; TODO: File save logic
    mov eax, 0
    jmp dispatch_done
    
dispatch_build_run:
    ; TODO: Build/run logic
    mov eax, 0
    jmp dispatch_done
    
dispatch_ai_complete:
    ; TODO: AI completion logic
    mov eax, -1             ; Not implemented (no models available)
    jmp dispatch_done
    
dispatch_search:
    ; TODO: Search logic
    mov eax, 0
    jmp dispatch_done
    
dispatch_help:
    ; Return help text length or invoke help display
    mov eax, 1
    jmp dispatch_done
    
dispatch_exit:
    mov eax, 0
    jmp dispatch_done
    
dispatch_done:
    pop r12
    pop rbx
    ret
CommandDispatch ENDP

; ============================================================================
; String Parsing Utilities
; ============================================================================

; ParseCommand(input_string: rcx) -> opcode: rax
public ParseCommand
ParseCommand PROC
    push rbx
    xor eax, eax            ; eax = opcode (default 0 = NOOP)
    xor rbx, rbx            ; rbx = char counter
    
    ; Check first word
    mov al, byte ptr [rcx]
    cmp al, 0
    je parse_done
    
    ; Simple keyword matching
    cmp byte ptr [rcx], 'o'
    jne check_next_cmd
    cmp byte ptr [rcx+1], 'p'
    jne check_next_cmd
    cmp byte ptr [rcx+2], 'e'
    jne check_next_cmd
    ; "open" detected
    mov eax, CMD_FILE_OPEN
    jmp parse_done
    
check_next_cmd:
    cmp byte ptr [rcx], 's'
    jne check_build
    cmp byte ptr [rcx+1], 'a'
    jne check_build
    cmp byte ptr [rcx+2], 'v'
    jne check_build
    ; "save" detected
    mov eax, CMD_FILE_SAVE
    jmp parse_done
    
check_build:
    cmp byte ptr [rcx], 'b'
    jne check_ai
    cmp byte ptr [rcx+1], 'u'
    jne check_ai
    cmp byte ptr [rcx+2], 'i'
    jne check_ai
    cmp byte ptr [rcx+3], 'l'
    jne check_ai
    cmp byte ptr [rcx+4], 'd'
    jne check_ai
    ; "build" detected
    mov eax, CMD_BUILD_RUN
    jmp parse_done
    
check_ai:
    cmp byte ptr [rcx], 'a'
    jne check_search
    cmp byte ptr [rcx+1], 'i'
    jne check_search
    ; "ai" detected
    mov eax, CMD_AI_COMPLETE
    jmp parse_done
    
check_search:
    cmp byte ptr [rcx], 'e'
    jne parse_done
    cmp byte ptr [rcx+1], 'x'
    jne parse_done
    cmp byte ptr [rcx+2], 'i'
    jne parse_done
    cmp byte ptr [rcx+3], 't'
    jne parse_done
    ; "exit" detected
    mov eax, CMD_EXIT
    
parse_done:
    pop rbx
    ret
ParseCommand ENDP

; ============================================================================
; Command Result Formatter (formats response for GUI)
; ============================================================================

; FormatResult(result_code: rcx, output_buffer: rdx, buffer_size: r8) -> chars_written: rax
public FormatResult
FormatResult PROC
    push rbx
    push r12
    push r13
    
    mov r12, rdx            ; r12 = output_buffer
    mov r13, r8             ; r13 = buffer_size
    xor rax, rax            ; rax = chars_written
    xor rbx, rbx            ; rbx = current char
    
    ; Format based on result code
    cmp ecx, 0
    je format_success
    cmp ecx, -1
    je format_error
    
    ; Generic result
    mov byte ptr [r12], 'O'
    mov byte ptr [r12+1], 'K'
    mov rax, 2
    jmp format_done
    
format_success:
    mov byte ptr [r12], 'O'
    mov byte ptr [r12+1], 'K'
    mov byte ptr [r12+2], 0
    mov rax, 2
    jmp format_done
    
format_error:
    mov byte ptr [r12], 'E'
    mov byte ptr [r12+1], 'R'
    mov byte ptr [r12+2], 'R'
    mov byte ptr [r12+3], 0
    mov rax, 3
    jmp format_done
    
format_done:
    pop r13
    pop r12
    pop rbx
    ret
FormatResult ENDP

END
