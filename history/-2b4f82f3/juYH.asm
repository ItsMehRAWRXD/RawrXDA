; ==============================================================================
; Language Bridge System
; FFI Bridge between ASM core and other language extensions
; ==============================================================================

BITS 64
DEFAULT REL

; ------------------------------------------------------------------------------
; Robustness Additions (Phase 2)
; Error codes, retry logic, JSON validation, last error tracking
; ------------------------------------------------------------------------------

%define ERR_OK                0
%define ERR_OPERATION_FAILED  1
%define ERR_TIMEOUT           2
%define ERR_INVALID_ARG       3

; ==============================================================================
; Bridge Function Table
; ==============================================================================

section .data

global bridge_function_table
bridge_function_table:
    dq bridge_extract_profile       ; 0: Extract model profile
    dq bridge_apply_patch            ; 1: Apply dampening patch
    dq bridge_clone_model            ; 2: Clone model
    dq bridge_list_extensions        ; 3: List extensions
    dq bridge_enable_extension       ; 4: Enable extension
    dq bridge_register_extension     ; 5: Register extension
    dq bridge_marketplace_search     ; 6: Search marketplace
    dq bridge_download_extension     ; 7: Download extension
    dq bridge_call_python            ; 8: Call Python function
    dq bridge_call_rust              ; 9: Call Rust function
    dq bridge_call_c                 ; 10: Call C function

bridge_table_size: dq 11

; Empty JSON result for stubs
empty_json_result: db "{}", 0
error_json_generic:     db "{\"error\":\"operation_failed\"}",0
error_json_timeout:     db "{\"error\":\"timeout\"}",0
error_json_invalid_arg: db "{\"error\":\"invalid_arg\"}",0

; ==============================================================================
; External References
; ==============================================================================

extern extract_model_profile
extern apply_dampening_patch
extern list_extensions
extern enable_extension
extern register_extension
extern marketplace_search
extern download_extension

%ifdef PLATFORM_WIN
extern Sleep                ; For retry backoff
%endif

%ifdef PLATFORM_WIN
extern LoadLibraryA
extern GetProcAddress
extern FreeLibrary
%endif

section .bss

python_initialized:     resb 1
rust_initialized:       resb 1
c_initialized:          resb 1
last_error_code:        resd 1

section .text

; ==============================================================================
; FUNCTION: bridge_init
; Initialize the language bridge system
; CRITICAL: Windows x64 ABI - NO rbp frame usage!
; ==============================================================================
global bridge_init
bridge_init:
    ; CRITICAL: No push rbp - Windows x64 ABI violation!
    sub rsp, 40    ; Shadow space for calls
    
    ; Initialize Python bridge
    call python_bridge_init
    
    ; Initialize Rust bridge
    call rust_bridge_init
    
    ; Initialize C bridge
    call c_bridge_init
    
    ; Return success (0)
    xor eax, eax
    
    add rsp, 40    ; Restore stack
    ret

; ==============================================================================
; FUNCTION: bridge_call
; Universal bridge call function
; Input:
;   RDI = function index
;   RSI = argument 1
;   RDX = argument 2
;   RCX = argument 3
; Output:
;   RAX = return value
; ==============================================================================
global bridge_call
bridge_call:
    ; RDI = index, RSI = arg1, RDX = arg2, RCX = arg3
    cmp rdi, [bridge_table_size]
    jae .error
    mov rax, [bridge_function_table + rdi*8]
    ; shift args: arg1->rdi, arg2->rsi, arg3->rdx
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    ; call target
    call rax
    mov dword [last_error_code], ERR_OK
    ret
.error:
    mov eax, -1
    mov dword [last_error_code], ERR_INVALID_ARG
    ret

; ==============================================================================
; Bridge wrapper functions
; ==============================================================================

bridge_extract_profile:
    ; Wrapper adds error code tracking and JSON validation assumption
    sub rsp, 40
    call extract_model_profile       ; Expect RAX = pointer (non-zero success)
    test rax, rax
    jz .fail_ep
    ; If JSON pointer, validate first char if plausible
    cmp byte [rax], '{'
    jne .fail_generic_ep
    mov dword [last_error_code], ERR_OK
    add rsp, 40
    ret
