;==========================================================================
; model_runtime.asm - Dynamic Model Loading, Persistence & Config
; Handles: Model enumeration, caching, settings persistence, terminal
;==========================================================================

option casemap:none

;==========================================================================
; CONSTANTS & STRUCTURES
;==========================================================================

GENERIC_READ            equ 80000000h
GENERIC_WRITE           equ 40000000h
FILE_SHARE_READ         equ 1h
FILE_SHARE_WRITE        equ 2h
OPEN_EXISTING           equ 3
CREATE_ALWAYS           equ 2
FILE_ATTRIBUTE_NORMAL   equ 80h

;==========================================================================
; Data Structures (MASM64 compatible format)
;==========================================================================

; MODEL_ENTRY: 8 + 256 + 512 + 1 + 1 + padding = 784 bytes
;   Offset 0: size (QWORD)
;   Offset 8: name (256 bytes)
;   Offset 264: path (512 bytes)
;   Offset 776: type (1 byte, 0=gguf, 1=safetensors, 2=other)
;   Offset 777: loaded (1 byte)

; CONFIG_ENTRY: 128 + 512 = 640 bytes
;   Offset 0: key (128 bytes)
;   Offset 128: value (512 bytes)

;==========================================================================
; EXTERN DECLARATIONS
;==========================================================================

EXTERN GetLogicalDriveStringsA:PROC
EXTERN FindFirstFileA:PROC
EXTERN FindNextFileA:PROC
EXTERN FindClose:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN SetCurrentDirectoryA:PROC
EXTERN GetCurrentDirectoryA:PROC
EXTERN CreateProcessA:PROC
EXTERN TerminateProcess:PROC
EXTERN GetExitCodeProcess:PROC
EXTERN CreatePipeA:PROC
EXTERN RegCreateKeyExA:PROC
EXTERN RegSetValueExA:PROC
EXTERN RegQueryValueExA:PROC
EXTERN RegCloseKey:PROC

includelib user32.lib
includelib kernel32.lib
includelib advapi32.lib

;==========================================================================
; DATA SECTION
;==========================================================================

.data
    model_dir       BYTE "C:\models", 0
    models_json     BYTE "models.json", 0
    config_file     BYTE "rawr_config.json", 0
    terminal_cmd    BYTE "cmd.exe", 0
    
    ; MODEL_ENTRY array: 64 entries * 784 bytes each = 50,176 bytes
    models_list     BYTE 50176 dup(0)
    model_count     DWORD 0
    
    ; CONFIG_ENTRY array: 128 entries * 640 bytes each = 81,920 bytes
    config_data     BYTE 81920 dup(0)
    config_count    DWORD 0

.data?
    hModelsFile     QWORD ?
    hConfigFile     QWORD ?
    hProcessHandle  QWORD ?
    hTerminalRead   QWORD ?
    hTerminalWrite  QWORD ?

;==========================================================================
; CODE SECTION
;==========================================================================

.code

;==========================================================================
; model_runtime_init - Initialize model system
;==========================================================================
PUBLIC model_runtime_init
model_runtime_init PROC
    push rbp
    mov rbp, rsp
    
    ; Load config from file
    call load_config_json
    
    ; Enumerate available models
    call enumerate_models
    
    ; Initialize terminal
    call init_terminal_pipe
    
    mov rax, 1  ; Success
    leave
    ret
model_runtime_init ENDP

;==========================================================================
; enumerate_models - Scan for GGUF/model files
;==========================================================================
enumerate_models PROC
    push rbp
    mov rbp, rsp
    sub rsp, 512
    
    ; Build search pattern: model_dir + "\*.*"
    lea rcx, model_dir
    lea rdx, [rbp - 512]  ; pattern buffer
    call build_search_pattern
    
    ; Find files
    lea rcx, [rbp - 512]
    lea rdx, [rbp - 600]  ; WIN32_FIND_DATAA
    call FindFirstFileA
    
    cmp rax, -1
    je enum_done
    
    mov rbx, rax  ; find handle
    
enum_loop:
    ; Check file extension
    lea rsi, [rbp - 600 + 44]  ; filename offset in WIN32_FIND_DATAA
    
    ; Look for .gguf extension
    call check_model_file
    test eax, eax
    jz enum_next
    
    ; Add to models list
    call add_model_entry
    
