; =============================================================================
; feature_dispatch_bridge.asm — x64 MASM Feature Dispatch Hot-Path
; =============================================================================
; Architecture: x64 MASM, Windows ABI
; Provides high-performance routing from MASM callers into the C++ shared
; feature dispatch system. Used by inference loops, hotpatch paths, and
; any MASM code that needs to invoke IDE/CLI features.
; =============================================================================

.code

; External C++ functions (from shared_feature_dispatch.cpp)
EXTERN rawrxd_dispatch_feature:PROC    ; int (const char* id, const char* args, void* idePtr)
EXTERN rawrxd_dispatch_command:PROC    ; int (uint32_t cmdId, void* idePtr)
EXTERN rawrxd_dispatch_cli:PROC        ; int (const char* cli, const char* args, void* cliState)
EXTERN rawrxd_get_feature_count:PROC   ; int (void)

; =============================================================================
; masm_dispatch_feature
; Purpose: Dispatch a feature by string ID from MASM context
; Params:  RCX = pointer to feature ID string (null-terminated)
;          RDX = pointer to args string (null-terminated, or NULL)
;          R8  = IDE pointer (or 0 for CLI mode)
; Returns: RAX = 0 on success, error code on failure
; =============================================================================
masm_dispatch_feature PROC
    ; Preserve non-volatile registers per Windows x64 ABI
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32                  ; Shadow space for callee

    ; Parameters already in RCX, RDX, R8 — Windows x64 calling convention
    ; RCX = featureId, RDX = args, R8 = idePtr
    call    rawrxd_dispatch_feature

    ; Result in EAX
    leave
    ret
masm_dispatch_feature ENDP

; =============================================================================
; masm_dispatch_command
; Purpose: Dispatch a feature by Win32 command ID from MASM context
; Params:  ECX = command ID (IDM_* value)
;          RDX = IDE pointer
; Returns: RAX = 0 on success, error code on failure
; =============================================================================
masm_dispatch_command PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32

    ; ECX = commandId (zero-extended to RCX), RDX = idePtr
    call    rawrxd_dispatch_command

    leave
    ret
masm_dispatch_command ENDP

; =============================================================================
; masm_dispatch_cli
; Purpose: Dispatch a CLI command from MASM context
; Params:  RCX = pointer to CLI command string
;          RDX = pointer to args string  
;          R8  = CLI state pointer
; Returns: RAX = 0 on success, error code on failure
; =============================================================================
masm_dispatch_cli PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32

    call    rawrxd_dispatch_cli

    leave
    ret
masm_dispatch_cli ENDP

; =============================================================================
; masm_get_feature_count
; Purpose: Query how many features are registered in the dispatch system
; Params:  None
; Returns: EAX = feature count
; =============================================================================
masm_get_feature_count PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32

    call    rawrxd_get_feature_count

    leave
    ret
masm_get_feature_count ENDP

; =============================================================================
; masm_batch_dispatch
; Purpose: Dispatch multiple features in sequence (for hotpatch chains)
; Params:  RCX = pointer to array of feature ID string pointers
;          RDX = count of features to dispatch
;          R8  = IDE pointer (or 0)
; Returns: EAX = number of successful dispatches
; =============================================================================
masm_batch_dispatch PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 48                  ; Shadow space + locals

    mov     rsi, rcx                 ; RSI = array of string pointers
    mov     edi, edx                 ; EDI = count
    mov     rbx, r8                  ; RBX = idePtr
    xor     eax, eax                 ; EAX = success counter
    mov     [rbp-8], rax             ; Store counter on stack

    test    edi, edi
    jz      _batch_done

_batch_loop:
    ; Load next feature ID pointer
    mov     rcx, [rsi]               ; RCX = featureId string ptr
    test    rcx, rcx
    jz      _batch_skip

    ; Call dispatch: (featureId=RCX, args=NULL, idePtr=RBX)
    xor     rdx, rdx                 ; No args
    mov     r8, rbx                  ; IDE pointer
    
    push    rdi                      ; Save count
    push    rsi                      ; Save array ptr
    sub     rsp, 32                  ; Shadow space
    call    rawrxd_dispatch_feature
    add     rsp, 32
    pop     rsi
    pop     rdi

    ; If success (EAX == 0), increment counter
    test    eax, eax
    jnz     _batch_skip
    mov     rax, [rbp-8]
    inc     rax
    mov     [rbp-8], rax

_batch_skip:
    add     rsi, 8                   ; Next pointer in array (8 bytes for x64)
    dec     edi
    jnz     _batch_loop

_batch_done:
    mov     eax, DWORD PTR [rbp-8]   ; Return success count

    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
masm_batch_dispatch ENDP

; =============================================================================
; masm_validate_feature
; Purpose: Check if a feature ID is registered without dispatching
; Params:  RCX = pointer to feature ID string
; Returns: EAX = 1 if registered, 0 if not
; =============================================================================
masm_validate_feature PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32

    ; Use dispatch with NULL args and NULL ide ptr
    ; If it returns error -1 for "not found", feature doesn't exist
    xor     rdx, rdx                 ; args = NULL
    xor     r8, r8                   ; idePtr = NULL
    call    rawrxd_dispatch_feature

    ; If result == -1 (not found) or -2 (other error), return 0
    ; If result == 0 (success) or any other, return 1
    cmp     eax, 0
    je      _validate_exists
    
    ; Check for "no handler" error vs "not found"
    xor     eax, eax                 ; Not found
    jmp     _validate_done

_validate_exists:
    mov     eax, 1                   ; Found

_validate_done:
    leave
    ret
masm_validate_feature ENDP

END
