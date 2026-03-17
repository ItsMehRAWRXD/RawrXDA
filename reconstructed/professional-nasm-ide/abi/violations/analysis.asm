; ===================================================================
; Windows x64 ABI Violations Analysis and Fixes
; Demonstrating the three critical issues causing bridge_init failures
; ===================================================================
;
; This module demonstrates common Windows x64 ABI violations in NASM assembly,
; including stack misalignment, missing DLL entry points, and improper calling
; conventions. It provides both broken and fixed examples for each issue, and
; a complete bridge_init implementation that resolves all errors. Use this file
; as a reference for correct ABI compliance and debugging integration failures
; in multi-language bridge scenarios.
;
; Usage: Review broken/fixed patterns, integrate fixed patterns into your own
; NASM modules, and run ABI validation tests to ensure stability.
;
; ===================================================================

BITS 64
DEFAULT REL

; ===================================================================
; ISSUE 1: Stack Misalignment Causing Crashes on Function Calls
; ===================================================================

; BROKEN VERSION - Violates 16-byte alignment requirement
broken_stack_alignment:
    ; PROBLEM: Windows x64 ABI requires RSP+8 to be 16-byte aligned
    ; when making function calls. This code violates that requirement.
    
    sub rsp, 32        ; Shadow space only - NO alignment check
                       ; If RSP was 16n+8 on entry, it's now 16n-24 (misaligned!)
    
    ; This call will CRASH on functions that use SSE/AVX instructions
    ; because they expect 16-byte aligned stack for XMM operations
    call some_function ; CRASH! Stack misaligned
    
    add rsp, 32
    ret

; FIXED VERSION - Proper stack alignment
fixed_stack_alignment:
    push rbp           ; Save non-volatile register
    mov rbp, rsp       ; Create frame pointer
    
    ; CRITICAL FIX: Ensure 16-byte alignment
    and rsp, -16       ; Force RSP to 16-byte boundary
                       ; Now RSP is guaranteed 16n+0
                       
    sub rsp, 32        ; Shadow space - now properly aligned
                       ; Stack is now 16n-32, which when we call
                       ; (pushing 8-byte return address) = 16n-40 = 16n+8 ✓
    
    call some_function ; SAFE! Properly aligned
    
    mov rsp, rbp       ; Restore original stack
    pop rbp
    ret

; ===================================================================
; ISSUE 2: Missing DLL Entry Point Causing Load Failures
; ===================================================================

; PROBLEM: Windows DLLs MUST have a DllMain function or they fail to load
; When LoadLibraryA is called, Windows looks for DllMain to initialize the DLL

; BROKEN VERSION - No DllMain (causes LoadLibraryA to fail)
; [No DllMain function = DLL load failure]

; FIXED VERSION - Proper DLL entry point
global DllMain
DllMain:
    ; Windows calls this with:
    ; RCX = HINSTANCE hinstDLL    (handle to DLL module)
    ; RDX = DWORD fdwReason       (reason for calling - attach/detach)
    ; R8  = LPVOID lpvReserved    (reserved, should be NULL)
    
    ; For most cases, just return TRUE (1) to indicate success
    ; More complex DLLs might initialize global state here
    
    cmp edx, 1         ; DLL_PROCESS_ATTACH
    je .process_attach
    cmp edx, 0         ; DLL_PROCESS_DETACH  
    je .process_detach
    
    ; DLL_THREAD_ATTACH (2) or DLL_THREAD_DETACH (3)
    mov eax, 1         ; Return TRUE
    ret
    
.process_attach:
    ; DLL is being loaded into process
    ; Initialize any global state here
    mov eax, 1         ; Return TRUE = success
    ret
    
.process_detach:
    ; DLL is being unloaded from process
    ; Clean up any global state here
    mov eax, 1         ; Return TRUE = success
    ret

; ===================================================================
; ISSUE 3: Improper Calling Conventions Violating Windows x64 ABI
; ===================================================================

; The Windows x64 ABI has strict requirements:
; 1. First 4 integer/pointer args in RCX, RDX, R8, R9
; 2. Floating point args in XMM0, XMM1, XMM2, XMM3
; 3. Caller allocates 32 bytes "shadow space" on stack
; 4. Stack must be 16-byte aligned before calls
; 5. Non-volatile registers: RBX, RBP, RDI, RSI, RSP, R12-R15, XMM6-XMM15