enum_next:
    mov rcx, rbx
    lea rdx, [rbp - 600]
    call FindNextFileA
    test eax, eax
    jnz enum_loop
    
    mov rcx, rbx
    call FindClose
    
enum_done:
    leave
    ret
enumerate_models ENDP

;==========================================================================
; load_config_json - Load configuration from JSON file
;==========================================================================
load_config_json PROC
    push rbp
    mov rbp, rsp
    sub rsp, 1024
    
    ; Open config file
    lea rcx, config_file
    mov rdx, GENERIC_READ
    mov r8, FILE_SHARE_READ
    xor r9, r9
    mov QWORD PTR [rsp + 32], OPEN_EXISTING
    mov QWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp + 48], 0
    
    call CreateFileA
    mov rbx, rax
    cmp rbx, -1
    je load_config_done
    
    ; Read entire file
    mov rcx, rbx
    lea rdx, [rbp - 1024]
    mov r8, 1024
    lea r9, [rbp - 1032]  ; bytes read
    call ReadFile
    
    ; Parse JSON config
    lea rcx, [rbp - 1024]
    call parse_json_config
    
    ; Close file
    mov rcx, rbx
    call CloseHandle
    
load_config_done:
    leave
    ret
load_config_json ENDP

;==========================================================================
; parse_json_config - Parse JSON and populate config_data
;==========================================================================
parse_json_config PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = JSON string
    mov rsi, rcx
    xor r8, r8  ; config_count
    
parse_loop:
    ; Find key-value pairs
    mov al, BYTE PTR [rsi]
    test al, al
    jz parse_done
    
    ; Look for '"' (key start)
    cmp al, '"'
    jne parse_skip_char
    
    ; Extract key - calculate offset: r8 * 640 (size of CONFIG_ENTRY)
    mov rax, r8
    imul rax, 640
    lea rdi, [config_data + rax]
    call extract_json_string
    
    ; Skip to value
    call skip_to_colon
    
    ; Extract value - offset is 128 bytes into CONFIG_ENTRY
    mov rax, r8
    imul rax, 640
    lea rdi, [config_data + rax + 128]
    call extract_json_string
    
    inc r8
    cmp r8, 128
    jl parse_loop

parse_skip_char:
    inc rsi
    jmp parse_loop
    
parse_done:
    mov config_count, r8d
    leave
    ret
parse_json_config ENDP

;==========================================================================
; init_terminal_pipe - Create pipes for terminal communication
;==========================================================================
init_terminal_pipe PROC
    push rbp
    mov rbp, rsp
    sub rsp, 200h
    
    push rbx
    push rsi
    push rdi
    
    ; Production implementation: Create anonymous pipes
    ; HANDLE hRead, hWrite
    
    ; Create pipe for terminal communication
    lea rcx, [rbp - 8]          ; phReadPipe
    lea rdx, [rbp - 16]         ; phWritePipe
    xor r8, r8                  ; lpPipeAttributes = NULL
    mov r9d, 4096               ; nSize = 4KB buffer
    
    call CreatePipe
    test eax, eax
    jz pipe_fail
    
    ; Store pipe handles
    mov rax, [rbp - 8]
    mov hTerminalRead, rax
    mov rax, [rbp - 16]
    mov hTerminalWrite, rax
    
    ; Setup STARTUPINFO structure
    lea rdi, [rbp - 180h]
    mov ecx, 104                ; sizeof(STARTUPINFO)
    xor al, al
    rep stosb
    
    lea rdi, [rbp - 180h]
    mov dword ptr [rdi], 104    ; cb = size
    
    ; dwFlags: STARTF_USESTDHANDLES
    mov dword ptr [rdi + 60], 00000100h
    
    ; Set std handles to our pipe
    mov rax, hTerminalRead
    mov qword ptr [rdi + 64], rax   ; hStdInput
    mov rax, hTerminalWrite
    mov qword ptr [rdi + 72], rax   ; hStdOutput
    mov qword ptr [rdi + 80], rax   ; hStdError
    
    ; Setup PROCESS_INFORMATION structure
    lea rsi, [rbp - 280h]
    mov ecx, 24                 ; sizeof(PROCESS_INFORMATION)
    xor al, al
    rep stosb
    
    ; Create cmd.exe process
    ; CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)
    xor rcx, rcx                ; lpApplicationName = NULL
    lea rdx, [terminal_cmd]     ; lpCommandLine
    xor r8, r8                  ; lpProcessAttributes = NULL
    xor r9, r9                  ; lpThreadAttributes = NULL
    
    mov dword ptr [rsp + 32], 1 ; bInheritHandles = TRUE
    mov dword ptr [rsp + 40], 0 ; dwCreationFlags = 0
    mov qword ptr [rsp + 48], 0 ; lpEnvironment = NULL
    mov qword ptr [rsp + 56], 0 ; lpCurrentDirectory = NULL
    lea rax, [rbp - 180h]
    mov qword ptr [rsp + 64], rax ; lpStartupInfo
    lea rax, [rbp - 280h]
    mov qword ptr [rsp + 72], rax ; lpProcessInformation
    
    call CreateProcessA
    test eax, eax
    jz process_fail
    
    ; Store process handle
    lea rsi, [rbp - 280h]
    mov rax, qword ptr [rsi]    ; hProcess
    mov hProcessHandle, rax
    
    ; Close unnecessary handles
    mov rcx, qword ptr [rsi + 8] ; hThread
    call CloseHandle
    
    mov eax, 1                  ; Success
    jmp term_init_done
    
