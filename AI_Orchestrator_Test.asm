; ==============================================================================
; AI_Orchestrator_Test.asm
; Test harness for DAG-Based Agent Coordinator
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

EXTERN ExitProcess : PROC
EXTERN ROUTE_AGENT_TASK : PROC
EXTERN REGISTER_AGENT : PROC
EXTERN ADD_TASK : PROC
EXTERN ADD_DEPENDENCY : PROC
EXTERN EXECUTE_DAG : PROC
EXTERN GET_AGENT_STATUS : PROC
EXTERN RESET_DAG : PROC
EXTERN GET_EXECUTION_STATS : PROC

; Agent capability IDs
AGENT_RESEARCH      equ 1
AGENT_CODING        equ 2
AGENT_DEBUGGING     equ 3
AGENT_ANALYSIS      equ 4

.DATA
    ALIGN 16
    ; Execution stats buffer
    stats_buffer dq 3 dup(0)
    
    ; Test task IDs
    TASK_RESEARCH   equ 100
    TASK_CODING     equ 200
    TASK_DEBUGGING  equ 300
    TASK_ANALYSIS   equ 400
    
.CODE

PUBLIC main
main proc
    sub rsp, 40
    
    xor rbx, rbx                ; Test pass counter
    
    ; Test 1: Register research agent
    mov rcx, AGENT_RESEARCH
    lea rdx, RESEARCH_HANDLER
    call REGISTER_AGENT
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 2: Register coding agent
    mov rcx, AGENT_CODING
    lea rdx, CODING_HANDLER
    call REGISTER_AGENT
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 3: Register debugging agent
    mov rcx, AGENT_DEBUGGING
    lea rdx, DEBUGGING_HANDLER
    call REGISTER_AGENT
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 4: Route to research agent
    mov rcx, AGENT_RESEARCH
    call ROUTE_AGENT_TASK
    test rax, rax
    jz test_fail
    inc rbx
    
    ; Test 5: Add research task
    mov rcx, TASK_RESEARCH
    mov rdx, AGENT_RESEARCH
    xor r8, r8                  ; Priority 0
    xor r9, r9                  ; No dependencies
    call ADD_TASK
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 6: Add coding task (depends on research)
    mov rcx, TASK_CODING
    mov rdx, AGENT_CODING
    xor r8, r8
    mov r9, 1                   ; 1 dependency
    call ADD_TASK
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 7: Add dependency (research -> coding)
    mov rcx, TASK_RESEARCH
    mov rdx, TASK_CODING
    call ADD_DEPENDENCY
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 8: Execute DAG
    call EXECUTE_DAG
    cmp rax, 2                  ; Should execute 2 tasks
    jne test_fail
    inc rbx
    
    ; Test 9: Check execution stats
    lea rcx, stats_buffer
    call GET_EXECUTION_STATS
    test eax, eax
    jz test_fail
    
    ; Verify stats
    mov rax, stats_buffer[0]    ; Total tasks
    cmp rax, 2
    jne test_fail
    mov rax, stats_buffer[8]    ; Executed
    cmp rax, 2
    jne test_fail
    inc rbx
    
    ; Test 10: Reset DAG
    call RESET_DAG
    cmp rax, 2                  ; Should reset 2 tasks
    jne test_fail
    inc rbx
    
    ; Test 11: Verify reset (check status)
    mov rcx, TASK_RESEARCH
    call GET_AGENT_STATUS
    cmp rax, 0                  ; Should be PENDING (0)
    jne test_fail
    inc rbx
    
    ; All tests passed (11 tests)
    cmp rbx, 11
    jge test_success
    
test_fail:
    mov rcx, 1
    call ExitProcess
    
test_success:
    xor rcx, rcx
    call ExitProcess
    
main endp

; External handler references
EXTERN RESEARCH_HANDLER : PROC
EXTERN CODING_HANDLER : PROC
EXTERN DEBUGGING_HANDLER : PROC
EXTERN ANALYSIS_HANDLER : PROC

end