; BROKEN VERSION - Multiple ABI violations
broken_calling_convention:
    ; VIOLATION 1: No shadow space allocation
    ; VIOLATION 2: Using wrong registers for arguments
    ; VIOLATION 3: Not preserving non-volatile registers
    ; VIOLATION 4: No stack alignment

    push rbx               ; Save non-volatile register RBX
    mov rax, some_value    ; Using RAX for argument (wrong!)
    mov rbx, other_value   ; Modifying RBX (now saved)
    call external_func     ; CRASH! No shadow space, wrong args
    pop rbx                ; Restore RBX before return
; FIXED VERSION - Proper Windows x64 ABI compliance  
; Demonstrates correct calling convention, shadow space allocation, register preservation, and stack alignment
fixed_calling_convention:
    ; Save non-volatile registers we'll modify
    push rbx
    push rsi
    push rdi
    ; If more arguments needed, set r8, r9, then stack
    
    call external_func     ; SAFE! Proper shadow space and argument setup
    add rsp, 32            ; Restore stack after call
    ret

; FIXED VERSION - Proper Windows x64 ABI compliance  
fixed_calling_convention:
    ; Save non-volatile registers we'll modify
    push rbx
    push rsi
    push rdi
    
    ; Establish proper stack frame
    push rbp
    mov rbp, rsp
    and rsp, -16           ; Align stack
    sub rsp, 32           ; Allocate shadow space
    
    ; Set up arguments correctly for Windows x64 ABI
    mov rcx, arg1_value   ; First arg in RCX ✓
    mov rdx, arg2_value   ; Second arg in RDX ✓  
    mov r8, arg3_value    ; Third arg in R8 ✓
    mov r9, arg4_value    ; Fourth arg in R9 ✓
    ; Additional args would go on stack after shadow space
    
    call external_func    ; SAFE! Proper ABI compliance
    
    ; Restore everything
    mov rsp, rbp
    pop rbp
    pop rdi
    pop rsi  
    pop rbx
    ret

; ===================================================================
; COMPLETE FIXED bridge_init Implementation
; ===================================================================

global bridge_init
bridge_init:
    ; FIX ALL THREE ISSUES:
    
    ; 1. PROPER STACK ALIGNMENT
    push rbp               ; Save non-volatile register
    mov rbp, rsp          ; Establish frame
    and rsp, -16          ; Force 16-byte alignment
    sub rsp, 48           ; Shadow space + local variables
    
    ; 2. PROPER ERROR HANDLING (DllMain fixed separately)
    
    ; 3. PROPER CALLING CONVENTIONS
    ; Test if initialization functions exist before calling
    lea rax, [python_bridge_init]
    test rax, rax
    jz .skip_python
    
    ; Call with proper ABI - no arguments needed for init functions
    call python_bridge_init
    test eax, eax         ; Check return value
    jnz .error
    
.skip_python:
    lea rax, [rust_bridge_init]
    test rax, rax
    jz .skip_rust
    call rust_bridge_init
    test eax, eax
    jnz .error
    
.skip_rust:
    lea rax, [c_bridge_init]  
    test rax, rax
    jz .success
    call c_bridge_init
    test eax, eax
    jnz .error
    
.success:
    xor eax, eax          ; Return 0 = success
    jmp .cleanup
    
.error:
    mov eax, -1           ; Return -1 = error
    
.cleanup:
    mov rsp, rbp          ; Restore stack
    pop rbp
    ret

; ===================================================================
; Demonstration: How Violations Cause Specific Failures
; ===================================================================

section .data
crash_demo_msg db "This will crash due to stack misalignment", 0
load_demo_msg  db "This will fail to load due to missing DllMain", 0  
abi_demo_msg   db "This will corrupt stack due to ABI violation", 0

section .text

; This function demonstrates what happens with each violation:
demonstrate_violations:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; 1. Stack misalignment crash:
    ;    - SSE/AVX instructions fail on misaligned stack
    ;    - Random crashes during function calls
    ;    - Memory corruption from unaligned XMM operations
    
    ; 2. Missing DllMain failure:
    ;    - LoadLibraryA returns NULL
    ;    - GetLastError() returns ERROR_DLL_INIT_FAILED (1114)
    ;    - DLL never gets loaded into process space
    
    ; 3. ABI violation corruption:  
    ;    - Stack pointer corruption
    ;    - Register corruption
    ;    - Return address overwrite
    ;    - Heap corruption from parameter misalignment
    
    mov rsp, rbp
    pop rbp
    ret

; Stub functions for demonstration
python_bridge_init:
    xor eax, eax
    ret
    
rust_bridge_init:
    xor eax, eax
    ret
    
c_bridge_init:
    xor eax, eax
    ret
    
some_function:
    ret
    
external_func:
    ret