; ===================================================================
; Professional NASM IDE - Complete ABI-Compliant Bridge System
; Fixes all three critical Windows x64 ABI violations
; ===================================================================

BITS 64
DEFAULT REL

; Required Windows API functions
extern LoadLibraryA
extern GetProcAddress
extern GetLastError
extern GetModuleHandleA

section .data
    ; Bridge initialization status
    bridge_status dd 0
    
    ; DLL names for bridge components
    python_dll_name db "python_bridge.dll", 0
    rust_dll_name db "rust_bridge.dll", 0
    c_dll_name db "c_bridge.dll", 0
    
    ; Function names to load
    python_init_name db "python_bridge_init", 0
    rust_init_name db "rust_bridge_init", 0
    c_init_name db "c_bridge_init", 0
    
    ; Error messages
    init_success_msg db "Bridge initialization successful", 0
    init_error_msg db "Bridge initialization failed", 0
    
    ; DLL handles and function pointers
    python_dll_handle dq 0
    rust_dll_handle dq 0
    c_dll_handle dq 0
    
    python_init_func dq 0
    rust_init_func dq 0
    c_init_func dq 0

section .bss
    error_code resd 1

section .text

; ===================================================================
; FIX #1: PROPER DLL ENTRY POINT
; This is MANDATORY for Windows DLL loading to succeed
; ===================================================================
global DllMain
DllMain:
    ; DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
    ; RCX = hinstDLL, RDX = fdwReason, R8 = lpvReserved
    
    push rbp
    mov rbp, rsp
    and rsp, -16        ; Ensure 16-byte alignment
    sub rsp, 32         ; Shadow space
    
    ; Check the reason for DLL call
    cmp edx, 1          ; DLL_PROCESS_ATTACH
    je .process_attach
    cmp edx, 0          ; DLL_PROCESS_DETACH
    je .process_detach
    cmp edx, 2          ; DLL_THREAD_ATTACH
    je .thread_attach
    cmp edx, 3          ; DLL_THREAD_DETACH
    je .thread_detach
    
    ; Unknown reason, but return success
    mov eax, 1
    jmp .exit
    
.process_attach:
    ; DLL is being loaded - initialize global state
    mov dword [bridge_status], 0    ; Reset status
    mov eax, 1                      ; Return TRUE
    jmp .exit
    
.process_detach:
    ; DLL is being unloaded - cleanup
    call cleanup_bridge_resources
    mov eax, 1                      ; Return TRUE
    jmp .exit
    
.thread_attach:
.thread_detach:
    ; Thread events - just return success
    mov eax, 1
    
.exit:
    mov rsp, rbp
    pop rbp
    ret

; ===================================================================
; FIX #2: PROPER STACK ALIGNMENT AND CALLING CONVENTIONS
; Ensures 16-byte alignment and follows Windows x64 ABI
; ===================================================================
global bridge_init
bridge_init:
    ; Save non-volatile registers (Windows x64 ABI requirement)
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    ; Establish proper stack frame
    push rbp
    mov rbp, rsp
    
    ; CRITICAL: Ensure 16-byte stack alignment
    ; Windows x64 ABI requires RSP+8 to be 16-byte aligned before CALL
    and rsp, -16        ; Force RSP to 16-byte boundary
    sub rsp, 64         ; Allocate shadow space + local variables
    
    ; Initialize status
    mov dword [bridge_status], 1    ; Mark as initializing
    
    ; Load and initialize Python bridge
    call load_python_bridge
    test eax, eax
    jnz .error
    
    ; Load and initialize Rust bridge  
    call load_rust_bridge
    test eax, eax
    jnz .error
    
    ; Load and initialize C bridge
    call load_c_bridge
    test eax, eax
    jnz .error
    
    ; All bridges loaded successfully
    mov dword [bridge_status], 2    ; Mark as initialized
    xor eax, eax                    ; Return success
    jmp .cleanup
    
.error:
    mov dword [bridge_status], -1   ; Mark as failed
    mov eax, -1                     ; Return error
    
