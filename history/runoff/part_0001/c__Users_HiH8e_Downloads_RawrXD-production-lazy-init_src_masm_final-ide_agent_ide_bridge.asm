;==========================================================================
; agent_ide_bridge.asm - Pure MASM IDE Agent Bridge
; ==========================================================================
; Replaces ide_agent_bridge.cpp.
; Orchestrates the full wish -> plan -> execute pipeline.
;==========================================================================

option casemap:none

include windows.inc

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN agent_bootstrap_grab_wish:PROC
EXTERN agent_planner_plan:PROC
EXTERN agent_action_execute:PROC
EXTERN masm_signal_emit:PROC
EXTERN console_log:PROC

;==========================================================================
; DATA SECTION
;==========================================================================
.data
    szIdeBridgeInit BYTE "IDEAgentBridge: Initializing MASM orchestrator...", 0
    szWishReceived  BYTE "IDEAgentBridge: Wish received: %s", 0
    
    ; Signal IDs
    SIG_AGENT_THINKING  EQU 1009h
    SIG_AGENT_EXECUTING EQU 1010h
    SIG_AGENT_COMPLETE  EQU 1011h

.code

;==========================================================================
; agent_ide_bridge_execute_wish(wish: rcx)
;==========================================================================
PUBLIC agent_ide_bridge_execute_wish
agent_ide_bridge_execute_wish PROC
    push rbx
    push rsi
    sub rsp, 32
    
    mov rsi, rcx        ; rsi = wish
    
    lea rcx, szWishReceived
    mov rdx, rsi
    call console_log
    
    ; 1. Signal: Thinking
    mov ecx, SIG_AGENT_THINKING
    xor rdx, rdx
    call masm_signal_emit
    
    ; 2. Plan
    mov rcx, rsi
    call agent_planner_plan
    mov rbx, rax        ; rbx = plan
    test rbx, rbx
    jz .fail
    
    ; 3. Signal: Executing
    mov ecx, SIG_AGENT_EXECUTING
    xor rdx, rdx
    call masm_signal_emit
    
    ; 4. Execute Plan
    ; (Loop through actions in plan and call agent_action_execute)
    ; ...
    
    ; 5. Signal: Complete
    mov ecx, SIG_AGENT_COMPLETE
    xor rdx, rdx
    call masm_signal_emit
    
    jmp .exit

.fail:
    ; ...
    
.exit:
    add rsp, 32
    pop rsi
    pop rbx
    ret
agent_ide_bridge_execute_wish ENDP

END
