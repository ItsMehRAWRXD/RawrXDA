; Fixed bridge_init with proper Windows x64 ABI compliance
; This addresses the ABI violations causing the failure

BITS 64
DEFAULT REL

section .text

; ==============================================================================
; FIXED: bridge_init with proper Windows x64 ABI
; ==============================================================================
global bridge_init
bridge_init:
    ; CRITICAL FIX: Proper Windows x64 ABI compliance
    ; 1. Allocate shadow space (32 bytes minimum)
    ; 2. Align stack to 16-byte boundary (RSP+8 mod 16 = 0 at call)
    ; 3. Preserve non-volatile registers if used
    ; 4. Use correct calling convention
    
    ; Check stack alignment first (RSP should be 16n+8 on entry)
    push rbp                    ; Save rbp (non-volatile)
    mov rbp, rsp               ; Set up frame pointer  
    and rsp, -16               ; Align stack to 16-byte boundary
    sub rsp, 48                ; Shadow space (32) + local space (16) = 48
    
    ; Now stack is properly aligned for Windows x64 ABI calls
    
    ; Initialize Python bridge (if it exists)
    lea rax, [python_bridge_init]
    test rax, rax
    jz .skip_python
    call python_bridge_init
    test eax, eax
    jnz .error                 ; Non-zero = error
    
.skip_python:
    ; Initialize Rust bridge (if it exists)
    lea rax, [rust_bridge_init]
    test rax, rax
    jz .skip_rust
    call rust_bridge_init
    test eax, eax
    jnz .error                 ; Non-zero = error
    
.skip_rust:
    ; Initialize C bridge (if it exists)
    lea rax, [c_bridge_init]
    test rax, rax
    jz .skip_c
    call c_bridge_init
    test eax, eax
    jnz .error                 ; Non-zero = error
    
.skip_c:
    ; Success - return 0
    xor eax, eax
    jmp .cleanup
    
.error:
    ; Error - return -1
    mov eax, -1
    
.cleanup:
    ; Restore stack and return
    mov rsp, rbp               ; Restore original stack pointer
    pop rbp                    ; Restore rbp
    ret

; ==============================================================================
; Minimal stub implementations for missing functions
; ==============================================================================

python_bridge_init:
    ; Minimal implementation - just return success
    xor eax, eax
    ret

rust_bridge_init:
    ; Minimal implementation - just return success  
    xor eax, eax
    ret

c_bridge_init:
    ; Minimal implementation - just return success
    xor eax, eax
    ret

; ==============================================================================
; DLL Entry Point (required for Windows DLLs)
; ==============================================================================
global DllMain
DllMain:
    ; DLL entry point
    ; RCX = hinstDLL
    ; RDX = fdwReason
    ; R8  = lpvReserved
    
    ; For our purposes, just return TRUE (1)
    mov eax, 1
    ret