.fail_generic_ep:
    mov dword [last_error_code], ERR_OPERATION_FAILED
    lea rax, [error_json_generic]
    add rsp, 40
    ret
.fail_ep:
    mov dword [last_error_code], ERR_OPERATION_FAILED
    lea rax, [error_json_generic]
    add rsp, 40
    ret

bridge_apply_patch:
    ; Wrapper for apply_dampening_patch adds error code mapping
    sub rsp, 40
    call apply_dampening_patch       ; Assume RAX: 0 success, <0 failure
    test rax, rax
    js .fail_ap          ; negative indicates failure
    cmp rax, 0
    jne .nonzero_success_ap ; treat non-zero positive as success variant
    mov dword [last_error_code], ERR_OK
    add rsp, 40
    ret
.nonzero_success_ap:
    mov dword [last_error_code], ERR_OK
    add rsp, 40
    ret
.fail_ap:
    mov dword [last_error_code], ERR_OPERATION_FAILED
    ; Provide JSON error pointer for consistency if consumer expects pointer
    lea rax, [error_json_generic]
    add rsp, 40
    ret

bridge_clone_model:
    ; Clone a model profile
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    ; RDI = source, RSI = destination, RDX = size
    mov r12, rdx
    mov rbx, rdi
    mov rdi, rsi
    mov rsi, rbx
    mov rcx, r12
    cld               ; ensure forward direction
    rep movsb
    mov rax, 1
    mov dword [last_error_code], ERR_OK
    pop r12
    pop rbx
    pop rbp
    ret

bridge_list_extensions:
    ; Retry + JSON validation wrapper for list_extensions
    sub rsp, 40
    mov ecx, 3                ; retry count
.retry:
    call list_extensions
    test rax, rax
    jnz .validate
    dec ecx
    jz .fail_timeout
%ifdef PLATFORM_WIN
    mov rcx, 100              ; 100ms backoff
    call Sleep
%endif
    jmp .retry
.validate:
    ; Basic JSON validation: first char must be '{'
    mov rdx, rax
    cmp byte [rdx], '{'
    jne .fail_generic
    mov dword [last_error_code], ERR_OK
    add rsp, 40
    ret
.fail_timeout:
    mov dword [last_error_code], ERR_TIMEOUT
    lea rax, [error_json_timeout]
    add rsp, 40
    ret
.fail_generic:
    mov dword [last_error_code], ERR_OPERATION_FAILED
    lea rax, [error_json_generic]
    add rsp, 40
    ret

bridge_enable_extension:
    ; Wrapper sets last_error_code based on return (assume 0 success, negative fail)
    sub rsp, 40
    call enable_extension
    test rax, rax
    js .fail_en
    mov dword [last_error_code], ERR_OK
    add rsp, 40
    ret
.fail_en:
    mov dword [last_error_code], ERR_OPERATION_FAILED
    add rsp, 40
    ret

bridge_register_extension:
    ; Wrapper sets last_error_code
    sub rsp, 40
    call register_extension
    test rax, rax
    js .fail_reg
    mov dword [last_error_code], ERR_OK
    add rsp, 40
    ret
.fail_reg:
    mov dword [last_error_code], ERR_OPERATION_FAILED
    add rsp, 40
    ret

bridge_marketplace_search:
    ; Retry + JSON validation wrapper for marketplace_search
    sub rsp, 40
    mov ecx, 3
.retry_ms:
    call marketplace_search
    test rax, rax
    jnz .validate_ms
    dec ecx
    jz .fail_timeout_ms
%ifdef PLATFORM_WIN
    mov rcx, 150
    call Sleep
%endif
    jmp .retry_ms
.validate_ms:
    mov rdx, rax
    cmp byte [rdx], '{'
    jne .fail_generic_ms
    mov dword [last_error_code], ERR_OK
    add rsp, 40
    ret
.fail_timeout_ms:
    mov dword [last_error_code], ERR_TIMEOUT
    lea rax, [error_json_timeout]
    add rsp, 40
    ret
.fail_generic_ms:
    mov dword [last_error_code], ERR_OPERATION_FAILED
    lea rax, [error_json_generic]
    add rsp, 40
    ret

