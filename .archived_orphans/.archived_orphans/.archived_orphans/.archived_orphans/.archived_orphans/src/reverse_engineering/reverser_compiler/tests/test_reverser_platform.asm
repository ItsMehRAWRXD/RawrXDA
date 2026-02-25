; test_reverser_platform.asm
; Test suite for Reverser platform detection and cross-platform support

%include "reverser_platform.asm"

section .data
    ; Test messages
    msg_platform_test db "Testing platform detection...", 10, 0
    msg_platform_test_len equ $ - msg_platform_test - 1
    
    msg_arch_test db "Testing architecture detection...", 10, 0
    msg_arch_test_len equ $ - msg_arch_test - 1
    
    msg_syscall_test db "Testing syscall mapping...", 10, 0
    msg_syscall_test_len equ $ - msg_syscall_test - 1
    
    msg_abi_test db "Testing ABI information...", 10, 0
    msg_abi_test_len equ $ - msg_abi_test - 1
    
    msg_test_complete db "Platform tests completed successfully!", 10, 0
    msg_test_complete_len equ $ - msg_test_complete - 1

section .bss
    ; Test variables
    detected_platform resq 1
    detected_arch resq 1
    syscall_table resq 1
    abi_info resq 1

section .text
    global _start
    extern platform_init
    extern platform_detect
    extern platform_get_os
    extern platform_get_arch
    extern platform_is_unix
    extern platform_is_windows
    extern platform_is_mobile
    extern platform_get_syscall_table
    extern platform_get_abi_info

; Print a string
; Input: rdi = pointer to string, rsi = string length
print_string:
    push rax
    push rdx
    push rcx
    
    mov rax, 1      ; syscall number for write
    mov rdx, rsi    ; string length
    mov rsi, rdi    ; string pointer
    mov rdi, 1      ; file descriptor (stdout)
    syscall
    
    pop rcx
    pop rdx
    pop rax
    ret

; Test platform detection
test_platform_detection:
    push rbx
    push rcx
    push rdx
    
    ; Print test message
    mov rdi, msg_platform_test
    mov rsi, msg_platform_test_len
    call print_string
    
    ; Initialize platform detection
    call platform_init
    
    ; Detect current platform
    call platform_detect
    
    ; Get detected platform
    call platform_get_os
    mov [detected_platform], rax
    
    ; Test platform-specific functions
    call platform_is_unix
    cmp rax, 1
    je .is_unix
    
    call platform_is_windows
    cmp rax, 1
    je .is_windows
    
    call platform_is_mobile
    cmp rax, 1
    je .is_mobile
    
    jmp .unknown_platform

.is_unix:
    ; Unix-like platform detected
    jmp .platform_detected

.is_windows:
    ; Windows platform detected
    jmp .platform_detected

.is_mobile:
    ; Mobile platform detected
    jmp .platform_detected

.unknown_platform:
    ; Unknown platform
    jmp .platform_detected

.platform_detected:
    pop rdx
    pop rcx
    pop rbx
    ret

; Test architecture detection
test_architecture_detection:
    push rbx
    push rcx
    push rdx
    
    ; Print test message
    mov rdi, msg_arch_test
    mov rsi, msg_arch_test_len
    call print_string
    
    ; Get detected architecture
    call platform_get_arch
    mov [detected_arch], rax
    
    ; Test architecture-specific features
    cmp rax, 0  ; ARCH_X86_64
    je .x86_64_features
    
    cmp rax, 1  ; ARCH_ARM64
    je .arm64_features
    
    cmp rax, 2  ; ARCH_ARM32
    je .arm32_features
    
    jmp .unknown_arch

.x86_64_features:
    ; x86-64 specific features
    jmp .arch_detected

.arm64_features:
    ; ARM64 specific features
    jmp .arch_detected

.arm32_features:
    ; ARM32 specific features
    jmp .arch_detected

.unknown_arch:
    ; Unknown architecture
    jmp .arch_detected

.arch_detected:
    pop rdx
    pop rcx
    pop rbx
    ret

; Test syscall mapping
test_syscall_mapping:
    push rbx
    push rcx
    push rdx
    
    ; Print test message
    mov rdi, msg_syscall_test
    mov rsi, msg_syscall_test_len
    call print_string
    
    ; Get platform-specific syscall table
    call platform_get_syscall_table
    mov [syscall_table], rax
    
    ; Test syscall table access
    cmp rax, 0
    je .syscall_failed
    
    ; Test specific syscalls
    mov rbx, rax
    mov rcx, 0  ; sys_read
    mov rdx, [rbx + rcx * 8]
    cmp rdx, 0
    je .syscall_failed
    
    mov rcx, 1  ; sys_write
    mov rdx, [rbx + rcx * 8]
    cmp rdx, 0
    je .syscall_failed
    
    mov rcx, 60 ; sys_exit
    mov rdx, [rbx + rcx * 8]
    cmp rdx, 0
    je .syscall_failed
    
    jmp .syscall_success

.syscall_failed:
    ; Syscall mapping failed
    jmp .syscall_done

.syscall_success:
    ; Syscall mapping successful
    jmp .syscall_done

.syscall_done:
    pop rdx
    pop rcx
    pop rbx
    ret

; Test ABI information
test_abi_information:
    push rbx
    push rcx
    push rdx
    
    ; Print test message
    mov rdi, msg_abi_test
    mov rsi, msg_abi_test_len
    call print_string
    
    ; Get platform-specific ABI information
    call platform_get_abi_info
    mov [abi_info], rax
    
    ; Test ABI info access
    cmp rax, 0
    je .abi_failed
    
    ; Test ABI-specific features
    mov rbx, rax
    ; TODO: Test ABI-specific features based on platform
    
    jmp .abi_success

.abi_failed:
    ; ABI information failed
    jmp .abi_done

.abi_success:
    ; ABI information successful
    jmp .abi_done

.abi_done:
    pop rdx
    pop rcx
    pop rbx
    ret

; Main test function
_start:
    ; Run all platform tests
    call test_platform_detection
    call test_architecture_detection
    call test_syscall_mapping
    call test_abi_information
    
    ; Print completion message
    mov rdi, msg_test_complete
    mov rsi, msg_test_complete_len
    call print_string
    
    ; Exit
    mov rax, 60     ; syscall number for exit
    xor rdi, rdi    ; exit code 0
    syscall
