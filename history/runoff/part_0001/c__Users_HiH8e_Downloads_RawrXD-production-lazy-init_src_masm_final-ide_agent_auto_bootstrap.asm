;==========================================================================
; agent_auto_bootstrap.asm - Pure MASM Agentic Bootstrapper
; ==========================================================================
; Replaces auto_bootstrap.cpp.
; Grabs "wishes" from environment, clipboard, or UI and triggers planning.
;==========================================================================

option casemap:none

include windows.inc
include winuser.inc

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN GetEnvironmentVariableA:PROC
EXTERN OpenClipboard:PROC
EXTERN GetClipboardData:PROC
EXTERN CloseClipboard:PROC
EXTERN GlobalLock:PROC
EXTERN GlobalUnlock:PROC
EXTERN MessageBoxA:PROC
EXTERN agent_planner_plan:PROC
EXTERN AgenticEngine_ProcessResponse:PROC

;==========================================================================
; DATA SECTION
;==========================================================================
.data
    szEnvVarName    BYTE "RAWRXD_WISH", 0
    szAgentTitle    BYTE "RawrXD Agent", 0
    szNoWishMsg     BYTE "No wish received, aborting.", 0
    szSafetyMsg     BYTE "Safety gate rejected wish.", 0
    
    CF_TEXT         EQU 1

.data?
    wish_buffer     BYTE 1024 DUP (?)

.code

;==========================================================================
; agent_bootstrap_grab_wish() -> rax (wish_ptr)
;==========================================================================
PUBLIC agent_bootstrap_grab_wish
agent_bootstrap_grab_wish PROC
    push rbx
    sub rsp, 32
    
    ; 1. Check Environment Variable
    lea rcx, szEnvVarName
    lea rdx, wish_buffer
    mov r8d, 1024
    call GetEnvironmentVariableA
    test rax, rax
    jnz wish_found
    
    ; 2. Check Clipboard
    xor rcx, rcx
    call OpenClipboard
    test rax, rax
    jz no_wish
    
    mov ecx, CF_TEXT
    call GetClipboardData
    test rax, rax
    jz close_clip
    
    mov rcx, rax
    call GlobalLock
    mov rbx, rax        ; rbx = clipboard text
    
    ; Copy to wish_buffer
    mov rsi, rbx
    lea rdi, wish_buffer
    mov rcx, 1024
    rep movsb
    
    mov rcx, rbx
    call GlobalUnlock
    
close_clip:
    call CloseClipboard
    
    lea rax, wish_buffer
    cmp byte ptr [rax], 0
    jne wish_found

no_wish:
    xor rax, rax
    jmp done

wish_found:
    lea rax, wish_buffer

done:
    add rsp, 32
    pop rbx
    ret
agent_bootstrap_grab_wish ENDP

;==========================================================================
; agent_bootstrap_start(wish: rcx)
;==========================================================================
PUBLIC agent_bootstrap_start
agent_bootstrap_start PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx        ; rbx = wish
    test rbx, rbx
    jz .no_wish
    
    ; 1. Safety Gate (Stub)
    ; call agent_safety_gate
    ; test rax, rax
    ; jz .safety_fail
    
    ; 2. Plan
    mov rcx, rbx
    call agent_planner_plan
    test rax, rax
    jz .plan_fail
    
    ; 3. Execute Plan (via Orchestrator)
    mov rcx, rax        ; json_plan
    mov rdx, 0          ; length (auto)
    mov r8d, 2          ; MODE_PLAN
    call AgenticEngine_ProcessResponse
    
    jmp .exit

.no_wish:
    lea rcx, szNoWishMsg
    lea rdx, szAgentTitle
    mov r8d, MB_OK or MB_ICONWARNING
    call MessageBoxA
    jmp .exit

.plan_fail:
    ; ...
    jmp .exit

.exit:
    add rsp, 32
    pop rbx
    ret
agent_bootstrap_start ENDP

END