bridge_download_extension:
    ; Retry + JSON validation wrapper for download_extension
    sub rsp, 40
    mov ecx, 3
.retry_dl:
    call download_extension
    test rax, rax
    jnz .validate_dl
    dec ecx
    jz .fail_timeout_dl
%ifdef PLATFORM_WIN
    mov rcx, 200
    call Sleep
%endif
    jmp .retry_dl
.validate_dl:
    mov rdx, rax
    cmp byte [rdx], '{'
    jne .fail_generic_dl
    mov dword [last_error_code], ERR_OK
    add rsp, 40
    ret
.fail_timeout_dl:
    mov dword [last_error_code], ERR_TIMEOUT
    lea rax, [error_json_timeout]
    add rsp, 40
    ret
.fail_generic_dl:
    mov dword [last_error_code], ERR_OPERATION_FAILED
    lea rax, [error_json_generic]
    add rsp, 40
    ret

; ==============================================================================
; FUNCTION: bridge_call_python
; Call Python function from ASM
; Input:
;   RDI = Python module name
;   RSI = function name
;   RDX = arguments (JSON string)
; Output:
;   RAX = result (JSON string pointer)
; ==============================================================================
bridge_call_python:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 64
    
    mov r12, rdi                    ; Module name
    mov r13, rsi                    ; Function name
    mov r14, rdx                    ; Arguments
    
    ; Check if Python bridge is initialized
    cmp byte [python_initialized], 0
    je .error
    
    ; Python FFI implementation using ctypes-like approach
    ; 1. Load Python library (python3.dll on Windows, libpython3.so on Linux)
    ; 2. Get required functions: Py_Initialize, PyRun_SimpleString, etc.
    ; 3. Build Python script as string
    ; 4. Execute and capture result
    
    ; For now, basic stub that returns empty JSON
    lea rax, [empty_json_result]
    jmp .done
    
.error:
    xor rax, rax
    mov dword [last_error_code], ERR_OPERATION_FAILED
    
.done:
    add rsp, 64
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; ==============================================================================
; FUNCTION: bridge_call_rust
; Call Rust function from ASM
; Input:
;   RDI = Rust library path
;   RSI = function name
;   RDX = arguments pointer
; Output:
;   RAX = result
; ==============================================================================
bridge_call_rust:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 64
    
    mov r12, rdi                    ; Rust library path
    mov r13, rsi                    ; Function name
    mov rbx, rdx                    ; Arguments pointer
    
    ; Rust FFI implementation via dynamic linking
    ; 1. Load .dll/.so using LoadLibrary/dlopen
    ; 2. Get function pointer using GetProcAddress/dlsym
    ; 3. Call Rust function (uses C ABI)
    ; 4. Return result
    
%ifdef PLATFORM_WIN
    ; LoadLibraryA(library_path)
    mov rcx, r12
    call LoadLibraryA
    test rax, rax
    jz .error
    
    mov r12, rax                    ; Save library handle
    
    ; GetProcAddress(handle, function_name)
    mov rcx, r12
    mov rdx, r13
    call GetProcAddress
    test rax, rax
    jz .error
    
    ; Call Rust function with arguments
    mov rcx, rbx
    call rax
    
    ; Free library
    mov rcx, r12
    call FreeLibrary
    
    jmp .done
%else
    ; Linux: dlopen/dlsym approach
    xor rax, rax
%endif
    
.error:
    xor rax, rax
    mov dword [last_error_code], ERR_OPERATION_FAILED
    
.done:
    add rsp, 64
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; ==============================================================================
; FUNCTION: bridge_call_c
; Call C function from ASM
; Input:
;   RDI = shared library path
;   RSI = function name
;   RDX = arguments pointer
; Output:
;   RAX = result
; ==============================================================================
bridge_call_c:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov r12, rdi                    ; Library path
    mov r13, rsi                    ; Function name
    
    ; Open shared library
    mov rdi, r12
    mov rsi, 2                      ; RTLD_NOW
    call dlopen_wrapper
    
    test rax, rax
    jz .error
    
    mov rbx, rax                    ; Library handle
    
    ; Get function symbol
    mov rdi, rbx
    mov rsi, r13
    call dlsym_wrapper
    
    test rax, rax
    jz .error
    
    ; Call function (assuming no args for now)
    call rax
    
    ; Close library
    mov rdi, rbx
    call dlclose_wrapper
    
    jmp .done
    