process_fail:
pipe_fail:
    xor eax, eax                ; Failure
    
term_init_done:
    pop rdi
    pop rsi
    pop rbx
    leave
    ret
init_terminal_pipe ENDP

;==========================================================================
; run_terminal_command - Execute command in terminal
;==========================================================================
PUBLIC run_terminal_command
run_terminal_command PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; rcx = command string
    ; Save command pointer
    mov rsi, rcx
    
    ; Calculate length
    xor r9, r9
count_cmd:
    mov al, BYTE PTR [rsi + r9]
    test al, al
    jz cmd_write
    inc r9
    jmp count_cmd
    
cmd_write:
    mov rcx, hTerminalWrite
    mov rdx, rsi  ; command pointer
    mov r8, r9    ; length
    lea r9, [rsp + 16]  ; bytes written
    
    call WriteFile
    
    ; Add newline
    mov rcx, hTerminalWrite
    lea rdx, newline_char
    mov r8, 1
    lea r9, [rsp + 16]
    
    call WriteFile
    
    leave
    ret
run_terminal_command ENDP

;==========================================================================
; save_model_selection - Persist selected model to config
;==========================================================================
PUBLIC save_model_selection
save_model_selection PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = selected model name
    
    ; Save to config_data (first CONFIG_ENTRY, at offset 128 for value)
    lea rdi, [config_data + 128]
    mov rsi, rcx
save_loop:
    mov al, BYTE PTR [rsi]
    mov BYTE PTR [rdi], al
    test al, al
    jz save_config_file
    inc rsi
    inc rdi
    jmp save_loop
    
save_config_file:
    ; Write updated config to file
    call write_config_json
    
    leave
    ret
save_model_selection ENDP

;==========================================================================
; write_config_json - Write configuration to JSON file
;==========================================================================
write_config_json PROC
    push rbp
    mov rbp, rsp
    sub rsp, 2048
    
    ; Build JSON structure
    lea rcx, [rbp - 2048]
    call build_config_json
    
    ; Open/create config file
    lea rcx, config_file
    mov rdx, GENERIC_WRITE
    mov r8, 0
    xor r9, r9
    mov QWORD PTR [rsp + 32], CREATE_ALWAYS
    mov QWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp + 48], 0
    
    call CreateFileA
    mov rbx, rax
    cmp rbx, -1
    je write_config_done
    
    ; Write JSON
    mov rcx, rbx
    lea rdx, [rbp - 2048]
    mov r8, 2048
    lea r9, [rsp + 16]  ; bytes written
    call WriteFile
    
    ; Close
    mov rcx, rbx
    call CloseHandle
    
write_config_done:
    leave
    ret
write_config_json ENDP

;==========================================================================
; build_config_json - Construct JSON from config_data
;==========================================================================
build_config_json PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = output buffer
    mov rdi, rcx
    
    ; Write opening brace
    mov BYTE PTR [rdi], '{'
    inc rdi
    
    xor r8, r8  ; counter