.cleanup:
    ; Restore stack and registers (proper ABI cleanup)
    mov rsp, rbp
    pop rbp
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; ===================================================================
; FIX #3: COMPREHENSIVE ERROR HANDLING
; Proper error checking and graceful failure handling
; ===================================================================

load_python_bridge:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; Try to load Python bridge DLL
    lea rcx, [python_dll_name]
    call LoadLibraryA
    test rax, rax
    jz .load_failed
    
    ; Store DLL handle
    mov [python_dll_handle], rax
    
    ; Get function address
    mov rcx, rax
    lea rdx, [python_init_name]
    call GetProcAddress
    test rax, rax
    jz .func_not_found
    
    ; Store function pointer
    mov [python_init_func], rax
    
    ; Call initialization function with proper ABI
    call rax
    test eax, eax
    jnz .init_failed
    
    ; Success
    xor eax, eax
    jmp .exit
    
.load_failed:
    ; DLL load failed - get error code
    call GetLastError
    mov [error_code], eax
    mov eax, 1
    jmp .exit
    
.func_not_found:
    ; Function not found in DLL
    mov dword [error_code], 127     ; ERROR_PROC_NOT_FOUND
    mov eax, 2
    jmp .exit
    
.init_failed:
    ; Function call failed
    mov dword [error_code], eax
    mov eax, 3
    
.exit:
    mov rsp, rbp
    pop rbp
    ret

load_rust_bridge:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; Try to load Rust bridge DLL
    lea rcx, [rust_dll_name]
    call LoadLibraryA
    test rax, rax
    jz .load_failed
    
    mov [rust_dll_handle], rax
    
    ; Get function address
    mov rcx, rax
    lea rdx, [rust_init_name]
    call GetProcAddress
    test rax, rax
    jz .func_not_found
    
    mov [rust_init_func], rax
    
    ; Call initialization function
    call rax
    test eax, eax
    jnz .init_failed
    
    xor eax, eax
    jmp .exit
    
.load_failed:
    call GetLastError
    mov [error_code], eax
    mov eax, 4
    jmp .exit
    
.func_not_found:
    mov dword [error_code], 127
    mov eax, 5
    jmp .exit
    
.init_failed:
    mov dword [error_code], eax
    mov eax, 6
    
.exit:
    mov rsp, rbp
    pop rbp
    ret

load_c_bridge:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; Try to load C bridge DLL
    lea rcx, [c_dll_name]
    call LoadLibraryA
    test rax, rax
    jz .load_failed
    
    mov [c_dll_handle], rax
    
    ; Get function address
    mov rcx, rax
    lea rdx, [c_init_name]
    call GetProcAddress
    test rax, rax
    jz .func_not_found
    
    mov [c_init_func], rax
    
    ; Call initialization function
    call rax
    test eax, eax
    jnz .init_failed
    
    xor eax, eax
    jmp .exit
    
.load_failed:
    call GetLastError
    mov [error_code], eax
    mov eax, 7
    jmp .exit
    
.func_not_found:
    mov dword [error_code], 127
    mov eax, 8
    jmp .exit
    
.init_failed:
    mov dword [error_code], eax
    mov eax, 9
    
.exit:
    mov rsp, rbp
    pop rbp
    ret

; ===================================================================
; CLEANUP AND UTILITY FUNCTIONS
; ===================================================================

cleanup_bridge_resources:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; Reset all handles and status
    mov qword [python_dll_handle], 0
    mov qword [rust_dll_handle], 0
    mov qword [c_dll_handle], 0
    mov qword [python_init_func], 0
    mov qword [rust_init_func], 0
    mov qword [c_init_func], 0
    mov dword [bridge_status], 0
    
    mov rsp, rbp
    pop rbp
    ret

; Get current bridge status
global get_bridge_status
get_bridge_status:
    mov eax, [bridge_status]
    ret

; Get last error code
global get_bridge_error
get_bridge_error:
    mov eax, [error_code]
    ret

; ===================================================================
; EXPORTS SECTION
; Make functions available to external code
; ===================================================================
section .drectve info
    db '/EXPORT:bridge_init', 0
    db '/EXPORT:get_bridge_status', 0
    db '/EXPORT:get_bridge_error', 0
    db '/EXPORT:DllMain', 0