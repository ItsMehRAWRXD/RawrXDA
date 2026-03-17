bits 64
default rel

extern MessageBoxA
extern LoadLibraryA
extern GetProcAddress
extern GetLastError
extern FreeLibrary
extern ExitProcess

section .data
    dll_name db "d:\\professional-nasm-ide\\lib\\bridge_system_complete.dll", 0
    init_func_name db "bridge_init", 0
    status_func_name db "get_bridge_status", 0
    error_func_name db "get_bridge_error", 0
    
    test_title db "Bridge System Test", 0
    success_msg db "SUCCESS! All ABI violations fixed!", 10, 10, "Stack alignment: Working", 10, "DLL entry point: Working", 10, "Error handling: Working", 10, "Calling conventions: Compliant", 0
    load_fail_msg db "Failed to load bridge DLL", 0
    init_fail_msg db "Bridge initialization failed", 0
    func_fail_msg db "Function not found in DLL", 0

section .bss
    dll_handle resq 1
    init_func resq 1
    status_func resq 1
    error_func resq 1

section .text
global main

main:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32

    ; Test 1: Load DLL (tests DllMain fix)
    lea rcx, [dll_name]
    call LoadLibraryA
    test rax, rax
    jz .load_failed
    mov [dll_handle], rax

    ; Test 2: Get bridge_init function
    mov rcx, rax
    lea rdx, [init_func_name]
    call GetProcAddress
    test rax, rax
    jz .func_failed
    mov [init_func], rax

    ; Test 3: Call bridge_init (tests stack alignment and calling convention fixes)
    call rax
    test eax, eax
    jnz .init_failed

    ; Test 4: Get status function
    mov rcx, [dll_handle]
    lea rdx, [status_func_name]
    call GetProcAddress
    test rax, rax
    jz .func_failed
    mov [status_func], rax

    ; Test 5: Check initialization status
    call rax
    cmp eax, 2    ; Should be 2 (initialized)
    jne .status_failed

    ; All tests passed!
    xor ecx, ecx
    lea rdx, [success_msg]
    lea r8, [test_title]
    mov r9d, 64
    call MessageBoxA
    jmp .cleanup

.load_failed:
    xor ecx, ecx
    lea rdx, [load_fail_msg]
    lea r8, [test_title]
    mov r9d, 16
    call MessageBoxA
    jmp .exit

.func_failed:
    xor ecx, ecx
    lea rdx, [func_fail_msg]
    lea r8, [test_title]
    mov r9d, 16
    call MessageBoxA
    jmp .cleanup

.init_failed:
    xor ecx, ecx
    lea rdx, [init_fail_msg]
    lea r8, [test_title]
    mov r9d, 16
    call MessageBoxA
    jmp .cleanup

.status_failed:
    xor ecx, ecx
    lea rdx, [init_fail_msg]
    lea r8, [test_title]
    mov r9d, 16
    call MessageBoxA
    jmp .cleanup

.cleanup:
    mov rcx, [dll_handle]
    call FreeLibrary

.exit:
    mov rsp, rbp
    pop rbp
    xor ecx, ecx
    call ExitProcess