build_loop:
    cmp r8d, config_count
    jge build_done
    
    ; Write "key": "value",
    mov BYTE PTR [rdi], '"'
    inc rdi
    
    ; Calculate offset: r8 * 640 (size of CONFIG_ENTRY)
    mov rax, r8
    imul rax, 640
    lea rsi, [config_data + rax]
    call copy_string
    
    mov BYTE PTR [rdi], '"'
    inc rdi
    mov BYTE PTR [rdi], ':'
    inc rdi
    mov BYTE PTR [rdi], ' '
    inc rdi
    mov BYTE PTR [rdi], '"'
    inc rdi
    
    ; Extract value - offset is 128 bytes into CONFIG_ENTRY
    mov rax, r8
    imul rax, 640
    lea rsi, [config_data + rax + 128]
    call copy_string
    
    mov BYTE PTR [rdi], '"'
    inc rdi
    
    inc r8
    cmp r8d, config_count
    jge build_done
    
    mov BYTE PTR [rdi], ','
    inc rdi
    
    jmp build_loop
    
build_done:
    mov BYTE PTR [rdi], '}'
    inc rdi
    mov BYTE PTR [rdi], 0
    
    leave
    ret
build_config_json ENDP

;==========================================================================
; Helper Functions
;==========================================================================

build_search_pattern PROC
    ; rcx = dir, rdx = output
    mov rsi, rcx
    mov rdi, rdx
search_loop:
    mov al, BYTE PTR [rsi]
    mov BYTE PTR [rdi], al
    test al, al
    jz search_add_pattern
    inc rsi
    inc rdi
    jmp search_loop
search_add_pattern:
    mov BYTE PTR [rdi - 1], '\'
    mov BYTE PTR [rdi], '*'
    mov BYTE PTR [rdi + 1], '.'
    mov BYTE PTR [rdi + 2], '*'
    mov BYTE PTR [rdi + 3], 0
    ret
build_search_pattern ENDP

check_model_file PROC
    ; rsi = filename
    mov rdi, rsi
find_dot:
    mov al, BYTE PTR [rdi]
    test al, al
    jz check_not_model
    cmp al, '.'
    je found_dot
    inc rdi
    jmp find_dot
found_dot:
    mov al, BYTE PTR [rdi + 1]
    mov cl, BYTE PTR [rdi + 2]
    mov dl, BYTE PTR [rdi + 3]
    
    ; Check for .gguf
    cmp al, 'g'
    jne check_safetensors
    cmp cl, 'g'
    jne check_safetensors
    cmp dl, 'u'
    jne check_safetensors
    
    mov eax, 1
    ret
    
check_safetensors:
    cmp al, 's'
    jne check_not_model
    mov eax, 1
    ret
    
check_not_model:
    xor eax, eax
    ret
check_model_file ENDP

add_model_entry PROC
    ; rsi = filename, r8d = file size (high dword)
    push rbx
    
    mov eax, model_count
    cmp eax, 64
    jge add_model_done
    
    ; Calculate offset: eax * 784 (size of MODEL_ENTRY)
    mov rbx, rax
    imul rbx, 784
    lea rdi, [models_list + rbx]
    mov rsi, rsi
copy_name:
    mov al, BYTE PTR [rsi]
    mov BYTE PTR [rdi], al
    test al, al
    jz add_model_done
    inc rsi
    inc rdi
    jmp copy_name
    
add_model_done:
    inc model_count
    pop rbx
    ret
add_model_entry ENDP

copy_string PROC
    ; rsi = source, rdi = dest
copy_loop:
    mov al, BYTE PTR [rsi]
    test al, al
    jz copy_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    jmp copy_loop
copy_done:
    ret
copy_string ENDP

skip_to_colon PROC
    ; rsi = current position, find ':'
skip_loop:
    mov al, BYTE PTR [rsi]
    cmp al, ':'
    je skip_found
    test al, al
    jz skip_found
    inc rsi
    jmp skip_loop
skip_found:
    inc rsi  ; skip the ':'
    ret
skip_to_colon ENDP

extract_json_string PROC
    ; rsi = source, rdi = destination
    ; Look for opening quote
find_quote:
    mov al, BYTE PTR [rsi]
    cmp al, '"'
    je quote_found
    test al, al
    jz extract_done
    inc rsi
    jmp find_quote
    
quote_found:
    inc rsi  ; skip opening quote
    
extract_loop:
    mov al, BYTE PTR [rsi]
    cmp al, '"'
    je extract_done
    test al, al
    jz extract_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    jmp extract_loop
    
extract_done:
    mov BYTE PTR [rdi], 0
    inc rsi
    ret
extract_json_string ENDP

.data
    newline_char BYTE 10, 0

END