.error:
    xor rax, rax
    mov dword [last_error_code], ERR_OPERATION_FAILED
    
.done:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; ==============================================================================
; Dynamic Linking Wrappers (stubs for actual libc calls)
; ==============================================================================

dlopen_wrapper:
    ; Would call dlopen from libc
    ; For now, return dummy handle
    mov rax, 1
    ret

dlsym_wrapper:
    ; Would call dlsym from libc
    xor rax, rax
    ret

dlclose_wrapper:
    ret

; ==============================================================================
; Bridge Initialization Functions
; ==============================================================================

python_bridge_init:
    sub rsp, 40
    mov byte [python_initialized], 1
    xor eax, eax
    add rsp, 40
    ret

rust_bridge_init:
    sub rsp, 40
    mov byte [rust_initialized], 1
    xor eax, eax
    add rsp, 40
    ret

c_bridge_init:
    sub rsp, 40
    mov byte [c_initialized], 1
    xor eax, eax
    add rsp, 40
    ret

; ==============================================================================
; FUNCTION: export_to_python
; Export ASM function table to Python ctypes
; Output:
;   Prints function table for Python import
; ==============================================================================
global export_to_python
export_to_python:
    push rbp
    mov rbp, rsp
    
    lea rdi, [python_export_header]
    call print_string
    
    ; Print function addresses
    lea rdi, [msg_extract]
    call print_string
    lea rdi, [extract_model_profile]
    call print_pointer
    
    lea rdi, [msg_apply_patch]
    call print_string
    lea rdi, [apply_dampening_patch]
    call print_pointer
    
    pop rbp
    ret

print_pointer:
    push rbp
    mov rbp, rsp
    sub rsp, 48              ; enough space for "0x" + 16 hex + null
    mov rdx, rdi             ; preserve pointer value
    lea rdi, [rsp]           ; buffer
    mov rax, rdx             ; pointer value in rax for hex_to_string
    call hex_to_string
    lea rdi, [rsp]
    call print_string
    mov rdi, 10
    call print_char
    add rsp, 48
    pop rbp
    ret

hex_to_string:
    ; rax = value, rdi = buffer
    mov byte [rdi], '0'
    mov byte [rdi+1], 'x'
    mov rcx, 16
    mov rbx, rax
    mov rsi, 0
.hex_loop:
    mov rdx, rbx
    shr rdx, 60              ; top nibble
    cmp rdx, 9
    jbe .digit
    add rdx, 87              ; 'a' - 10
    jmp .store
.digit:
    add rdx, 48              ; '0'
.store:
    mov [rdi+2+rsi], dl
    shl rbx, 4
    inc rsi
    loop .hex_loop
    mov byte [rdi+18], 0
    ret

print_string:
    push rbp
    mov rbp, rsp
    push rdi
    call strlen
    mov rdx, rax
    pop rsi
    mov rdi, 1
    mov rax, 1
    syscall
    pop rbp
    ret

strlen:
    xor rax, rax
.loop:
    cmp byte [rdi+rax], 0
    je .done
    inc rax
    jmp .loop
.done:
    ret

print_char:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rsp], dil
    lea rsi, [rsp]
    mov rdx, 1
    mov rdi, 1
    mov rax, 1
    syscall
    add rsp, 16
    pop rbp
    ret

; (Removed duplicate .bss declarations for initialization flags)

section .rodata
python_export_header:   db "Python FFI Function Table:", 10, 0
msg_extract:            db "extract_model_profile = ", 0
msg_apply_patch:        db "apply_dampening_patch = ", 0

; ------------------------------------------------------------------------------
; FUNCTION: bridge_get_last_error
; Return last_error_code (int32 in EAX)
; ------------------------------------------------------------------------------
global bridge_get_last_error
bridge_get_last_error:
    mov eax, [last_error_code]
    ret
